/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

/* 
   $Id: l2cap.c,v 1.196 2004/07/16 18:58:52 chineape Exp $

   Link Layer Control and Adaptation Protocol

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
		Imre Deak <ext-imre.deak@nokia.com>
*/		


/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <asm/bitops.h>

/* Local Includes */
#define FILEBIT	DBL2CAP

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>


/* L2CAP layer attributes */
btlist_head_t	l2cap_protos;		/* upper protocol list */
btlist_head_t	l2cap_chs;		/* L2CAP channel list */

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
l2cap_groups_t	l2cap_grps;		/* L2CAP group struct */
#endif

__u16		lcid = 0x40;
__u8		cmd_id = 0x00;
__u16		lpsm = 0x1001;

/* PSM value 0x1001 - 0xFFFF - dynamic */

int		sysctl_l2cap_mtu = L2CAP_MTU;
int		sysctl_l2cap_cl_mtu = L2CAP_MTU;
int		sysctl_l2cap_flush_to = 0xFFFF;
#ifdef CONFIG_AFFIX_BT_1_2
int		sysctl_l2cap_mps = L2CAP_MAX_MPS;
int 		sysctl_l2cap_modes = L2CAP_LOCAL_EXT_FEATURES_MASK;
int		sysctl_l2cap_txwindow = 32;
int 		sysctl_l2cap_max_transmit = 32;
int 		sysctl_l2cap_ret_timeout = 1000;
int 		sysctl_l2cap_mon_timeout = 1000;
#endif


/* ------------------------------------------------------------- */
void l2cap_purge_req_queue(l2cap_ch *ch);
int __l2ca_config_req(l2cap_ch *ch);


static inline __u8 __alloc_id(l2cap_ch *ch)
{
	if (++cmd_id == 0)
		cmd_id++;
	return cmd_id;
}

static inline void l2cap_config_start(l2cap_ch *ch)
{
	ENTERSTATE(ch, CON_CONFIG);
	atomic_set(&ch->cfgreq_count, 0);
	clear_bit(L2CAP_FLAGS_CFGIN_DONE, &ch->flags);
	clear_bit(L2CAP_FLAGS_CFGOUT_DONE, &ch->flags);
	clear_bit(L2CAP_FLAGS_INFOREQ_DONE,&ch->flags);
}


/************    Client Protocol Management ***************/


l2cap_proto *__l2cap_proto_lookup(__u16 psm)
{
	l2cap_proto	*proto;

	btl_for_each (proto, l2cap_protos) {
		if (proto->psm == psm)
			break;
	}
	if (!proto) {
		btl_for_each (proto, l2cap_protos) {
			if (proto->psm == 0xFFFF)
				break;
		}
	}
	return proto;
}

int l2ca_register_protocol(__u16 psm, l2cap_proto_ops *pops)
{
	int		err = -EINVAL;
	l2cap_proto	*proto;
	__u16		counter;

	DBFENTER;
	btl_write_lock(&l2cap_protos);
	if (psm == 0) {
		/* dynamically */
		proto = NULL;
		for (counter = 0x1001; counter > 0x1000; counter += 2) {
			proto = __l2cap_proto_lookup(lpsm);
			psm = lpsm;
			lpsm += 2;
			if (lpsm < 0x1001)
				lpsm = 0x1001;
			if (!proto)
				break;
		}
	} else
		proto = __l2cap_proto_lookup(psm);
	if (!proto) {
		proto = kmalloc(sizeof(*proto), GFP_ATOMIC);
		if (proto == NULL) {
			err = -ENOMEM;
			goto exit;
		}
		proto->psm = psm;
		proto->ops = pops;
		proto->disabled = 0;
		__btl_add_tail(&l2cap_protos, proto);
		err = 0;
	}
exit:
	btl_write_unlock(&l2cap_protos);
	DBFEXIT;
	return (!err) ? psm : err;
}

void l2ca_unregister_protocol(__u16 psm)
{
	l2cap_proto	*proto;

	DBFENTER;
	btl_write_lock(&l2cap_protos);
	proto = __l2cap_proto_lookup(psm);
	if (proto) {
		__btl_unlink(&l2cap_protos, proto);
		kfree(proto);
	}
	btl_write_unlock(&l2cap_protos);
	DBFEXIT;
}

void l2ca_disable_protocol(__u16 psm, int disable)
{
	l2cap_proto	*proto;

	DBFENTER;
	btl_write_lock(&l2cap_protos);
	proto = __l2cap_proto_lookup(psm);
	if (proto)
		proto->disabled = disable;
	btl_write_unlock(&l2cap_protos);
	DBFEXIT;
}


/*******************************   Channel Management  ************************/


#ifdef CONFIG_AFFIX_BT_1_2
#include "l2cap_rm.c"
#endif 

static int l2cap_release(l2cap_ch *ch)
{
	int	state = SETSTATE(ch, DEAD);
	l2cap_stop_timer(ch);
#ifdef CONFIG_AFFIX_BT_1_2
	l2cap_stop_monitor_timer(ch);
	l2cap_stop_retransmission_timer(ch);
	/*l2cap_onedottwo_close(ch);*/
#endif
	l2cap_purge_req_queue(ch);
	return state;
}

/*
   L2CAP timer
*/
void l2cap_timer(unsigned long data)
{
	l2cap_ch	*ch = (l2cap_ch*)data;

	DBFENTER;
	switch(l2cap_release(ch)) {
		case CON_W4_CONRSP:
			l2ca_connect_cfm(ch, AFFIX_ERR_TIMEOUT, 0);
			break;
		case CON_CONFIG:
			if (test_and_clear_bit(L2CAP_FLAGS_CONFIG, &ch->flags))
				l2ca_config_cfm(ch, AFFIX_ERR_TIMEOUT);
			else
				l2ca_connect_cfm(ch, AFFIX_ERR_TIMEOUT, 0);
			break;
		default:
			BTDEBUG("State is not handled!!!\n");
			break;
	}
	l2ca_put(ch);
	DBFEXIT;
}



/*
   probably the reason to use "con"
*/
l2cap_ch * __l2cap_lookup(__u16 cid)
{
	l2cap_ch	*ch;

	DBFENTER;
	btl_for_each (ch, l2cap_chs) {
		DBPRT("Looking up for l2cap channel with CID: %x, Current channel: %x, STATE: %x, CHANNEL LCID: %x\n",cid,(int)ch,(int)ch->state,ch->lcid);
		if (STATE(ch) != DEAD && ch->lcid == cid){
			DBPRT("Found it!!!!\n");
			DBFEXIT;
			return ch;
		}
	}
	DBPRT("None found !!!!\n");
	DBFEXIT;
	return NULL;
}

l2cap_ch * l2cap_lookup(__u16 cid)
{
	l2cap_ch	*ch;

	btl_read_lock(&l2cap_chs);
	ch = __l2cap_lookup(cid);
	if (ch)
		l2ca_hold(ch);
	btl_read_unlock(&l2cap_chs);
	return ch;
}

static inline __u16 __alloc_cid(void)
{
	__u16		cid, counter;
	l2cap_ch	*ch;

	DBFENTER;
	for (counter = 0x40; counter >= 0x40 || (cid = L2CAP_CID_NULL); counter++) {
		cid = lcid++;
		if (lcid < 0x40)
			lcid = 0x40;
		if ((ch = __l2cap_lookup(cid)) == NULL)
			break;
	}
	DBFEXIT;
	return cid;
}

/*
 * Create the channel object
 */

l2cap_ch *l2ca_create(l2cap_proto_ops *pops)
{
	l2cap_ch	*ch;

	DBFENTER;

	ch = kmalloc(sizeof(l2cap_ch), GFP_ATOMIC);
	if (!ch)
		return NULL;
	memset(ch, 0, sizeof(l2cap_ch));

	btl_write_lock(&l2cap_chs);
	ch->lcid = __alloc_cid();
	if (ch->lcid == L2CAP_CID_NULL) {
		kfree(ch);
		ch = NULL;
		goto exit;
	}
        if (!try_module_get(pops->owner)) {
		kfree(ch);
		ch = NULL;
		goto exit;
	}
	atomic_set(&ch->refcnt, 1);
	spin_lock_init(&ch->callback_lock);
	ch->callback_cpu = -1;
	ch->ops = pops;			/* proto ops */

	//skb_queue_head_init(&ch->write_queue);
	skb_queue_head_init(&ch->req_queue);

	/* Initialize some members */
	init_timer(&ch->timer);
	ch->timer.data = (unsigned long)ch;
	ch->timer.function = l2cap_timer;

	/* set protocol default values */
	l2cap_default_cfgopt(&ch->cfgin);
	l2cap_default_cfgopt(&ch->cfgout);

#ifdef CONFIG_AFFIX_BT_1_2
	/* set l2cap 1.2 default values */
	l2cap_onedottwo_init(ch);
#endif
	
	ENTERSTATE(ch, CON_CLOSED);		/* initial state */
	__btl_add_tail(&l2cap_chs, ch);		/* add to list */
exit:
	btl_write_unlock(&l2cap_chs);
	DBFEXIT;
	return ch;
}

l2cap_ch *l2cap_create(__u16 psm, int *err)
{
	l2cap_proto	*proto;
	l2cap_ch	*ch = NULL;

	btl_read_lock(&l2cap_protos);
	proto = __l2cap_proto_lookup(psm);
	if (proto) {
		*err = 0;
		if (proto->disabled)
			goto exit;
		ch = l2ca_create(proto->ops);
		if (!ch) /* No resources */
			*err = -ENOMEM;
		ch->psm = psm;
	} else {
		*err = -EPROTONOSUPPORT;
		ch = NULL;
	}
exit:
	btl_read_unlock(&l2cap_protos);
	return ch;
}

void __l2cap_destroy(l2cap_ch *ch)
{
	DBFENTER;
	__btl_unlink(&l2cap_chs, ch);
	skb_queue_purge(&ch->req_queue);
#ifdef CONFIG_AFFIX_BT_1_2
	l2cap_onedottwo_close(ch);
#endif
	if (ch->con)
		hcc_put(ch->con);
	if (ch->hci)
		hci_put(ch->hci);
	if (ch->ops)  {
	        module_put(ch->ops->owner);
	}
	kfree(ch);
	DBFEXIT;
}

static inline void __l2ca_put(l2cap_ch *ch)
{
	if (atomic_dec_and_test(&ch->refcnt))
		__l2cap_destroy(ch);
}

void l2ca_put(l2cap_ch *ch)
{
	DBFENTER;
	if (atomic_read(&ch->refcnt) == 1 || ch->priv == NULL) {
		/* orphan */
		l2ca_disconnect_req(ch);
	}
	if (atomic_dec_and_test(&ch->refcnt)) {
		btl_write_lock(&l2cap_chs);
		if (atomic_read(&ch->refcnt) == 0)
			__l2cap_destroy(ch);
		btl_write_unlock(&l2cap_chs);
	}
	DBFEXIT;
}

/* ------------------------------------------------------------- */

