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
   $Id: af_hci.c,v 1.210 2004/07/22 14:38:03 chineape Exp $

   AF_AFFIX - HCI Protocol Address family for socket interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>
#include <linux/version.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/notifier.h>
#include <linux/poll.h>
#include <linux/kmod.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/utsname.h>

#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>

/* Local Includes */
#define FILEBIT	DBAFHCI

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>

/* include it for kernel HCI commands support */
static int	errno;
#include <affix/hci_cmds.h>


btlist_head_t	hci_socks;
spinlock_t	hpf_spinlock = SPIN_LOCK_UNLOCKED;

static inline int ___test_and_set_flag(unsigned long *flag, int value)
{
	int	ret = *flag & value;
	if (!ret)
		*flag |= value;
	return ret;
}
	
static inline int ___test_and_clear_flag(unsigned long *flag, int value)
{
	int	ret = *flag & value;
	if (ret)
		*flag &= ~value;
	return ret;
}
	/*
 * debuging stuff
 */
static long 	sock_count = 0;

typedef struct  {
	affix_sock_t	base;

	unsigned long	flags;
	hci_con		*con;
	hci_struct	*hci;
	__u64		event_mask;
	unsigned long	pkt_mask;

	/* command processing */
	hci_lock_t	*lock;		/* waiting here */
	__u16		opcode;

} hci_sock_t;

int hpf_cmd_handler(hci_sock_t *hsk, __u8 event, struct sk_buff *skb);
int hpf_lock_hci(struct sock *sk, int lock);

static inline hci_sock_t *hpf_get(struct sock *sk)
{
	return (hci_sock_t*)sk->sk_prot;
}

static inline void hpf_unlock(hci_sock_t *hsk)
{
	if (hsk->lock) {
		hci_unlock_cmd(hsk->lock);
		hsk->lock = NULL;
	}
}

/***************************    Connection Management   ********************************/

hci_sock_t *hpf_alloc(int priority)
{
	hci_sock_t	*hsk;

	hsk = (hci_sock_t*)kmalloc(sizeof(hci_sock_t), priority);
	if (!hsk)
		return NULL;
	memset(hsk, 0, sizeof(hci_sock_t));
	return hsk;
}

void hpf_destruct(struct sock *sk)
{
	hci_sock_t	*hsk = hpf_get(sk);

	DBFENTER;
	kfree(hsk);
	sock_count--;	/* XXX */
	DBFEXIT;
}

void hpf_destroy(struct sock *sk)
{
	hci_sock_t	*hsk = hpf_get(sk);
	
	DBFENTER;
        sk->sk_err = 0;
	sk->sk_state = CON_CLOSED;
	sk->sk_shutdown = SHUTDOWN_MASK;
	btl_unlink(&hci_socks, hsk);
	if (hsk->hci) {
		hci_put(hsk->hci);
		hsk->hci = NULL;
	}
	if (hsk->con) {
		hcc_close(hsk->con);
		hsk->con = NULL;
	}
	if (sk->sk_pair) {
		sk->sk_pair->sk_ack_backlog--;
		sock_put(sk->sk_pair);
		sk->sk_pair = NULL;
	}
	if (hsk->flags & AFFIX_FLAGS_CTLMASK)
		affix_ctl_mode &= ~(hsk->flags & AFFIX_FLAGS_CTLMASK);

	if (sk->sk_protocol == BTPROTO_HCIACL || sk->sk_protocol == BTPROTO_HCISCO) {
		struct sk_buff	*skb;

		DBPRT("queue len: %d\n", skb_queue_len(&sk->sk_receive_queue));
		/* remove all packets and check if threre is a connection in the queue */
		while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
			if (skb->sk != sk)	/* A pending connection */
				hpf_destroy(skb->sk);
			kfree_skb(skb);
		}
	}
	sock_put(sk);
	DBFEXIT;
}

struct sock *hpf_create(struct socket *sock, int family, int priority, int zero_it)
{
	struct sock	*sk;
	hci_sock_t	*hsk;

	DBFENTER;
	sk  = sk_alloc(family, priority, zero_it, NULL);
	if (!sk)
		return NULL;
	sock_init_data(sock, sk);
	hsk = hpf_alloc(priority);
	if (!hsk) {
		sk_free(sk);
		return NULL;
	}
	hsk->base.sk = sk;
	sk->sk_prot = (void*)hsk;
	//__btl_add_tail(&hci_socks, hsk);
	sock_count++;	/* XXX */
	sk_set_owner(sk, THIS_MODULE);
	DBFEXIT;
	return sk;
}

int hpf_connect_req(struct sock *sk, struct sockaddr_affix *addr)
{
	int		err = 0;
	hci_struct	*hci;
	hci_sock_t	*hsk = hpf_get(sk);
#ifdef CONFIG_AFFIX_HCI_BROADCAST
	hci_struct 	*selhci;
#endif

	DBFENTER;

	if (addr->devnum) {
		hci = hci_lookup_devnum(addr->devnum);	/* find device */
		if (hci == NULL)
			return -ENODEV;
	} else
		hci = NULL;

	if (sk->sk_protocol == BTPROTO_HCIACL) {
#ifdef CONFIG_AFFIX_HCI_BROADCAST
		if (sk->sk_type == SOCK_DGRAM)
			err = lp_connect_broadcast(hci, &addr->bda, &hsk->con, &selhci);
		else
#endif
			err = lp_connect_req(hci, &addr->bda, &hsk->con);
	
		if (err) {
			if (hci)
				hci_put(hci);
			return err;
		}
		hsk->hci = hci;
		if (__hcc_linkup(hsk->con))
			sk->sk_state = CON_ESTABLISHED;
	} 
#if defined(CONFIG_AFFIX_SCO)
	else if (sk->sk_protocol == BTPROTO_HCISCO) {
            err = lp_add_sco(hci, &addr->bda, &hsk->con);
		if (err) {
			if (hci)
				hci_put(hci);
			return err;
		}
		hsk->hci = hci;
	}
#endif
	DBFEXIT;
	return err;
}

static inline void hpf_reset(struct sock *sk, int reason)
{
	affix_sock_reset(sk, reason, NULL);
}

