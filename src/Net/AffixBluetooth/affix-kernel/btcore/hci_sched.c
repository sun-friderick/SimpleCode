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
   $Id: hci_sched.c,v 1.61 2004/02/19 16:54:12 kassatki Exp $

   HCI Packet Scheduler

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
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <asm/bitops.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/pkt_sched.h>

/* Local Includes */
#define FILEBIT	DBHCISCHED

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>

#if defined(CONFIG_AFFIX_SCO)

static inline struct sco_info *hci_queue_sco(hci_con *con, int len)
{
	struct sco_info	*sco;
	
	sco = kmalloc(sizeof(*sco), GFP_ATOMIC);
	if (!sco)
		return NULL;
	sco->handle = con->chandle;
	sco->len = len;
	btl_add_tail(&con->sco_pending, sco);
	return sco;
}

void hcc_sco_timer(unsigned long p)
{
	struct hci_con		*con = (void*)p;
	int			sent = HCI_SCO_CHUNK, flag = 0;
	struct sco_info		*sco;

	DBFENTER;
	if (!atomic_read(&con->pending))
		return;
	while (sent) {
		sco = btl_dequeue(&con->sco_pending);
		if (!sco)
			break;
		if (sent < sco->len) {
			sco->len -= sent;
			btl_add_head(&con->sco_pending, sco);
			break;
		}
		sent -= sco->len;
		atomic_dec(&con->pending);
		atomic_inc(&con->hci->sco_count);
		flag = 1;
		kfree(sco);
	}
	if (flag)
		hcidev_schedule(con->hci);
	if (atomic_read(&con->pending))
		mod_timer(&con->sco_timer, jiffies + HCI_SCO_TIMEOUT);
	else
		hcc_put(con);
	DBFEXIT;
}
#endif

/* Queue discipline for HCI */

static int hci_enqueue(hci_struct *hci, hci_con *con, struct sk_buff *skb)
{
	DBFENTER;
	DBPARSEHCI(skb->data, skb->len, skb->pkt_type, TO_HOSTCTRL);
	DBDUMP(skb->data, skb->len);
	do_gettimeofday(&skb->stamp);
	switch (skb->pkt_type) {
		case HCI_ACL:
			if (con)
				skb_queue_tail(&con->tx_queue, skb);
			else
				skb_queue_tail(&hci->acl_queue, skb);
			break;
#if defined(CONFIG_AFFIX_SCO)
		case HCI_SCO:
			if (con)
				skb_queue_tail(&con->tx_queue, skb);
			else
				skb_queue_tail(&hci->sco_queue, skb);
			break;
#endif
		case HCI_COMMAND:
			skb_queue_tail(&hci->cmd_queue, skb);
			break;
		default:
			DBPRT("Unknown packet type: %d\n", skb->pkt_type);
			return -EINVAL;
	}
	DBFEXIT;
	return 0;
}

/*
   this function is called if the ncc_xmit returns none-zero value..
   I should queue packet back 
   and check dev status to do not send again from hci_dequeue
*/
static int hci_requeue(hci_struct *hci, hci_con *con, struct sk_buff *skb)
{
	DBFENTER;
	switch (skb->pkt_type) {
		case HCI_ACL:
			if (con) {
				/* Remove ACL header */
				skb_pull(skb, HCI_ACL_HDR_LEN);
				skb_queue_head(&con->tx_queue, skb);
				atomic_dec(&con->pending);
				hcc_put(con);
			} else
				skb_queue_head(&hci->acl_queue, skb);
			atomic_inc(&hci->acl_count);
			break;
#if defined(CONFIG_AFFIX_SCO)
		case HCI_SCO:
			if (con) {
				/* Remove SCO header */
				skb_pull(skb, HCI_SCO_HDR_LEN);
				skb_queue_head(&con->tx_queue, skb);
				atomic_dec(&con->pending);
				if (hci->audio_mode & AFFIX_AUDIO_SYNC)
					kfree(btl_dequeue_tail(&con->sco_pending));
				hcc_put(con);
			} else
				skb_queue_head(&hci->sco_queue, skb);
			if (hci->audio_mode & (AFFIX_AUDIO_ASYNC | AFFIX_AUDIO_SYNC))
				atomic_inc(&hci->sco_count);
			break;
#endif
		case HCI_COMMAND:
			skb_queue_head(&hci->cmd_queue, skb);
			atomic_inc(&hci->cmd_count);
			break;
	}
	if (net_ratelimit())
		printk(KERN_DEBUG "%s deferred output. It is buggy.\n", skb->dev->name);
	DBFEXIT;
	return 0;
}

/*
   select candidate connection to be sended from
*/
static struct sk_buff *select_packet(hci_struct *hci, __u8 type, hci_con **ret)
{
	hci_con		*con;
	struct sk_buff	*rskb = NULL, *skb;

	DBFENTER;

	*ret = NULL;