void l2cap_skb_timer(unsigned long data)
{
	struct sk_buff	*skb = (void*)data;
	l2cap_ch	*ch = l2cap_cb(skb)->ch;
	l2cap_cmd_t 	*req =(l2cap_cmd_t *)skb->data;
	
	kfree_skb(skb);	/* refcnt-- */
	spin_lock_bh(&ch->req_queue.lock);
	if (!skb->list) {
		spin_unlock_bh(&ch->req_queue.lock);
		l2ca_put(ch);
		return;
	}
	__skb_unlink(skb, skb->list);
	spin_unlock_bh(&ch->req_queue.lock);
	/* do processing */
#ifdef CONFIG_AFFIX_BT_1_2
	/* we need this to avoid misbehaving 1.1 devices */
	if ((req->code == L2CAP_SIG_INFOREQ) && (STATE(ch) == CON_CONFIG)) {
		set_bit(L2CAP_FLAGS_INFOREQ_DONE ,&ch->flags);
		/*set_bit(L2CAP_CFGOPT_MTU,&ch->cfgout.flags);*/
		clear_bit(L2CAP_CFGOPT_RFC,&ch->cfgout.flags);
		__l2ca_config_req(ch);
	}
	else {
		l2cap_timer((unsigned long)ch);	// release a channel
	}
	
#else 
	l2cap_timer((unsigned long)ch);	// release a channel
#endif
	kfree_skb(skb);		/* free request */
}

/*
 * delete req from the queue
 * stop the timer
 */
void __l2cap_del_req(struct sk_buff *skb)
{
	if (skb->list)
		__skb_unlink(skb, skb->list);
	if (del_timer(&l2cap_cb(skb)->timer)) {
		__l2ca_put(l2cap_cb(skb)->ch);
		kfree_skb(skb);	// refcnt
	}
}

void l2cap_del_req(l2cap_ch *ch, struct sk_buff *skb)
{
	DBFENTER;
	spin_lock_bh(&ch->req_queue.lock);
	if (skb->list) {
		__l2cap_del_req(skb);
		kfree_skb(skb);	// l2cap_add_req got a ref
	}
	spin_unlock_bh(&ch->req_queue.lock);
	DBFEXIT;
}

void l2cap_purge_req_queue(l2cap_ch *ch)
{
	struct sk_buff	*skb;
	DBFENTER;
	while ((skb = skb_dequeue(&ch->req_queue))) {
		__l2cap_del_req(skb);
		kfree_skb(skb);
	}
	DBFEXIT;
}

/*
 * it locks the request queue and exit
 */
struct sk_buff *__l2cap_get_req(hci_con *con, __u8 ident, __u16 lcid, l2cap_ch **ch)
{
	struct sk_buff	*skb;
	
	btl_for_each (*ch, l2cap_chs) {
		DBPRT("ch: %p, %d = %d, state: %d\n", *ch, lcid, (*ch)->lcid, STATE(*ch));
		if (__is_dead(*ch) || 
				(con && (*ch)->con != con) || (lcid && (*ch)->lcid != lcid)) {
			continue;
		}
		DBPRT("req queue len: %d\n", (*ch)->req_queue.qlen);
		spin_lock_bh(&(*ch)->req_queue.lock);
		btl_for_each (skb, (*ch)->req_queue) {
			if (l2cap_cb(skb)->ident == ident)
				return skb;	// queue locked
		}
		spin_unlock_bh(&(*ch)->req_queue.lock);
	}
	return NULL;
}

struct sk_buff *l2cap_get_req(hci_con *con, __u8 ident, __u16 lcid, l2cap_ch **ch)
{
	struct sk_buff	*skb;

	DBFENTER;
	btl_read_lock(&l2cap_chs);
	skb = __l2cap_get_req(con, ident, lcid, ch);
	DBPRT("found: %p\n", skb);
	if (skb) {
		l2ca_hold(*ch);
		/* queue locked */
		__l2cap_del_req(skb);
		spin_unlock_bh(&(*ch)->req_queue.lock);
	}
	btl_read_unlock(&l2cap_chs);
	DBFEXIT;
	return skb;
}

void l2cap_add_req(l2cap_ch *ch, int ident, struct sk_buff *skb, 
		unsigned long timeout, void (*func)(unsigned long data))
{
	struct timer_list	*timer;
	DBFENTER;
	spin_lock_bh(&ch->req_queue.lock);
	skb_get(skb);	// get a reference
	l2cap_cb(skb)->ch = ch;
	l2cap_cb(skb)->ident = ident;
	/* init timer */
	timer = &l2cap_cb(skb)->timer;
	init_timer(timer);
	timer->data = (unsigned long)skb;
	timer->function = func;
	l2cap_cb(skb)->timeout = timeout;
	__skb_queue_tail(&ch->req_queue, skb);
	if (timeout && __hcc_linkup(ch->con)) {
		l2ca_hold(ch);
		skb_get(skb);
		DBPRT("ch: %p, starting timer...\n", ch);
		mod_timer(timer, jiffies + timeout);
	}
	spin_unlock_bh(&ch->req_queue.lock);
	DBFEXIT;
}

void l2cap_reset_req_queue(l2cap_ch *ch)
{
	struct sk_buff	*skb;
	DBFENTER;
	spin_lock_bh(&ch->req_queue.lock);
	btl_for_each (skb, ch->req_queue) {
		if (!l2cap_cb(skb)->timeout)
			continue;
		DBPRT("ch: %p, starting timer...\n", ch);
		if (!mod_timer(&l2cap_cb(skb)->timer, jiffies + l2cap_cb(skb)->timeout)) {
			l2ca_hold(ch);
			skb_get(skb);
		}
	}
	spin_unlock_bh(&ch->req_queue.lock);
	DBFEXIT;
}

/****************************  SIGNALING and DATA SENDING  ***********************/

int l2cap_send_data(l2cap_ch *ch, struct sk_buff *skb)
{
	__u16		len = skb->len;
	l2cap_hdr_t	*hdr;
	int		err;

	DBFENTER;
	hdr = (l2cap_hdr_t*)skb_push(skb, L2CAP_HDR_LEN);
	hdr->length = __htob16(len);
	hdr->cid = __htob16(ch->rcid);
	DBPRT("send L2CAP Packet, len = %d\n", skb->len);
	err = lp_send_data(ch->con, skb);
	DBFEXIT;
	return err;
}

int l2cap_send_signal(hci_con *con, struct sk_buff *skb)
{
	__u16		len = skb->len;
	l2cap_hdr_t	*hdr;
	int		err;

	DBFENTER;
	hdr = (l2cap_hdr_t*)skb_push(skb, L2CAP_HDR_LEN);
	hdr->length = __htob16(len);
	hdr->cid = __htob16(L2CAP_CID_SIGNAL);
	DBPRT("Send L2CAP Packet, len = %d\n", skb->len);
	DBDUMP(skb->data, skb->len);
	err = lp_send_data(con, skb);
	DBFEXIT;
	return err;
}

struct sk_buff *l2cap_build_cmd(__u8 code, __u8 ident, __u16 len, void *data, int optlen, void *opt)
{
	l2cap_cmd_t	*cmd;
	struct sk_buff	*skb;

	DBFENTER;

	skb = alloc_skb(HCI_SKB_RESERVE + L2CAP_HDR_LEN + len + optlen, GFP_ATOMIC);
	if (!skb)
		return NULL;
	skb_reserve(skb, HCI_SKB_RESERVE + L2CAP_HDR_LEN);
	skb_put(skb, len + optlen);
	
	memcpy(skb->data, data, len);
	if (optlen)
		memcpy(skb->data + len, opt, optlen);
	cmd = (void*)skb->data;
	cmd->code = code;
	cmd->id = ident;
	cmd->length = __htob16(len + optlen - sizeof(l2cap_cmd_t));
	DBFEXIT;
	return skb;
}

int l2cap_send_cmd(hci_con *con, __u8 code, __u8 ident, __u16 len, void *data, int optlen, void *opt)
{
	struct sk_buff	*skb;
	int		err;

	DBFENTER;
	skb = l2cap_build_cmd(code, ident, len, data, optlen, opt);
	if (!skb)
		return -ENOMEM;
	DBPRT("send L2CAP cmd, len = %d\n", skb->len);
	DBDUMP(skb->data, skb->len);
	err = l2cap_send_signal(con, skb);
	if (err)
		kfree_skb(skb);	
	DBFEXIT;
	return 0;
}

int l2cap_send_req(l2cap_ch *ch, unsigned long timeout, __u8 code, __u8 ident, 
		__u16 len, void *data, int optlen, void *opt)
{
	struct sk_buff		*skb, *clone;
	int			err;

	DBFENTER;
	skb = l2cap_build_cmd(code, ident, len, data, optlen, opt);
	if (!skb)
		return -ENOMEM;
	DBPRT("send L2CAP cmd, len = %d\n", skb->len);
	DBDUMP(skb->data, skb->len);
	clone = skb_clone(skb, GFP_ATOMIC);
	if (!clone) {
		kfree_skb(skb);
		return -ENOMEM;
	}
	l2cap_add_req(ch, ident, skb, timeout, l2cap_skb_timer);
	err = l2cap_send_signal(ch->con, clone);
	if (err) {
		l2cap_del_req(ch, skb);
		kfree_skb(clone);
	}
	kfree_skb(skb);	
	DBFEXIT;
	return 0;
}

/* ------------------------------------------------------------- */

/*
 * configuration stuff
 */
void l2cap_default_cfgopt(l2cap_cfg_opt *opt)
{
	opt->mtu = sysctl_l2cap_mtu;
	opt->flush_to = sysctl_l2cap_flush_to;
	opt->flags = 0;
	 
	opt->qos.flags = 0;
	
	opt->qos.service_type = L2CAP_QOS_BEST_EFFORT;
	opt->qos.token_rate = 0x00000000;
	opt->qos.token_size = 0x00000000;
	opt->qos.bandwidth = 0x00000000;
	opt->qos.latency = 0xFFFFFFFF;
	opt->qos.delay_variation = 0xFFFFFFFF;
	
	opt->rfc.mode = L2CAP_BASIC_MODE;/* L2CAP mode: 00:Basic, 01: Retransmission, 02: Flow control, other values: RESERVED. */
	opt->rfc.txwindow_size = sysctl_l2cap_txwindow; /* TxWindow size. Size of the transmission window. */
	opt->rfc.max_transmit = sysctl_l2cap_max_transmit;/* MaxTransmit. Number of transmission of a single I-frame that L2CAP is allowed. */
	opt->rfc.retransmission_timeout = sysctl_l2cap_ret_timeout;/* Retransmission time-out Value in miliseconds of the retransmission time-out. */
	opt->rfc.monitor_timeout = sysctl_l2cap_mon_timeout;/* Monitor time-out. Value in miliseconds of the interval at which S-frames should be transmitted.*/
	opt->rfc.mps = sysctl_l2cap_mps;	/* Maximum PDU payload size */

}