/* 
   HPF protocol subsystem.  Client of HCI. Callback functions
*/
int hpf_recv_raw(hci_struct *hci, hci_con *con, struct sk_buff *skb)
{
	hci_sock_t		*hsk, *next;
	struct sk_buff		*new_skb;
	struct sock		*sk;

	DBFENTER;
	if (hci)
		hci_skb(skb)->devnum = hci->devnum;
	if (con)
		hci_skb(skb)->conid = con->id;
	btl_read_lock(&hci_socks);
	btl_for_each_safe (hsk, hci_socks, next) {
		sk = hsk->base.sk;
		if (sk->sk_type != SOCK_RAW) {
			/* HCIACL or HCISCO sockets - connection oriented */
			if (!con || hsk->con != con)	/* this check is enough?? */
				continue;
			/* connection established */
		} else {
			if (hsk->hci && hsk->hci != hci)
				continue;
			
			if (hsk->flags & AFFIX_FLAGS_PROMISC)
				goto recv;
			
			if (skb->pkt_type & HCI_PKT_OUTGOING)
				continue;
				
			if (!test_bit(skb->pkt_type, &hsk->pkt_mask))
				continue;
				
			if (skb->pkt_type == HCI_EVENT) {
				__u8	event = *skb->data;
				if (!test_bit64((event - 1) & 0x3F, &hsk->event_mask))
					continue;
					
				if (hsk->flags & AFFIX_FLAGS_W4_STATUS) {
					/* command executing socket */
					if (hpf_cmd_handler(hsk, event, skb) != 0) /* not ower command */
						continue;
				}
			}
		}
recv:
		new_skb = skb_clone(skb, GFP_ATOMIC);
		if (!new_skb)
			break;
		if (skb->pkt_type == HCI_SCO)
			sock_queue_rcv_skb(hsk->base.sk, new_skb);
		else
			affix_sock_rx(hsk->base.sk, new_skb, NULL);
	}
	btl_read_unlock(&hci_socks);
#ifdef CONFIG_AFFIX_L2CAP
	if (con) {
		if (!(skb->pkt_type & HCI_PKT_OUTGOING) && __is_acl(con)) {
			/* ACL L2CAP packet */
			struct sk_buff		*acl;
			hci_acl_hdr_t		*hdr;
		
			acl = skb_get(skb);
			if (acl) {
				hdr = (void*)acl->data;
				skb_pull(acl, HCI_ACL_HDR_LEN);
				lp_receive_data(con, HCI_PB_FLAG(hdr->Connection_Handle) == HCI_PB_FIRST,
						HCI_BC_FLAG(hdr->Connection_Handle), acl);
			}
		}
	}
#endif
	kfree_skb(skb);
	DBFEXIT;
	return 0;
}

int hci_deliver_msg(void *msg, int size)
{
	struct sk_buff	*skb;
	
	skb = alloc_skb(1 + size, gfp_any());
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, 1);
	memcpy(skb_put(skb, size), msg, size);
	skb->pkt_type = HCI_MGR;
	return hpf_recv_raw(NULL, NULL, skb);
}

struct sock *hpf_dup(struct sock *osk)
{
	struct sock	*sk;

	sk = hpf_create(NULL, PF_AFFIX, GFP_ATOMIC, 1);
	if (!sk)
		return NULL;
	sk->sk_pair	= osk;
	sk->sk_protocol = osk->sk_protocol;
	sk->sk_type     = osk->sk_type;
	sk->sk_destruct = osk->sk_destruct;
	sk->sk_priority = osk->sk_priority;
	return sk;
}

int hpf_connect_cfm(hci_con *con, int status)
{
	struct sock	*sk, *pair;
	hci_sock_t	*hsk, *phsk;
	int		type;
	
	DBFENTER;
#ifdef CONFIG_AFFIX_L2CAP
	if (__is_acl(con))
		lp_connect_cfm(con, status);
#endif
	btl_write_lock(&hci_socks);
	btl_for_each (phsk, hci_socks) {
		pair = phsk->base.sk;
		if (!pair->sk_socket)
			continue;
		if (sock_flag(pair, SOCK_DEAD))
			continue;
		if (pair->sk_protocol == BTPROTO_HCIACL)
			type = HCI_LT_ACL;
# if defined(CONFIG_AFFIX_SCO)
		else if (pair->sk_protocol == BTPROTO_HCISCO )
			type = HCI_LT_SCO;
# endif
		else
			continue;
		if (con->link_type != type)
			continue;
		if (pair->sk_state == CON_LISTEN) {
			if (pair->sk_bound_dev_if && pair->sk_bound_dev_if != con->hci->devnum)
				continue;
			if (pair->sk_ack_backlog >= pair->sk_max_ack_backlog || status != 0)
				continue;
			DBPRT("Server socket found: %p\n", pair);
			sk = hpf_dup(pair);
			if (!sk)
				break;
		} else if (pair->sk_state == CON_CONNECTING) {
			if (phsk->con != con)
				continue;
			sk = pair;
			if (status) {
				hpf_reset(sk, status);
				continue;
			}
		} else
			continue;

		hsk = hpf_get(sk);
		// set attributes for hci socket
		sk->sk_bound_dev_if = con->hci->devnum;
		sk->sk_state = CON_ESTABLISHED;
		if (hsk->hci == NULL) {
			hsk->hci = con->hci;
			hci_hold(con->hci);
		}
		if (hsk->con == NULL) {
			hsk->con = con;
			hcc_hold(con);
		}
		if (sk->sk_pair){ /* incomming connection */
			sock_hold(pair);
			pair->sk_ack_backlog++;
			if (affix_sock_ready(sk) != 0) {
				hpf_destroy(sk);
				break;
			}
		} else
			sk->sk_state_change(sk);
	}
	btl_write_unlock(&hci_socks);
	DBFEXIT;
	return 0;
}

int hpf_disconnect_ind(hci_con *con)
{
	hci_sock_t	*hsk;

	DBFENTER;
#ifdef CONFIG_AFFIX_L2CAP	
	lp_disconnect_ind(con);
#endif
	btl_read_lock(&hci_socks);
	btl_for_each (hsk, hci_socks) {
		if (hsk->con == con)
			hpf_reset(hsk->base.sk, 0);
	}
	btl_read_unlock(&hci_socks);
	DBFEXIT;
	return 0;
}


/******************************  HCI PROTOCOL FAMILY SUBSYSTEM  **************************/

int hpf_release(struct socket *sock)
{
	struct sock	*sk = sock->sk;
	hci_sock_t	*hsk;

	DBFENTER;
	if (!sk)
		return 0;
	
	sock_orphan(sk);	/* important */
        sock->sk = NULL;

	hpf_lock_hci(sk, 0);	// possibly unlock
	hsk = hpf_get(sk);
	
	if (___test_and_clear_flag(&hsk->flags, AFFIX_FLAGS_PROMISC))
		hci_promisc_count--;

	if (hsk->flags & AFFIX_FLAGS_SUPER) {
		if (hsk->hci) {
			hci_struct	*hci = hsk->hci;
			hci->open_count--;
			if (!hci->open_count && !test_bit(HCIDEV_STATE_START, &hci->state))
				hcidev_close(hci);
		}
	}
	hpf_destroy(sk);
	DBFEXIT;
	return 0;
}

