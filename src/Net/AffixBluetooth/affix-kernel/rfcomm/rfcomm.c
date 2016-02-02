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
   $Id: rfcomm.c,v 1.131 2004/05/25 16:02:18 kassatki Exp $

   RFCOMM - RFCOMM protocol for Bluetooth

	Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
		Imre Deak <ext-imre.deak@nokia.com>
*/		

#include <linux/config.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>

#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <net/sock.h>

#define FILEBIT	DBRFCOMM

/* Local Includes */
#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>

#include <affix/rfcomm.h>
#include "bty.h"


#define _INITIATOR(sn)		(test_bit(RFCOMM_FLAGS_INITIATOR, &(sn)->flags)? 0x00 : 0x01)
#define _GET_LENGTH(len)	(__btoh16(len)>>1)
#define _SET_LENGTH(len)	(__htob16(len<<1))

#define _SET_PF(ctr) 		((ctr) | (1 << 4)) 
#define _CLR_PF(ctr) 		((ctr) & 0xef)
#define _GET_PF(ctr) 		(((ctr) >> 4) & 0x1)


/* Local Variables */

btlist_head_t	rfcomm_protos;	/* upper protocol list */
btlist_head_t	rfcomm_sns;	/* sessions list */
__u8 		crctable[256];

/* references */
extern l2cap_proto_ops rfcomm_ops;

static int	channel_mask = 0;
int		sysctl_rfcomm_mtu = RFCOMM_GUESS_MTU;

int rfcon_connect_cfm(rfcomm_con *con, int status);
int rfcon_control_ind(rfcomm_con *con, int type);

void rfcomm_timer(unsigned long data);
rfcomm_sn *rfcomm_alloc(void);
rfcomm_sn *rfcomm_create(BD_ADDR *bda);
rfcomm_sn *rfcomm_lookup(BD_ADDR *bda);
rfcomm_con *__rfcon_create(rfcomm_sn *sn, __u8 dlci);
int rfcomm_connect(rfcomm_sn *sn);
int rfcomm_connect_req(rfcomm_con *con, rfcomm_sn **ret);
int rfcomm_disconnect_req(rfcomm_sn *sn);
int rfcomm_connect_cfm(rfcomm_sn *sn, int status);
int rfcomm_configure(rfcomm_sn *sn);
int rfcomm_config_complete(rfcomm_sn *sn, int status);

/* commands */
int rfcomm_send_sabm(rfcomm_sn *sn, __u8 dlci);
int rfcomm_send_ua(rfcomm_sn *sn, __u8 dlci);
int rfcomm_send_disc(rfcomm_sn *sn, __u8 dlci);
int rfcomm_send_dm(rfcomm_sn *sn, __u8 dlci);
int _rfcomm_send_dm(rfcomm_sn *sn, __u8 dlci, int solicited);
int rfcomm_send_uih(rfcomm_sn *sn, __u8 dlci, __u8 *data, uint len);

int rfcomm_send_credit(rfcomm_con *con, int credit);
int rfcomm_send_msc(rfcomm_con *con, __u8 cr, int brk);
int rfcomm_send_pn(rfcomm_con *con, __u8 cr);

/* CRC stuff */
__u8 rfcomm_crc_calc(void *data, unsigned int length);
__u32 rfcomm_crc_check(void *data, unsigned int length, __u8 check_sum);


/*
 * rfcomm client protocol stuff
 */
rfcomm_proto *__rfcomm_proto_lookup(__u8 channel)
{
	rfcomm_proto	*proto;

	btl_for_each (proto, rfcomm_protos) {
		if (proto->channel == channel)
			break;
	}
	return proto;
}

int rfcomm_register_protocol(__u16 *channel, rfcomm_proto_ops *ops, void *param)
{
	int		err = -EBUSY, mask = 1, sch = *channel;
	rfcomm_proto	*proto;

	DBFENTER;
	btl_write_lock(&rfcomm_protos);
	/* check if channel is static */
	if (sch != 0) {
		mask = 1 << (sch-1);
		if ((channel_mask & mask) != 0) {
			err = -EBUSY;
			goto exit;
		}
	} else {
		/* 
		   find free server channel. 
		   channel_mask is guarded with rfcomm_protos write lock
		 */
		for (sch = 1, mask = 1;  sch <= 30; sch++, mask <<= 1)
			if ((channel_mask & mask) == 0)
				break;
	}
	if (sch <= 30) {
		proto = kmalloc(sizeof(*proto), GFP_ATOMIC);
		if (proto == NULL) {
			err = -ENOMEM;
			goto exit;
		}

		channel_mask |= mask;
		*channel = sch;

		proto->channel = sch;
		proto->ops = ops;
		__btl_add_tail(&rfcomm_protos, proto);
		err = 0;
	}
exit:
	btl_write_unlock(&rfcomm_protos);
	DBFEXIT;
	return err;
}

void rfcomm_unregister_protocol(__u16 channel)
{
	rfcomm_proto	*proto;

	DBFENTER;
	btl_write_lock(&rfcomm_protos);
	proto = __rfcomm_proto_lookup(channel);
	if (proto != NULL) {
		__btl_unlink(&rfcomm_protos, proto);
		kfree(proto);
		/*  note that channel mask is guarded with rfcomm_protos write lock. */
		channel_mask &= ~(1<<(channel-1));
	}
	btl_write_unlock(&rfcomm_protos);
	DBFEXIT;
}

/*
 * connection stuff
 */
rfcomm_con *__rfcon_lookup(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_con	*con;

	DBFENTER;
	btl_for_each (con, sn->cons) {
		if (STATE(con) != DEAD && con->dlci == dlci)
			break;
	}
	DBFEXIT;
	return con;
}

rfcomm_con *rfcon_lookup(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_con	*con;

	DBFENTER;
	btl_read_lock(&sn->cons);
	con = __rfcon_lookup(sn, dlci);
	if (con)
		rfcon_hold(con);
	btl_read_unlock(&sn->cons);
	DBFEXIT;
	return con;
}

rfcomm_con *rfcon_create(BD_ADDR *bda, __u8 dlci, rfcomm_proto_ops *ops)
{
	rfcomm_con	*con;

	DBFENTER;

	con = kmalloc(sizeof(rfcomm_con), GFP_ATOMIC);
	if (!con)
		return NULL;
	memset(con, 0, sizeof(rfcomm_con));

	atomic_set(&con->refcnt, 1);	/* use it */
	spin_lock_init(&con->callback_lock);
	con->callback_cpu = -1;

	/* initialize some members */
	init_timer(&con->timer);
	con->timer.data = (unsigned long)con;
	con->timer.function = rfcon_timer;

	con->ops = ops;
	con->bda = *bda;
	con->dlci = dlci & 0x3F;

	/* 
	 * set protocol default parameters
	 */
	/* modem */
	con->peer_modem.ea = 1;
	con->peer_modem.fc = 1;		/* disabled */
	con->peer_modem.mr = 0;
	con->modem.ea = 1;
	con->modem.fc = 1;		/* disabled */
	con->modem.mr = 0;

	/* parameters */
	rfcon_setframesize(con, sysctl_rfcomm_mtu);

	/* port */
	con->param.bit_rate = RFCOMM_9600;
	con->param.format |= RFCOMM_CS8;
	con->param.format |= RFCOMM_1STOP;
	con->param.format |= RFCOMM_PARDIS;
	con->param.format |= RFCOMM_PARODD;
	con->param.fc = 0;
	con->param.mask = __htob16(RFCOMM_PARAM_MASK);	// all above are set

	// XON/XOFF skiped - not used
	
	/* credits */
	clear_bit(RFCOMM_FLAGS_CFC, &con->flags);
	set_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags);
	set_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
	
	ENTERSTATE(con, CON_CLOSED);	/* set initial state */

	DBFEXIT;
	return con;
}

void __rfcon_destroy(rfcomm_con *con)
{
	DBFENTER;
	if (con->sn) {
		__btl_unlink(&con->sn->cons, con);
		rfcomm_put(con->sn);
	}
	if (con->hci)
		hci_put(con->hci);
	kfree(con);
	DBFEXIT;
}

rfcomm_con *__rfcon_create(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_proto	*proto;
	rfcomm_con	*con;

	DBFENTER;
	/* lookup channel */
	btl_read_lock(&rfcomm_protos);
	proto = __rfcomm_proto_lookup(RFCOMM_SCH(dlci));
	if (!proto) {
		btl_read_unlock(&rfcomm_protos);
		return NULL;
	}
	con = rfcon_create(&sn->bda, dlci, proto->ops);
	if (!con) {
		btl_read_unlock(&rfcomm_protos);
		return NULL;
	}
	hci_hold(sn->ch->hci);	// set reference
	con->hci = sn->ch->hci;
	rfcon_bind_session(con, sn);
	btl_read_unlock(&rfcomm_protos);
	DBFEXIT;
	return con;
}

