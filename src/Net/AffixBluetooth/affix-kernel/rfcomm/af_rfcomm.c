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
   $Id: af_rfcomm.c,v 1.139 2004/03/16 11:58:30 kassatki Exp $

   AF_RFCOMM - RFCOMM Address family for socket interface

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
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <net/sock.h>

#define FILEBIT	DBAFRFCOMM

/* Local Includes */
#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>

#include <affix/rfcomm.h>
#include "bty.h"

int sysctl_rfcomm_wmem = 65535;
int sysctl_rfcomm_rmem = 65535;

btlist_head_t			rfcomm_socks;
extern rfcomm_proto_ops 	rpf_proto;

void rpf_tx_task(void *data);
int rpf_xmit_wakeup(struct sock *sk);

/*
 * debuging stuff
 */
static long sock_count = 0;

/*
  Memory management
*/

long skb_count = 0;

void __rpf_destroy(struct sock *sk);
void rpf_destroy(struct sock *sk);

/* 
 * Write buffer destructor automatically called from kfree_skb. 
 */
void rpf_write_space(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);
	
	DBFENTER;
	read_lock(&sk->sk_callback_lock);
	if (sock_writeable(sk) && !sock_flag(sk, SOCK_DEAD) && rsk->con)
		bty_control_ind(sk, RFCOMM_WRITE_SPACE);
	read_unlock(&sk->sk_callback_lock);
	DBFEXIT;
}

void rpf_data_ready(struct sock *sk, int len)
{
	DBFENTER;
	read_lock(&sk->sk_callback_lock);
	bty_data_ind(sk);
	read_unlock(&sk->sk_callback_lock);
	DBFEXIT;
}

void rpf_state_change(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);
	
	DBFENTER;
	read_lock(&sk->sk_callback_lock);
	if (sk->sk_shutdown == SHUTDOWN_MASK) {
		if (!(rsk->port.flags & RFCOMM_SOCK_BOUND)) {
			// unregister if not bound
			bty_unregister_sock(rsk->port.line);
			if (sk->sk_socket)
				sock_put(sk);
			else
				rpf_destroy(sk);	/* not owned by "socket" - BTY case */
		} else {
			bty_control_ind(sk, RFCOMM_SHUTDOWN);
			/*
			 * client will close BTY and call rpf_disconnect_bty() -> rpf_reset();
			 */
		}
	} else {
		bty_control_ind(sk, RFCOMM_ESTABLISHED);
	}
	read_unlock(&sk->sk_callback_lock);
	DBFEXIT;
}

/*
 * Allocate a skb from the socket's send buffer.
 * It's not waiting !!!
 */
struct sk_buff *rpf_wmalloc(struct sock *sk, unsigned long size, int force, int priority)
{
	struct sk_buff	*skb;

	DBFENTER;
	if (!size)
		return NULL;
	skb = sock_wmalloc(sk, RFCOMM_SKB_RESERVE + size + RFCOMM_FCS_SIZE, force, priority);
	if (!skb)
		return NULL;
	skb_reserve(skb, RFCOMM_SKB_RESERVE);
	rfcomm_cb(skb)->size = size;
	DBFEXIT;
	return skb;
}

/***************************    Connection Management   ********************************/

struct sock *rpf_lookup_sock(hci_struct *hci, int sch)
{
	rfcomm_sock_t	*rsk, *zero = NULL;
	struct sock	*sk = NULL;
	
	btl_read_lock(&rfcomm_socks);
	btl_for_each (rsk, rfcomm_socks) {
		if (rsk->base.sk->sk_state == CON_LISTEN && rsk->base.sport == sch) {
			if (!zero && rsk->base.sk->sk_bound_dev_if == 0)
				zero = rsk;
			if (rsk->base.sk->sk_bound_dev_if == hci->devnum)
				break;
		}
	}
	if (!rsk)
		rsk = zero;
	if (rsk) {
		sock_hold(rsk->base.sk);
		sk = rsk->base.sk;
	}
	btl_read_unlock(&rfcomm_socks);
	return sk;
}

void rpf_destruct(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);

	DBFENTER;
	kfree(rsk);
	sock_count--;	/* XXX */
	DBFEXIT;
}