int hpf_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	int			err = 0;
	struct sockaddr_affix	*sa = (void*)addr;

	DBFENTER;
#ifndef	CONFIG_AFFIX_HCI_BROADCAST
	// DGRAM sockets not allowed to bind?
	if (sock->type == SOCK_DGRAM && !bda_zero(&sa->bda)) {
		sock->state = SS_UNCONNECTED;
		return -ECONNREFUSED;
		
	}
#endif
	sock->sk->sk_bound_dev_if = sa->devnum;
	DBFEXIT;
	return err;
}

/*
    Connect to remote device
    Wait until connection established
*/
int hpf_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags)
{
	struct sock		*sk = sock->sk;
	struct sockaddr_affix	*saddr = (void*)addr;
	int			err = 0;

	DBFENTER;

#ifndef	CONFIG_AFFIX_HCI_BROADCAST
	// Only NULL BDAs allowed for DGRAM sockets
	if (sock->type == SOCK_DGRAM && !bda_zero(&saddr->bda)) {
		sock->state = SS_UNCONNECTED;
		return -ECONNREFUSED;
		
	}
#endif
	
	/* deal with restarts */
	if (sk->sk_state == CON_ESTABLISHED && sock->state == SS_CONNECTING) {
		sock->state = SS_CONNECTED;
		return 0;
	}
	if (sk->sk_state == CON_CLOSED && sock->state == SS_CONNECTING) {
		sock->state = SS_UNCONNECTED;
		return -ECONNREFUSED;
	}
#ifndef CONFIG_AFFIX_HCI_BROADCAST
	if (sk->sk_state == CON_ESTABLISHED)
#else
	if (sock->type == SOCK_SEQPACKET && sk->sk_state == CON_ESTABLISHED)
#endif
		return -EISCONN;      /* No reconnect on a seqpacket socket */
	
	/* Move to connecting socket, start sending Connect Requests */
	sock->state = SS_CONNECTING;
	sk->sk_state   = CON_CONNECTING;

	err = hpf_connect_req(sk, saddr);
	if (err < 0)
		goto exit;

	err = affix_sock_wait_for_state(sk, CON_ESTABLISHED, flags & O_NONBLOCK);
	if (err)
		goto exit;

	if (sk->sk_state == CON_ESTABLISHED) {
		sock->state = SS_CONNECTED;
	}

 exit:
	DBFEXIT;
	return err;
}

static inline int hpf_open_hci(struct sock *sk, struct hci_open *param)
{
	int			err = 0;
	hci_struct		*hci = NULL;
	hci_sock_t		*hsk = hpf_get(sk);

	DBFENTER;

	if (param->cmd & HCI_OPEN_SUPER)
		hsk->flags |= AFFIX_FLAGS_SUPER;

	switch (param->cmd & ~HCI_OPEN_MASK) {
	case HCI_OPEN_NAME:
		btl_read_lock(&hcidevs);
		btl_for_each (hci, hcidevs) {
			if (!hci->deadbeaf && strncmp(hci->name, param->name, IFNAMSIZ) == 0 &&
				(hci_running(hci) || (param->cmd & HCI_OPEN_SUPER))) {
				hci_hold(hci);
				break;
			}
		}
		btl_read_unlock(&hcidevs);
		if (!hci)
			return -ENODEV;
		break;
	case HCI_OPEN_ID:
		btl_read_lock(&hcidevs);
		btl_for_each (hci, hcidevs) {
			if (!hci->deadbeaf && hci->devnum == param->devnum &&
				(hci_running(hci) || (param->cmd & HCI_OPEN_SUPER))) {
				hci_hold(hci);
				break;
			}
		}
		btl_read_unlock(&hcidevs);
		if (!hci)
			return -ENODEV;
		break;
	case HCI_OPEN_EVENT:
		hsk->pkt_mask |= (1 << HCI_EVENT);
		break;
	case HCI_OPEN_MGR:
		hsk->pkt_mask |= (1 << HCI_MGR);
		break;
	}
	if (hci && (hsk->flags & AFFIX_FLAGS_SUPER)) {
		/* open net device */
		err = hcidev_open(hci);
		if (err) {
			hci_put(hci);
			return err;
		}
		hci->open_count++;
	}
	hsk->hci = hci;
	DBFEXIT;
	return err;
}

int hpf_lock_hci(struct sock *sk, int lock)
{
	hci_sock_t	*hsk = hpf_get(sk);
	hci_struct	*hci = hsk->hci;

	DBFENTER;
	if (!hci)
		return -ENODEV;
	if (hsk->flags & AFFIX_FLAGS_LOCK) {
		if (!lock) {
			hsk->flags &= ~AFFIX_FLAGS_LOCK;
			hci->pid = -1;
			up_write(&hci->sema);
		}
	} else if (lock) {
		down_write(&hci->sema);
		hci->pid = current->pid;
		hsk->flags |= AFFIX_FLAGS_LOCK;
	}
	DBFEXIT;
	return 0;
}

static inline int hpf_listen_event(struct sock *sk, __u64 *event_mask)
{
	int		err = 0;
	hci_sock_t	*hsk = hpf_get(sk);

	DBFENTER;
	hsk->event_mask = *event_mask;
	DBFEXIT;
	return err;
}


/* ------------------- device specific stuff ------------------ */

static inline int hpf_set_audio(hci_struct *hci, struct affix_audio *audio)
{
	int	err;
	DBFENTER;
	if (!hci)
		return -ENODEV;
	hci->audio_mode = audio->mode;
	err =  hcidev_ioctl(hci, SIOCHCI_SET_AUDIO, NULL);
	DBFEXIT;
	return (err == -ENOIOCTLCMD) ? 0 : err;
}

static inline int hpf_hci_disc(hci_struct *hci, struct sockaddr_affix *sa)
{
	hci_con		*con;

	DBFENTER;
	con = hcc_lookup_acl(hci, &sa->bda);
	if (!con)
		return -EINVAL;
	lp_disconnect_req(con, 0x13);
	hcc_put(con);
	yield();	// allow affixd to get an event
	DBFEXIT;
	return 0;
}

static inline int hpf_get_conn(hci_struct *hci, struct affix_conn_info *ci)
{
	hci_con		*con;

	DBFENTER;
	if (!hci)
		return -ENODEV;
	con = hcc_lookup_acl(hci, &ci->bda);
	if (!con)
		return -EINVAL;
	ci->dport = con->chandle;
	hcc_put(con);
	DBFEXIT;
	return 0;
}

int hpf_get_devs(int *devs)
{
	hci_struct	*hci;
	int		count = 0;

	DBFENTER;
	btl_read_lock(&hcidevs);
	btl_for_each (hci, hcidevs) {
		if (hci->deadbeaf)
			continue;
		devs[count++] = hci->devnum;
		if (count == HCI_MAX_DEVS)
			break;
	}
	btl_read_unlock(&hcidevs);
	DBFEXIT;
	return count;
}