/* decode configuration options from packet to the object */
int l2cap_unpack_cfgopt(struct sk_buff *skb, l2cap_cfg_opt *opt, int req)
{
	l2cap_cfgopt_t 	*o;
	l2cap_qos_t	*qos;
#ifdef CONFIG_AFFIX_BT_1_2
	l2cap_rfc_t	*rfc;
#endif

	DBFENTER;
	/*
	 * if req == 1 -> configuration request options
	 * opt->flags has bits set for unaccepted parameter values by Affix
	 * but we accept everything may be :)
	 *
	 * if req == 0 -> configuration response options
	 * opt->flags has bits set for unaccepted parameter on remote side
	 * they will be sent in new config request
	 */
	opt->flags = 0;
	while(skb->len > 2) {
		o = (l2cap_cfgopt_t*)skb->data;
		switch (o->type) {
			case L2CAP_CFGOPT_MTU:
			case L2CAP_CFGOPT_MTU_SKIP:
				if ((skb->len < 4) || (o->length != 2))
					return L2CAP_CFGRSP_REJECT;
				opt->mtu = __btoh16(__get_u16(o->data));
				skb_pull(skb, 4);
				if (!req)
					set_bit(L2CAP_CFGOPT_MTU, &opt->flags);
				break;
			case L2CAP_CFGOPT_FLUSHTO:
			case L2CAP_CFGOPT_FLUSHTO_SKIP:
				if ((skb->len < 4) || (o->length != 2))
					return L2CAP_CFGRSP_REJECT;
				opt->flush_to = __btoh16(__get_u16(o->data));
				skb_pull(skb, 4);
				if (!req)
					set_bit(L2CAP_CFGOPT_FLUSHTO, &opt->flags);
				break;
			case L2CAP_CFGOPT_QOS:
			case L2CAP_CFGOPT_QOS_SKIP:	
				if ((skb->len < 24) || (o->length != 22))
					return L2CAP_CFGRSP_REJECT;
				qos = (l2cap_qos_t*)o->data;
				opt->qos.flags = qos->flags;
				opt->qos.service_type = qos->service_type;
				opt->qos.token_rate = __btoh32(qos->token_rate);
				opt->qos.token_size = __btoh32(qos->token_size);
				opt->qos.bandwidth = __btoh32(qos->bandwidth);
				opt->qos.latency = __btoh32(qos->latency);
				opt->qos.delay_variation = __btoh32(qos->delay_variation);		
				skb_pull(skb, 24);
				if (!req)
					set_bit(L2CAP_CFGOPT_QOS, &opt->flags);
				break;
#ifdef CONFIG_AFFIX_BT_1_2
			case L2CAP_CFGOPT_RFC:
			case L2CAP_CFGOPT_RFC_SKIP:
				if ((skb->len < 11) || (o->length != 9))
					return L2CAP_CFGRSP_REJECT;
				rfc = (l2cap_rfc_t*)o->data;
				opt->rfc.mode = rfc->mode;
				opt->rfc.txwindow_size = rfc->txwindow_size;
				opt->rfc.max_transmit = rfc->max_transmit;
				opt->rfc.retransmission_timeout = __btoh16(rfc->retransmission_timeout);
				opt->rfc.monitor_timeout = __btoh16(rfc->monitor_timeout);
				opt->rfc.mps = __btoh16(rfc->mps);
				skb_pull(skb,11);
				if (!req)
					set_bit(L2CAP_CFGOPT_RFC, &opt->flags);
				break;
#endif
			default:
				/* option unknown */
				if (L2CAP_CFGOPT_SKIP & o->type){
					/* Skip and continue processing.	*/
					skb_pull(skb,o->length);
				}
				else{
					/* Reject the configuration request */
					return L2CAP_CFGRSP_UNKNOWN_OPT;
				}
				break;

		}
	}
	DBFEXIT;
	return L2CAP_CFGRSP_SUCCESS;
}
#ifdef CONFIG_AFFIX_BT_1_2
int l2cap_unpack_check_cfgopt(l2cap_ch *ch,struct sk_buff *skb, l2cap_cfg_opt *opt,int req)
{
	int res = 0;

	res = l2cap_unpack_cfgopt(skb,opt,req);
	if (res == L2CAP_CFGRSP_SUCCESS) { /* NOTE: !req redundant check take it away */
		/* TO-DO: QoS checks */
			
		/* Flow Control and Retransmission checks */	
		if	((opt->rfc.mode > L2CAP_FLOW_CONTROL_MODE) ||	/* Unknown mode ! (Unclear test rethink) */
			(opt->rfc.txwindow_size < 1) || 		/* Invalid value */
			(opt->rfc.txwindow_size > 32) ||		/* Invalid value */
			(opt->rfc.max_transmit < 1) ||			/* Invalid value */
			(opt->rfc.mps <  L2CAP_MIN_SIGMTU))		/* Invalid value */
		{
				return L2CAP_CFGRSP_REJECT;
		}

		if (((ch->cfgout.rfc.mode == L2CAP_BASIC_MODE) && (opt->rfc.mode != L2CAP_BASIC_MODE)) || ((ch->cfgout.rfc.mode != L2CAP_BASIC_MODE) && (opt->rfc.mode == L2CAP_BASIC_MODE))) /* No basic mode allowed with new modes.*/
		{
			opt->rfc.mode = L2CAP_BASIC_MODE;
			ch->cfgout.rfc.mode = L2CAP_BASIC_MODE;
			res = L2CAP_CFGRSP_PARAMETERS;
			set_bit(L2CAP_CFGOPT_RFC, &opt->flags);
		}

		if (opt->rfc.retransmission_timeout < L2CAP_AFFIX_MIN_RETRANSMISSION_TIMEOUT)
		{
			opt->rfc.retransmission_timeout =  L2CAP_AFFIX_MIN_RETRANSMISSION_TIMEOUT;
			set_bit(L2CAP_CFGOPT_RFC, &opt->flags);
			res = L2CAP_CFGRSP_PARAMETERS;

		}

		if (opt->rfc.monitor_timeout < L2CAP_AFFIX_MIN_MONITOR_TIMEOUT)
		{
			opt->rfc.retransmission_timeout =  L2CAP_AFFIX_MIN_MONITOR_TIMEOUT;
			set_bit(L2CAP_CFGOPT_RFC, &opt->flags);
			res = L2CAP_CFGRSP_PARAMETERS;
		}
	}
	return res;
}
#endif

/* encode configurations options from object to the packet */
int l2cap_pack_cfgopt(__u8 *options, l2cap_cfg_opt *opt)
{
	__u8		*optr = options;
	l2cap_cfgopt_t 	*o = (l2cap_cfgopt_t*)optr;
	l2cap_qos_t	*qos;
	l2cap_rfc_t	*rfc;

	DBFENTER;
	
	/* Construct options buffer */
	if (test_bit(L2CAP_CFGOPT_MTU, &opt->flags)) {
		o->type = L2CAP_CFGOPT_MTU;
		o->length = 2;
		__put_u16(o->data, __htob16(opt->mtu));
		optr += 4;
	}
	if (test_bit(L2CAP_CFGOPT_FLUSHTO, &opt->flags)) {
		o = (l2cap_cfgopt_t *)optr; 
		o->type = L2CAP_CFGOPT_FLUSHTO;
		o->length = 2;
		__put_u16(o->data, __htob16(opt->flush_to));
	 	optr += 4;
	}
	if (test_bit(L2CAP_CFGOPT_QOS, &opt->flags)) {
		o = (l2cap_cfgopt_t *)optr; 
		o->type = L2CAP_CFGOPT_QOS;
		o->length = 22;
		qos = (l2cap_qos_t*)o->data;
		qos->flags = opt->qos.flags;
		qos->service_type = opt->qos.service_type;
		qos->token_rate = __htob32(opt->qos.token_rate);
		qos->token_size = __htob32(opt->qos.token_size);
		qos->bandwidth = __htob32(opt->qos.bandwidth);
		qos->latency = __htob32(opt->qos.latency);
		qos->delay_variation = __htob32(opt->qos.delay_variation);
		optr += 24;
	}
#ifdef CONFIG_AFFIX_BT_1_2
	if (test_bit(L2CAP_CFGOPT_RFC, &opt->flags)) {
		o = (l2cap_cfgopt_t *)optr; 
		o->type = L2CAP_CFGOPT_RFC;
		o->length = L2CAP_RFC_OPTION_LEN;
		rfc = (l2cap_rfc_t *)o->data;
		rfc->mode = opt->rfc.mode;
		rfc->txwindow_size = opt->rfc.txwindow_size;
		rfc->max_transmit = opt->rfc.max_transmit;
		rfc->retransmission_timeout = __htob16(opt->rfc.retransmission_timeout);
		rfc->monitor_timeout = __htob16(opt->rfc.monitor_timeout);
		rfc->mps = __htob16(opt->rfc.mps);
		optr += 11; 
	}
#endif
	DBFEXIT;
	return optr - options;
}

/* Configuration helpers */
int __l2ca_config_req(l2cap_ch *ch)
{
	int		result = 0, total;
	__u8		options[256], optlen, *ptr;
	__u16		flags = 0;
	l2cap_cfgopt_t	*opt;
	/*
	 * notes: Affix configuration options now does not exceed L2CAP_MIN_SIGMTU (32 bytes now)
	 * but the code is general and fragmens options in L2CAP_MIN_SIGMTU chunks
	 */
	DBFENTER;
	/* pack options */
	total = l2cap_pack_cfgopt(options, &ch->cfgout);
	DBPRT("packed total: %d\n", total);
	ptr = options;
	if (total <= 0) {
		/* Page 47 L2CAP specification 1.2: "Even if all default values are acceptable 
		 * a Configuration Reques packet with no options shall be sent */
		l2cap_config_req(ch, L2CAP_RTX_TIMEOUT, __alloc_id(ch), ch->rcid, flags, 0, NULL);
		atomic_inc(&ch->cfgreq_count);
	}
	else {
		while (total > 0) {
			flags = 0;
			optlen = 0;
			opt = (void*)ptr;
			/* PRE-CONDITION: opt->length + sizeof(*opt) <= L2CAP_MIN_SIGMTU */
			/* If the pre-condition is not met then we will be in an endless loop here and hang the kernel !!!! */
			while ((total - optlen >0) && ((optlen + opt->length + sizeof(*opt)) <= L2CAP_MIN_SIGMTU)) {
				optlen += (opt->length + sizeof(*opt));	// to send
				//total -= (opt->length + sizeof(*opt));	// left
				opt = (void*)(ptr + optlen);		// next to check
				DBPRT("Inner while => Option len (%i) Total len (%i)",optlen,total);
			}
			//optlen += (opt->length + sizeof(*opt)); // to send
			//total -= (opt->length + sizeof(*opt));        // left
			//opt = (void*)(ptr + optlen);            // next to check                     
			total -= optlen;
			if (total>0)
				flags |= L2CAP_CFGREQ_MORE;	// some options left
			l2cap_config_req(ch, L2CAP_RTX_TIMEOUT, __alloc_id(ch), ch->rcid, flags, optlen, ptr);
			atomic_inc(&ch->cfgreq_count);
			ptr += optlen;		// next to start
			DBPRT("Outer while => Option len (%i) Total len (%i)",optlen,total);
		}
	}
	DBFEXIT;
	return result;
}

int __l2ca_config_rsp(l2cap_ch *ch, __u16 flags, __u16 result)
{
	int	err = 0;
	__u8	options[256], olen;

	DBFENTER;
	/* put options */	
	olen = l2cap_pack_cfgopt(options, &ch->cfgin);

	if (result == L2CAP_CFGRSP_SUCCESS && !(flags & L2CAP_CFGRSP_MORE)) {
		set_bit(L2CAP_FLAGS_CFGIN_DONE, &ch->flags);
		if (test_bit(L2CAP_FLAGS_CFGOUT_DONE, &ch->flags))
			ENTERSTATE(ch, CON_OPEN);
	}
	err = l2cap_config_rsp(ch->con, ch->rspid, ch->rcid, flags, result, olen, options);
	if (STATE(ch) == CON_OPEN) {
		if (test_and_clear_bit(L2CAP_FLAGS_CONFIG, &ch->flags))
			l2ca_config_cfm(ch, result);
		else
			l2ca_connect_cfm(ch, result, 0);
	}
	DBFEXIT;
	return err;
}