int rpf_close(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);
	struct sk_buff	*skb;
	
	DBFENTER;
        sk->sk_err = 0;
	sk->sk_state = CON_CLOSED;
	sk->sk_shutdown = SHUTDOWN_MASK;
	skb_queue_purge(&sk->sk_write_queue);
	if (rsk->con) {
		rfcon_orphan(rsk->con);
		rfcon_put(xchg(&rsk->con, NULL));
	}
	if (sk->sk_pair) {
		sk->sk_pair->sk_ack_backlog--;
		sock_put(xchg(&sk->sk_pair, NULL));
	}
	while((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		if (skb->sk != sk) {
			/* A pending connection */
			// BTY never owns such sockets
			__rpf_destroy(skb->sk);
		}	
		kfree_skb(skb);
	}
	DBFEXIT;
	return 0;
}

void __rpf_destroy(struct sock *sk)
{
	DBFENTER;
	sock_set_flag(sk, SOCK_DEAD);
	__btl_unlink(&rfcomm_socks, rpf_get(sk));
	rpf_close(sk);
	sock_put(sk);
	DBFEXIT;
}

void rpf_destroy(struct sock *sk)
{
	btl_write_lock(&rfcomm_socks);
	__rpf_destroy(sk);
	btl_write_unlock(&rfcomm_socks);

}
/*
 *
 * sk_cachep = kmem_cache_create("sock", sizeof(struct sock), 0, SLAB_HWCACHE_ALIGN, 0, 0);
 *
 */
struct sock *rpf_create(struct socket *sock, int family, int priority, int zero_it)
{
	struct sock	*sk;
	rfcomm_sock_t	*rsk;

	DBFENTER;
	sk  = sk_alloc(family, priority, zero_it, NULL);
	if (!sk)
		return NULL;
	sock_init_data(sock, sk);
	
	sk->sk_rcvbuf = sysctl_rfcomm_rmem;
	sk->sk_sndbuf = sysctl_rfcomm_wmem;

	rsk = (rfcomm_sock_t*)kmalloc(sizeof(rfcomm_sock_t), priority);
	if (!rsk) {
		sk_free(sk);
		return NULL;
	}
	memset(rsk, 0, sizeof(rfcomm_sock_t));
	
	sk->sk_state = CON_CLOSED;
	sk->sk_prot = (void*)rsk;
	rsk->base.sk = sk;
	rsk->port.line = -1;
	tasklet_init(&rsk->tx_task, (void*)rpf_tx_task, (unsigned long) sk);
	/* set attributes */
	rsk->security = HCI_SECURITY_AUTH;	/* default is on */

	btl_add_tail(&rfcomm_socks, rsk);
	sock_count++;	/* XXX */
	sk_set_owner(sk, THIS_MODULE);
	DBFEXIT;
	return sk;
}

int rpf_connect_req(struct sock *sk, struct sockaddr_affix *addr)
{
	rfcomm_sock_t 	*rsk = rpf_get(sk);
	rfcomm_con	*con;
	int		err;
	hci_struct	*hci;

	DBFENTER;
	if (addr->port < 1 || addr->port > 30)
		return -EINVAL;

	if (addr->devnum) {
		hci = hci_lookup_devnum(addr->devnum);	/* find device */
		if (hci == NULL)
			return -ENODEV;
	} else
		hci = NULL;

	// set address
	rsk->base.dport = addr->port;
	con = rfcon_create(&addr->bda, RFCOMM_DLCI(addr->port), &rpf_proto);
	if (con == NULL) {
		if (hci)
			hci_put(hci);
		err = -ENOMEM;
		goto exit;
	}
	rsk->con = con;
	con->hci = hci;
	rfcon_graft(con, sk);
	con->security = rsk->security;

	/* set connection parameters */
	err = rfcon_connect_req(con);
  exit:
	DBFEXIT;
	return err;
}


static inline void rpf_reset(struct sock *sk, int status)
{
	// rpf_destroy() is used only when connection has sk_pair
	affix_sock_reset(sk, status, rpf_destroy);
//	affix_sock_reset(sk, status, NULL);
}

/* 
   RRF protocol subsystem.  Client of RFCOMM. Callback functions
*/
void rpf_rfree(struct sk_buff *skb)
{
	struct sock	*sk = skb->sk;
	rfcomm_sock_t	*rsk = rpf_get(sk);

	atomic_sub(skb->truesize, &sk->sk_rmem_alloc);
	if (rsk->con)
		rfcon_set_rxspace(rsk->con, sk->sk_rcvbuf - atomic_read(&sk->sk_rmem_alloc));
}