static inline int hpf_setget_attr(hci_struct *hci, int get, struct hci_dev_attr *da)
{
	DBFENTER;
	if (!hci) {
		da->name[IFNAMSIZ-1] = '\0';
		btl_read_lock(&hcidevs);
		btl_for_each (hci, hcidevs) {
			if (!hci->deadbeaf && 
				((da->devnum && da->devnum == hci->devnum) || 
				 (!da->devnum && strncmp(hci->name, da->name, IFNAMSIZ) == 0))) {
				hci_hold(hci);
				break;
			}
		}
		btl_read_unlock(&hcidevs);
		if (!hci)
			return -ENODEV;
	} else
		hci_hold(hci);
	if (get) {
		snprintf(da->name, IFNAMSIZ, "%s", hci->name);
		da->bda = hci->bda;
		da->devnum = hci->devnum;
		da->flags = hci->flags;
		da->pkt_type = hci->pkt_type;
		da->stats = hci->stats;
	}
	hci_put(hci);
	DBFEXIT;
	return 0;
}

static inline int hpf_setget_ctl(struct sock *sk, int get, int *mode)
{
	int 		err = 0;
	hci_sock_t	*hsk = hpf_get(sk);

	DBFENTER;
	if (get) {
		*mode = affix_ctl_mode;
		return 0;
	}
	if (affix_ctl_mode & *mode)
		return -EBUSY;
	affix_ctl_mode |= *mode;
	hsk->flags |= *mode;
	DBFEXIT;
	return err;

}

static inline int hpf_start_dev(hci_struct *hci)
{
	int	err, fd, timeout;
	__u8	hci_version, lmp_version;
	__u16	hci_revision, manfid, lmp_subversion;

	DBFENTER;

	if (test_bit(HCIDEV_STATE_START, &hci->state))
		return 0;
			
	hci->pkt_type = HCI_PT_DM1 | HCI_PT_DH1;

	fd = _hci_open_id(hci->devnum);
	if (fd < 0)
		return -ENODEV;

	DBPRT("Device opened...\n");
	//set_current_state(TASK_INTERRUPTIBLE);
	//schedule_timeout(2*HZ);
	
	/* try first to wait a bit */
	timeout = sysctl_hci_req_timeout;
	sysctl_hci_req_timeout = 10;
	err = HCI_ReadBDAddr(fd, &hci->bda);
	sysctl_hci_req_timeout = timeout;
	if (err)
		goto initerr;

	err = HCI_ReadBufferSize(fd, &hci->acl_mtu, &hci->sco_mtu, &hci->acl_num, &hci->sco_num);
	if (err)
		goto initerr;

	err = HCI_ReadLocalVersionInformation(fd, &hci_version, &hci_revision, 
						&lmp_version, &manfid, &lmp_subversion);
	if (err)
		goto initerr;

	hci->manfid = manfid;

	err = HCI_ReadLocalSupportedFeatures(fd, &hci->lmp_features);
	if (err)
		goto initerr;

	/* some general settings */
	err = HCI_WriteClassOfDevice(fd, HCI_COD_INFORMATION | HCI_COD_NETWORKING | HCI_COD_COMPUTER | HCI_COD_HANDPC);
	if (err)
		goto initerr;
		
	/* set some parameters */
	down_read(&uts_sem);
	err = HCI_ChangeLocalName(fd, system_utsname.nodename);
	up_read(&uts_sem);
	if (err)
		goto initerr;

	err = HCI_WriteSecurityMode(fd, HCI_SECURITY_SERVICE);
	if (err)
		goto initerr;

	err = HCI_WriteScanEnable(fd, hci->flags>>HCI_FLAGS_SCAN_BITS);	// not visible by default
	if (err)
		goto initerr;
		
	if (hci->lmp_features & HCI_LF_3SLOTS)
		hci->pkt_type |= HCI_PT_DM3 | HCI_PT_DH3;
	if (hci->lmp_features & HCI_LF_5SLOTS)
		hci->pkt_type |= HCI_PT_DM5 | HCI_PT_DH5;
	if (hci->lmp_features & HCI_LF_SCO) {
		hci->pkt_type |= HCI_PT_HV1;
		if (hci->lmp_features & HCI_LF_HV2)
			hci->pkt_type |= HCI_PT_HV2;
		if (hci->lmp_features & HCI_LF_HV3)
			hci->pkt_type |= HCI_PT_HV3;
	}
	if (!(hci->lmp_features & HCI_LF_SWITCH)) {
		hci->flags |= HCI_ROLE_DENY_SWITCH;	// deny switch if no support
		hci->flags &= ~HCI_ROLE_BECOME_MASTER;
	}
#ifdef CONFIG_AFFIX_DTL1_FIX
	if (manfid == HCI_LMP_NOKIA) {
		hci->fixit = 1;
		/*  Removed when UART RI flow control was introduced ??? */
		hci->acl_num = 1;
	}
#endif
	atomic_set(&hci->acl_count, hci->acl_num);
	atomic_set(&hci->sco_count, hci->sco_num);

	hci_close(fd);
	set_bit(HCIDEV_STATE_START, &hci->state);
	hci->flags |= HCI_FLAGS_UP;
	hcidev_state_change(hci, HCIDEV_UP);
	DBFEXIT;
	return 0;
initerr:
	err = (err<0)?-errno:err;
	hci_close(fd);
	BTERROR("Bluetooth device initialization failed: %d\n", err);
	return err;
}

static inline int hpf_stop_dev(hci_struct *hci)
{
	int	err, fd;
	
	DBFENTER;

	if (!test_and_clear_bit(HCIDEV_STATE_START, &hci->state))
		return 0;
			
	hci->flags &= ~HCI_FLAGS_UP;
	hci->flags = 0;	//XXX:

	fd = _hci_open_id(hci->devnum);
	if (fd < 0)
		return -ENODEV;
	err = __HCI_Reset(fd);	// reset
	//set_current_state(TASK_INTERRUPTIBLE);
	//schedule_timeout(HZ/5); //wait for reset is done
	hci_close(fd);
	DBFEXIT;
	return err;
}

static inline int hpf_startstop_dev(hci_struct *hci, int start)
{
	int 	err = 0;
	DBFENTER;
	/* up/down */
	if (!hci)
		return -ENODEV;
	if (!(hcidev_running(hci)))
		return -EINVAL;
	if (start)
		err = hpf_start_dev(hci);
	else
		err = hpf_stop_dev(hci);
	DBFEXIT;
	return err;
}