/* handler functions */

int l2cap_sig_reject(hci_con *con, struct sk_buff *skb)
{
#ifdef CONFIG_AFFIX_BT_1_2
	l2cap_cmd_t	*cmd = (l2cap_cmd_t*)skb->data;
	l2cap_cmd_t	*req = NULL;
	l2cap_ch	*ch;
	__u8		id = cmd->id;
	struct sk_buff *req_skb = NULL;
	
	DBFENTER;
	/* Initially we should do nothing. But we need this hack to avoid miss behaving 1.1 devices to hang the configuration process */
	req_skb = l2cap_get_req(con, id, 0 , &ch);
	if (req_skb) {
		req = (l2cap_cmd_t *)req_skb->data;
		if ((req->code == L2CAP_SIG_INFOREQ) && (STATE(ch) == CON_CONFIG)) {
			set_bit(L2CAP_FLAGS_INFOREQ_DONE ,&ch->flags);
			clear_bit(L2CAP_CFGOPT_RFC, &ch->cfgout.flags);
			__l2ca_config_req(ch);
		}
		kfree_skb(req_skb);
	}	
	
#ifdef CONFIG_AFFIX_DEBUG
	/* Do nothing */
	DBPRT("We have rejected command, id: %d\n", id);
#endif

#endif
	kfree_skb(skb);
	return 0;
}