int rpf_data_ind(rfcomm_con *con, struct sk_buff *skb)
{
	struct sock	*sk = (struct sock*)con->priv;

	DBFENTER;
	skb_get(skb);
	affix_sock_rx(sk, skb, rpf_rfree);
	DBFEXIT;
	return 0;
}

struct sock *rpf_dup(struct sock *osk)
{
	struct sock	*sk;

	sk = rpf_create(NULL, PF_AFFIX, GFP_ATOMIC, 1);
	if (!sk)
		return NULL;
	sk->sk_pair	= osk;
	sk->sk_protocol = osk->sk_protocol;
	sk->sk_type     = osk->sk_type;
	sk->sk_destruct = osk->sk_destruct;
	sk->sk_priority = osk->sk_priority;

	rpf_get(sk)->port.flags = rpf_get(osk)->port.flags;		/* duplicate flags */
	rpf_get(sk)->security = rpf_get(osk)->security;		/* duplicate sec_level */

	return sk;
}

int rpf_connect_ind(rfcomm_con *con)
{
	struct sock	*sk, *pair;
	rfcomm_sock_t	*rsk;

	DBFENTER;
	pair = rpf_lookup_sock(con->hci, RFCOMM_SCH(con->dlci));
	if (!pair) {
		rfcon_connect_rsp(con, L2CAP_CONRSP_RESOURCE);
		return -1;
	}
	if (pair->sk_ack_backlog >= pair->sk_max_ack_backlog) {
		rfcon_connect_rsp(con, L2CAP_CONRSP_RESOURCE);
		sock_put(pair);
		return -1;
	}
	if (sock_flag(pair, SOCK_DEAD)) {
		rfcon_connect_rsp(con, L2CAP_CONRSP_RESOURCE);
		sock_put(pair);
		return -1;
	}
	sk = rpf_dup(pair);
	if (!sk) {
		rfcon_connect_rsp(con, L2CAP_CONRSP_RESOURCE);
		sock_put(pair);
		return -1;
	}
	rsk = rpf_get(sk);
	pair->sk_ack_backlog++;
	rfcon_hold(con);
	rsk->base.dport = RFCOMM_SCH(con->dlci);
	rfcon_graft(con, sk);
	rsk->con = con;
	con->security = rsk->security;

	/* set connection parameters */
	rfcon_connect_rsp(con, L2CAP_CONRSP_SUCCESS);	/* Response is here */
	DBFEXIT;
	return 0;
}

int rpf_connect_cfm(rfcomm_con *con, int status)
{
	struct sock	*sk = (struct sock*)con->priv;
	
	DBFENTER;
	if (status) 
		goto err;
	sk->sk_bound_dev_if = con->hci->devnum;
	rfcon_set_rxspace(con, sk->sk_rcvbuf - atomic_read(&sk->sk_rmem_alloc));

	if (rpf_get(sk)->port.flags & RFCOMM_SOCK_BTY) {
		// disable
		rfcon_set_rxfc(con, AFFIX_FLOW_OFF);
	} else {
		// enable
		rfcon_set_rxfc(con, AFFIX_FLOW_ON);
	}
	if (sk->sk_pair){ /* incomming connection */
		if (affix_sock_ready(sk))
			goto err;
	} else if (!sock_flag(sk, SOCK_DEAD)) {
		sk->sk_state = CON_ESTABLISHED;
		sk->sk_state_change(sk);
	}
	DBFEXIT;
	return 0;
err:
	rpf_reset(sk, status);
	return 0;
}


int rpf_disconnect_ind(rfcomm_con *con)
{
	struct sock	*sk = (struct sock*)con->priv;

	DBFENTER;
	rpf_reset(sk, 0);
	DBFEXIT;
	return 0;
}

int rpf_control_ind(rfcomm_con *con, int type)
{
	struct sock	*sk = (struct sock*)con->priv;
	
	DBFENTER;
	switch (type) {
		case RFCOMM_TX_WAKEUP:
			rpf_xmit_wakeup(sk);
			// fall through...
		default:
			if (rpf_get(sk)->port.line >= 0) {
				bty_control_ind(sk, type);
			} else {
				/* may be check flow here */
				if (!sock_flag(sk, SOCK_DEAD))
					sk->sk_state_change(sk);
			}
			break;
	}
	DBFEXIT;
	return 0;
}