static inline int hpf_setget_pkttype(hci_struct *hci, int get, int *pkt_type)
{
	DBFENTER;
	if (!hci)
		return -ENODEV;
	if (get) {
		*pkt_type = hci->pkt_type;
		return 0;
	}
	hci->pkt_type = (*pkt_type) ? *pkt_type:(HCI_PT_DM1|HCI_PT_DH1|HCI_PT_DM3|HCI_PT_DH3|HCI_PT_DM5|HCI_PT_DH5|HCI_PT_HV1);
	DBFEXIT;
	return 0;
}

static inline int hpf_set_secmode(hci_struct *hci, int sec_mode)
{
	DBFENTER;
	if (!hci)
		return -ENODEV;
	if (!sec_mode)
		return -EINVAL;
	hci->flags &= ~HCI_FLAGS_SECURITY;
	hci->flags |= sec_mode;
	DBFEXIT;
	return 0;
}

static inline int hpf_set_role(hci_struct *hci, int role)
{
	DBFENTER;
	if (!hci)
		return -ENODEV;
	role &= HCI_FLAGS_ROLE;
	hci->flags = (hci->flags & ~HCI_FLAGS_ROLE) | role;
	if (!(hci->lmp_features & HCI_LF_SWITCH)) {
		hci->flags |= HCI_ROLE_DENY_SWITCH;	// deny switch if no support
		hci->flags &= ~HCI_ROLE_BECOME_MASTER;
	}
	DBFEXIT;
	return 0;
}

static inline int hpf_set_scan(hci_struct *hci, int scan_mode)
{
	DBFENTER;
	if (!hci)
		return -ENODEV;
	hci->flags &= ~HCI_FLAGS_SCAN;
	hci->flags |= (scan_mode << HCI_FLAGS_SCAN_BITS);
	DBFEXIT;
	return 0;
}

static inline int hpf_setup_uart(hci_struct *hci, struct open_uart *uart)
{
	int	err;

	DBFENTER;
	btl_read_lock(&hcidevs);
	btl_for_each (hci, hcidevs) {
		if (hcidev_present(hci) && 
				strncmp(hci->name, uart->dev, IFNAMSIZ) == 0) {
			hci_hold(hci);
			break;
		}
	}
	btl_read_unlock(&hcidevs);
	if (!hci)
		return -ENODEV;
	err = hcidev_ioctl(hci, BTIOC_SETUP_UART, uart);
	hci_put(hci);
	DBFEXIT;
	return err;
}

static struct affix_pan_operations	affix_pan_ops;
static struct affix_pan_operations      affix_hidp_ops;

int affix_set_hidp(struct affix_pan_operations *ops)
{
	DBFENTER;
	
	if (ops == NULL)
		affix_hidp_ops.owner = NULL;
	else
		affix_hidp_ops = *ops;
	
	DBFEXIT;
	return 0;
}

int affix_set_pan(struct affix_pan_operations *ops)
{
	DBFENTER;
	
	if (ops == NULL)
		affix_pan_ops.owner = NULL;
	else
		affix_pan_ops = *ops;
	
	DBFEXIT;
	return 0;
}


int hpf_init_pan(struct pan_init *arg)
{
	int	err = -ENODEV;

	DBFENTER;
#if defined(CONFIG_AFFIX_PAN_MODULE)
	if (!affix_pan_ops.owner  || !try_module_get(affix_pan_ops.owner)) {
		request_module("affix_pan");
		if (!affix_pan_ops.owner  || !try_module_get(affix_pan_ops.owner))
			return -ENODEV;
	}
#elif !defined(CONFIG_AFFIX_PAN)
		return -EPROTONOSUPPORT;
#endif
	if (affix_pan_ops.ioctl)
		err = affix_pan_ops.ioctl(BTIOC_PAN_INIT, (unsigned long)arg);
	module_put(affix_pan_ops.owner);
	DBFEXIT;
	return err;
}


/*
    our system calls for user space programs
*/
int hpf_ioctl(struct socket *sock, unsigned int cmd,  unsigned long arg)
{
	struct sock	*sk = sock->sk;
	hci_sock_t	*hsk = hpf_get(sk);
	int		err = 0;

	DBFENTER;

	if (_IOC_TYPE(cmd) == BTIOC_MAGIC) {
		switch(cmd) {
			case BTIOC_OPEN_HCI:
				err = hpf_open_hci(sk, (struct hci_open*)arg);
				break;
			case BTIOC_LOCK_HCI:
				err = hpf_lock_hci(sk, *(int*)arg);
				break;
			case BTIOC_START_DEV:
				err = hpf_startstop_dev(hsk->hci, *(int*)arg);
				break;
			case BTIOC_DBMGET:
				*(__u32*)arg = affix_dbmask;
				break;
			case BTIOC_DBMSET:
				affix_dbmask = *(__u32*)arg;
				break;
			case BTIOC_ADDPINCODE:
				err = affix_add_pin((struct PIN_Code *)arg);
				break;
			case BTIOC_REMOVEPINCODE:
				err = affix_remove_pin((BD_ADDR*)arg);
				break;
			case BTIOC_ADDLINKKEY:
				err = affix_add_key((struct link_key*)arg);
				break;
			case BTIOC_REMOVELINKKEY:
				err = affix_remove_key((BD_ADDR*)arg);
				break;
			case BTIOC_SETUP_UART:
				err = hpf_setup_uart(hsk->hci, (struct open_uart*)arg);
				break;
			case BTIOC_OPEN_UART:
				err = affix_open_uart((struct open_uart*)arg);
				break;
			case BTIOC_CLOSE_UART:
				err = affix_close_uart((struct open_uart*)arg);
				break;
			case BTIOC_GETDEVS:
				err = hpf_get_devs((int*)arg);
				break;
			case BTIOC_SET_CTL:
				err = hpf_setget_ctl(sk, 0, (int*)arg);
				break;
			case BTIOC_GET_CTL:
				err = hpf_setget_ctl(sk, 1, (int*)arg);
				break;
			case BTIOC_SET_AUDIO:
				err = hpf_set_audio(hsk->hci, (struct affix_audio*)arg);
				break;
			case BTIOC_HCI_DISC:
				err = hpf_hci_disc(hsk->hci, (struct sockaddr_affix*)arg);
				break;
			case BTIOC_GET_CONN:
				err = hpf_get_conn(hsk->hci, (struct affix_conn_info*)arg);
				break;
			case BTIOC_GET_ATTR:
				err = hpf_setget_attr(hsk->hci, 1, (struct hci_dev_attr*)arg);
				break;
			case BTIOC_SET_ATTR:
				err = hpf_setget_attr(hsk->hci, 0, (struct hci_dev_attr*)arg);
				break;
			case BTIOC_SET_PKTTYPE:
				err = hpf_setget_pkttype(hsk->hci, 0, (int*)arg);
				break;
			case BTIOC_GET_PKTTYPE:
				err = hpf_setget_pkttype(hsk->hci, 1, (int*)arg);
				break;
			case BTIOC_SET_SECMODE:
				err = hpf_set_secmode(hsk->hci, *(int*)arg);
				break;
			case BTIOC_SET_ROLE:
				err = hpf_set_role(hsk->hci, *(int*)arg);
				break;
			case BTIOC_SET_SCAN:
				err = hpf_set_scan(hsk->hci, *(int*)arg);
				break;
				/*
				 * PAN stuff
				 */
			case BTIOC_PAN_INIT:
				err = hpf_init_pan((struct pan_init*)arg);
				break;
			case BTIOC_PAN_CONNECT:
				if (affix_pan_ops.ioctl)
					err = affix_pan_ops.ioctl(cmd, (unsigned long)arg);
				else
					err = -ENODEV;
				break;
				/*
				 * HIDP
				 */
		        case BTIOC_HIDP_MODIFY:
		        case BTIOC_HIDP_DELETE:
		        case BTIOC_HIDP_GET_LIST:
			        if (affix_hidp_ops.ioctl)
				        err = affix_hidp_ops.ioctl(cmd, (unsigned long)arg);
			        else
				        err = -ENODEV;
			        break;
			default:
				return -ENOIOCTLCMD;
		}
	} else {
		/* deliver it to device driver */
		if (hsk->hci)
			err = hcidev_ioctl(hsk->hci, cmd, (void*)arg);
	}
	DBFEXIT;
	return err;
}