static inline void rfcon_destroy(rfcomm_con *con)
{
	rfcomm_sn	*sn = con->sn;

	DBFENTER;
	if (sn) {
		rfcomm_hold(sn);
		btl_write_lock(&sn->cons);
	}
	if (atomic_read(&con->refcnt) == 0)
		__rfcon_destroy(con);
	if (sn) {
		btl_write_unlock(&sn->cons);
		rfcomm_put(sn);
	}
	DBFEXIT;
}

void rfcon_put(rfcomm_con *con)
{
	DBFENTER;
	if (atomic_read(&con->refcnt) == 1 || 
			(!con->priv && STATE(con) != CON_W4_CONREQ)) {
		/* orphan */
		rfcon_disconnect_req(con);
	}
	if (atomic_dec_and_test(&con->refcnt)) {
		rfcon_destroy(con);
	}
	DBFEXIT;
}


int rfcon_connect_req(rfcomm_con *con)
{
	rfcomm_sn	*sn;
	int		err = 0;
	rfcomm_con	*c;

	DBFENTER;

	err = rfcomm_connect_req(con, &sn);
	if (err) {
		DBPRT("Unable to create RFCOMM session\n");
		return err;
	}

	con->dlci |= _INITIATOR(sn);	/* set session bit */

	/* check here if session already has connection */
	c = rfcon_lookup(sn, con->dlci);
	if (c) {
		rfcon_put(c);
		rfcomm_put(sn);
		return -EEXIST;
	}

	ENTERSTATE(con, CON_CLOSED);
	rfcon_bind_session(con, sn);
	rfcomm_put(sn);

	if (!rfcomm_connected(sn))
		return 0;

	if (STATE(con) == CON_CLOSED) {
		ENTERSTATE(con, CON_W4_CONRSP);
		rfcomm_send_sabm(sn, con->dlci);
		rfcon_start_timer(con, RFCOMM_CONRSP_TIMEOUT);	/* T1_TIMEOUT */
	}

	DBFEXIT;
	return err;
}

int rfcon_disconnect_req(rfcomm_con *con)
{
	rfcomm_sn	*sn = con->sn;

	DBFENTER;
	if (STATE(con) == CON_OPEN) {
		ENTERSTATE(con, CON_W4_DISCRSP);
		rfcomm_send_disc(sn, con->dlci);
		rfcon_start_timer(con, RFCOMM_DISCRSP_TIMEOUT);
	} else if (STATE(con) == CON_W4_CONRSP) {
		_rfcomm_send_dm(sn, con->dlci, 0);	// unsolicited
		rfcon_start_timer(con, RFCOMM_DISCRSP_TIMEOUT);
	}
	DBFEXIT;
	return 0;
}

int rfcon_check_auth(rfcomm_con *con)
{
	/* check access rights here */
	//DBPRT("sec_mode: %x, sec_level: %x\n", con->hci->sec_mode, rpf_get(pair)->sec_level);
	if ((con->hci->flags & HCI_SECURITY_SERVICE) && 
		(!(con->hci->flags & HCI_SECURITY_AUTH) && (con->security & HCI_SECURITY_AUTH))) {
		/* it has to be always ok */
		hci_con	*link = con->sn->ch->con;
		if (!hcc_authenticated(link)) {
			/* start pairing */
			ENTERSTATE(con, CON_W4_AUTHRSP);
			lp_auth_req(link);
			return 1;
		}
	}
	return 0;
}

int __rfcon_connect_rsp(rfcomm_con *con, int status)
{
	int	err = 0;

	DBFENTER;
	if (status == 0) {
		ENTERSTATE(con, CON_OPEN);
		rfcon_init_cfc(con);
		rfcomm_send_ua(con->sn, con->dlci);
	} else {
		ENTERSTATE(con, DEAD);
		rfcomm_send_dm(con->sn, con->dlci);
	}
	DBFEXIT;
	return err;
}

int rfcon_connect_rsp(rfcomm_con *con, int status)
{
	int	err = 0;

	DBFENTER;
	if (!con->sn)
		return 0;
	if (STATE(con) == CON_W4_LCONRSP) {
		if (status == 0) {
			err = rfcon_check_auth(con);
			if (err)	// auth in progress
				return 0;
		}
		__rfcon_connect_rsp(con, status);
		if (status == 0)
			con->ops->connect_cfm(con, 0);
	}
	DBFEXIT;
	return err;
}

/* to socket level */

static inline int rfcon_connect_ind(rfcomm_con *con)
{
	int	err;
	spin_lock(&con->callback_lock);
	con->callback_cpu = smp_processor_id();
	err = con->ops->connect_ind(con);
	con->callback_cpu = -1;
	spin_unlock(&con->callback_lock);
	return err;
}

int rfcon_connect_cfm(rfcomm_con *con, int status)
{
	int	err = 0;
	
	spin_lock(&con->callback_lock);
	con->callback_cpu = smp_processor_id();
	if (con->priv)
		err = con->ops->connect_cfm(con, status);
	con->callback_cpu = -1;
	spin_unlock(&con->callback_lock);
	return err;
}

static inline int rfcon_disconnect_ind(rfcomm_con *con)
{
	int	err = 0;
	spin_lock(&con->callback_lock);
	con->callback_cpu = smp_processor_id();
	if (con->priv)
		err = con->ops->disconnect_ind(con);
	con->callback_cpu = -1;
	spin_unlock(&con->callback_lock);
	return err;
}

int rfcon_control_ind(rfcomm_con *con, int type)
{
	int	err = 0;
	spin_lock(&con->callback_lock);
	con->callback_cpu = smp_processor_id();
	if (con->priv)
		err = con->ops->control_ind(con, type);
	con->callback_cpu = -1;
	spin_unlock(&con->callback_lock);
	return err;
}

static inline int rfcon_data_ind(rfcomm_con *con, struct sk_buff *skb)
{
	int	err = -ENOTCONN;
	spin_lock(&con->callback_lock);
	con->callback_cpu = smp_processor_id();
	if (con->priv)
		err = con->ops->data_ind(con, skb);
	con->callback_cpu = -1;
	spin_unlock(&con->callback_lock);
	return err;
}

/*
 * connection timer
 */
void rfcon_timer(unsigned long data)
{
	rfcomm_con	*con = (rfcomm_con*)data;

	DBFENTER;
	switch (SETSTATE(con, DEAD)) {
		case CON_W4_CONRSP:
			rfcon_connect_cfm(con, RFCOMM_STATUS_FAILURE);
			break;
		case CON_W4_CONREQ:
		case CON_CLOSED: // cons waiting for connect
			if (STATE(con->sn) == CON_CONFIG) {
				// PN sent
				rfcomm_config_complete(con->sn, RFCOMM_STATUS_FAILURE);
			}
			break;
		default:
			BTDEBUG("State is not handled!!!\n");
			break;
	}
	rfcon_put(con);
	DBFEXIT;
}


/*
 * session management
 */

static inline rfcomm_sn *__rfcomm_lookup(BD_ADDR *bda)
{
	rfcomm_sn	*sn;

	DBFENTER;
	btl_for_each (sn, rfcomm_sns) {
		if (STATE(sn) != DEAD && memcmp(&sn->bda, bda, 6) == 0)
			break;
	}
	DBFEXIT;
	return sn;
}

rfcomm_sn *rfcomm_create(BD_ADDR *bda)
{
	rfcomm_sn	*sn;

	DBFENTER;

	btl_write_lock(&rfcomm_sns);
	if (bda_zero(bda))
		sn = NULL;
	else
		sn = __rfcomm_lookup(bda);
	if (!sn) {
		sn = kmalloc(sizeof(rfcomm_sn), GFP_ATOMIC);
		if (!sn) {
			BTERROR("Memory allocation failed\n");
			goto exit;
		}
		memset(sn, 0, sizeof(rfcomm_sn));

		atomic_set(&sn->refcnt, 0);

		/* initialize internal data structure */
		init_timer(&sn->timer);
		sn->timer.data = (unsigned long)sn;
		sn->timer.function = rfcomm_timer;

		btl_head_init(&sn->cons);
		memcpy(&sn->bda, bda, 6);
		
		/* set protocol default parameters */
		sn->peer_fc = 1;				/* enabled */
		sn->fc = 1;					/* enabled */
		sn->rx_credit = 0; 				/* disable */
		clear_bit(RFCOMM_FLAGS_CFC, &sn->flags);	/* non-credit based */

		ENTERSTATE(sn, CON_CLOSED);			/* initial state */
		__btl_add_tail(&rfcomm_sns, sn);
	}
	rfcomm_hold(sn);
exit:
	btl_write_unlock(&rfcomm_sns);
	DBFEXIT;
	return sn;
}

int __rfcomm_destroy(rfcomm_sn *sn)
{
	DBFENTER;
	__btl_unlink(&rfcomm_sns, sn);
	if (sn->ch) {
		l2ca_orphan(sn->ch);
		l2ca_put(sn->ch);
	}
	kfree(sn);
	DBFEXIT;
	return 0;
}