/* Callback functions for RFCOMM layer */
rfcomm_proto_ops rpf_proto = {
	rpf_data_ind,
	rpf_connect_ind,
	rpf_connect_cfm,
	rpf_disconnect_ind,
	rpf_control_ind
};


/******************************  RFCOMM PROTOCOL FAMILY SUBSYSTEM  **************************/

int rpf_release(struct socket *sock)
{
	struct sock	*sk = sock->sk;
	rfcomm_sock_t	*rsk;

	DBFENTER;
	if (!sk)
		return 0;
        sock->sk = NULL;
	sock_orphan(sk);	/* important */
	rsk  = rpf_get(sk);
	if (rsk->port.line >= 0) {
		// connection present. don't kill socket
		sock_reset_flag(sk, SOCK_DEAD);
		sock_put(sk);
		goto exit;
	}

	tasklet_kill(&rsk->tx_task);	// it's not activated here because we are socket

	if (rsk->base.sport)	{/* server socket */
		int		count = 0;
		rfcomm_sock_t	*_rsk;
		/* count registered sockets */
		btl_read_lock(&rfcomm_socks);
		btl_for_each (_rsk, rfcomm_socks) {
			if (_rsk->base.sport == rsk->base.sport)
				count++;
		}
		btl_read_unlock(&rfcomm_socks);
		if (count == 1) /* we only use it */
			rfcomm_unregister_protocol(rsk->base.sport);
	}
	rpf_destroy(sk);
exit:
	DBFEXIT;
	return 0;
}

int rpf_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	struct sock		*sk = sock->sk;
	rfcomm_sock_t		*rsk = NULL;
	struct sockaddr_affix	*saddr = (void*)addr;
	int			err = -EINVAL, count = 0;

	DBFENTER;
	btl_read_lock(&rfcomm_socks);
	if (saddr->port) {
		btl_for_each (rsk, rfcomm_socks) {
			if (rsk->base.sport == saddr->port) {
				count++;
				if (rsk->base.sk->sk_bound_dev_if == saddr->devnum)
					break;
			}
		}
	}
	if (!rsk) {
		err = 0;
		if (!count)
			err = rfcomm_register_protocol(&saddr->port, &rpf_proto, sk);
		if (!err) {
			rsk = rpf_get(sk);
			rsk->base.dport = rsk->base.sport = saddr->port;
			sk->sk_bound_dev_if = saddr->devnum;
		}
	}
	btl_read_unlock(&rfcomm_socks);
	DBFEXIT;
	return err;
}

int rpf_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags)
{
	return affix_sock_connect(sock, addr, alen, flags, rpf_connect_req);
}

/*
  ioctl helpers
*/
int rpf_open_bty(struct sock *sk, int *line)
{
	int	err;
	
	DBFENTER;
	if (!(rpf_get(sk)->port.flags & RFCOMM_SOCK_BOUND) && sk->sk_state != CON_ESTABLISHED)
		return -ENOTCONN;
	err = bty_register_sock(sk, *line);
	if (err < 0)
		return err;
	/* set callbacks */
	write_lock_bh(&sk->sk_callback_lock);
	sk->sk_state_change =  rpf_state_change;	// called by sk->sk_state_change()
	sk->sk_data_ready = rpf_data_ready;		// called by sk->sk_data_ready()
	sk->sk_write_space = rpf_write_space;		// called by kfree_skb()
	write_unlock_bh(&sk->sk_callback_lock);
	*line = err;
	DBFEXIT;
	return 0;
}

int rpf_bind_bty(struct sock *sk, struct rfcomm_port *port)
{
	rfcomm_sock_t		*rsk = rpf_get(sk);

	DBFENTER;
	rsk->port.flags |= RFCOMM_SOCK_BTY | RFCOMM_SOCK_BOUND;
	rsk->port.addr = port->addr;
	return rpf_open_bty(sk, &port->line);
}

int rpf_close_bty(int line)
{
	struct sock	*sk;
	
	DBFENTER;
	// we take ownership of the sk
	sk = bty_unregister_sock(line);
	if (!sk)
		return -ENODEV;
	if (sk->sk_socket) {	/* still owned by "socket" */
		sock_put(sk);
		return -EBUSY;
	}
	rpf_destroy(sk);
	DBFEXIT;
	return 0;
}