int hpf_getname(struct socket *sock, struct sockaddr *addr, int *alen, int peer)
{
	struct sock		*sk = sock->sk;
	struct sockaddr_affix	*sa = (struct sockaddr_affix*)addr;
	hci_sock_t		*hsk = hpf_get(sk);

	DBFENTER;
	sa->family = AF_AFFIX;
	sa->devnum = sk->sk_bound_dev_if;
	if (hsk->con) {
		sa->bda = hsk->con->bda;
		sa->port = hsk->con->chandle;
	}
	*alen = sizeof(struct sockaddr_affix);
	DBFEXIT;
	return 0;

}

int hpf_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
	struct sock	*sk = sock->sk;
	hci_sock_t	*hsk = hpf_get(sk);
	int		value;

	DBFENTER;
	//lock_sock(sk);
	if (level != SOL_AFFIX)
		return -EOPNOTSUPP;
	switch (optname) {
		case BTSO_EVENT_MASK:
			if (copy_from_user(&hsk->event_mask, optval, sizeof(hsk->event_mask)))
				return -EFAULT;
			break;
		case BTSO_PKT_MASK:
			if (copy_from_user(&hsk->pkt_mask, optval, sizeof(hsk->pkt_mask)))
				return -EFAULT;
			break;
		case BTSO_PROMISC:
			if (copy_from_user(&value, optval, sizeof(value)))
				return -EFAULT;
			if (value) {
				if (!___test_and_set_flag(&hsk->flags, AFFIX_FLAGS_PROMISC))
					hci_promisc_count++;
			} else {
				if (___test_and_clear_flag(&hsk->flags, AFFIX_FLAGS_PROMISC))
					hci_promisc_count--;
			}
			break;
		default:
			return -ENOPROTOOPT;
	}
	//release_sock(sk);
	DBFEXIT;
	return 0;
}

int hpf_cmd_handler(hci_sock_t *hsk, __u8 event, struct sk_buff *skb)
{
	struct Command_Complete_Event	*cce;
	struct Command_Status_Event	*cse;
	struct sock			*sk = hsk->base.sk;

	DBFENTER;

	if (sk->sk_err) {
		hsk->event_mask = 0;
		hpf_unlock(hsk);
		return -1;
	}
	cce = (void*)skb->data;
	cse = (void*)skb->data;

	if (event == HCI_E_COMMAND_COMPLETE) {
		if (cce->Command_Opcode != hsk->opcode)
			return -1;
		hsk->event_mask &= ~COMMAND_COMPLETE_MASK;/* we need only one */
		hpf_unlock(hsk);
	} else if (event == HCI_E_COMMAND_STATUS) {
		if (cse->Command_Opcode != hsk->opcode)
			return -1;
		hsk->event_mask &= ~COMMAND_STATUS_MASK;/* we need only one */
		if (cse->Status != 0)
			hsk->event_mask = 0;
		hpf_unlock(hsk);
	}
	DBFEXIT;
	return 0;// accept event
}

int hpf_wait_for_completion(struct sock *sk)
{
	int		err = 0;
	struct sk_buff	*skb;
	hci_sock_t	*hsk = hpf_get(sk);
	long		timeo = sk->sk_rcvtimeo;	//old timeout

	DBFENTER;
	sk->sk_rcvtimeo = sysctl_hci_req_timeout * HZ;	// default 5 sec.
	while ((hsk->flags & AFFIX_FLAGS_W4_STATUS) && err == 0) {
		skb = skb_recv_datagram(sk, 0, 0, &err);
		if (!skb) {
			if (err != -ERESTARTSYS) //sock_intr_errno(timeo)
				hsk->flags &= ~AFFIX_FLAGS_CMD_PENDING;
			if (err == -EAGAIN)
				err = -EIO;
			if (err)
				break;
		} else {
			struct Command_Status_Event	*cse = (void*)skb->data;
			
			if (cse->hdr.EventCode == HCI_E_COMMAND_COMPLETE) {
				hsk->flags &= ~AFFIX_FLAGS_W4_STATUS;
				/* results of the command */
				skb_queue_head(&sk->sk_receive_queue, skb);
				continue;
			} else if (cse->hdr.EventCode == HCI_E_COMMAND_STATUS) {
				hsk->flags &= ~AFFIX_FLAGS_W4_STATUS;
				err = cse->Status;
			}
			kfree_skb(skb);
		}
	}
	sk->sk_rcvtimeo = timeo;
	DBFEXIT;
	return err;
}