rfcomm_con *find_active(btlist_head_t *cons)
{
	rfcomm_con	*con;

	btl_read_lock(cons);
	btl_for_each (con, *cons) {
		if (STATE(con) != DEAD) {
			rfcon_hold(con);
			break;
		}
	}
	btl_read_unlock(cons);
	return con;
}

int rfcomm_unused(rfcomm_sn *sn)
{
	if (atomic_read(&sn->refcnt) == 1) {
		rfcomm_con	*con;

		con = find_active(&sn->cons);
		if (con)
			rfcon_put(con);
		else
			return 1;
	}
	return 0;
}

void __rfcomm_put(rfcomm_sn *sn)
{
	if (STATE(sn) == CON_OPEN && rfcomm_unused(sn)) {
		/* start defer disconnection timer */
		rfcomm_start_timer(sn, RFCOMM_DEFER_DISC_TIMEOUT);
	}
	if (atomic_dec_and_test(&sn->refcnt))
		__rfcomm_destroy(sn);
}

void rfcomm_put(rfcomm_sn *sn)
{
	DBFENTER;
	if (STATE(sn) == CON_OPEN && rfcomm_unused(sn)) {
		/* start defer disconnection timer */
		rfcomm_start_timer(sn, RFCOMM_DEFER_DISC_TIMEOUT);
	}
	if (atomic_dec_and_test(&sn->refcnt)) {
		btl_write_lock(&rfcomm_sns);
		if (atomic_read(&sn->refcnt) == 0)
			__rfcomm_destroy(sn);
		btl_write_unlock(&rfcomm_sns);
	}
	DBFEXIT;
}

void rfcomm_timer(unsigned long data)
{
	rfcomm_sn	*sn = (rfcomm_sn*)data;

	DBFENTER;
	switch (STATE(sn)) {
		case CON_W4_CONRSP:
			ENTERSTATE(sn, DEAD);
			rfcomm_connect_cfm(sn, RFCOMM_STATUS_FAILURE);
			break;
		case CON_OPEN:
			/* defer disc timeout, ... */
			if (rfcomm_unused(sn))
				rfcomm_disconnect_req(sn);
			break;
		default:
			ENTERSTATE(sn, DEAD);
	}
	rfcomm_put(sn);
	DBFEXIT;
}


/* 
   1. Create and configure L2CAP channel
   2. Send SABM command on DLCI 0
   */
static inline int __rfcomm_connect_req(hci_struct *hci, rfcomm_sn *sn)
{
	l2cap_ch	*ch;
	int		err;

	DBFENTER;

	set_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	ch = l2ca_create(&rfcomm_ops);
	if (ch == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	sn->ch = ch;
	if (hci)
		hci_hold(hci);	// set reference
	ch->hci = hci;
	l2ca_graft(ch, sn);
	l2ca_set_mtu(ch, RFCOMM_LONG_HDR_SIZE + RFCOMM_CREDIT_SIZE + sysctl_rfcomm_mtu + RFCOMM_FCS_SIZE);
	err = l2ca_connect_req(ch, &sn->bda, RFCOMM_PSM);
exit:
	DBFEXIT;
	return err;
}

int rfcomm_connect_req(rfcomm_con *con, rfcomm_sn **ret)
{
	int		err = 0;
	rfcomm_sn	*sn;

	DBFENTER;
	sn = rfcomm_create(&con->bda);
	if (!sn)
		return -ENOMEM;
	if (STATE(sn) == CON_CLOSED) {
		ENTERSTATE(sn, CON_W4_CONRSP);
		err = __rfcomm_connect_req(con->hci, sn);
	}
	*ret = sn;
	DBFENTER;
	return err;
}

/*
 * session is ready to use
 */
int rfcomm_config_complete(rfcomm_sn *sn, int status)
{
	rfcomm_con	*con, *next;

	DBFENTER;
	btl_write_lock(&sn->cons);
	btl_for_each_safe (con, sn->cons, next) {
		rfcon_hold(con);
		if (status == 0) {
			if (STATE(con) == CON_CLOSED) {
				ENTERSTATE(con, CON_W4_CONRSP);
				con->dlci |= _INITIATOR(sn);			/* set session bit */
				rfcomm_send_sabm(sn, con->dlci);		/* send SABM */
				rfcon_start_timer(con, RFCOMM_CONRSP_TIMEOUT);	/* T1_TIMEOUT */
			}
		} else {
			ENTERSTATE(con, DEAD);
			rfcon_connect_cfm(con, RFCOMM_STATUS_FAILURE);
		}
		__rfcon_put(con);
	}
	btl_write_unlock(&sn->cons);
	DBFEXIT;
	return 0;
}

		
int rfcomm_connect_cfm(rfcomm_sn *sn, int status)
{
	rfcomm_con	*con;
	int		err = 0;

	DBFENTER; 
	if (status)
		return rfcomm_config_complete(sn, status);

	if (test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags)) {
		/* session initiator */
		set_bit(RFCOMM_FLAGS_CFC, &sn->flags);
		con = find_active(&sn->cons);
		if (con) {
			/* FIXME: check state here, should be CON_CLOSED */
			/* session in CONFIG state */
			rfcomm_send_pn(con, RFCOMM_MCC_CMD);
			rfcon_start_timer(con, RFCOMM_T2_TIMEOUT);
			rfcon_put(con);
		}
	} else {
		/* not initiator, start a timer to hold a session and wait for PN or SABM */
		rfcomm_start_timer(sn, RFCOMM_CFGREQ_TIMEOUT);
	}
	DBFEXIT;
	return err;
}

int rfcomm_disconnect_ind(rfcomm_sn *sn)
{
	rfcomm_con	*con, *next;

	DBFENTER;
	btl_write_lock(&sn->cons);
	btl_for_each_safe (con, sn->cons, next) {
		rfcon_hold(con);
		rfcon_stop_timer(con);
		switch (SETSTATE(con, DEAD)) {
			case DEAD:
			case CON_W4_DISCRSP:
				break;
			default:
				rfcon_disconnect_ind(con);
		}
		__rfcon_put(con);
	}
	btl_write_unlock(&sn->cons);
	DBFEXIT;
	return 0;
}

int rfcomm_disconnect_req(rfcomm_sn *sn)
{
	DBFENTER;
	if (STATE(sn) == CON_OPEN || STATE(sn) == CON_CONFIG) {
		ENTERSTATE(sn, CON_W4_DISCRSP);
		rfcomm_send_disc(sn, 0);
		rfcomm_start_timer(sn, RFCOMM_DISCRSP_TIMEOUT);
		return 1;
	}
	DBFEXIT;
	return 0;
}


/* 
 * RFCOMM transmitting stuff
 */
int rfcomm_send(rfcomm_sn *sn, struct sk_buff *skb)
{
	int	err;

	DBFENTER;
	DBPARSERFCOMM(skb->data, skb->len, TO_HOSTCTRL);
	DBDUMPCHAR(skb->data, skb->len);
	DBDUMP(skb->data, skb->len);
	err = l2ca_send_data(sn->ch, skb);
	if (err)
		kfree_skb(skb);
	DBFEXIT;
	return err;
}

int rfcomm_send_ctrl(rfcomm_sn *sn, rfcomm_hdr_t *pkt)
{
	struct sk_buff	*skb;

	skb = alloc_skb(L2CAP_SKB_RESERVE + RFCOMM_CTRL_SIZE, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, L2CAP_SKB_RESERVE);
	skb_put(skb, RFCOMM_CTRL_SIZE);
	if (pkt)
		memcpy(skb->data, pkt, RFCOMM_CTRL_SIZE);
	pkt = (void*)skb->data;
	pkt->ea = 1;		/* only one address field */
	pkt->lea = 1;
	pkt->len = 0;
	pkt->data[0] = rfcomm_crc_calc(pkt, RFCOMM_CTRL_CRC_CHECK);
	return rfcomm_send(sn, skb);
}	

int rfcomm_send_sabm(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_hdr_t	pkt;

	DBFENTER;
	DBPRT("Creating SABM packet to DLCI %d\n", dlci);
	pkt.cr = test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	pkt.dlci = dlci;
	pkt.control = _SET_PF(RFCOMM_SABM);
	return rfcomm_send_ctrl(sn, &pkt);
}

int rfcomm_send_ua(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_hdr_t	pkt;

	DBFENTER;
	DBPRT("Creating UA packet to DLCI %d\n",  dlci);
	pkt.cr = !test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	pkt.dlci = dlci;
	pkt.control = _SET_PF(RFCOMM_UA);
	return rfcomm_send_ctrl(sn, &pkt);
}

int _rfcomm_send_dm(rfcomm_sn *sn, __u8 dlci, int solicited)
{
	rfcomm_hdr_t	pkt;

	DBFENTER;
	DBPRT("Creating DM packet to DLCI %d\n", dlci);
	pkt.cr = !test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	pkt.dlci = dlci;
	pkt.control = solicited?_SET_PF(RFCOMM_DM):_CLR_PF(RFCOMM_DM);
	return rfcomm_send_ctrl(sn, &pkt);
}