int rpf_get_ports(struct rfcomm_ports *info)
{
	int		line, count;
	struct sock	*sk;
	struct rfcomm_port	*pi = info->ports;
	
	DBFENTER;
	// check space for fill
	if (!access_ok(VERIFY_WRITE, pi, info->size * sizeof(struct rfcomm_port)))
		return -EFAULT;
	DBPRT("Size: %d\n", info->size);
 	for (line = 0, count = 0; line < bty_maxdev && count < info->size; line++) {
		sk = bty_get_sock(line);
		if (!sk)
			continue;
		if (!(rpf_get(sk)->port.flags & RFCOMM_SOCK_BOUND)) {
			rfcomm_con	*con = rpf_get(sk)->con;
			if (con) {
				rpf_get(sk)->port.addr.bda = con->bda;
				rpf_get(sk)->port.addr.port = RFCOMM_SCH(con->dlci);
				rpf_get(sk)->port.addr.devnum = con->hci ? con->hci->devnum : 0;
			}
		}
		if (sk->sk_state == CON_ESTABLISHED)
			rpf_get(sk)->port.flags |= RFCOMM_SOCK_CONNECTED;
		else
			rpf_get(sk)->port.flags &= ~RFCOMM_SOCK_CONNECTED;

		__copy_to_user(pi, &rpf_get(sk)->port, sizeof(struct rfcomm_port)); 
		pi++;
		count++;
		sock_put(sk);
	}
	info->count = count;
	DBPRT("Count: %d\n", count);
	DBFEXIT;
	return 0;
}

int rpf_settype(struct sock *sk, int type)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);

	DBFENTER;
	if (type > RFCOMM_TYPE_BTY)
		return -EINVAL;
	if (type == RFCOMM_TYPE_BTY)
		rsk->port.flags |= RFCOMM_SOCK_BTY;
	DBFEXIT;
	return 0;
}

int rpf_connect_bty(struct sock *sk, wait_queue_head_t *wq)
{
	int	err;
	DBFENTER;

	if (rpf_get(sk)->port.flags & RFCOMM_SOCK_BOUND) {
		sk->sk_state = CON_CONNECTING;
		sk->sk_shutdown = 0;
		err = rpf_connect_req(sk, &rpf_get(sk)->port.addr);
		if (err < 0)
			return err;
		err = affix_sock_wait_for_state_sleep(sk, CON_ESTABLISHED, 0, wq);
		if (err)
			return err;
	} else {
		if (!rpf_get(sk)->con || sk->sk_state != CON_ESTABLISHED)
			return -ENOTCONN;
	}
	DBFEXIT;
	return 0;
}

void rpf_disconnect_bty(struct sock *sk)
{
	DBFENTER;
	if (rpf_get(sk)->port.flags & RFCOMM_SOCK_BOUND) {
		// disconnect on close
		rpf_close(sk);
	}
	DBFEXIT;
}

int rpf_ioctl(struct socket *sock, unsigned int cmd,  unsigned long arg)
{
	int	err = 0;

	DBFENTER;
	switch(cmd) {
		case SIOCRFCOMM_OPEN_BTY:
			err = rpf_open_bty(sock->sk, (int*)arg);
			break;
		case SIOCRFCOMM_CLOSE_BTY:
			err = rpf_close_bty(*(int*)arg);
			break;
		case SIOCRFCOMM_GET_PORTS:
			err = rpf_get_ports((void*)arg);
			break;
		case SIOCRFCOMM_SETTYPE:
			err = rpf_settype(sock->sk, *(int*)arg);
			break;
		case SIOCRFCOMM_BIND_BTY:
			err = rpf_bind_bty(sock->sk, (struct rfcomm_port*)arg);
			break;
		default:
			DBPRT("Unprocessed ioctl: %x\n", cmd);
			return -ENOIOCTLCMD;
	}
	DBFEXIT;
	return err;
}