int hpf_send_request(struct sock *sk, struct sk_buff *skb, int flags)
{
	hci_sock_t	*hsk = hpf_get(sk);
	int		err;

	if (hsk->flags & AFFIX_FLAGS_CMD_PENDING)
		goto wait;
	/*
	   Lock HCI before registering to prevent receiving events
	   from the same command
	*/
	if (hsk->hci->pid != current->pid)
		down_read(&hsk->hci->sema);
	hsk->lock = hci_lock_cmd(hsk->hci, hsk->opcode);
	if (!hsk->lock) {
		kfree_skb(skb);
		//if (hsk->hci->pid != current->pid)
			up_read(&hsk->hci->sema);// it will never fail on exclusive lock
		return -ENOMEM;
	}
	/* set some fields */
	if (!(flags & HCI_SKIP_STATUS))
		hsk->flags |= AFFIX_FLAGS_W4_STATUS;
	else
		hsk->flags &= ~AFFIX_FLAGS_W4_STATUS;

	if (hsk->flags & AFFIX_FLAGS_SUPER)
		atomic_inc(&hsk->hci->cmd_count);	// allow certainly to send a command

	skb_queue_purge(&sk->sk_receive_queue);	// Purge unexpected events
	hsk->opcode = __get_u16(skb->data);	/* BT byte order */
	hsk->pkt_mask = 1 << HCI_EVENT;
	/* send a command */
	skb->pkt_type = HCI_COMMAND;
	if (!(flags & HCI_NO_UART_ENCAP))
		skb_pull(skb, 1);
	err = hci_queue_xmit(hsk->hci, NULL, skb);
	if (err) {
		kfree_skb(skb);
		goto exit;
	}
	hsk->flags |= AFFIX_FLAGS_CMD_PENDING;
wait:
	err = hpf_wait_for_completion(sk);
	if (err < 0)
		goto exit;
	// err has COMMAND_STATUS in this case
exit:
	hsk->flags &= ~AFFIX_FLAGS_CMD_PENDING;
	hpf_unlock(hsk);
	if (hsk->hci->pid != current->pid)
		up_read(&hsk->hci->sema);
	hsk->opcode = 0;	/* zero opcode */
	return err;
}

/*
    size - size of the iovec array, size of the message
*/
int hpf_sendmsg_raw(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock	*sk = sock->sk;
	hci_sock_t	*hsk = hpf_get(sk);
	int		err;
	struct sk_buff	*skb = NULL;
	hci_struct	*hci = hsk->hci;

	DBFENTER;
	/* deal with restarts */
	if (hsk->flags & AFFIX_FLAGS_CMD_PENDING)
		goto wait;
	if (!hsk->hci)
		return -ENODEV;
	
	skb = sock_alloc_send_skb(sk, hsk->hci->hdrlen + size, msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
		return err;
	skb_reserve(skb, hsk->hci->hdrlen);

	err = memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);
	if (err)
		goto out;
		
	if (msg->msg_flags & HCI_REQUEST_MODE) {
wait:
		err = hpf_send_request(sk, skb, msg->msg_flags);
	} else {
		skb->pkt_type = *skb->data;	/*  get packet type */
		skb_pull(skb, 1);
		err = hci_queue_xmit(hci, NULL, skb);
		if (err)
			goto out;
		err = size;
	}
	DBFEXIT;
	return err;
out:
	kfree_skb(skb);
	DBPRT("err: %d\n", err);
	return err;
}

int hpf_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock		*sk = sock->sk;
	hci_sock_t		*hsk;
	int			err = -ENOTCONN;
	struct sk_buff		*skb;

	DBFENTER;

	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))
		return -EINVAL;
	if (sock->state != SS_CONNECTED) 
		return -ENOTCONN;
	if (sk->sk_err)
		return sock_error(sk);
	if (sk->sk_shutdown & SHUTDOWN_MASK) {
		if (!(msg->msg_flags & MSG_NOSIGNAL))
			send_sig(SIGPIPE, current, 0);
		return -EPIPE;
	}
	if (sk->sk_state != CON_ESTABLISHED)
		return -ENOTCONN;

	hsk = hpf_get(sk);
	skb = sock_alloc_send_skb(sk, size + HCI_SKB_RESERVE, msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
		return err;
	skb_reserve(skb, HCI_SKB_RESERVE);
	
	err = memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);
	if (err)
		goto err;
	/* send skb */
#ifdef CONFIG_AFFIX_HCI_BROADCAST
	if (sock->type == SOCK_DGRAM)
		err = lp_broadcast_data(hsk->con, skb);
	else
#endif
		err = lp_send_data(hsk->con, skb);
	if (err)
		goto err;
	
 	DBFEXIT;
	return size;
 err:
 	kfree_skb(skb);
	return err;
}

int hci_put_cmsg(struct msghdr *msg, int level, int type, int len, void *data)
{
	struct cmsghdr	*cm = (struct cmsghdr*)msg->msg_control;
	struct cmsghdr	cmhdr;
	int 		cmlen = CMSG_SPACE(len);
	int 		err;

	if (cm == NULL || msg->msg_controllen < cmlen) {
		msg->msg_flags |= MSG_CTRUNC;
		return 0;
	}
	cmhdr.cmsg_level = level;
	cmhdr.cmsg_type = type;
	cmhdr.cmsg_len = CMSG_LEN(len);
	err = -EFAULT;
	if (copy_to_user(cm, &cmhdr, sizeof cmhdr))
		goto out; 
	if (copy_to_user(CMSG_DATA(cm), data, len))
		goto out;
	msg->msg_control += cmlen;
	msg->msg_controllen -= cmlen;
	err = 0;
out:
	return err;
}

int hpf_recvmsg_raw(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
{
	struct sock		*sk = sock->sk;
	int			err = -ENOTCONN;
	struct sk_buff		*skb;

	DBFENTER;
	skb = skb_recv_datagram(sk, (flags & ~MSG_DONTWAIT) | MSG_PEEK, flags & MSG_DONTWAIT, &err);
	if (!skb)
		goto exit;
	DBPRT("We received %d bytes\n", skb->len);
	DBDUMP(skb->data, skb->len);
	
	if (!(flags & HCI_NO_UART_ENCAP))
		*skb_push(skb, 1) = skb->pkt_type;	// add pkt_type
	
	if (msg->msg_name) {
		hci_con		*con;
		/* set address info */
		struct sockaddr_affix	*sa = msg->msg_name;
		msg->msg_namelen = sizeof(*sa);
		sa->family = AF_AFFIX;
		sa->devnum = hci_skb(skb)->devnum;
		if (sk->sk_type != SOCK_RAW) {
			con = hpf_get(sk)->con;
			if (con) {
				sa->bda = con->bda;
				sa->port = con->chandle;
			}
		} else if (hpf_get(sk)->flags & AFFIX_FLAGS_PROMISC) {
			skb->pkt_type &= ~HCI_PKT_OUTGOING;
			memset(&sa->bda, 0, 6);
			if (skb->pkt_type == HCI_ACL || skb->pkt_type == HCI_SCO) {
				con = hcc_lookup_id(hci_skb(skb)->conid);
				if (con) {
					sa->bda = con->bda;
					hcc_put(con);
				}
			}
		}
	}

	skb->h.raw = skb->data;
	if (skb->len <= size)
		size = skb->len;
	else if (sock->type == SOCK_SEQPACKET) {
		msg->msg_flags |= MSG_TRUNC;
		if (!(flags & MSG_PEEK))
			skb_trim(skb, size);
	}
	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, size);
	if (!(flags & MSG_PEEK))
		skb_pull(skb, size);
	if (!skb->len) {
		skb_unlink(skb);
		kfree_skb(skb);
	}
	kfree_skb(skb);		// remove MSG_PEEK link
	err = size;
 exit:
	DBFEXIT;
	return err;
}