int rfcomm_send_dm(rfcomm_sn *sn, __u8 dlci)
{
	return _rfcomm_send_dm(sn, dlci, 1);
}

int rfcomm_send_disc(rfcomm_sn *sn, __u8 dlci)
{
	rfcomm_hdr_t	pkt;

	DBFENTER;
	DBPRT("Creating DISC packet to DLCI %d\n", dlci);
	pkt.cr = test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	pkt.dlci = dlci;
	pkt.control = _SET_PF(RFCOMM_DISC);
	return rfcomm_send_ctrl(sn, &pkt);
}

void set_uih_hdr(rfcomm_hdr_t *pkt, __u8 dlci, uint len, __u8 cr)
{
	DBFENTER;
	pkt->ea = 1;
	pkt->cr = cr;
	pkt->dlci = dlci;
	pkt->control = _CLR_PF(RFCOMM_UIH);
	if(len > RFCOMM_SHORT_PAYLOAD_SIZE) {
		((rfcomm_long_hdr_t*) pkt)->len = _SET_LENGTH(len);  
	} else {
		pkt->lea = 1;
		pkt->len = len;  
	}
	DBFEXIT;
}

int rfcomm_send_credit(rfcomm_con *con, int credit)
{
	rfcomm_sn	*sn = con->sn;
	struct sk_buff	*skb;
	rfcomm_hdr_t	*pkt;

	DBFENTER;
	if (credit <= 0)
		return 0;
	credit &= 0xFF;
	DBPRT("Sending credits: %d\n", credit);
	skb = alloc_skb(L2CAP_SKB_RESERVE + sizeof(*pkt) + RFCOMM_CREDIT_SIZE + RFCOMM_FCS_SIZE, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, L2CAP_SKB_RESERVE);
	skb_put(skb, sizeof(*pkt) + RFCOMM_CREDIT_SIZE);

	pkt = (void*)skb->data;
	set_uih_hdr((void*)pkt, con->dlci, 0, test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags));
	pkt->control = _SET_PF(RFCOMM_UIH);
	pkt->data[0] = credit;
	atomic_add(credit, &con->rx_credit);
	DBPRT("credits: rx: %d, rx_actual: %d, tx: %d\n", atomic_read(&con->rx_credit), 
					con->rx_credit_actual, atomic_read(&con->tx_credit));
	*skb->tail = rfcomm_crc_calc(pkt, RFCOMM_UIH_CRC_CHECK);
	skb_put(skb, RFCOMM_FCS_SIZE);		/* add FCS */
	DBFEXIT;
	return rfcomm_send(sn, skb);
}

int rfcomm_send_data(rfcomm_con *con, struct sk_buff *skb)
{
	rfcomm_sn	*sn = con->sn;
	int		len = skb->len;	/* data length */
	int		credit = 0;
	rfcomm_hdr_t	*pkt;

	DBFENTER;
	DBPRT("Creating UIH packet, len: %d, dlci: %d\n", len, con->dlci);

	if (test_bit(RFCOMM_FLAGS_CFC, &con->flags)) {
		DBPRT("credits: rx: %d, rx_actual: %d, tx: %d\n", atomic_read(&con->rx_credit), 
						con->rx_credit_actual, atomic_read(&con->tx_credit));
		atomic_dec(&con->tx_credit);	/* decrease remote credit */
		rfcon_update_txfc(con);
		if (!test_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags) && 
				((atomic_read(&con->rx_credit) << 1) <= con->rx_credit_actual)) {
			credit = con->rx_credit_actual - atomic_read(&con->rx_credit);
			if (credit > 0) {
				credit &= 0xFF;
				*skb_push(skb, 1) = credit;
				atomic_add(credit, &con->rx_credit);
			}
		}
	}
	if (len > RFCOMM_SHORT_PAYLOAD_SIZE)
		pkt = (rfcomm_hdr_t*)skb_push(skb, RFCOMM_LONG_HDR_SIZE);
	else
		pkt = (rfcomm_hdr_t*)skb_push(skb, RFCOMM_SHORT_HDR_SIZE);

	set_uih_hdr(pkt, con->dlci, len, test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags));
	if (credit > 0)
		pkt->control = _SET_PF(RFCOMM_UIH);
	*skb->tail = rfcomm_crc_calc(pkt, RFCOMM_UIH_CRC_CHECK);
	skb_put(skb, RFCOMM_FCS_SIZE);		/* add FCS */

	return rfcomm_send(sn, skb);
}	

/*
 * assume that skb contains pure command
 */
int rfcomm_send_cmd(rfcomm_sn *sn, __u8 type, __u8 cr, void *msg, int size)
{
	rfcomm_hdr_t		*pkt;
	rfcomm_cmd_hdr_t	*cmd;
	struct sk_buff		*skb;

	DBFENTER;
	skb = alloc_skb(L2CAP_SKB_RESERVE + sizeof(*pkt) + sizeof(*cmd) + size + RFCOMM_FCS_SIZE, 
			GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, L2CAP_SKB_RESERVE);
	skb_put(skb,  sizeof(*pkt) + sizeof(*cmd) + size);
	pkt = (void*)skb->data;
	cmd = (void*)pkt->data;
	cmd->ea = 1;
	cmd->cr = cr;
	cmd->type = type;
	cmd->lea = 1;
	cmd->len = size;
	if (msg)
		memcpy(cmd->data, msg, size);
	set_uih_hdr(pkt, 0, sizeof (*cmd) + size, test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags));
	*skb->tail = rfcomm_crc_calc(pkt, RFCOMM_UIH_CRC_CHECK);
	skb_put(skb, RFCOMM_FCS_SIZE);		/* add FCS */
	return rfcomm_send(sn, skb);
}	


int rfcomm_send_msc(rfcomm_con *con, __u8 cr, int brk)
{
	rfcomm_sn		*sn = con->sn;
	struct rfcomm_msc_full	msg;

	DBFENTER;
	/* set fields */
	msg.ea = 1;
	msg.cr = 1;	// always 1
	msg.dlci = con->dlci;
	msg.sig = cr ? con->modem : con->peer_modem;
	if (brk)
		msg.brk = cr ? con->break_signal : con->peer_break_signal;
	/* send */
	rfcomm_send_cmd(sn, RFCOMM_MSC, cr, &msg, sizeof(msg) - (brk ? 0 : 1));
	DBFEXIT;
	return 0;
}