	if (type == HCI_LT_ACL)
		skb = skb_peek(&hci->acl_queue);
	else
#if defined(CONFIG_AFFIX_SCO)
		skb = skb_peek(&hci->sco_queue);
#else
		return NULL;
#endif
	/* FIXME - now return with this skb */
	if (skb)
		return skb;
		
	btl_read_lock(&hcicons);	//lock
	/* find first */
	btl_for_each (con, hcicons) {
		if (__hcc_linkup(con) && con->hci == hci && con->link_type == type &&
		    (skb = skb_peek(&con->tx_queue)) != NULL ) {
			*ret = con;
			rskb = skb;
			break;
		}
	}
	if (rskb == NULL) /* nothing */
		goto exit;
	/* find best */
	btl_for_each_cur(con, hcicons) {
		if (__hcc_linkup(con) && con->hci == hci && con->link_type == type &&
		    (skb = skb_peek(&con->tx_queue)) != NULL ) {
			if (atomic_read(&con->pending) == atomic_read(&((*ret)->pending))) {
				if (cmp_timeval(&skb->stamp, &rskb->stamp)) {
					*ret = con;
					rskb = skb;
				}
			} else if (atomic_read(&con->pending) < atomic_read(&((*ret)->pending))) {
				*ret = con;
				rskb = skb;
			}
		}
	}
	if (*ret)
		hcc_hold(*ret);
exit:
	btl_read_unlock(&hcicons);	//unlock
	DBFEXIT;
	return rskb;
}

static struct sk_buff *acl_dequeue(hci_con *con, struct sk_buff *skb)
{
	struct sk_buff		*s;
	int			len, pb;
	HCI_ACL_Packet_Header	*hdr;
	hci_struct		*hci = con->hci;

	pb = hci_skb(skb)->pb;
	if (skb->len <= con->mtu) {
		skb_unlink(skb);
		s = skb;
		len = s->len;
	} else {
		len = con->mtu;
		s = alloc_skb(HCI_SKB_RESERVE + len, GFP_ATOMIC);
		if (s == NULL) {
			BTERROR("alloc_skb() failed\n");
			return NULL;
		}
		skb_reserve(s, HCI_SKB_RESERVE);
		memcpy(skb_put(s, len), skb->data, len);

		s->pkt_type = HCI_ACL;
		hci_skb(s)->pb = pb;
		s->stamp = skb->stamp;
		s->dev = (void*)hci;

		skb_pull(skb, len);
		hci_skb(skb)->pb = HCI_PB_MORE;	/* update current Packet Boundary */
	}
	hdr = (void*)skb_push(s, HCI_ACL_HDR_LEN);
	
#ifdef CONFIG_AFFIX_HCI_BROADCAST
	if (!bda_zero(&con->bda))
		hdr->Connection_Handle = __htob16(con->chandle | pb | HCI_BC_PP);
	else
		hdr->Connection_Handle = __htob16(HCI_BROADCAST_CHANDLE | pb | HCI_BC_PICONET);
#else

	hdr->Connection_Handle = __htob16(con->chandle | pb | HCI_BC_PP);
#endif
	
	hdr->Length = __htob16(len);
	return s;
}

#if defined(CONFIG_AFFIX_SCO)
static struct sk_buff *sco_dequeue(hci_con *con, struct sk_buff *skb)
{
	struct sk_buff		*s;
	int			len;
	HCI_SCO_Packet_Header	*hdr;
	hci_struct		*hci = con->hci;

	if (skb->len <= con->mtu) {
		skb_unlink(skb);
		s = skb;
		len = s->len;
	} else {
		len = con->mtu;
		s = alloc_skb(HCI_SKB_RESERVE + len, GFP_ATOMIC);
		if (s == NULL) {
			BTERROR("No memory\n");
			return NULL;
		}
		skb_reserve(s, HCI_SKB_RESERVE);
		memcpy(skb_put(s, len), skb->data, len);

		s->pkt_type = HCI_SCO;
		s->stamp = skb->stamp;
		s->dev = (void*)hci;
		skb_pull(skb, len);
	}
	hdr = (void*)skb_push(s, HCI_SCO_HDR_LEN);
	hdr->Connection_Handle = __htob16(con->chandle);
	hdr->Length = len;
	return s;
}
#endif

static struct sk_buff *hci_dequeue(hci_struct *hci, hci_con **con)
{
	struct sk_buff		*acl, *sco, *cmd, *r;
	hci_con			*acl_con = NULL, *sco_con = NULL;

	DBFENTER;
	DBPRT("cmd_count: %d, acl_count: %d, sco_count: %d\n",
			atomic_read(&hci->cmd_count), 
			atomic_read(&hci->acl_count),
			atomic_read(&hci->sco_count));