int rpf_getname(struct socket *sock, struct sockaddr *uaddr, int *alen, int peer)
{
	struct sock		*sk = sock->sk;
	struct sockaddr_affix	*addr = (struct sockaddr_affix*)uaddr;
	rfcomm_sock_t		*rsk = rpf_get(sk);

	DBFENTER;
	addr->family = AF_AFFIX;
	addr->devnum = sk->sk_bound_dev_if;
	if (rsk->con)
		addr->bda = rsk->con->bda;
	addr->port = rsk->base.dport;
	*alen = sizeof(struct sockaddr_affix);
	DBFEXIT;
	return 0;

}

int rpf_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
	rfcomm_sock_t	*rsk = rpf_get(sock->sk);

	DBFENTER;
	switch (optname) {
		case BTSO_SECURITY:
			if (get_user(rsk->security, (int*)optval))
				return -EFAULT;
			break;
		default:
			return -EINVAL;
	}
	DBFEXIT;
	return 0;
}

int rpf_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen)
{
	rfcomm_sock_t	*rsk = rpf_get(sock->sk);
	int		err = 0;

	DBFENTER;
	switch (optname) {
		case BTSO_MTU:{
			int	mtu;
			if (sock->sk->sk_state != CON_ESTABLISHED)
				return -ENOTCONN;
			mtu = rfcon_getmtu(rsk->con);
			if (put_user(mtu, (int*)optval))
				return -EFAULT;
			if (put_user(sizeof(int), optlen))
				return -EFAULT;
		}
		break;

		default:
			return -EINVAL;
	}
	DBFEXIT;
	return err;
}

int rpf_xmit_wakeup(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);
	int		err;
	struct sk_buff	*skb;
	
	DBFENTER;
	set_bit(RFCOMM_XMIT_WAKEUP, &rsk->xmit_flags);
restart:
	if (test_and_set_bit(RFCOMM_XMIT_BUSY, &rsk->xmit_flags))
		return 0;
	clear_bit(RFCOMM_XMIT_WAKEUP, &rsk->xmit_flags);
	err = 0;
	for (;;) {
		if (sk->sk_state != CON_ESTABLISHED) {
			err = -ENOTCONN;
			break;
		}
		if (!rfcon_get_txfc(rsk->con))
			break;
		skb = skb_dequeue(&sk->sk_write_queue);
		if (!skb)
			break;
		DBPRT("Message is sent, len: %d\n", skb->len);
		DBDUMP(skb->data, skb->len);
		err = rfcomm_send_data(rsk->con, skb);
		if (err) {
			//skb_queue_head(&sk->sk_write_queue, skb);
			// it's gone
			break;
		}
	}
	clear_bit(RFCOMM_XMIT_BUSY, &rsk->xmit_flags);
	if (test_bit(RFCOMM_XMIT_WAKEUP, &rsk->xmit_flags))
		goto restart;
	DBFEXIT;
	return err;
}

void rpf_tx_task(void *data)
{
	struct sock	*sk = data;
	DBFENTER;
	rpf_xmit_wakeup(sk);
	DBFEXIT;
}

/*
 * exported for BTY
 *
 * transmit now if now write space
 * otherwise later
 */
void rpf_send_data(struct sock *sk)
{
	rfcomm_sock_t	*rsk = rpf_get(sk);
	unsigned long	flags;
	struct sk_buff	*skb;
	int		tx_now = 0;
	
	DBFENTER;
	spin_lock_irqsave(&sk->sk_write_queue.lock, flags);
	skb = skb_peek_tail(&sk->sk_write_queue);
	if (skb && rfcomm_skb_tailroom(skb) == 0)
		tx_now = 1;
	spin_unlock_irqrestore(&sk->sk_write_queue.lock, flags);
	if (tx_now)
		rpf_xmit_wakeup(sk);
	else {
		// PPP optimization: we do transmission after all chunk has been written
		// it may be useless now because we have flow control buffering
		tasklet_schedule(&rsk->tx_task);
	}
	DBFEXIT;
}