int rfcomm_send_rpn(rfcomm_con *con, __u8 cr)
{
	rfcomm_sn		*sn = con->sn;
	struct rfcomm_rpn	msg;

	DBFENTER;
	/* set fields */
	msg.ea = 1;
	msg.cr = 1;
	msg.dlci = con->dlci;
	msg.param = cr ? con->peer_param : con->param;
	/* send */
	rfcomm_send_cmd(sn, RFCOMM_RPN, cr, &msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int rfcomm_send_rls(rfcomm_con *con, __u8 cr)
{
	rfcomm_sn		*sn = con->sn;
	struct rfcomm_rls	msg;

	DBFENTER;
	/* set fields */
	msg.ea = 1;
	msg.cr = 1;
	msg.dlci = con->dlci;
	msg.res = 0;	/* Reserved bits are always set to 0 */
	msg.line_status = (cr)?con->line_status:con->peer_line_status;
	/* send */
	rfcomm_send_cmd(sn, RFCOMM_RLS, cr, &msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int rfcomm_send_pn(rfcomm_con *con, __u8 cr)
{
	rfcomm_sn		*sn = con->sn;
	struct rfcomm_pn	msg;

	DBFENTER;
	/* set fields */
	memset(&msg, 0, sizeof(msg));	/* zero it */
	msg.dlci = con->dlci;
	msg.frame_size = __htob16(con->frame_size);	/* may propose less */
	msg.priority = con->priority;
	if (test_bit(RFCOMM_FLAGS_CFC, &sn->flags)) {
		msg.flow = (cr == RFCOMM_MCC_CMD) ? 0x0F : 0x0E;
		if (sn->rx_credit > 0x07)
			sn->rx_credit = 0x07;
		msg.credit = sn->rx_credit;	/* let's accept */
	}
	/* send */
	rfcomm_send_cmd(sn, RFCOMM_PN, cr, &msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int rfcomm_send_nsc(rfcomm_sn *sn, __u8 type)
{
	struct rfcomm_nsc	msg;

	DBFENTER;
	/* set fields */
	msg.cmd_type = type;
	/* send */
	rfcomm_send_cmd(sn, RFCOMM_NSC, RFCOMM_MCC_CMD, &msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int rfcomm_send_fc(rfcomm_sn *sn, __u8 fc, int cr)
{
	DBFENTER;
	if (fc)
		rfcomm_send_cmd(sn, RFCOMM_FCON, cr, NULL, 0 /* RFCOMM_MCC_CMD */);
	else
		rfcomm_send_cmd(sn, RFCOMM_FCOFF, cr, NULL, 0 /* RFCOMM_MCC_CMD */);
	DBFEXIT;
	return 0;
}


/* receiving side */

/* packets processing */

int rfcomm_recv_sabm(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t	*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_con	*con = NULL;
	int		err = 0;
	__u8		dlci = pkt->dlci;

	DBFENTER;
	DBPRT("SABM received, dlci: %d\n", dlci);
	if (dlci == 0) {
		switch (STATE(sn)) {
			case CON_W4_CONREQ:
				rfcomm_stop_timer(sn);	/* conn_timeout */
				ENTERSTATE(sn, CON_CONFIG);	// need PN
				rfcomm_send_ua(sn, 0);
				rfcomm_connect_cfm(sn, RFCOMM_STATUS_SUCCESS);
				break;
			default:
				rfcomm_send_dm(sn, 0);
		}
	} else {
		if (STATE(sn) == CON_CONFIG) {
			/* 
			 * received sabm without prior cmd_pn
			 * switch to non-credit based FC
			 */
			rfcomm_stop_timer(sn);
			ENTERSTATE(sn, CON_OPEN);
		}
		/* check if dlci is already exists - PN, RPN */
		con = rfcon_lookup(sn, dlci);
		if (!con) {
			con = __rfcon_create(sn, dlci);
			if (!con) {
				rfcomm_send_dm(sn, dlci);
				err = -ENOMEM;
				goto exit;
			}		
		} else {
			if (STATE(con) != CON_W4_CONREQ) {
				/* should never happen */
				rfcomm_send_dm(sn, dlci);
				goto exit;
			}
			// CON_W4_CONREQ
			/* we got cmd_pn before. stop timer */
			rfcon_stop_timer(con);
		}
		ENTERSTATE(con, CON_W4_LCONRSP);
		set_bit(RFCOMM_FLAGS_SABM_RECV, &con->flags);
		rfcon_connect_ind(con);
	}
exit:
	if (con)
		rfcon_put(con);
	kfree_skb(skb);
	DBFEXIT;
	return err;
}

int rfcomm_recv_ua(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t	*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_con	*con;
	int		err = 0;
	__u8		dlci = pkt->dlci;

	DBFENTER;
	DBPRT("UA received, dlci: %d\n", dlci);
	if (dlci == 0) {
		rfcomm_stop_timer(sn);
		switch (STATE(sn)) {
			case CON_W4_CONRSP:
				ENTERSTATE(sn, CON_CONFIG);
				rfcomm_connect_cfm(sn, RFCOMM_STATUS_SUCCESS);
				break;
			case CON_W4_DISCRSP:
				ENTERSTATE(sn, DEAD);
				break;
			default:
				break;
		}
	} else {
		con = rfcon_lookup(sn, dlci);
		if (!con) {
			DBPRT("CON not found\n");
			goto exit;
		}
		rfcon_stop_timer(con);
		if (!con->hci) {
			con->hci = sn->ch->hci;
			hci_hold(con->hci);
		}
		switch (STATE(con)) {
			case CON_W4_CONRSP:
				ENTERSTATE(con, CON_OPEN);
				rfcon_init_cfc(con);
				rfcon_connect_cfm(con, RFCOMM_STATUS_SUCCESS);
				break;
			case CON_W4_DISCRSP:
				ENTERSTATE(con, DEAD);
				break;
			default:
				break;
		}
		rfcon_put(con);
	}
exit:
	kfree_skb(skb);
	DBFEXIT;
	return err;
}

int rfcomm_recv_dm(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t	*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_con	*con;
	__u8		dlci = pkt->dlci;
	int		err = 0;

	DBFENTER;
	DBPRT("DM received, dlci: %d\n", dlci);
	if (dlci == 0) {
		rfcomm_stop_timer(sn);
		switch (SETSTATE(sn, DEAD)) {
			case CON_W4_CONRSP:
				rfcomm_connect_cfm(sn, RFCOMM_STATUS_FAILURE);
				break;
			default:
				//CON_W4_DISCRSP
				break;
		}
	} else {
		/* search connecting cons */
		con = rfcon_lookup(sn, dlci);
		if (!con) {
			DBPRT("CON not found\n");
			goto exit;
		}
		rfcon_stop_timer(con);
		switch (SETSTATE(con, DEAD)) {
			case CON_CLOSED:
				if (STATE(sn) == CON_CONFIG) {
					// PN sent
					rfcomm_config_complete(sn, RFCOMM_STATUS_FAILURE);
				}
				break;
			case CON_W4_AUTHRSP:
			case CON_W4_CONRSP:
				rfcon_connect_cfm(con, RFCOMM_STATUS_FAILURE);
				break;
			default:
				//CON_W4_DISCRSP
				BTDEBUG("State is not handled!!!\n");
				break;
		}
		rfcon_put(con);
	}
exit:
	kfree_skb(skb);
	DBFEXIT;
	return err;
}

int rfcomm_recv_disc(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t	*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_con	*con;
	int		err = 0;
	__u8		dlci = pkt->dlci;

	DBFENTER;
	DBPRT("DISC received, dlci: %d\n", dlci);
	if (dlci == 0) {
		switch (SETSTATE(sn, DEAD)) {
			default:
				rfcomm_disconnect_ind(sn);
				rfcomm_send_ua(sn, 0);
		}
	} else {
		/* search connecting cons */
		con = rfcon_lookup(sn, dlci);
		if (!con) {
			rfcomm_send_dm(sn, dlci);
			goto exit;
		}
		switch (SETSTATE(con, DEAD)) {
			case CON_W4_DISCRSP:
				/* disc timer is running here
				 * do not ~dead~ con, return to that state
				 */
				ENTERSTATE(con, CON_W4_DISCRSP);
				break;
			case CON_OPEN:
				rfcon_disconnect_ind(con);
				break;
			default:
				BTDEBUG("State is not handled!!!\n");
				break;
		}
		rfcomm_send_ua(sn, dlci);
		rfcon_put(con);
	}
exit:
	kfree_skb(skb);
	DBFEXIT;
	return err;
}


int rfcomm_recv_data(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t	*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_con	*con;
	__u8		dlci = pkt->dlci, credit;

	DBFENTER;
	DBPRT("DATA frame has been received, dlci: %d, skblen: %d\n", dlci, skb->len);
	skb_pull(skb, sizeof(rfcomm_hdr_t) + !pkt->lea);
	DBPRT("DATA frame has been received, dlci: %d, skblen: %d\n", dlci, skb->len);
	con = rfcon_lookup(sn, dlci);
	if (!con) {
		DBPRT("CON not found\n");
		goto exit;
	}
	if (test_bit(RFCOMM_FLAGS_CFC, &con->flags)) {
		if (_GET_PF(pkt->control)) { /* read credit */
			credit = *skb->data;
			DBPRT("got credit: %d\n", credit);
			skb_pull(skb, 1);
			atomic_add(credit, &con->tx_credit);
			rfcon_update_txfc(con);
		}
		/* check current credit situation */
		if (skb->len && atomic_read(&con->rx_credit)) { //FIXME: accept anyway?
			atomic_dec(&con->rx_credit);
			rfcon_data_ind(con, skb);
		}
		DBPRT("credits: rx: %d, rx_actual: %d, tx: %d\n", atomic_read(&con->rx_credit), 
						con->rx_credit_actual, atomic_read(&con->tx_credit));
	} else {
		if (skb->len)
			rfcon_data_ind(con, skb);
	}
	rfcon_put(con);
exit:
	kfree_skb(skb);	// it must be skb_get()
	DBFEXIT;
	return 0;
}

/*
 * DLC parameter negotiation
 * should be used before DLC creation
 *
 * response..
 * replies with local proposals
 */

int rfcomm_recv_pn(rfcomm_sn *sn, struct sk_buff *skb, int cr)
{
	struct rfcomm_pn	*msg = (void*)skb->data;
	__u8		dlci = msg->dlci;
	rfcomm_con	*con;

	DBFENTER;

	con = rfcon_lookup(sn, dlci);
	if (cr == RFCOMM_MCC_CMD) {
		if (!con) {
			/* PN comes before SABM for DLCI!=0. Spec 1.1 */
#if 0
			if (test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags)) {
				// initiator send PN first!!!
				rfcomm_send_dm(sn, dlci);
				return -1;
			}
#endif
			con = __rfcon_create(sn, dlci);
			if (!con) {
				rfcomm_send_dm(sn, dlci);
				return -1;
			}
			/* this is orphan con now. start timer */
			ENTERSTATE(con, CON_W4_CONREQ);	/* need sabm next */
			rfcon_start_timer(con, RFCOMM_CONREQ_TIMEOUT);
		}
		/* store peer parameters */
		con->priority = msg->priority;
		/* accept peer frame size as it is */
		rfcon_setframesize(con, __btoh16(msg->frame_size));

		if (STATE(sn) == CON_CONFIG && msg->flow == 0x0F && 
				!test_and_set_bit(RFCOMM_FLAGS_CFC, &sn->flags))
			sn->tx_credit = msg->credit;

		/* set response local parameters */
		rfcomm_send_pn(con, RFCOMM_MCC_RSP);
		/* ready */
		if (!test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags)) {//FIXME ASAP
			if (STATE(sn) == CON_CONFIG) {
				rfcomm_stop_timer(sn);
				ENTERSTATE(sn, CON_OPEN);
				rfcomm_config_complete(sn, 0);
			}
		}
	} else {
		/* check response here */
		if (!con) {
			rfcomm_send_dm(sn, dlci);
			return -1;
		}
		if (STATE(sn) == CON_CONFIG)
			rfcon_stop_timer(con);

		/* store peer parameters */
		con->priority = msg->priority;
		/* accept peer frame size as it is */
		rfcon_setframesize(con, __btoh16(msg->frame_size));
		/*
		 * check credit based stuff
		 */
		if (STATE(sn) == CON_CONFIG) {
			sn->tx_credit = msg->credit;
			if (msg->flow != 0x0E)
				clear_bit(RFCOMM_FLAGS_CFC, &sn->flags);
		}
		/*
		 * session configured. send sabm for pending connections.
		 */
		if (STATE(sn) == CON_CONFIG) {
			ENTERSTATE(sn, CON_OPEN);
			rfcomm_config_complete(sn, 0);
		}
	}
	rfcon_put(con);
	DBFEXIT;
	return 0;
}

/*
   Remote con line status

   response - received values
   */
int rfcomm_recv_rls(rfcomm_sn *sn, struct sk_buff *skb, int cr)
{
	struct rfcomm_rls	*msg = (void*)skb->data;
	__u8		dlci = msg->dlci;
	rfcomm_con	*con;

	DBFENTER;

	con = rfcon_lookup(sn, dlci);
	if (cr == RFCOMM_MCC_CMD) {
		if (!con) {
			rfcomm_send_dm(sn, dlci);
			return -1;
		}
		con->peer_line_status = msg->line_status;
		rfcomm_send_rls(con, RFCOMM_MCC_RSP);
		rfcon_control_ind(con, RFCOMM_LINE_STATUS);
	} else {
		/* response... */
		if (!con)
			return -1;
		/* stop the timer here... */
	}	
	rfcon_put(con);

	DBFEXIT;
	return 0;
}

/*
 * Set remote port communication settings
 * response - analyze and accept or ... (rules)
 */
int rfcomm_recv_rpn(rfcomm_sn *sn, struct sk_buff *skb, int cr)
{
	struct rfcomm_rpn	*msg = (void*)skb->data;
	__u8		dlci = msg->dlci;
	rfcomm_con	*con;

	DBFENTER;
	con = rfcon_lookup(sn, dlci);
	if (cr == RFCOMM_MCC_CMD) {
		if (!con) {
			/* RPN can come before SABM for DLCI!=0. Spec 1.1 */
			con = __rfcon_create(sn, dlci);
			if (!con) {
				rfcomm_send_dm(sn, dlci);
				return -1;
			}
			/* this is orphan con now. start timer */
			ENTERSTATE(con, CON_W4_CONREQ);	/* need sabm next */
			rfcon_start_timer(con, RFCOMM_CONREQ_TIMEOUT);
		}
		if (skb->len > sizeof(__u8)) {
			/* new settings, accept them all for now */
			con->param = msg->param;
			rfcon_control_ind(con, RFCOMM_PORT_PARAM);
		}
		rfcomm_send_rpn(con, RFCOMM_MCC_RSP);
	} else {
		/* response... */
		if (!con)
			return -1;
		/* stop the timer here... */
	}	
	rfcon_put(con);
	DBFEXIT;
	return 0;
}

/*
   Modem Status Command

   response - received values
   */
int rfcomm_recv_msc(rfcomm_sn *sn, struct sk_buff *skb, int cr)
{
	rfcomm_cmd_hdr_t	*hdr = (rfcomm_cmd_hdr_t*)(skb->data-sizeof(rfcomm_cmd_hdr_t));
	struct rfcomm_msc_full	*msg = (void*)skb->data;
	__u8			dlci = msg->dlci;
	rfcomm_con		*con;

	DBFENTER;
	DBPRT("MSC cmd length: %d\n", hdr->len);
	con = rfcon_lookup(sn, dlci);
	if (cr == RFCOMM_MCC_CMD) {
		if (!con) {
			rfcomm_send_dm(sn, dlci);
			return -1;
		}
		con->peer_modem = msg->sig;
		if (hdr->len == 3) {
			con->peer_break_signal = msg->brk;
			rfcomm_send_msc(con, RFCOMM_MCC_RSP, 1);	/* response + break */
		} else
			rfcomm_send_msc(con, RFCOMM_MCC_RSP, 0);	/* response */
		rfcon_control_ind(con, (hdr->len == 3) ? RFCOMM_MODEM_STATUS_BRK : RFCOMM_MODEM_STATUS);
		if (!test_bit(RFCOMM_FLAGS_CFC, &con->flags))
			rfcon_update_txfc(con);
	} else {
		/* response... */
		if (!con)
			return -1;
		/* stop the timer here... */
	}
	rfcon_put(con);
	DBFEXIT;
	return 0;
}

int rfcomm_recv_fc(rfcomm_sn *sn, __u8 fc, int cr)
{
	__u8		delta_flow = sn->peer_fc;
	rfcomm_con	*con;

	DBFENTER;
	if (cr == RFCOMM_MCC_CMD) {
		sn->peer_fc = fc;
		delta_flow ^= fc;

		if (delta_flow && !test_bit(RFCOMM_FLAGS_CFC, &sn->flags)) {
			/* state changed */
			btl_read_lock(&sn->cons);	
			btl_for_each (con, sn->cons)
				rfcon_update_txfc(con);
			btl_read_unlock(&sn->cons);
		}
		rfcomm_send_fc(sn, fc, RFCOMM_MCC_RSP);
	} else {
		/* response... */
		/* stop the timer here... */
	}
	DBFEXIT;
	return 0;
}

int rfcomm_recv_cmd(rfcomm_sn *sn, struct sk_buff *skb)
{
	rfcomm_hdr_t		*pkt = (rfcomm_hdr_t*)skb->data;
	rfcomm_cmd_hdr_t	*cmd;
	int			err = 0;

	DBFENTER;

	/* packet is correct here - was checked earlier */
	if (pkt->lea) {
		skb_pull(skb, RFCOMM_SHORT_HDR_SIZE);
	} else {
		skb_pull(skb, RFCOMM_LONG_HDR_SIZE);
	}
	cmd = (void*)skb->data;
	if (skb->len < sizeof(rfcomm_cmd_hdr_t) && 
			skb->len < cmd->len + sizeof(rfcomm_cmd_hdr_t)) {
		BTDEBUG("cmd packet too small, drop it...\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	skb_pull(skb, sizeof(rfcomm_cmd_hdr_t));
	skb_trim(skb, cmd->len);

	DBPRT("RFCOMM %s: %x\n",(cmd->cr)?"cmd":"rsp", cmd->type);

	switch (cmd->type) {
		case RFCOMM_MSC:
			rfcomm_recv_msc(sn, skb, cmd->cr);
			break;
		case RFCOMM_RPN:
			rfcomm_recv_rpn(sn, skb, cmd->cr);
			break;
		case RFCOMM_RLS:
			rfcomm_recv_rls(sn, skb, cmd->cr);
			break;
		case RFCOMM_PN:
			rfcomm_recv_pn(sn, skb, cmd->cr);
			break;
		case RFCOMM_FCON:
			rfcomm_recv_fc(sn, 1, cmd->cr);
			break;
		case RFCOMM_FCOFF:
			rfcomm_recv_fc(sn, 0, cmd->cr);
			break;
		case RFCOMM_NSC:
			/* check what was a command */
			break;
		case RFCOMM_TEST:
			if (cmd->cr == RFCOMM_MCC_CMD) {
				rfcomm_send_cmd(sn,RFCOMM_TEST,RFCOMM_MCC_RSP,skb->data,skb->len);
			} 
			else {
				/* response... */
				/* If it is implemeted in the future, we can give an API to send TEST command. 
				 * Then we can signal back to the "socket" that
				 * everything has going fine. Also we will need to stop the timer for that. 
		 		 * BY NOW WE JUST IGNORE IT !!!!
		 		 */
			}
			break;
		default:
			DBPRT("Unsupported command: %#02x\n", cmd->type);
			rfcomm_send_nsc(sn, cmd->type);
	}
	kfree_skb(skb);
	DBFEXIT;
	return err;
}


/*
   these functions should send appropriate commands
*/
void rfcon_set_mcr(rfcomm_con *con, __u8 mcr)
{
	DBFENTER;
	if (con->modem.mr ^ mcr) {
		con->modem.mr = mcr;
		rfcomm_send_msc(con, RFCOMM_MCC_CMD, 0);
	}
	DBFEXIT;
}

void rfcon_set_rxspace(rfcomm_con *con, int size)
{
	DBFENTER;
	/* calc total credit */
	con->rx_credit_actual = (size > 0)? size >> con->power : 0;
	/* send credit */
	if (!test_bit(RFCOMM_FLAGS_CFC, &con->flags))
		return;
	if (test_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags))
		return;
	if ((atomic_read(&con->rx_credit) << 1) <= con->rx_credit_actual)
		rfcomm_send_credit(con, con->rx_credit_actual - atomic_read(&con->rx_credit));
	DBFEXIT;
}

void rfcon_set_rxfc(rfcomm_con *con, int flow)
{
	DBFENTER;
	if (flow == AFFIX_FLOW_ON) {
		clear_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags);
		con->modem.mr = RFCOMM_RTR | RFCOMM_RTC | RFCOMM_DV;
		con->modem.fc = 0;	// 0 == enable
	} else {
		set_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags);
		con->modem.mr = 0;	// Zero
		con->modem.fc = 1;
	}
	rfcomm_send_msc(con, RFCOMM_MCC_CMD, 0);
	if (!test_bit(RFCOMM_FLAGS_CFC, &con->flags))
		return;
	if (test_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags))
		return;
	rfcomm_send_credit(con, con->rx_credit_actual - atomic_read(&con->rx_credit));
	DBFEXIT;
}

void rfcon_init_cfc(rfcomm_con *con)
{
	if (!test_bit(RFCOMM_FLAGS_CFC, &con->sn->flags)) {
		clear_bit(RFCOMM_FLAGS_CFC, &con->flags);
		return;
	}
	set_bit(RFCOMM_FLAGS_CFC, &con->flags);
	atomic_set(&con->rx_credit, con->sn->rx_credit);
	if (atomic_read(&con->rx_credit) != 0)
		clear_bit(RFCOMM_FLAGS_RX_THROTTLED, &con->flags);
	atomic_set(&con->tx_credit, con->sn->tx_credit);
	if (atomic_read(&con->tx_credit) != 0)
		clear_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
}

void rfcon_update_txfc(rfcomm_con *con)
{
	if (test_bit(RFCOMM_FLAGS_CFC, &con->flags)) {
		if (atomic_read(&con->tx_credit) != 0) {
			if (test_and_clear_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags))
				rfcon_control_ind(con, RFCOMM_TX_WAKEUP);
		} else
			set_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
	} else {
		if (con->sn->peer_fc && !con->peer_modem.fc) {
			clear_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
			rfcon_control_ind(con, RFCOMM_TX_WAKEUP);
		} else {
			set_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
		}
	}
}

void rfcon_set_param(rfcomm_con *con, struct rfcomm_port_param *p)
{
	con->peer_param = *p;
	con->peer_param.mask = __htob16(RFCOMM_PARAM_MASK);
	rfcomm_send_rpn(con, RFCOMM_MCC_CMD);
}

void rfcon_set_break(rfcomm_con *con, __u8 b)
{
	con->break_signal.b = RFCOMM_BREAK;
	rfcomm_send_msc(con, RFCOMM_MCC_CMD, 1);
}


/* FCS table functions */

/* Calulates a reversed CRC table for the FCS check */

void rfcomm_create_crc_table(__u8 table[])
{
	int	i ,j;
	__u8 	data;
	__u8 	code_word = (__u8) 0xe0;  /*pol = x8+x2+x1+1*/
	__u8 	sr = (__u8) 0;            /*Shiftregister initiated to zero*/

	for (j=0; j < 256; j++) {
		data = (__u8) j;

		for (i=0;i<8;i++) {
			if((data & 0x1)^(sr & 0x1)) {
				sr >>= 1;
				sr ^= code_word;
			}
			else {
				sr >>= 1;
			}

			data >>= 1;
			sr &= 0xff;
		}

		table[j] = sr;
		sr = 0;
	} 
}

/* Calculates the checksum according to the RFCOMM specification */

__u8 rfcomm_crc_calc(void *data, unsigned int length)
{
	__u8	fcs = 0xff, *ptr = data;

	while (length--) {
		fcs = crctable[fcs^*ptr++];
	}
	/*Ones complement*/
	return 0xff-fcs;
}

#define CRC_VALID 0xcf

/* This functions check whether the checksum is correct or not. Length is
   the number of bytes in the message, data points to the beginning of the
   message */

__u32 rfcomm_crc_check(void *data, unsigned int length, __u8 check_sum)
{
	__u8	fcs = 0xff, *ptr = data;

	while (length--) {
		fcs = crctable[fcs^*ptr++];
	}
	fcs=crctable[fcs^check_sum];
	if (fcs == (uint) 0xcf)/*CRC_VALID)*/{
		return 0;
	} else {
		DBPRT("CRC INVALID: %#02x != 0xcf\n", fcs);
		return 1;
	}
}


/*
 * L2CAP client subsystem.
 * 
 */

int __rfcomm_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	rfcomm_sn	*sn = (rfcomm_sn*)ch->priv;
	rfcomm_hdr_t	*pkt;
	int		err = -EINVAL, len;
	__u8		control;

	DBFENTER;

	DBPARSERFCOMM(skb->data, skb->len, FROM_HOSTCTRL);
	DBDUMPCHAR(skb->data, skb->len);
	DBDUMP(skb->data, skb->len);
	
	/* process RFCOMM data packets */

	pkt = (rfcomm_hdr_t*)skb->data;
	if (skb->len < RFCOMM_SHORT_HDR_SIZE || (!pkt->lea && skb->len < RFCOMM_LONG_HDR_SIZE)) {
		BTDEBUG("RFCOMM packet too short, drop it...\n");
		goto exit;
	}
		
	control = _CLR_PF(pkt->control);

	if (pkt->lea) { /* short */
		len = pkt->len + RFCOMM_SHORT_HDR_SIZE;
	} else {
		len = _GET_LENGTH(((rfcomm_long_hdr_t*)pkt)->len) + RFCOMM_LONG_HDR_SIZE;
	}
	len += RFCOMM_FCS_SIZE;

	if (control == RFCOMM_UIH) {
		if (pkt->dlci != 0)
			len += _GET_PF(pkt->control); /* credit based flow control */
		if (skb->len < len) {
			BTDEBUG("RFCOMM packet too short, drop it...\n");
			goto exit;
		}
		DBPRT("frame length: %d\n", len);
		err =  rfcomm_crc_check(pkt, RFCOMM_UIH_CRC_CHECK, skb->data[len-1]);
	} else
		err =  rfcomm_crc_check(pkt, RFCOMM_CTRL_CRC_CHECK, pkt->data[0]);

	if (err) {
		/* FCS error */
		BTDEBUG("RFCOMM pkt w/ wrong checksum. drop it...\n");
		goto exit;
	}

	/* 
	 * cut packet and remove FCS 
	 * each packet has only one command anyway
	 */
	skb_trim(skb, len - RFCOMM_FCS_SIZE);

	rfcomm_hold(sn);
	switch(_CLR_PF(pkt->control)) {
		case RFCOMM_SABM:
			rfcomm_recv_sabm(sn, skb);
			break;
		case RFCOMM_UA:
			rfcomm_recv_ua(sn, skb);
			break;
		case RFCOMM_DM:
			rfcomm_recv_dm(sn, skb);
			break;
		case RFCOMM_DISC:
			rfcomm_recv_disc(sn, skb);
			break;
		case RFCOMM_UIH:
			if (pkt->dlci == 0) /* we have a command */
				rfcomm_recv_cmd(sn, skb);
			else
				rfcomm_recv_data(sn, skb);
			break;
		default:
			DBPRT("Unknown frame format\n");
			rfcomm_put(sn);
			goto exit;
	}
	rfcomm_put(sn);
	DBFEXIT;
	return 0;
exit:
	kfree_skb(skb);
	DBFEXIT;
	return err;
}

int __rfcomm_connect_ind(l2cap_ch *ch)
{
	rfcomm_sn	*sn;

	DBFENTER;

	sn = rfcomm_create(&ch->bda);
	if (!sn) {
		l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
		return -1;
	}
	clear_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags);
	l2ca_hold(ch);
	sn->ch = ch;
	l2ca_graft(ch, sn);
	l2ca_set_mtu(ch, RFCOMM_LONG_HDR_SIZE + RFCOMM_CREDIT_SIZE + sysctl_rfcomm_mtu + RFCOMM_FCS_SIZE);
	ENTERSTATE(sn, CON_W4_CONREQ);
	l2ca_connect_rsp(ch, L2CAP_CONRSP_SUCCESS, 0);

	rfcomm_start_timer(sn, RFCOMM_CONREQ_TIMEOUT);
	rfcomm_put(sn);

	DBFEXIT;
	return 0;
}

int __rfcomm_connect_cfm(l2cap_ch *ch, int result, int status)
{
	rfcomm_sn	*sn = (rfcomm_sn*)ch->priv;

	DBFENTER;
	rfcomm_hold(sn);
	if (result) {
		ENTERSTATE(sn, DEAD);
		rfcomm_connect_cfm(sn, RFCOMM_STATUS_FAILURE);
		goto exit;
	}
	if (test_bit(RFCOMM_FLAGS_INITIATOR, &sn->flags)) {
		rfcomm_send_sabm(sn, 0);
		rfcomm_start_timer(sn, RFCOMM_T1_TIMEOUT);	/* T1_TIMEOUT */
	}
exit:	
	rfcomm_put(sn);
	DBFEXIT;
	return 0;
}


int __rfcomm_config_ind(l2cap_ch *ch)
{
	rfcomm_sn	*sn = (rfcomm_sn*)ch->priv;
	__u16		result = 0;

	DBFENTER;
	if (!sn)
		return -ENODEV;
	l2ca_set_mtu(ch, RFCOMM_LONG_HDR_SIZE + RFCOMM_CREDIT_SIZE + sysctl_rfcomm_mtu + RFCOMM_FCS_SIZE);
	l2ca_config_rsp(ch, L2CAP_CFGRSP_SUCCESS);
	DBFEXIT;
	return result;
}

int __rfcomm_config_cfm(l2cap_ch *ch, int result)
{
	rfcomm_sn	*sn = (rfcomm_sn*)ch->priv;

	DBFENTER;
	if (result) {
		ENTERSTATE(sn, DEAD);
		rfcomm_disconnect_ind(sn);
	}
	DBFEXIT;
	return 0;
}

int __rfcomm_disconnect_ind(l2cap_ch *ch)
{
	rfcomm_sn	*sn = (rfcomm_sn*)ch->priv;

	DBFENTER;
	rfcomm_hold(sn);
	ENTERSTATE(sn, DEAD);
	rfcomm_stop_timer(sn);
	rfcomm_disconnect_ind(sn);
	rfcomm_put(sn);
	DBFEXIT;
	return 0;
}

/* Callback functions for L2CAP layer */
l2cap_proto_ops rfcomm_ops = {
owner:		THIS_MODULE,
data_ind:	__rfcomm_data_ind,
connect_ind:	__rfcomm_connect_ind,
connect_cfm:	__rfcomm_connect_cfm,
config_ind:	__rfcomm_config_ind,
config_cfm:	__rfcomm_config_cfm,
disconnect_ind:	__rfcomm_disconnect_ind
};

static inline int rpf_auth_complete(hci_con *link)
{
	rfcomm_sn	*sn, *_sn;
	rfcomm_con	*con, *_con;
	int		status;

	DBFENTER;
	btl_write_lock(&rfcomm_sns);
	btl_for_each_safe (sn, rfcomm_sns, _sn) {
		rfcomm_hold(sn);
		btl_write_lock(&sn->cons);
		btl_for_each_safe (con, sn->cons, _con) {
			if (STATE(con) != CON_W4_AUTHRSP)
				continue;
			if (con->sn->ch->con != link)
				continue;
			rfcon_hold(con);
			status = L2CAP_CONRSP_SUCCESS;
			if (!hcc_authenticated(link))
				status = L2CAP_CONRSP_SECURITY;
			/* set connection parameters */
			__rfcon_connect_rsp(con, status);
			rfcon_connect_cfm(con, status);
			__rfcon_put(con);
		}
		btl_write_unlock(&sn->cons);
		__rfcomm_put(sn);
	}
	btl_write_unlock(&rfcomm_sns);
	DBFEXIT;
	return 0;
}

int affix_event(struct notifier_block *nb, unsigned long event, void *arg)
{
	switch (event) {
		case HCICON_AUTH_COMPLETE:
			rpf_auth_complete(arg);
			break;
		default:
			break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block affix_notifier_block = {
	notifier_call:	affix_event,
};

#ifdef CONFIG_PROC_FS

int rfcomm_proc_read(char *buf, char **start, off_t offset, int len)
{
	int 		count = 0;
	rfcomm_sn	*sn;
	rfcomm_con	*con;

	DBFENTER;

	count += sprintf(buf+count, "Sessions (%d):\n", rfcomm_sns.len);
	btl_read_lock(&rfcomm_sns);
	btl_for_each (sn, rfcomm_sns) {
		count += sprintf(buf+count, 
				"sn: bda: %s, refcnt: %d, state: %d,\n"
				"    cfc: %s, rx_credit: %d, tx_credit: %d\n", 
				bda2str(&sn->bda), atomic_read(&sn->refcnt), STATE(sn),
				test_bit(RFCOMM_FLAGS_CFC, &sn->flags)?"yes":"no", 
				sn->rx_credit, sn->tx_credit);
		btl_read_lock(&sn->cons);
		btl_for_each (con, sn->cons) {
			count += sprintf(buf+count, 
					"       con: dlci: %d, refcnt: %d, state: %d,\n"
					"            cfc: %s, rx_credit: %d, tx_credit: %d, mtu: %d\n",
					con->dlci, atomic_read(&con->refcnt), STATE(con),
					test_bit(RFCOMM_FLAGS_CFC, &con->flags)?"yes":"no",
					atomic_read(&con->rx_credit), atomic_read(&con->tx_credit), con->frame_size);
		}
		btl_read_unlock(&sn->cons);
	}
	btl_read_unlock(&rfcomm_sns);

	DBFEXIT;
	return count;
}

struct proc_dir_entry	*rfcomm_proc;

#endif

int __init rfcomm_init(void)
{
	int	err;

	DBFENTER;
	rfcomm_create_crc_table(crctable);
	btl_head_init(&rfcomm_protos);
	btl_head_init(&rfcomm_sns);
	err = -ENOMEM;	
#ifdef CONFIG_PROC_FS
	rfcomm_proc = create_proc_info_entry("rfcomm", 0, proc_affix, rfcomm_proc_read);
	if (rfcomm_proc == NULL) {
		BTERROR("Unable to register proc fs entry\n");
		goto err1;
	}
#endif
	/* Register RFCOMM protocol */
	err =  l2ca_register_protocol(RFCOMM_PSM, &rfcomm_ops);
	if (err < 0) {
		BTERROR("Unable to register RFCOMM protocol\n");
		goto err2;
	}
	affix_register_notifier(&affix_notifier_block);
	DBFEXIT;
	return 0;
err2:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("rfcomm", proc_affix);
err1:
#endif
	return err;

}

void __exit rfcomm_exit(void)
{
	DBFENTER;
	l2ca_unregister_protocol(RFCOMM_PSM);
	affix_unregister_notifier(&affix_notifier_block);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("rfcomm", proc_affix);
#endif
	DBFEXIT;
}

/**********************************************************************************/

int rfcomm_sysctl_register(void);
void rfcomm_sysctl_unregister(void);

int init_rpf(void);
void exit_rpf(void);

#if 0 //def MODULE
static int can_unload(void)
{
	DBFENTER;
	l2ca_disable_protocol(RFCOMM_PSM, 1);
	if (GET_USE_COUNT(THIS_MODULE) > 0) {
		l2ca_disable_protocol(RFCOMM_PSM, 0);
		return -EBUSY;
	}
	DBFEXIT;
	return 0;
}
#endif

int __init init_rfcomm(void)
{
	int	err = 0;

	printk("Affix Bluetooth RFCOMM Protocol loaded (affix_rfcomm)\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");

#if 0 //def MODULE
	if (!mod_member_present(THIS_MODULE, can_unload)) {
		return -EBUSY;
	}
	(THIS_MODULE)->can_unload = can_unload;
#endif

#if defined(CONFIG_SYSCTL)
	err = rfcomm_sysctl_register();
	if (err)
		goto err1;
#endif
	err = rfcomm_init();
	if (err != 0)
		goto err2;

	err = init_rpf();
	if (err != 0)
		goto err3;

	err = bty_init();
	if (err != 0)
		goto err4;

	return 0;
err4:
	exit_rpf();
err3:	
	rfcomm_exit();
err2:
#if defined(CONFIG_SYSCTL)
	rfcomm_sysctl_unregister();
err1:
#endif
	return err;
}

void __exit exit_rfcomm(void)
{
	bty_exit();
	exit_rpf();
	rfcomm_exit();
#if defined(CONFIG_SYSCTL)
	rfcomm_sysctl_unregister();
#endif
}

/*if we are in the kernel we call it from hci.c */
module_init(init_rfcomm);
module_exit(exit_rfcomm);

MODULE_AUTHOR("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>");
MODULE_DESCRIPTION("Affix RFCOMM module");
MODULE_LICENSE("GPL");
MODULE_PARM(bty_maxdev, "i"); /* For insmod */