int l2cap_sig_connect_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_conreq_t	*req = (l2cap_conreq_t*)skb->data;
	l2cap_ch	*ch;
	int		err;

	DBFENTER;
	req->scid = __btoh16(req->scid);
	req->psm = __btoh16(req->psm);

	ch = l2cap_create(req->psm, &err);
	if (!ch) {
		if (err == -ENOMEM)
			l2cap_connect_rsp(con, req->id, req->scid, L2CAP_CID_NULL, 
					L2CAP_CONRSP_RESOURCE, L2CAP_CONRSP_NOINFO);
		else
			/* Send response with unknown PSM options */
			l2cap_connect_rsp(con, req->id, req->scid, L2CAP_CID_NULL, 
					L2CAP_CONRSP_PSM, L2CAP_CONRSP_NOINFO);
		goto exit;
	}
	DBPRT("ch: %p, lcid: %x, rcid: %x, state: %d\n", ch, ch->lcid, ch->rcid, ch->state);
	ch->con = con;
	hcc_hold(con);
	set_bit(L2CAP_FLAGS_SERVER, &ch->flags);
	ch->rspid = req->id;
	ch->rcid = req->scid;
	ch->bda = con->bda;
	hci_hold(con->hci);	// set reference
	ch->hci = con->hci;

	ENTERSTATE(ch, CON_W4_LCONRSP);
	l2ca_connect_ind(ch);	//XXX: check security here and start auth
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_connect_rsp(hci_con *con, struct sk_buff *skb)
{
	l2cap_conrsp_t	*rsp = (l2cap_conrsp_t*)skb->data;
	l2cap_ch	*ch;
	struct sk_buff	*req_skb;

	DBFENTER;
	rsp->dcid = __btoh16(rsp->dcid);
	rsp->scid = __btoh16(rsp->scid);
	rsp->result = __btoh16(rsp->result);
	rsp->status = __btoh16(rsp->status);
	
	req_skb = l2cap_get_req(con, rsp->id, rsp->scid, &ch);
	if (!req_skb)
		goto exit;
	DBPRT("ch: %p, lcid: %x, rcid: %x, result: %d, status: %d, state: %d\n",
			ch, ch->lcid, ch->rcid, rsp->result, rsp->status, ch->state);
#if 0			
	if (!ch->hci) {
		hci_hold(con->hci);	// set reference
		ch->hci = con->hci;
	}
#endif
	if (STATE(ch) == CON_W4_CONRSP) {
		switch (rsp->result) {
			case L2CAP_CONRSP_SUCCESS:
				ch->rcid = rsp->dcid;
				l2cap_config_start(ch);
#ifdef CONFIG_AFFIX_BT_1_2
				l2cap_info_req(ch, L2CAP_RTX_TIMEOUT,__alloc_id(ch),L2CAP_INFO_EXT_FEATURES);
#else
				__l2ca_config_req(ch);	/* send config_req */
#endif
				break;
			case L2CAP_CONRSP_PENDING:
				l2cap_add_req(ch, rsp->id, req_skb, L2CAP_ERTX_TIMEOUT, l2cap_skb_timer);
				break;
			default:
				/* L2CAP_CONRSP_NEG */
				l2cap_release(ch);	//dead
				l2ca_connect_cfm(ch, rsp->result, rsp->status);
		}
	}
	kfree_skb(req_skb);
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_config_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_cfgreq_t	*req = (l2cap_cfgreq_t*)skb->data;
	l2cap_ch	*ch;
	__u16		result;

	DBFENTER;
	req->dcid = __btoh16(req->dcid);
	req->flags = __btoh16(req->flags);
	
	ch = l2cap_lookup(req->dcid);
	if (!ch) {
		l2cap_command_rej(con, req->id, L2CAP_CMDREJ_INVCID, 4, &req->dcid);
		goto exit;
	}
	DBPRT("ch: %p, lcid: %x, rcid: %x, state: %d\n", ch, ch->lcid, ch->rcid, ch->state);
	switch (STATE(ch)) {
		case CON_CLOSED:
			l2cap_command_rej(con, req->id, L2CAP_CMDREJ_INVCID, 4, &req->dcid);
			break;
		case CON_OPEN:
			/* Here I have to disable data transmission as well ??? */
			set_bit(L2CAP_FLAGS_CONFIG, &ch->flags);
			/* be a configuration server */
			set_bit(L2CAP_FLAGS_SERVER, &ch->flags);
			l2cap_config_start(ch);
			/* fall trhought */
		case CON_CONFIG:
			ch->rspid = req->id;
			/* stop dead config timer */
			if (test_bit(L2CAP_FLAGS_CFGOUT_DONE, &ch->flags))
				l2cap_stop_timer(ch);
			skb_pull(skb, sizeof(*req));	/* remove header */
			/* extract config options */
#ifdef CONFIG_AFFIX_BT_1_2
			result = l2cap_unpack_check_cfgopt(ch, skb, &ch->cfgin, 1);
#else
			result = l2cap_unpack_cfgopt(skb, &ch->cfgin, 1);
#endif
			if (!(req->flags & L2CAP_CFGREQ_MORE) &&
					test_bit(L2CAP_FLAGS_CONFIG, &ch->flags) && 
						  test_bit(L2CAP_FLAGS_SERVER, &ch->flags)) {
				/* got request */
				l2ca_config_ind(ch);
			} else
				__l2ca_config_rsp(ch, req->flags, result);
			break;
		default:
			BTDEBUG("State is not handled\n");
	}
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_config_rsp(hci_con *con, struct sk_buff *skb)
{
	l2cap_ch	*ch;
	l2cap_cfgrsp_t	*rsp = (l2cap_cfgrsp_t*)skb->data;
	struct sk_buff	*req_skb;
	l2cap_cfgreq_t	*req;

	DBFENTER;
	rsp->scid = __btoh16(rsp->scid);
	rsp->result = __btoh16(rsp->result);
	rsp->flags = __btoh16(rsp->flags);

	req_skb = l2cap_get_req(con, rsp->id, rsp->scid, &ch);
	if (!req_skb)
		goto exit;
	DBPRT("ch: %p, lcid: %x, rcid: %x, flags: %x, result: %d, state: %d, cfgreq_count: %d\n",
			ch, ch->lcid, ch->rcid, rsp->flags, rsp->result, ch->state, 
			atomic_read(&ch->cfgreq_count));
	atomic_dec(&ch->cfgreq_count);
	req = (void*)req_skb->data;
	if (STATE(ch) == CON_CONFIG) {
		/* extract options */
		skb_pull(skb, sizeof(*rsp));
#ifdef CONFIG_AFFIX_BT_1_2
		l2cap_unpack_check_cfgopt(ch, skb, &ch->cfgout, 0);
#else
		l2cap_unpack_cfgopt(skb, &ch->cfgout, 0);
#endif		
		/* check result and flags */
		if (rsp->result == L2CAP_CFGRSP_PARAMETERS) {
			/* received options - proposal -> accept it and send new request */
			__l2ca_config_req(ch);
			rsp->result = 0;
		}
		if ((rsp->flags & L2CAP_CFGRSP_MORE) && !(__btoh16(req->flags) & L2CAP_CFGREQ_MORE)) {
			/* extra options are comming - send empty request */
			ch->cfgout.flags = 0;
			__l2ca_config_req(ch);
		}
		if (!(rsp->flags & L2CAP_CFGRSP_MORE) && atomic_read(&ch->cfgreq_count) == 0) {
			/* not more response will come */
			if (rsp->result == L2CAP_CFGRSP_SUCCESS) {
				set_bit(L2CAP_FLAGS_CFGOUT_DONE, &ch->flags);
				if (test_bit(L2CAP_FLAGS_CFGIN_DONE, &ch->flags)) {
					ENTERSTATE(ch, CON_OPEN);
				} else
					l2cap_start_timer(ch, L2CAP_RTX_TIMEOUT);
			} else
				l2cap_release(ch);	//dead
			if (STATE(ch) == CON_OPEN) {
				/* send upper */
			 	if (test_and_clear_bit(L2CAP_FLAGS_CONFIG, &ch->flags))
					l2ca_config_cfm(ch, rsp->result);
				else
					l2ca_connect_cfm(ch, rsp->result, 0);
			}
		}
	}
	kfree_skb(req_skb);
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_disconnect_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_discreq_t	*req = (l2cap_discreq_t*)skb->data;
	l2cap_ch	*ch;

	DBFENTER;
	req->scid = __btoh16(req->scid);
	req->dcid = __btoh16(req->dcid);
	
	ch = l2cap_lookup(req->dcid);
	if (!ch || ch->rcid != req->scid) {
		l2cap_command_rej(con, req->id, L2CAP_CMDREJ_INVCID, 4, &req->dcid);
		if (ch)
			l2ca_put(ch);
		goto exit;
	}
	/*
	 * Check first if we sent disconnect_req already
	 */
	switch (STATE(ch)) {
		case CON_CLOSED:
		case CON_W4_DISCRSP:
			break;
		default:
			ch->rspid = req->id;
			l2cap_release(ch);	//dead
			l2ca_disconnect_ind(ch);
	}
	l2cap_disconnect_rsp(ch->con, req->id, req->scid, req->dcid);
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_disconnect_rsp(hci_con *con, struct sk_buff *skb)
{
	l2cap_discrsp_t	*rsp = (l2cap_discrsp_t*)skb->data;
	l2cap_ch	*ch;
	struct sk_buff	*req_skb;

	DBFENTER;
	rsp->scid = __btoh16(rsp->scid);
	
	req_skb = l2cap_get_req(con, rsp->id, rsp->scid, &ch);
	if (!req_skb)
		goto exit;
	if (STATE(ch) == CON_W4_DISCRSP) {
		l2cap_release(ch);	//dead
	}
	kfree_skb(req_skb);
	l2ca_put(ch);
exit:
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_echo_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_cmd_t	*cmd;

	DBFENTER;
	cmd = (void*)skb->data;
	l2cap_send_cmd(con, L2CAP_SIG_ECHORSP, cmd->id, sizeof(*cmd), cmd, cmd->length, cmd->data);
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_echo_rsp(hci_con *con, struct sk_buff *skb)
{
	l2cap_cmd_t	*rsp = (void*)skb->data;
	l2cap_ch	*ch;
	struct sk_buff	*req_skb;

	DBFENTER;
	req_skb = l2cap_get_req(con, rsp->id, 0, &ch);
	if (!req_skb) {
		kfree_skb(skb);
		return 0;
	}
	skb_pull(skb, sizeof(*rsp));
	l2ca_control_ind(ch, L2CAP_EVENT_PING, skb);
	kfree_skb(skb);
	kfree_skb(req_skb);
	l2ca_put(ch);
	DBFEXIT;
	return 0;
}


#ifdef CONFIG_AFFIX_BT_1_2
int l2cap_sig_info_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_inforeq_t	*req = (void*)skb->data;
	__u16		mtu = __htob16(sysctl_l2cap_cl_mtu);
	__u32		ext_features_mask = __htob32(sysctl_l2cap_modes);
	
	DBFENTER;
	req->type = __btoh16(req->type);
	switch (req->type) {
		case L2CAP_INFO_CL_MTU:
			l2cap_info_rsp(con, req->id, L2CAP_INFO_CL_MTU, L2CAP_INFORSP_SUCCESS, 2, (__u8*)&mtu);
			break;
		case L2CAP_INFO_EXT_FEATURES:
			l2cap_info_rsp(con, req->id, L2CAP_INFO_EXT_FEATURES, L2CAP_INFORSP_SUCCESS, 4, (__u8*)&ext_features_mask);
			break;
		default:
			l2cap_info_rsp(con, req->id, req->type, L2CAP_INFORSP_NOTSUPPORTED, 0, NULL);
			break;
	}
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int l2cap_sig_info_rsp(hci_con *con, struct sk_buff *skb)
{
	l2cap_inforsp_t	*rsp = (void*)skb->data;
	l2cap_ch	*ch;
	struct sk_buff	*req_skb;
	l2cap_inforeq_t	*req;
	unsigned long	ext_features_mask = 0;

	DBFENTER;
	rsp->length = __btoh16(rsp->length);
	rsp->type = __btoh16(rsp->type);
	rsp->result = __btoh16(rsp->result);

	req_skb = l2cap_get_req(con, rsp->id, 0, &ch);
	req = (void *)req_skb->data;
	
	if (req_skb) {
		if (req->type == rsp->type) {
			switch (rsp->type) {
				case L2CAP_INFO_CL_MTU:
					BTDEBUG(" Ignoring Conecctionless mtu info response.\n");
					break;
				case L2CAP_INFO_EXT_FEATURES:
					clear_bit(L2CAP_CFGOPT_RFC,&ch->cfgout.flags);
					if (rsp->result == L2CAP_INFORSP_SUCCESS) {
						ext_features_mask = __btoh32(__get_u32(rsp->data));
						if (test_bit(L2CAP_EXT_FEATURE_RET, &ext_features_mask) && (ch->cfgout.rfc.mode == L2CAP_RETRANSMISSION_MODE)) {
							DBPRT("Setting RETRANSMISSION MODE\n");
							ch->cfgout.rfc.mode = L2CAP_RETRANSMISSION_MODE;
						}
						else if (test_bit(L2CAP_EXT_FEATURE_FC, &ext_features_mask) && 
							((ch->cfgout.rfc.mode == L2CAP_RETRANSMISSION_MODE) || (ch->cfgout.rfc.mode == L2CAP_FLOW_CONTROL_MODE))) {
							DBPRT("Setting FLOW CONTROL MODE\n");
							ch->cfgout.rfc.mode = L2CAP_FLOW_CONTROL_MODE;
						}
						else {
							DBPRT("Info rsp: Setting BASIC MODE\n");
							ch->cfgout.rfc.mode = L2CAP_BASIC_MODE;
						}
						set_bit(L2CAP_CFGOPT_RFC,&ch->cfgout.flags);
						if (test_bit(L2CAP_EXT_FEATURE_QOS,&ext_features_mask)) {
							set_bit(L2CAP_CFGOPT_QOS,&ch->cfgout.flags);
						}
					}
					break;
				default:
					BTDEBUG("Unknown INFO type. Ignoring info response.\n");
					break;
			}
		}
		if (STATE(ch) == CON_CONFIG) {
			set_bit(L2CAP_FLAGS_INFOREQ_DONE ,&ch->flags);
			__l2ca_config_req(ch);
		}
		kfree_skb(req_skb);
	}
	kfree_skb(skb);
	l2ca_put(ch);
	DBFEXIT;
	return 0;
}


#else
int l2cap_sig_info_req(hci_con *con, struct sk_buff *skb)
{
	l2cap_inforeq_t	*req = (void*)skb->data;
	__u16		mtu = __htob16(sysctl_l2cap_cl_mtu);

	DBFENTER;
	req->type = __btoh16(req->type);
	switch (req->type) {
		case L2CAP_INFO_CL_MTU:
			l2cap_info_rsp(con, req->id, L2CAP_INFO_CL_MTU, L2CAP_INFORSP_SUCCESS, 2, (__u8*)&mtu);
			break;
		default:
			l2cap_info_rsp(con, req->id, req->type, L2CAP_INFORSP_NOTSUPPORTED, 0, NULL);
			break;
	}
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}
#endif

void l2cap_sig_ind(hci_con *con, struct sk_buff *skb)
{
	struct sk_buff	*cskb;
	__u8		code;
	__u16		cmdlen;
	l2cap_cmd_t	*cmd = (l2cap_cmd_t*)skb->data;

	DBFENTER;

	if (skb->len < 4) /* less then hdrlen */
		goto exit;
	cmdlen = __btoh16(cmd->length) + 4;	// +4 .. because of header of the command

	while (skb->len >= cmdlen) {	/* Process several commands	*/
		code = cmd->code;

		cskb = skb_clone(skb, GFP_ATOMIC);
		skb_trim(cskb, cmdlen);
		cskb->pkt_type = code;

		switch (code) {
			case L2CAP_SIG_REJECT:
				l2cap_sig_reject(con, cskb);
				break;
			case L2CAP_SIG_CONREQ:
				DBPRT("SIGNAL: CONNECT_REQ\n");
				l2cap_sig_connect_req(con, cskb);
				break;
			case L2CAP_SIG_CONRSP:
				DBPRT("SIGNAL: CONNECT_RSP\n");
				l2cap_sig_connect_rsp(con, cskb);
				break;
			case L2CAP_SIG_CFGREQ:
				DBPRT("SIGNAL: CONFIG_REQ\n");
				l2cap_sig_config_req(con, cskb);
				break;
			case L2CAP_SIG_CFGRSP:
				DBPRT("SIGNAL: CONFIG_RSP\n");
				l2cap_sig_config_rsp(con, cskb);
				break;
			case L2CAP_SIG_DISCREQ:
				DBPRT("SIGNAL: DISCONNECT_REQ\n");
				l2cap_sig_disconnect_req(con, cskb);
				break;
			case L2CAP_SIG_DISCRSP:
				DBPRT("SIGNAL: DISCONNECT_RSP\n");
				l2cap_sig_disconnect_rsp(con, cskb);
				break;
			case L2CAP_SIG_ECHOREQ:
				DBPRT("SIGNAL: ECHO_REQ\n");
				l2cap_sig_echo_req(con, cskb);
				break;
			case L2CAP_SIG_ECHORSP:
				DBPRT("SIGNAL: ECHO_RSP\n");
				l2cap_sig_echo_rsp(con, cskb);
				break;
			case L2CAP_SIG_INFOREQ:
				DBPRT("SIGNAL: INFO_REQ\n");
				l2cap_sig_info_req(con, cskb);
				break;
			case L2CAP_SIG_INFORSP:
				l2cap_sig_info_rsp(con,cskb);
				break;
			case L2CAP_SIG_SINGLEPING:
				kfree_skb(cskb);
				break;
			default:
				DBPRT("Unknown command: %#02x\n", code);
				l2cap_cmd_t	*cmd = (l2cap_cmd_t*)skb->data;
				l2cap_command_rej(con, cmd->id, L2CAP_CMDREJ_NOTSUP, 0, NULL);
				kfree_skb(cskb);
		}
		skb_pull(skb, cmdlen);
		if (skb->len < 4)
			break;
		cmd = (l2cap_cmd_t*)skb->data;
		cmdlen = __btoh16(cmd->length) + 4;
	}

exit:
	kfree_skb(skb);
	DBFEXIT;
}

void l2cap_cl_ind(hci_con *con, struct sk_buff *skb)
{
	DBFENTER;
	DBPRT("Group addressing is not supported yet\n");
	kfree_skb(skb);
	DBFEXIT;
}

#ifdef CONFIG_AFFIX_BT_1_2
void l2cap_data_ind(hci_con *con, __u16 cid, struct sk_buff *skb)
{
	l2cap_ch	*ch;

	DBFENTER;
	ch = l2cap_lookup(cid);
	if (!ch) {
		DBPRT("Unknown CID (%x)\n",cid);
		kfree_skb(skb);
		return;
	}
	if (STATE(ch) == CON_OPEN) {
		if (LOCAL_MODE(ch) == L2CAP_BASIC_MODE) {
			skb_pull(skb,L2CAP_HDR_LEN);
		}
		else {
			skb = l2cap_retransmission_flowcontrol(ch,skb);
			skb = l2cap_reassembly(ch,skb);
			l2cap_try_to_send(ch);
		}
		
		if (skb) {
			skb->pkt_type = L2CAP_SIG_DATA;
			l2ca_data_ind(ch,skb);
		}
	}
	else {
		DBPRT("Channel not in open state\n");
		kfree_skb(skb);
	}
	l2ca_put(ch);
	DBFEXIT;
}
#else
void l2cap_data_ind(hci_con *con, __u16 cid, struct sk_buff *skb)
{
	l2cap_ch	*ch;

	DBFENTER;
	ch = l2cap_lookup(cid);
	if (!ch) {
		kfree_skb(skb);
		return;
	}
	if (STATE(ch) == CON_OPEN) {
		skb->pkt_type = L2CAP_SIG_DATA;
		l2ca_data_ind(ch, skb);
	} else {
		DBPRT("Channel closed\n");
		kfree_skb(skb);
	}
	l2ca_put(ch);
	DBFEXIT;
}
#endif

/********************   Signalling commands   ***************************/

int l2cap_command_rej(hci_con *con, __u8 id, __u16 reason, __u16 length, void *data)
{
	l2cap_cmdrej_t	cmd;

	DBFENTER;
	cmd.reason = __htob16(reason);
	l2cap_send_cmd(con, L2CAP_SIG_REJECT, id, sizeof(cmd), &cmd, length, data);
	DBFEXIT;
	return 0;
}

int l2cap_connect_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 psm, __u16 lcid)
{
	l2cap_conreq_t	cmd;

	DBFENTER;
	cmd.psm = __htob16(psm);
	cmd.scid = __htob16(lcid);
	l2cap_send_req(ch, timeout, L2CAP_SIG_CONREQ, id, sizeof(cmd), &cmd, 0, NULL);
	DBFEXIT;
	return 0;
}

int l2cap_connect_rsp(hci_con *con, __u8 id, __u16 rcid, __u16 lcid, __u16 result, __u16 status)
{
	l2cap_conrsp_t	cmd;

	DBFENTER;
	cmd.scid = __htob16(rcid);
	cmd.dcid = __htob16(lcid);
	cmd.result = __htob16(result);
	cmd.status = __htob16(status);
	l2cap_send_cmd(con, L2CAP_SIG_CONRSP, id, sizeof(cmd), &cmd, 0, NULL);
	DBFEXIT;
	return 0;
}

int l2cap_config_req(l2cap_ch *ch, unsigned long timeout, __u8 id, 
		__u16 rcid, __u16 flags, __u8 len, __u8 *options)
{
	l2cap_cfgreq_t	cmd;

	DBFENTER;
	cmd.dcid = __htob16(rcid);
	cmd.flags = __htob16(flags);
	l2cap_send_req(ch, timeout, L2CAP_SIG_CFGREQ, id, sizeof(cmd), &cmd, len, options);
	DBFEXIT;
	return 0;
}

int l2cap_config_rsp(hci_con *con, __u8 id, __u16 rcid, __u16 flags, __u16 result, __u8 len, __u8 *options)
{
	l2cap_cfgrsp_t	cmd;

	DBFENTER;
	cmd.scid = __htob16(rcid);
	cmd.flags = __htob16(flags);
	cmd.result = __htob16(result);
	l2cap_send_cmd(con, L2CAP_SIG_CFGRSP, id, sizeof(cmd), &cmd, len, options);
	DBFEXIT;
	return 0;
}

int l2cap_disconnect_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 rcid, __u16 lcid)
{
	l2cap_discreq_t	cmd;

	DBFENTER;
	cmd.dcid = __htob16(rcid);
	cmd.scid = __htob16(lcid);
	l2cap_send_req(ch, timeout, L2CAP_SIG_DISCREQ, id, sizeof(cmd), &cmd, 0, NULL);
	DBFEXIT;
	return 0;
}

int l2cap_disconnect_rsp(hci_con *con, __u8 id, __u16 rcid, __u16 lcid)
{
	l2cap_discrsp_t	cmd;

	DBFENTER;
	cmd.dcid = __htob16(lcid);
	cmd.scid = __htob16(rcid);
	DBPRT("Disconnection RESPONSE SEND !!!! DCID: %d SCID: %d\n",rcid,lcid);
	l2cap_send_cmd(con, L2CAP_SIG_DISCRSP, id, sizeof(cmd), &cmd, 0, NULL);
	DBFEXIT;
	return 0;
}

int l2cap_echo_req(l2cap_ch *ch, unsigned long timeout, __u8 code, __u8 id, void *data, int len)
{
	l2cap_cmd_t	cmd;

	DBFENTER;
	l2cap_send_req(ch, timeout, code, id, sizeof(cmd), &cmd, len, data);
	DBFEXIT;
	return 0;
}

int l2cap_info_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 type)
{
	l2cap_inforeq_t	cmd;

	DBFENTER;
	cmd.type = __htob16(type);
	l2cap_send_req(ch, timeout, L2CAP_SIG_INFOREQ, id, sizeof(cmd), &cmd, 0, NULL);
	DBFEXIT;
	return 0;
}