/*
    size - size of the iovec array, size of the message
*/
int rpf_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock		*sk = sock->sk;
	rfcomm_sock_t		*rsk = rpf_get(sk);
	int			err = -ENOTCONN;
	struct sk_buff		*txbuff;
	int			mtu, count = 0;

	DBFENTER;

	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))
		return -EINVAL;

	while (size) {
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

		txbuff = skb_dequeue_tail(&sk->sk_write_queue);
		if (txbuff && rfcomm_skb_tailroom(txbuff) == 0) {
			/* no space */
			skb_queue_tail(&sk->sk_write_queue, txbuff);
			txbuff = NULL;
		}
		if (!txbuff) {
			/* allocate new one */
			mtu = rfcon_getmtu(rsk->con);
			txbuff = sock_alloc_send_skb(sk, RFCOMM_SKB_RESERVE + mtu + RFCOMM_FCS_SIZE,
					msg->msg_flags & MSG_DONTWAIT, &err);
			if (!txbuff)
				break;
			skb_reserve(txbuff, RFCOMM_SKB_RESERVE);
			rfcomm_cb(txbuff)->size = mtu;
		}
		mtu = btmin(size, rfcomm_skb_tailroom(txbuff));
		err = memcpy_fromiovec(txbuff->tail, msg->msg_iov, mtu);
		if (err) {
			skb_queue_tail(&sk->sk_write_queue, txbuff);
			break;
		}
		skb_put(txbuff, mtu);
		count += mtu;
		size -= mtu;
		skb_queue_tail(&sk->sk_write_queue, txbuff);
		rpf_xmit_wakeup(sk);
	}
	DBFEXIT;
	return (!err) ? count : err;
}

int rpf_shutdown(struct socket *sock, int how)
{
	int		err;
	struct sock	*sk = sock->sk;

	sock->state = SS_UNCONNECTED;
	err = rpf_close(sk);
	if (!sock_flag(sk, SOCK_DEAD)) {
		sk->sk_state_change(sk);
	}
	return err;
}


/*
    Protocol family operations
*/

struct proto_ops rpf_ops = {
	owner:		THIS_MODULE,
	family:		PF_AFFIX,

	release:	rpf_release,
	bind:		rpf_bind,
	connect:	rpf_connect,
	socketpair:	sock_no_socketpair,
	accept:		affix_sock_accept,
	getname:	rpf_getname,
	poll:		datagram_poll,
	ioctl:		rpf_ioctl,
	listen:		affix_sock_listen,
	shutdown:	rpf_shutdown,
	setsockopt:	rpf_setsockopt,
	getsockopt:	sock_no_getsockopt,
	sendmsg:	rpf_sendmsg,
	recvmsg:	affix_sock_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage
};

int __rpf_create(struct socket *sock, int protocol)
{
	struct sock		*sk;

	DBFENTER;
	sock->state = SS_UNCONNECTED;
	if (sock->type != SOCK_STREAM)
		return -ESOCKTNOSUPPORT;
	sk = rpf_create(sock, PF_AFFIX, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;
	sk->sk_destruct = rpf_destruct;
	sk->sk_protocol = protocol;
	sock->ops = &rpf_ops;
	DBFEXIT;
	return 0;
}

struct net_proto_family rpf_family_ops = {
	owner:	THIS_MODULE,
	family:	PF_AFFIX,
	create:	__rpf_create
};

#ifdef CONFIG_PROC_FS

int rpf_proc_read(char *buf, char **start, off_t offset, int len)
{
	int count = 0;

	DBFENTER;
	count += sprintf(buf+count, "skb allocated: %ld\n", skb_count);
	count += sprintf(buf+count, "RFCOMM sockets in use: %ld, list len: %d\n",
			sock_count, rfcomm_socks.len);
	DBFEXIT;
	return count;
}

struct proc_dir_entry	*rpf_proc;

#endif

int __init init_rpf(void)
{
	int	err;
	
	DBFENTER;
	btl_head_init(&rfcomm_socks);
#ifdef CONFIG_PROC_FS
	rpf_proc = create_proc_info_entry("rfcomm_sock", 0, proc_affix, rpf_proc_read);
	if (rpf_proc == NULL) {
		BTERROR("Unable to register proc fs entry\n");
		return -EINVAL;
	}
#endif
	err = affix_sock_register(&rpf_family_ops, BTPROTO_RFCOMM);
	if (err) {
		goto err1;
	}
	DBFEXIT;
	return 0;
err1:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("rfcomm_sock", proc_affix);
#endif
	return err;
}

void __exit exit_rpf(void)
{
	DBFENTER;
	affix_sock_unregister(BTPROTO_RFCOMM);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("rfcomm_sock", proc_affix);
#endif
	DBFEXIT;
}

/*
 * check tx_task(). it's used only for BTY but not stopped when /dev/bty closed
 */