int hpf_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
{
	int	err = -ENOTCONN;

	DBFENTER;
	if (sock->state != SS_CONNECTED) 
		return -ENOTCONN;
	err = hpf_recvmsg_raw(iocb, sock, msg, size, flags);
	DBFEXIT;
	return err;
}

/*
    HCI Protocol Family operations
*/

struct proto_ops hpf_raw_ops = {
	owner:		THIS_MODULE,
	family:		PF_AFFIX,

	release:	hpf_release,
	bind:		sock_no_bind,//hpf_bind,
	connect:	sock_no_connect,//hpf_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept,//hpf_accept,
	getname:	hpf_getname,
	poll:		datagram_poll,
	ioctl:		hpf_ioctl,
	listen:		sock_no_listen,//hpf_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	hpf_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	hpf_sendmsg_raw,
	recvmsg:	hpf_recvmsg_raw,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage
};

/*
 * ops for ACL/SCO sockets
 */
struct proto_ops hpf_data_ops = {
	owner:		THIS_MODULE,
	family:		PF_AFFIX,

	release:	hpf_release,
	bind:		hpf_bind,
	connect:	hpf_connect,
	socketpair:	sock_no_socketpair,
	accept:		affix_sock_accept,
	getname:	hpf_getname,
	poll:		datagram_poll,
	ioctl:		hpf_ioctl,
	listen:		affix_sock_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	hpf_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	hpf_sendmsg,
	recvmsg:	hpf_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage
};


int __hpf_create(struct socket *sock, int protocol)
{
	struct sock	*sk;

	DBFENTER;
	if ((sock->type == SOCK_RAW && protocol != BTPROTO_HCI)
#ifdef CONFIG_AFFIX_HCI_BROADCAST
		|| (sock->type == SOCK_DGRAM && protocol != BTPROTO_HCIACL)
#endif
			)
		return -ESOCKTNOSUPPORT;

	if (sock->type != SOCK_RAW && sock->type != SOCK_SEQPACKET) {
#ifdef CONFIG_AFFIX_HCI_BROADCAST
		if (sock->type != SOCK_DGRAM)
#endif
			return -ESOCKTNOSUPPORT;
	}
	sock->state = SS_UNCONNECTED;
	sk = hpf_create(sock, PF_AFFIX, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;
	sk->sk_destruct = hpf_destruct;
	sk->sk_protocol = protocol;
	if (sock->type == SOCK_RAW)
		sock->ops = &hpf_raw_ops;
	else
		sock->ops = &hpf_data_ops;
	btl_add_tail(&hci_socks, hpf_get(sk));
	DBFEXIT;
	return 0;
}

struct net_proto_family hpf_family_ops = {
	owner:		THIS_MODULE,
	family:	PF_AFFIX,
	create:	__hpf_create
};

static int affix_event(struct notifier_block *nb, unsigned long event, void *arg)
{
#if 0
	switch (event) {
		case HCIDEV_DOWN:
			break;
		default:
			break;
	}
#endif
	return NOTIFY_DONE;
}

static struct notifier_block affix_notifier_block = {
	notifier_call:	affix_event,
};

#ifdef CONFIG_PROC_FS

int hpf_proc_read(char *buf, char **start, off_t offset, int len)
{
	int count = 0;

	DBFENTER;
	count += sprintf(buf+count, "HCI sockets in use: %ld, list len: %d\n",
			sock_count, hci_socks.len);
	DBFEXIT;
	return count;
}

struct proc_dir_entry	*hpf_proc;

#endif

int __init init_hpf(void)
{
	int	err;
	
	DBFENTER;
#ifdef CONFIG_PROC_FS
	hpf_proc = create_proc_info_entry("hci_sock", 0, proc_affix, hpf_proc_read);
	if (!hpf_proc) {
		BTERROR("Unable to register proc fs entry\n");
		return -EINVAL;
	}
#endif
	err = affix_sock_register(&hpf_family_ops, BTPROTO_HCI);
	if (err)
		goto err1;
	err = affix_sock_register(&hpf_family_ops, BTPROTO_HCIACL);
	if (err)
		goto err2;
#if defined(CONFIG_AFFIX_SCO)
	err = affix_sock_register(&hpf_family_ops, BTPROTO_HCISCO);
	if (err)
		goto err3;
#endif
	affix_register_notifier(&affix_notifier_block);
	btl_head_init(&hci_socks);
	DBFEXIT;
	return 0;
#if defined(CONFIG_AFFIX_SCO)
err3:
	affix_sock_unregister(BTPROTO_HCIACL);
#endif
err2:
	affix_sock_unregister(BTPROTO_HCI);
err1:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("hci_sock", proc_affix);
#endif
	return err;
}

void __exit exit_hpf(void)
{
	DBFENTER;
	affix_unregister_notifier(&affix_notifier_block);
#if defined(CONFIG_AFFIX_SCO)
	affix_sock_unregister(BTPROTO_HCISCO);
#endif
	affix_sock_unregister(BTPROTO_HCIACL);
	affix_sock_unregister(BTPROTO_HCI);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("hci_sock", proc_affix);
#endif
	DBFEXIT;
}

/* Exporting stuff */
EXPORT_SYMBOL(hci_open_dev);
#if defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MODULE)
EXPORT_SYMBOL(HCI_SwitchRole);
EXPORT_SYMBOL(HCI_WriteScanEnable);
EXPORT_SYMBOL(HCI_ReadScanEnable);
EXPORT_SYMBOL(HCI_WritePageScanActivity);
EXPORT_SYMBOL(HCI_WriteInquiryScanActivity);
EXPORT_SYMBOL(HCI_WriteClassOfDevice);
EXPORT_SYMBOL(HCI_WriteLinkSupervisionTimeout);
EXPORT_SYMBOL(HCI_ReadBDAddr);
EXPORT_SYMBOL(HCI_ReadRSSI);
EXPORT_SYMBOL(HCI_GetLinkQuality);
EXPORT_SYMBOL(HCI_ReadTransmitPowerLevel);
EXPORT_SYMBOL(HCI_Inquiry);
EXPORT_SYMBOL(HCI_ChangeConnectionPacketType);
#endif

/*
 * 1. set mode mask for connecting sockets
 * 2. disconnect based on mode mask
 */