int l2cap_info_rsp(hci_con *con, __u8 id, __u16 type, __u16 result, __u16 length, __u8 *data)
{
	l2cap_inforsp_t	cmd;

	DBFENTER;
	cmd.type = __htob16(type);
	cmd.result = __htob16(result);
	l2cap_send_cmd(con, L2CAP_SIG_INFORSP, id, sizeof(cmd), &cmd, length, data);
	DBFEXIT;
	return 0;
}


/****************************   Upper to L2CAP services   ************************/

int l2ca_connect_req(l2cap_ch *ch, BD_ADDR *bda, __u16 psm)
{
	int		err = 0;

	DBFENTER;
	/* Create Baseband connection */
	err = __l2ca_connect_req(ch, bda);
	if (err)
		return err;
	/* set some attributes */
	ch->bda = *bda;
	ch->psm = psm;
	ch->rcid = L2CAP_CID_NULL;
	ENTERSTATE(ch, CON_W4_CONRSP); 
	if (ch->psm) {
		/* connection oriented */
		l2cap_connect_req(ch, L2CAP_RTX_TIMEOUT, __alloc_id(ch), ch->psm, ch->lcid);
	} else {
		if (__hcc_linkup(ch->con)) {
			/* connectionless */
			ENTERSTATE(ch, CON_OPEN);
			l2ca_connect_cfm(ch, 0, 0);
		}
	}
	DBFEXIT;
	return 0;
}

int l2ca_connect_rsp(l2cap_ch *ch, __u16 result, __u16 status)
{
	DBFENTER;

	if (STATE(ch) != CON_W4_LCONRSP)
		return -1;
	l2cap_config_start(ch);	/* place changed */
	l2cap_connect_rsp(ch->con, ch->rspid, ch->rcid, ch->lcid, result, status);
	if (result == L2CAP_CONRSP_SUCCESS) {
#ifdef CONFIG_AFFIX_BT_1_2
		l2cap_info_req(ch, L2CAP_RTX_TIMEOUT,__alloc_id(ch),L2CAP_INFO_EXT_FEATURES);
#else
		__l2ca_config_req(ch);
#endif
	} else if (result >= L2CAP_CONRSP_PSM) {
		l2cap_release(ch);	//dead
	}
	DBFEXIT;
	return 0;
}

/*
   before issueing configuration request
   ones should set configuration options on the object
*/
int l2ca_config_req(l2cap_ch *ch)
{
	int	result = 0;

	DBFENTER;
	switch (STATE(ch)) {
		case CON_CLOSED:
			l2ca_config_cfm(ch, L2CAP_CFGRSP_REJECT);	// ConfigCfmNeg
			break;
		case CON_OPEN:
			/* Here I have to disable data transmission as well ??? */
			set_bit(L2CAP_FLAGS_CONFIG, &ch->flags);
			clear_bit(L2CAP_FLAGS_SERVER, &ch->flags);
			l2cap_config_start(ch);
		case CON_CONFIG:
			result = __l2ca_config_req(ch);
			break;
		default:
			break;
	}
	DBFEXIT;
	return result;
}

int l2ca_config_rsp(l2cap_ch *ch, __u16 result)
{
	int	err = 0;

	DBFENTER;
	if (STATE(ch) == CON_CONFIG) {
		err = __l2ca_config_rsp(ch, 0, result);
		if (result == L2CAP_CFGRSP_SUCCESS)
			__l2ca_config_req(ch);
	}
	DBFEXIT;
	return err;
}

int l2ca_disconnect_req(l2cap_ch *ch)
{
	DBFENTER;
	if (STATE(ch) != DEAD && STATE(ch) != CON_CLOSED && STATE(ch) != CON_W4_DISCRSP) {
		if (ch->psm) {
			ENTERSTATE(ch, CON_W4_DISCRSP);
			l2cap_disconnect_req(ch, L2CAP_RTX_TIMEOUT_DISC, __alloc_id(ch), ch->rcid, ch->lcid);
		} else {
			/* connectionless */
			l2cap_release(ch);	//dead
		}
	}
	DBFEXIT;
	return 0;
}

int l2ca_disconnect_rsp(l2cap_ch *ch)
{
	if (STATE(ch) == CON_W4_LDISCRSP) {
		l2cap_release(ch);	//dead
		l2cap_disconnect_rsp(ch->con, ch->rspid, ch->rcid, ch->lcid);
	}
	return 0;
}

#ifdef CONFIG_AFFIX_BT_1_2
int l2cap_try_to_send(l2cap_ch *ch)
{
	int	err = 0,frame_sent = 0;
	struct sk_buff	*lskb = NULL, *clone_skb = NULL;  
	
	DBFENTER;

	if ((STATE(ch) == CON_OPEN) && (spin_trylock(&ch->xmit_lock))) {
		while (!skb_queue_empty(&ch->tx_buffer) && ch->next_tx_frame && !err && !TxWindowFull(ch)) {
			lskb = ch->next_tx_frame;
			if (lskb->next == (struct sk_buff*)&ch->tx_buffer) { /* If we are at the end of the queue then there is no more frames to send */
				ch->next_tx_frame = NULL;
			}
			else {
				ch->next_tx_frame = lskb->next;
			}

			set_reqseq(lskb,ch->expected_tx_seq);
			set_txseq(lskb,ch->next_tx_seq);
			set_fcs(lskb,calculate_fcs(lskb));
			
			/* We need to keep a copy of the packet until it gets acknoledged */
			clone_skb = skb_clone(lskb,GFP_ATOMIC);
			if (!clone_skb) {
				ch->next_tx_frame = lskb;
				return -ENOMEM;
			}
			
			DBPRT("L2CAP 1.2: send L2CAP Packet, len = %d\n", lskb->len);
			DBPRT("L2CAP 1.2: ReqSeq: %d TxSeq: %d\n", ch->expected_tx_seq, ch->next_tx_seq); 
			
			ch->next_tx_seq = (ch->next_tx_seq + 1) % 64;	
		
			err = lp_send_data(ch->con,clone_skb);

			if (err) {
				ch->next_tx_frame = lskb;
				ch->next_tx_seq = (ch->next_tx_seq - 1) % 64;	
			}
			frame_sent = 1;
		}
		
		if (!l2cap_retransmission_timer_pending(ch) && frame_sent && !err) {
			l2cap_stop_monitor_timer(ch);
			l2cap_start_retransmission_timer(ch,ch->cfgin.rfc.retransmission_timeout*HZ/1000);
		}
		spin_unlock(&ch->xmit_lock);
	}
	/* ELSE: if it is already sending packets let's skip this call then */
	DBFEXIT;
	return err;
}

int l2cap_send_frame(l2cap_ch *ch, struct sk_buff *skb)
{
	int		err = 0;

	DBFENTER;

	if (skb->len + L2CAP_HDR_LEN > ch->cfgout.mtu) { //NOTE: Check value L2CAP_HDR_LEN
		DBPRT("L2CAP 1.2: Segmenting the SDU\n");
		if(l2cap_segmentation_sdu(ch,skb) == NULL) {
			err = -ENOMEM;
			goto exit;
		}
	}
	else {
		DBPRT("L2CAP 1.2: Unsegmented SDU\n");
		err = append_unsegmented_hdr(ch,skb);
		if (err) goto exit;
		enqueue_tx_buffer(ch,skb);
	}
	err = l2cap_try_to_send(ch);	
exit:
	DBFEXIT;
	return err;

}