	/* select the element from queues */
	cmd = (atomic_read(&hci->cmd_count)) ? skb_peek(&hci->cmd_queue) : NULL;
	acl = (atomic_read(&hci->acl_count)) ? select_packet(hci, HCI_LT_ACL, &acl_con) : NULL;
#if defined(CONFIG_AFFIX_SCO)
	sco = (atomic_read(&hci->sco_count)) ? select_packet(hci, HCI_LT_SCO, &sco_con) : NULL;
#else
	sco = NULL;
#endif
	if (sco) {
		if (cmd)
			r = (cmp_timeval(&sco->stamp, &cmd->stamp) ? sco : cmd);
		else
			r = sco;
	} else {
		if (cmd)
			r = cmd;
		else {
			r = acl;	// sco == NULL and cmd == NULL
			if (!r)
				return NULL;
			goto exit;
		}
	}
	if (acl)
		r = (cmp_timeval(&r->stamp, &acl->stamp) ? r : acl);
 exit:
	hci->stats.tx_bytes += r->len;
	switch (r->pkt_type) {
		case HCI_COMMAND:
			skb_unlink(r);
			atomic_dec(&hci->cmd_count);
			*con = NULL;
			hci->stats.tx_cmd++;
			break;
		case HCI_ACL:
			if (acl_con) {
				r = acl_dequeue(acl_con, r);
				if (!r)
					break;
				atomic_inc(&acl_con->pending);
				hcc_hold(acl_con);
			} else {
				skb_unlink(r);
				//hci->acl_priority++;
			}
			*con = acl_con;
			atomic_dec(&hci->acl_count);
			hci->stats.tx_acl++;
			break;
#if defined(CONFIG_AFFIX_SCO)
		case HCI_SCO:
			if (sco_con) {
				r = sco_dequeue(sco_con, r);
				if (!r)
					break;
				hcc_hold(sco_con);
				if (hci->audio_mode & AFFIX_AUDIO_SYNC) {
					if (!atomic_read(&sco_con->pending)) {
						/* restart SCO engine */
						btl_purge(&sco_con->sco_pending);
						if (!mod_timer(&sco_con->sco_timer, jiffies + HCI_SCO_TIMEOUT))
							hcc_hold(sco_con);
					}
					hci_queue_sco(sco_con, r->len);
				}
				atomic_inc(&sco_con->pending);
			} else {
				skb_unlink(r);
				//hci->sco_priority++;
			}
			*con = sco_con;
			if (hci->audio_mode & (AFFIX_AUDIO_ASYNC | AFFIX_AUDIO_SYNC))
				atomic_dec(&hci->sco_count);
			hci->stats.tx_sco++;
			break;
#endif
	}
	if (acl_con)
		hcc_put(acl_con);
	if (sco_con)
		hcc_put(sco_con);
	DBFEXIT;
	return r;
}

static int hci_tx_wakeup(hci_struct *hci)
{
	struct sk_buff	*skb;
	int		err;
	hci_con		*con;

	DBFENTER;
	if (hcidev_queue_stopped(hci))
		return 0;
	if (spin_trylock(&hci->xmit_lock)) {
		for (;;) {
			if (hcidev_queue_stopped(hci))
				break;
			if ((skb = hci_dequeue(hci, &con)) == NULL)
				break;
			spin_unlock(&hci->queue_lock);	/* unlock queue */
			/* do promisc */
			if (hci_promisc_count) {
				struct sk_buff	*new_skb;

				new_skb = skb_clone(skb, GFP_ATOMIC);
				if (new_skb) {
					new_skb->pkt_type |= HCI_PKT_OUTGOING;
					hpf_recv_raw(hci, con, new_skb);
				}
			}
			hci->xmit_lock_owner = smp_processor_id();
			err = hci->send(hci, skb);
			hci->xmit_lock_owner = -1;
			spin_lock(&hci->queue_lock);	/* lock queue */
			if (err) {
				hci_requeue(hci, con, skb);
				hcidev_schedule(hci);	/* schedule later */
				break;
			}
			if (con)
				hcc_put(con);
		}
		spin_unlock(&hci->xmit_lock);
	} else {
		/* So, someone grabbed the driver. */
		if (hci->xmit_lock_owner == smp_processor_id()) {
			if (net_ratelimit())
				BTDEBUG("Dead loop on Bluetooth device %s\n", hci->name);
		}
	}
	DBFEXIT;
	return 0;
}

void hci_tx_task(unsigned long arg)
{
	hci_struct	*hci = (hci_struct *)arg;

	DBFENTER;
	if (spin_trylock(&hci->queue_lock)) {
		hci_tx_wakeup(hci);
		spin_unlock(&hci->queue_lock);
	} else
		hcidev_schedule(hci);
	DBFEXIT;
}

int hci_queue_xmit(hci_struct *hci, hci_con *con, struct sk_buff *skb)
{
	int	err;

	DBFENTER;
	if (!hci) {
		/* connection enqueue */
		return hci_enqueue(hci, con, skb);
	}
	if (!hcidev_running(hci))
		return -ENETDOWN;	/* fine ??? */
	/* Grab device queue */
	spin_lock_bh(&hci->queue_lock);
	err = hci_enqueue(hci, con, skb);
	if (!err)
		hci_tx_wakeup(hci);
	spin_unlock_bh(&hci->queue_lock);
	DBFEXIT;
	return err;
}