int l2ca_send_data(l2cap_ch *ch, struct sk_buff *skb)
{
	int	err = 0;

	DBFENTER;
	if (STATE(ch) != CON_OPEN) {
		DBPRT("We are not in OPEN state, state: %d\n", STATE(ch));
		err = -ENOTCONN;
	}
 	else {	
		switch (REMOTE_MODE(ch)) {
			case L2CAP_BASIC_MODE:
				err=l2cap_send_data(ch,skb);
				break;
			case L2CAP_FLOW_CONTROL_MODE:
			case L2CAP_RETRANSMISSION_MODE:
				err=l2cap_send_frame(ch,skb); 
				break;
			default:
				DBPRT("Unknown MODE (%d)\n",REMOTE_MODE(ch));
				break;
		}
	}
	DBFEXIT;
	return err;
}
#else
int l2ca_send_data(l2cap_ch *ch, struct sk_buff *skb)
{
	int	err = 0;

	DBFENTER;
	if (STATE(ch) != CON_OPEN) {
		DBPRT("We are not in OPEN state, state: %d\n", STATE(ch));
		err = -ENOTCONN;
		goto exit;
	}
	err = l2cap_send_data(ch, skb);
exit:
	DBFEXIT;
	return err;
}
#endif

int __l2ca_ping(l2cap_ch *ch, BD_ADDR *bda, struct sk_buff *skb)
{
	int	err = 0;

	DBFENTER;
	/* set some attributes */
	ch->bda = *bda;
	ch->psm = 0;
	ch->rcid = L2CAP_CID_NULL;

	/* Create Baseband connection */
	err = __l2ca_connect_req(ch, bda);
	if (err)
		return err;
	err = l2cap_echo_req(ch, L2CAP_RTX_TIMEOUT, L2CAP_SIG_ECHOREQ, __alloc_id(ch), skb->data, skb->len);
	if (!err)
		kfree_skb(skb);
	DBFEXIT;
	return err;
}

int l2ca_ping(l2cap_ch *ch, struct sk_buff *skb)
{
	int	err = 0;

	DBFENTER;
	if (STATE(ch) != CON_OPEN) {
		DBPRT("We are not in OPEN state, state: %d\n", STATE(ch));
		return -ENOTCONN;
	}
	err = l2cap_echo_req(ch, L2CAP_RTX_TIMEOUT, L2CAP_SIG_ECHOREQ, __alloc_id(ch), skb->data, skb->len);
	if (!err)
		kfree_skb(skb);
	DBFEXIT;
	return err;
}

int l2ca_singleping(l2cap_ch *ch, struct sk_buff *skb)
{
	int	err = 0;

	DBFENTER;
	if (STATE(ch) != CON_OPEN) {
		DBPRT("We are not in OPEN state, state: %d\n", STATE(ch));
		err = -ENOTCONN;
		goto exit;
	}
	err = l2cap_echo_req(ch, L2CAP_RTX_TIMEOUT, L2CAP_SIG_SINGLEPING, __alloc_id(ch), skb->data, skb->len);
	if (!err)
		kfree_skb(skb);
exit:
	DBFEXIT;
	return err;
}



int l2ca_connect_ind(l2cap_ch *ch)
{
	int	err;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	err = ch->ops->connect_ind(ch);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_connect_cfm(l2cap_ch *ch, int result, int status)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv)
		err = ch->ops->connect_cfm(ch, result, status);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_config_ind(l2cap_ch *ch)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv)
		err = ch->ops->config_ind(ch);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_config_cfm(l2cap_ch *ch, int result)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv)
		err = ch->ops->config_cfm(ch, result);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_disconnect_ind(l2cap_ch *ch)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv)
		err = ch->ops->disconnect_ind(ch);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_disconnect_cfm(l2cap_ch *ch)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv && ch->ops->disconnect_cfm)
		err = ch->ops->disconnect_cfm(ch);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv)
		err = ch->ops->data_ind(ch, skb);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

int l2ca_control_ind(l2cap_ch *ch, int event, void *arg)
{
	int	err = 0;
	spin_lock(&ch->callback_lock);
	ch->callback_cpu = smp_processor_id();
	if (ch->priv && ch->ops->control_ind)
		err = ch->ops->control_ind(ch, event, arg);
	ch->callback_cpu = -1;
	spin_unlock(&ch->callback_lock);
	return err;
}

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
static inline void l2ca_group_hold(l2cap_group_t *grp)
{
	atomic_inc(&grp->refcnt);
}

l2cap_group_t * __l2cap_group_lookup(__u16 psm)
{
	l2cap_group_t	*grp;

	btl_for_each (grp, l2cap_grps.l2cap_groups) {
		if (grp->psm == psm)
			return grp;
	}
	return NULL;
}

l2cap_group_t * l2cap_group_lookup(__u16 psm)
{
	l2cap_group_t	*grp;

	btl_read_lock(&l2cap_grps.l2cap_groups);
	grp = __l2cap_group_lookup(psm);
	if (grp)
		l2ca_group_hold(grp);
	btl_read_unlock(&l2cap_grps.l2cap_groups);
	return grp;
}

void l2ca_group_put(l2cap_group_t *grp)
{
	DBFENTER;
	if (atomic_dec_and_test(&grp->refcnt)) {
		if (atomic_read(&grp->refcnt) == 0) {
			/* group destroyed */
			l2ca_put(grp->channel);		
			grp->channel = NULL;
		}
	}
	DBFEXIT;
}

int l2ca_create_group(int psm, l2cap_proto_ops *group_ops, l2cap_ch **ch)
{
	int err = 0;
	BD_ADDR hcibcbda = {{0x00,0x00,0x00,0x00,0x00,0x00}};
	l2cap_group_t *group = NULL;
	hci_struct *selhci = NULL;

	if ((group = l2cap_group_lookup(psm))) {
		l2ca_group_put(group);
		return -ENOMEM;
	}
	if ((l2ca_register_protocol(psm, group_ops)) < 0) {
		return -ENOMEM;
	}
	/* Try creating a channel for same */
	*ch = l2cap_create(psm, &err);
	if (err) {
		return -ENOMEM;
	}

	/* set some attributes */
	memcpy(&(*ch)->bda, &hcibcbda,6);
	(*ch)->psm = psm;
	(*ch)->rcid = L2CAP_CID_GROUP;

	/* allocate group data struct */
	group = kmalloc(sizeof(l2cap_group_t), GFP_ATOMIC);
	if (!group) {
		l2ca_put(*ch);
		l2ca_unregister_protocol(psm);
		return -ENOMEM;
	}

	atomic_set(&group->refcnt,1);
	group->psm = psm;
	
	group->channel  = *ch;
	/* hold the channel */
	l2ca_hold(*ch);

	btl_head_init(&group->bdas);
	
	/* exclusive */
	write_lock_bh(&l2cap_grps.lock);
	
	/* ok! channel created, now allocate hci handle for bc */
	if (!l2cap_grps.bchndl) {
		if (lp_connect_broadcast(NULL,&hcibcbda,&l2cap_grps.bchndl,&selhci)) {
			kfree(group);
			l2ca_put(*ch);
			l2ca_unregister_protocol(psm);
			write_unlock_bh(&l2cap_grps.lock);
			return -ENOMEM;
		}

		//set some attributes
		(*ch)->con = l2cap_grps.bchndl;

		//inc reference
		hcc_hold(l2cap_grps.bchndl);
		
		(*ch)->hci = selhci;

		//inc reference/
		hci_hold(selhci);
	}

	btl_add_tail(&l2cap_grps.l2cap_groups,group);

	//inc reference
	hcc_hold(l2cap_grps.bchndl);
	
	write_unlock_bh(&l2cap_grps.lock);

	return err;
}

int l2ca_remove_group(int psm)
{
	int err = 0;
	l2cap_group_t *group = NULL;

	if (!(group = l2cap_group_lookup(psm))) {
		return -ENOMEM;
	}
	
	l2ca_group_put(group);
	
	/* release references */	
	l2ca_put(group->channel);
	//hci_put(l2cap_grps.bchndl);

	l2ca_unregister_protocol(group->psm);

	/* destroy group */	
	l2ca_group_put(group);

	write_lock_bh(&l2cap_grps.lock);

	btl_unlink(&l2cap_grps.l2cap_groups, group);

	kfree(group);	// free group

	/* no groups left */
	if (btl_empty(&l2cap_grps.l2cap_groups)) {

		/* destroy everything and disconnect*/
		hcc_put(l2cap_grps.bchndl);
		hcc_put(l2cap_grps.bchndl);
		l2cap_grps.bchndl = NULL;

		write_unlock_bh(&l2cap_grps.lock);

	} else {
		hcc_put(l2cap_grps.bchndl);
		write_unlock_bh(&l2cap_grps.lock);
	}

	return err;

}

/* caller takes care of allocating space for skb for headers */
int l2ca_send_data_group(int psm, struct sk_buff *skb)
{
	//__u16		len = (skb->len+L2CAP_HDR_LEN);
	__u16		len = skb->len;
	l2cap_grp_hdr_t	*hdr;
	int		err;
	l2cap_group_t	*group;

	DBFENTER;

	//printk("packing, len = %d\n", skb->len);
	
	if (!(group = l2cap_group_lookup(psm))) {
		return -ENOMEM;
	}

	hdr = (l2cap_grp_hdr_t*)skb_push(skb, L2CAP_GROUP_HDR_LEN);
	hdr->length = __htob16(len);
	hdr->cid = __htob16(L2CAP_CID_GROUP); // FIXME : hardcode?
	hdr->psm = __htob16(group->channel->psm);
	l2ca_group_put(group);
	DBPRT("send L2CAP Group Packet, len = %d\n", skb->len);
	err = lp_broadcast_data(l2cap_grps.bchndl, skb);
	DBFEXIT;
	
	return err;
	
}

/*l2cap_proto_ops group_ops = {
owner:		THIS_MODULE,
data_ind:	__group_data_ind,
connect_ind:	NULL,
connect_cfm:	NULL,
config_ind:	NULL,
config_cfm:	NULL,
disconnect_ind:	NULL
};*/
#endif // CONFIG_AFFIX_L2CAP_GROUPS

/******************    Lower layer helper functions    *********************/

/*
 * Called from HCI layer when new event arrive
 */
struct sk_buff * lp_reassemble_packet(hci_con *con, __u8 first, struct sk_buff *skb)
{
	__u16		length;
	struct sk_buff	*s;

	DBFENTER;

	if (first) {
		if (con->rx_skb) {
			BTDEBUG("con->rx_skb is not NULL, len: %d\n", con->rx_skb->len);
			kfree_skb(con->rx_skb);
			con->rx_skb = NULL;
		}
		if (skb->len < L2CAP_HDR_LEN) {
			BTDEBUG("L2CAP packet too small, drop it...\n");
			goto exit;
		}
		length = __btoh16(__get_u16(skb->data));
		DBPRT("Real length: %d\n", length + L2CAP_HDR_LEN);
		//printk("Real length: %d\n", length + L2CAP_HDR_LEN);
		DBPRT("skb->len   : %d\n", skb->len);

		if (skb->len >= length + L2CAP_HDR_LEN)
			return skb;

		s = alloc_skb(length + L2CAP_HDR_LEN, GFP_ATOMIC);
		if (s == NULL)
			goto exit;
		memcpy(skb_put(s, skb->len), skb->data, skb->len);
		if (con->rx_skb)
			kfree_skb(con->rx_skb);
		con->rx_skb = s;		
	} else {
		s = con->rx_skb;
		if (s == NULL)
			goto exit;
		length = __btoh16(__get_u16(s->data));
		if (skb_tailroom(s) < skb->len) {
			DBPRT("Received packet too big\n");
			con->rx_skb = NULL;
			kfree_skb(s);
			goto exit;
		}
		memcpy(skb_put(s, skb->len), skb->data, skb->len);
		DBPRT("accumulated, len: %d\n", s->len);
		if (s->len >= length + L2CAP_HDR_LEN) {
			con->rx_skb = NULL;
			kfree_skb(skb);
			DBFEXIT;
			return s;
		}
	}
exit:
	kfree_skb(skb);
	DBFEXIT;
	return NULL;
}


#ifdef CONFIG_AFFIX_BT_1_2

int lp_receive_data(hci_con *con, __u8 first, __u8 bc, struct sk_buff *skb)
{
	l2cap_grp_hdr_t		*hdr;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	l2cap_group_t		*group;
	int err;
#endif
	__u16 length,cid;

	
	DBFENTER;

	skb = lp_reassemble_packet(con, first, skb);
	if (skb == NULL)
		return 0;	/* accumulated */

	DBPRT("L2CAP packet received\n");
	//DBDUMP(skb->data, skb->len);

	hdr = (l2cap_grp_hdr_t*)skb->data;
	length = __btoh16(hdr->length);
	cid = __btoh16(hdr->cid);

	if (skb->len < length) { /* wrong packet */
		BTDEBUG("L2CAP packet too small, drop it...\n");
		kfree_skb(skb);
		return 0;
	}

	switch (cid) {
		case L2CAP_CID_SIGNAL:
			skb_pull(skb,L2CAP_HDR_LEN);
			l2cap_sig_ind(con, skb);
			break;
		case L2CAP_CID_GROUP:
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
			__u16 psm = 0;
			skb_pull(skb, L2CAP_HDR_LEN+2); /* pull extra bytes for connless psm */
			
			psm = __btoh16(hdr->psm); /* we need to reverse the psm */

			/*look for that group*/
			if (!(group = l2cap_group_lookup(psm))) {
				kfree_skb(skb);
				return 0;	
			}
			if (group->channel->ops->data_ind) {
				/* consumer frees the skb */
				err = group->channel->ops->data_ind(group->channel,skb);
			} else {
				BTDEBUG("\nNo handler for group data\n");
				kfree_skb(skb);
			}
			l2ca_group_put(group);
#else
			l2cap_cl_ind(con, skb);
#endif
			break;
		default:	/* Dynamic channels */
			l2cap_data_ind(con, cid, skb);
	}
	DBFEXIT;
	return 0;
}
#else
int lp_receive_data(hci_con *con, __u8 first, __u8 bc, struct sk_buff *skb)
{
	l2cap_grp_hdr_t		*hdr;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	l2cap_group_t		*group;
	int err;
#endif

	DBFENTER;

	skb = lp_reassemble_packet(con, first, skb);
	if (skb == NULL)
		return 0;	/* accumulated */

	DBPRT("L2CAP packet received\n");
	//DBDUMP(skb->data, skb->len);

	hdr = (l2cap_grp_hdr_t*)skb->data;
	hdr->length = __btoh16(hdr->length);
	hdr->cid = __btoh16(hdr->cid);

	skb_pull(skb, L2CAP_HDR_LEN);

	if (skb->len < hdr->length) { /* wrong packet */
		BTDEBUG("L2CAP packet too small, drop it...\n");
		kfree_skb(skb);
		return 0;
	}

	switch (hdr->cid) {
		case L2CAP_CID_SIGNAL:
			l2cap_sig_ind(con, skb);
			break;
		case L2CAP_CID_GROUP:
#ifdef CONFIG_AFFIX_L2CAP_GROUPS

			skb_pull(skb, 2); /* pull extra bytes for connless psm */
			
			hdr->psm = __btoh16(hdr->psm); /* we need to reverse the psm */

			/*look for that group*/
			if (!(group = l2cap_group_lookup(hdr->psm))) {
				kfree_skb(skb);
				return 0;	
			}
			if (group->channel->ops->data_ind) {
				/* consumer frees the skb */
				err = group->channel->ops->data_ind(group->channel,skb);
			} else {
				BTDEBUG("\nNo handler for group data\n");
				kfree_skb(skb);
			}
			l2ca_group_put(group);
#else
			l2cap_cl_ind(con, skb);
#endif
			break;
		default:	/* Dynamic channels */
			l2cap_data_ind(con, hdr->cid, skb);
	}
	DBFEXIT;
	return 0;
}
#endif

int l2cap_map_status(int status)
{
	switch (status) {
		case 0:
			return 0;
		case HCI_ERR_PAGE_TIMEOUT:
			return AFFIX_ERR_TIMEOUT;
		default:
			return L2CAP_CONRSP_RESOURCE;
	}
}

int lp_connect_cfm(hci_con *con, int status)
{
	l2cap_ch	*ch, *next;
	
	DBFENTER;
	DBPRT("status: %d\n", status);
	status = l2cap_map_status(status);	// l2cap mapping
	btl_write_lock(&l2cap_chs);
	btl_for_each_safe (ch, l2cap_chs, next) {
		if (__is_dead(ch) || ch->con != con)
			continue;
		l2ca_hold(ch);	// hold here to protect from l2ca_put()
		if (!ch->hci && con->hci) {
			hci_hold(con->hci);	// set reference
			ch->hci = con->hci;
		}
		if (status) {
			/* unable to connect */
			switch (l2cap_release(ch)) {
				case CON_W4_CONRSP:
					l2ca_connect_cfm(ch, status, 0);
					break;
				default:
					BTDEBUG("State is not handled\n");
					break;
			}
		} else {
			l2cap_reset_req_queue(ch);	// start timers
			if (ch->psm == 0 && STATE(ch) == CON_W4_CONRSP) {
				ENTERSTATE(ch, CON_OPEN);
				l2ca_connect_cfm(ch, 0, 0);
			}
		}
		__l2ca_put(ch);
	}
	btl_write_unlock(&l2cap_chs);
	DBFEXIT;
	return 0;
}

int lp_disconnect_ind(hci_con *con)
{
	l2cap_ch	*ch, *next;

	DBFENTER;
	btl_write_lock(&l2cap_chs);
	btl_for_each_safe (ch, l2cap_chs, next) {
		if (__is_dead(ch) || ch->con != con)
			continue;
		l2ca_hold(ch);
		switch (l2cap_release(ch)) {
			case CON_CLOSED:
			case CON_W4_DISCRSP:
				break;
			default:
				l2ca_disconnect_ind(ch);
				break;
		}
		__l2ca_put(ch);
	}
	btl_write_unlock(&l2cap_chs);
	DBFEXIT;
	return 0;
}

int lp_qos_violation_ind(hci_con *con)
{
	l2cap_ch	*ch;
	DBFENTER;
	btl_read_lock(&l2cap_chs);
	btl_for_each (ch, l2cap_chs) {
		if (__is_dead(ch) || ch->con != con)
			continue;
		l2ca_control_ind(ch, L2CAP_EVENT_QOS_VIOLATION, NULL);
	}
	btl_read_unlock(&l2cap_chs);
	DBFEXIT;
	return 0;
}

int __l2ca_connect_req(l2cap_ch *ch, BD_ADDR *bda)
{
	int		err = 0;

	DBFENTER;
	/* Create Baseband connection */
	err = lp_connect_req(ch->hci, bda, &ch->con);
	if (err) {
		DBPRT("lp_connect_req() failed\n");
		return err;
	
	}
	if (!ch->hci && ch->con->hci) {
		ch->hci = ch->con->hci;
		hci_hold(ch->hci);
	}
	DBFEXIT;
	return 0;
}

/*********************************  INITIALIZATION   **************************************/

#ifdef CONFIG_PROC_FS

int l2cap_proc_read(char *buf, char **start, off_t offset, int len)
{
	int 		count = 0;
	l2cap_ch	*ch;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	l2cap_group_t	*group = NULL;
#endif

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	count += sprintf(buf+count, "Groups   (%d):\n", l2cap_grps.l2cap_groups.len);
	btl_read_lock(&l2cap_grps.l2cap_groups);
	btl_for_each(group,l2cap_grps.l2cap_groups) {
		count += sprintf(buf+count,"psm = %#x, refcnt = %#x, lcid = %#x\n", 
				group->psm, atomic_read(&group->refcnt), group->channel->lcid);
	}
	
	btl_read_unlock(&l2cap_grps.l2cap_groups);
#endif
	
	count += sprintf(buf+count, "Channels (%d):\n", l2cap_chs.len);
	btl_read_lock(&l2cap_chs);
	btl_for_each (ch, l2cap_chs) {
		count += sprintf(buf+count,
				"lcid: %#x, rcid: %#x, bda: %s, psm: %d, mtu: %d, mru: %d,\n"
				"  reqcnt: %d, refcnt: %d, state: %d\n",
				ch->lcid, ch->rcid, bda2str(&ch->bda), ch->psm, ch->cfgin.mtu, ch->cfgout.mtu,
				skb_queue_len(&ch->req_queue), atomic_read(&ch->refcnt), STATE(ch));
	}
	btl_read_unlock(&l2cap_chs);
	DBFEXIT;
	return count;
}

struct proc_dir_entry	*l2cap_proc;

#endif

int init_lpf(void);
void exit_lpf(void);

int __init init_l2cap(void)
{
	int	err = -EINVAL;
	
	DBFENTER;
	btl_head_init(&l2cap_protos);
	btl_head_init(&l2cap_chs);
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	BTDEBUG("Initializing the group list\n");
	btl_head_init(&l2cap_grps.l2cap_groups); /* init group list */
#endif
#ifdef CONFIG_PROC_FS
	l2cap_proc = create_proc_info_entry("l2cap", 0, proc_affix, l2cap_proc_read);
	if (!l2cap_proc) {
		BTERROR("Unable to register proc fs entry\n");
		goto err1;
	}
#endif
	err = init_lpf();
	if (err) {
		BTERROR("Unable to initialize RFCOMM socket layer\n");
		goto err2;
	}
	DBFEXIT;
	return 0;
err2:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("l2cap", proc_affix);
err1:
#endif
	return err;
}

void __exit exit_l2cap(void)
{
	DBFENTER;
	exit_lpf();
#ifdef CONFIG_PROC_FS
	remove_proc_entry("l2cap", proc_affix);
#endif
	DBFEXIT;
}

EXPORT_SYMBOL(l2ca_register_protocol);
EXPORT_SYMBOL(l2ca_unregister_protocol);
EXPORT_SYMBOL(l2ca_disable_protocol);
EXPORT_SYMBOL(l2ca_connect_req);
EXPORT_SYMBOL(l2ca_connect_rsp);
EXPORT_SYMBOL(l2ca_config_req);
EXPORT_SYMBOL(l2ca_config_rsp);
EXPORT_SYMBOL(l2ca_send_data);
EXPORT_SYMBOL(l2ca_create);
EXPORT_SYMBOL(l2ca_put);


/*

 1. l2cap_sig_config_rsp
 What's going on if result is not 0???

 2. l2cap_sig_reject should find a l2cap_con object which sent command
 with id == reqid
 check con->state and do something

*/

/* 
 * used states
 * DEAD
 * CON_CLOSED
 * CON_W4_CONRSP
 * CON_W4_LCONRSP
 * CON_W4_DISCRSP
 * CON_CONFIG
 * CON_OPEN
 * CON_W4_LDISCRSP - packet
 * 
 */
