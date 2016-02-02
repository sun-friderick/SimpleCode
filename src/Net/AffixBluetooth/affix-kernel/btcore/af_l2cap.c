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
   $Id: af_l2cap.c,v 1.91 2004/07/16 18:58:52 chineape Exp $

   AF_L2CAP - L2CAP Address family for socket interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
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

/* Local Includes */
#define FILEBIT	DBAFL2CAP

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>


/* LPF driver variables */
btlist_head_t			l2cap_socks;

extern l2cap_proto_ops 		lpf_proto;

/*
 * debuging stuff.
 */
static long 			sock_count = 0;

void lpf_destroy(struct sock *sk);

/***************************    Connection Management   ********************************/
unsigned int lpf_poll(struct file *file, struct socket *sock, poll_table *wait)
{
	unsigned int	mask;
	DBFENTER;
	mask = datagram_poll(file, sock, wait);
	DBPRT("poll() mask: %x\n", mask);
	DBFEXIT;
	return mask;
}


struct sock *lpf_lookup_sock(hci_struct *hci, __u16 psm)
{
	l2cap_sock_t	*lsk, *zero = NULL;
	struct sock	*sk = NULL;
	
	btl_read_lock(&l2cap_socks);
	btl_for_each (lsk, l2cap_socks) {
		if (lsk->base.sk->sk_state == CON_LISTEN && lsk->base.sport == psm) {
			if (!zero && lsk->base.sk->sk_bound_dev_if == 0)
				zero = lsk;
			if (hci->devnum == lsk->base.sk->sk_bound_dev_if)
				break;
		}
	}
	if (!lsk)
		lsk = zero;
	if (lsk) {
		sock_hold(lsk->base.sk);
		sk = lsk->base.sk;
	}
	btl_read_unlock(&l2cap_socks);
	return sk;
}

void lpf_destruct(struct sock *sk)
{
	DBFENTER;
	kfree(lpf_get(sk));
	sock_count--;	/* XXX */
	DBFEXIT;
}

int lpf_close(struct sock *sk)
{
	l2cap_sock_t	*lsk = lpf_get(sk);
	struct sk_buff	*skb;
	
	DBFENTER;
        sk->sk_err = 0;
	sk->sk_state = CON_CLOSED;
	sk->sk_shutdown = SHUTDOWN_MASK;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type == SOCK_DGRAM) {
		l2ca_remove_group(lsk->base.dport);
	} else
#endif
	if (lsk->ch) {
		l2ca_orphan(lsk->ch);
		l2ca_put(lsk->ch);
		lsk->ch = NULL;
	}
	if (sk->sk_pair) {
		sk->sk_pair->sk_ack_backlog--;
		sock_put(sk->sk_pair);
		sk->sk_pair = NULL;
	}
	/* remove all packets and check if threre is a connection in the queue */
	while((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
		if (skb->sk != sk)	/* A pending connection */
			lpf_destroy(skb->sk);
		kfree_skb(skb);
	}
	DBFEXIT;
	return 0;
}

void lpf_destroy(struct sock *sk)
{
	DBFENTER;
	btl_unlink(&l2cap_socks, lpf_get(sk));
	lpf_close(sk);
	sock_put(sk);
	DBFEXIT;
}

struct sock *lpf_create(struct socket *sock, int family, int priority, int zero_it)
{
	struct sock	*sk;
	l2cap_sock_t	*lsk;

	sk  = sk_alloc(family, priority, zero_it, NULL);
	if (!sk)
		return NULL;
	sock_init_data(sock, sk);

	lsk = (l2cap_sock_t*)kmalloc(sizeof(l2cap_sock_t), priority);
	if (!lsk) {
		sk_free(sk);
		return NULL;
	}
	memset(lsk, 0, sizeof(l2cap_sock_t));
	sk->sk_state = CON_CLOSED;
	sk->sk_prot = (void*)lsk;
	lsk->base.sk = sk;
	// set some options
	lsk->mru = L2CAP_MTU;	/* FIXME */
	sock_count++;		/* XXX */
	btl_add_tail(&l2cap_socks, lsk);
	sk_set_owner(sk, THIS_MODULE);
	return sk;
}

int lpf_connect_req(struct sock *sk, struct sockaddr_affix *addr)
{
	l2cap_sock_t 	*lsk = lpf_get(sk);
	l2cap_ch	*ch;
	int		err;
	hci_struct	*hci = NULL;

	DBFENTER;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type != SOCK_DGRAM)
#endif
	{
		if (addr->devnum) {
			hci = hci_lookup_devnum(addr->devnum);	/* find device */
			if (!hci)
				return -ENODEV;
		}
	}
	lsk->base.dport = addr->port;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type == SOCK_DGRAM)
		err = l2ca_create_group(addr->port, &lpf_proto,&ch);
	else
#endif
		ch = l2ca_create(&lpf_proto);
	if (!ch) {
		if (hci)
			hci_put(hci);
		err = -ENOMEM;
		goto exit;
	}
	lsk->ch = ch;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type != SOCK_DGRAM)
#endif
		ch->hci = hci;
	l2ca_graft(ch, sk);
#ifdef CONFIG_AFFIX_BT_1_2
	__l2cap_set_local_mode(lsk->ch,sk->sk_protocol);
#endif

	l2ca_set_mtu(lsk->ch, lsk->mru);

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type == SOCK_DGRAM) {
		/* connectionless */
		ENTERSTATE(ch, CON_OPEN);
		l2ca_connect_cfm(ch, 0, 0);
		err = 0;
	} else 
#endif
	{
		err = l2ca_connect_req(ch, &addr->bda, lsk->base.dport);
	}
 exit:
	DBFEXIT;
	return err;
}

static inline void lpf_reset(struct sock *sk, int reason)
{
	affix_sock_reset(sk, reason, lpf_destroy);
}

/* 
   LPF protocol subsystem.  Client of L2CAP. Callback functions
*/

int lpf_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	struct sock	*sk = (struct sock*)ch->priv;

	DBFENTER;
	affix_sock_rx(sk, skb, NULL);
	DBFEXIT;
	return 0;
}

struct sock *lpf_dup(struct sock *osk)
{
	struct sock	*sk;

	sk = lpf_create(NULL, PF_AFFIX, GFP_ATOMIC, 1);
	if (!sk)
		return NULL;

	sk->sk_pair	= osk;
	sk->sk_protocol = osk->sk_protocol;
	sk->sk_type     = osk->sk_type;
	sk->sk_destruct = osk->sk_destruct;
	sk->sk_priority = osk->sk_priority;

	return sk;
}
/*
 * ch hold here. need to put it
 */
int lpf_connect_ind(l2cap_ch *ch)
{
	struct sock	*sk, *pair;
	l2cap_sock_t	*lsk;

	DBFENTER;

	pair = lpf_lookup_sock(ch->hci, ch->psm);
	if (!pair) {
		l2ca_connect_rsp(ch, L2CAP_CONRSP_PSM, 0);
		return -1;
	}
	if (pair->sk_ack_backlog >= pair->sk_max_ack_backlog) {
		l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
		sock_put(pair);
		return -1;
	}
	sk = lpf_dup(pair);
	if (!sk) {
		l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
		sock_put(pair);
		return -1;
	}
	lsk = lpf_get(sk);
	lsk->base.dport = lpf_get(pair)->base.dport;
	pair->sk_ack_backlog++;

	l2ca_hold(ch);		// hold it
	lsk->ch = ch;
	l2ca_graft(ch, sk);
#ifdef CONFIG_AFFIX_BT_1_2
	__l2cap_set_local_mode(lsk->ch,sk->sk_protocol);
#endif
	l2ca_set_mtu(lsk->ch, lsk->mru);
	l2ca_connect_rsp(ch, L2CAP_CONRSP_SUCCESS, 0);
	
	DBFEXIT;
	return 0;
}

int lpf_connect_cfm(l2cap_ch *ch, int result, int status)
{
	struct sock	*sk = (struct sock*)ch->priv;
	l2cap_sock_t	*lsk = lpf_get(sk);
	
	DBFENTER;
	if (result)
		goto err;
	sk->sk_bound_dev_if = ch->hci->devnum;
#ifdef CONFIG_AFFIX_BT_1_2
	lsk->mtu = l2ca_get_mps(ch);
#else
	lsk->mtu = l2ca_get_mtu(ch);
#endif
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
	lpf_reset(sk, result);
	return 0;
}


int lpf_config_ind(l2cap_ch *ch)
{
	struct sock	*sk = (struct sock*)ch->priv;
	l2cap_sock_t	*lsk = lpf_get(sk);
	__u16		result = 0;

	DBFENTER;
	
	l2ca_set_mtu(lsk->ch, lsk->mru);
	result = l2ca_config_rsp(ch, L2CAP_CFGRSP_SUCCESS);
	
	DBFEXIT;
	return result;
}

int lpf_config_cfm(l2cap_ch *ch, int result)
{
	struct sock	*sk = (struct sock*)ch->priv;
	l2cap_sock_t	*lsk = lpf_get(sk);

	DBFENTER;
	if (result) {
		lpf_reset(sk, result);	// like in disconnect_ind
		return 0;
	}
#ifdef CONFIG_AFFIX_BT_1_2
	lsk->mtu = l2ca_get_mps(ch);
#else
	lsk->mtu = l2ca_get_mtu(ch);
#endif
	if (!sock_flag(sk, SOCK_DEAD)) {
		sk->sk_state = CON_ESTABLISHED;
		sk->sk_state_change(sk);
	}
	DBFEXIT;
	return 0;
}

int lpf_disconnect_ind(l2cap_ch *ch)
{
	struct sock	*sk = (struct sock*)ch->priv;

	DBFENTER;
	lpf_reset(sk, 0);
	DBFEXIT;
	return 0;
}

int lpf_control_ind(l2cap_ch *ch, int event, void *arg)
{
	struct sock	*sk = (struct sock*)ch->priv;

	DBFENTER;
	switch (event) {
		case L2CAP_EVENT_PING:
			skb_get(arg);
			affix_sock_rx(sk, arg, NULL);
			break;
		case L2CAP_EVENT_QOS_VIOLATION:
			break;
		case L2CAP_EVENT_TIMEOUT:
			break;
	}
	DBFEXIT;
	return 0;
}

/* Callback functions for L2CAP layer */
l2cap_proto_ops lpf_proto = {
	owner:		THIS_MODULE,
	data_ind:	lpf_data_ind,
	connect_ind:	lpf_connect_ind,
	connect_cfm:	lpf_connect_cfm,
	config_ind:	lpf_config_ind,
	config_cfm:	lpf_config_cfm,
	disconnect_ind:	lpf_disconnect_ind,
	control_ind:	lpf_control_ind
};


/******************************  L2CAP PROTOCOL FAMILY SUBSYSTEM  **************************/

int lpf_release(struct socket *sock)
{
	struct sock	*sk = sock->sk;
	l2cap_sock_t	*lsk;

	DBFENTER;
	if (!sk)
		return 0;
	sock_orphan(sk);	/* important */
        sock->sk = NULL;      
	lsk = lpf_get(sk);
	if (lsk->base.sport) {
		int		count = 0;
		l2cap_sock_t	*_lsk;
		/* count registered sockets */
		btl_read_lock(&l2cap_socks);
		btl_for_each (_lsk, l2cap_socks) {
			if (lsk->base.sport == _lsk->base.sport)
				count++;
		}
		btl_read_unlock(&l2cap_socks);
		if (count == 1)	/* we only use it */
			l2ca_unregister_protocol(lsk->base.sport);
	}
	lpf_destroy(sk);
	DBFEXIT;
	return 0;
}

int lpf_bind(struct socket *sock, struct sockaddr *addr, int addr_len)
{
	struct sock		*sk = sock->sk;
	l2cap_sock_t		*lsk = NULL;
	struct sockaddr_affix	*saddr = (void*)addr;
	int			err = -EBUSY, count = 0;

	DBFENTER;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS	
	if (sock->type == SOCK_DGRAM && !bda_zero(&saddr->bda))
		return -ENOMEM;
#endif
	btl_read_lock(&l2cap_socks);
	if (saddr->port) {
		/* find if already registered */
		btl_for_each (lsk, l2cap_socks) {
			if (lsk->base.sport == saddr->port) {
				count++;
				if (lsk->base.sk->sk_bound_dev_if == saddr->devnum)
					break;
			}
		}
	}
	if (!lsk) {
		if (!count)
			err = l2ca_register_protocol(saddr->port, &lpf_proto);
		else
			err = saddr->port;
		if (err >= 0) {
			lsk = lpf_get(sk);
			lsk->base.dport = lsk->base.sport = err;
			sk->sk_bound_dev_if = saddr->devnum;
			err = 0;
		}
	}
	btl_read_unlock(&l2cap_socks);
	DBFEXIT;
	return err;
}

int lpf_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags)
{
	return affix_sock_connect(sock, addr, alen, flags, lpf_connect_req);
}

int lpf_ioctl_config(struct socket *sock)
{
	struct sock	*sk = sock->sk;
	l2cap_sock_t	*lsk = lpf_get(sk);
	int		err = 0;

	DBFENTER;
	if (sock->state == SS_CONNECTING && sk->sk_state == CON_ESTABLISHED) {
		sock->state = SS_CONNECTED;
		return 0;
	}
	/* Move to connecting socket, start sending Connect Requests */
	sock->state = SS_CONNECTING;
	sk->sk_state = CON_CONNECTING;
	l2ca_set_mtu(lsk->ch, lsk->mru);
	err = l2ca_config_req(lsk->ch);
	if (err < 0)
		goto exit;

	err = affix_sock_wait_for_state(sk, CON_ESTABLISHED, sock->file->f_flags & O_NONBLOCK);
	if (err)
		goto exit;
	sock->state = SS_CONNECTED;
 exit:
	DBFEXIT;
	return err;
}

int lpf_ioctl(struct socket *sock, unsigned int cmd,  unsigned long arg)
{
	DBFENTER;
	switch (cmd) {
		case SIOCL2CAP_CONFIG:
			return lpf_ioctl_config(sock);
		case SIOCL2CAP_FLUSH:
			return affix_sock_flush(sock->sk);
		default:
			return -EINVAL;
	}
	DBFEXIT;
	return 0;
}

int lpf_getname(struct socket *sock, struct sockaddr *uaddr, int *alen, int peer)
{
	struct sock		*sk = sock->sk;
	struct sockaddr_affix	*addr = (struct sockaddr_affix*)uaddr;
	l2cap_sock_t		*lsk = lpf_get(sk);

	DBFENTER;
	addr->family = AF_AFFIX;
	addr->devnum = sk->sk_bound_dev_if;
	if (lsk->ch)
		addr->bda = lsk->ch->bda;
	addr->port = lsk->base.dport;
	*alen = sizeof(struct sockaddr_affix);
	DBFEXIT;
	return 0;

}

int lpf_setsockopt(struct socket *sock, int level, int optname, char *optval, int optlen)
{
	l2cap_sock_t	*lsk = lpf_get(sock->sk);
	int		err = -ENOPROTOOPT;

	DBFENTER;
	if (level != SOL_AFFIX)
		return -ENOPROTOOPT;
	lock_sock(sock->sk);
	switch (optname) {
		case BTSO_MTU:
			err = get_user(lsk->mru, (int*)optval);
			break;
		case BTSO_TYPE:
			break;
	}
	release_sock(sock->sk);
	DBFEXIT;
	return err;
}

int lpf_getsockopt(struct socket *sock, int level, int optname, char *optval, int *optlen)
{
	l2cap_sock_t	*lsk = lpf_get(sock->sk);

	DBFENTER;
	switch (optname) {
		case BTSO_MTU:
			if (sock->sk->sk_state != CON_ESTABLISHED)
				return -ENOTCONN;
			if (put_user(lsk->mtu, (int*)optval))
				return -EFAULT;
			if (put_user(sizeof(int), optlen))
				return -EFAULT;
			break;

		default:
			return -EINVAL;
	}
	DBFEXIT;
	return 0;
}


int lpf_getcmsg(struct msghdr *msg)
{
	DBPRT("control: %p, len: %d\n", msg->msg_control, msg->msg_controllen);
	if (msg->msg_control && msg->msg_controllen) {
		struct cmsghdr	*cmsg;
		for(cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
			if (cmsg->cmsg_level != SOL_AFFIX)
				continue;
			return cmsg->cmsg_type;
		}
	}
	return -1;
}

/*
    size - size of the iovec array, size of the message
*/
int lpf_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock		*sk = sock->sk;
	l2cap_sock_t		*lsk;
	int			err = -ENOTCONN;
	struct sk_buff		*skb;
	int			hdrlen, ctype;

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

	lsk = lpf_get(sk);
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
	if (sk->sk_type == SOCK_DGRAM)
		hdrlen = L2CAP_GROUP_SKB_RESERVE;
	else
#endif
		hdrlen = L2CAP_SKB_RESERVE;
	size = btmin(lsk->mtu, size);

	switch ((ctype = lpf_getcmsg(msg))) {
		case L2CAP_PING:
		case L2CAP_SINGLEPING:
			hdrlen += 4;	/* for command */	
			break;
	}
	skb = sock_alloc_send_skb(sk, size + hdrlen, msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
		return err;
	skb_reserve(skb, hdrlen);
	
	err = memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);
	if (err)
		goto error;
	DBDUMP(skb->data, skb->len);
	switch (ctype) {
		case L2CAP_PING:
			DBPRT("Pinging... %d bytes\n", size);
			err = l2ca_ping(lsk->ch, skb);
			break;
		case L2CAP_SINGLEPING:
			DBPRT("Pinging... %d bytes\n", size);
			err = l2ca_singleping(lsk->ch, skb);
			break;
		default:
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
			if (sk->sk_type == SOCK_DGRAM)
				err = l2ca_send_data_group(lsk->ch->psm, skb);
			else
#endif
				/* normal data */
				err = l2ca_send_data(lsk->ch, skb);
	}
	if (err)
		goto error;
	DBFEXIT;
	return size;
error:
	kfree_skb(skb);
	DBFEXIT;
	return err;
}

int lpf_shutdown(struct socket *sock, int how)
{
	int		err;
	struct sock	*sk = sock->sk;

	sock->state = SS_UNCONNECTED;
	err = lpf_close(sk);
	if (!sock_flag(sk, SOCK_DEAD)) {
		sk->sk_state_change(sk);
	}
	return err;
}

/*
    Protocol family operations
*/

struct proto_ops lpf_ops_data = {
	owner:		THIS_MODULE,
	family:		PF_AFFIX,

	release:	lpf_release,
	bind:		lpf_bind,
	connect:	lpf_connect,
	socketpair:	sock_no_socketpair,
	accept:		affix_sock_accept,
	getname:	lpf_getname,
	poll:		lpf_poll,
	ioctl:		lpf_ioctl,
	listen:		affix_sock_listen,
	shutdown:	lpf_shutdown,
	setsockopt:	lpf_setsockopt,
	getsockopt:	lpf_getsockopt,
	sendmsg:	lpf_sendmsg,
	recvmsg:	affix_sock_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage
};

struct proto_ops lpf_ops_raw = {
	owner:		THIS_MODULE,
	family:		PF_AFFIX,

	release:	lpf_release,
	bind:		sock_no_bind,		//lpf_bind,
	connect:	sock_no_connect, 	//lpf_connect,
	socketpair:	sock_no_socketpair,
	accept:		sock_no_accept, 	//lpf_accept,
	getname:	lpf_getname,
	poll:		lpf_poll,
	ioctl:		lpf_ioctl,
	listen:		sock_no_listen, 	//lpf_listen,
	shutdown:	sock_no_shutdown,
	setsockopt:	lpf_setsockopt,
	getsockopt:	lpf_getsockopt,
	sendmsg:	lpf_sendmsg,
	recvmsg:	affix_sock_recvmsg,
	mmap:		sock_no_mmap,
	sendpage:	sock_no_sendpage
};

int __lpf_create(struct socket *sock, int protocol)
{
	struct sock	*sk;

	DBFENTER;
	if (sock->type != SOCK_STREAM && sock->type != SOCK_SEQPACKET && sock->type != SOCK_RAW) {
#ifdef CONFIG_AFFIX_L2CAP_GROUPS
		if (sock->type != SOCK_DGRAM)
#endif
			return -ESOCKTNOSUPPORT;
	}
	sock->state = SS_UNCONNECTED;
	sk = lpf_create(sock, PF_AFFIX, GFP_KERNEL, 1);
	if (!sk)
		return -ENOMEM;
	sk->sk_destruct = lpf_destruct;
	sk->sk_protocol = protocol;
	if (sock->type == SOCK_RAW)
		sock->ops = &lpf_ops_raw;
	else
		sock->ops = &lpf_ops_data;
	DBFEXIT;
	return 0;
}


struct net_proto_family lpf_family_ops = {
	owner:		THIS_MODULE,
	family:	PF_AFFIX,
	create:	__lpf_create
};

#ifdef CONFIG_PROC_FS

int lpf_proc_read(char *buf, char **start, off_t offset, int len)
{
	int count = 0;

	DBFENTER;
	count += sprintf(buf+count, "L2CAP sockets in use: %ld, list len: %d\n",
			sock_count, l2cap_socks.len);
	DBFEXIT;
	return count;
}

struct proc_dir_entry		*lpf_proc;

#endif

int __init init_lpf(void)
{
	int	err;
	DBFENTER;

	btl_head_init(&l2cap_socks);
#ifdef CONFIG_PROC_FS
	lpf_proc = create_proc_info_entry("l2cap_sock", 0, proc_affix, lpf_proc_read);
	if (!lpf_proc) {
		BTERROR("Unable to register proc fs entry\n");
		return -EINVAL;
	}
#endif
	err = affix_sock_register(&lpf_family_ops, BTPROTO_L2CAP); /* NOTE L2CAP 1.2: BTPROTO_L2CAP == BTPROTO_L2CAP_BASIC */ 
#ifdef CONFIG_AFFIX_BT_1_2
	err = affix_sock_register(&lpf_family_ops, BTPROTO_L2CAP_RET);
	err = affix_sock_register(&lpf_family_ops, BTPROTO_L2CAP_FC);
#endif
	if (err)
		goto err1;
	DBFEXIT;
	return 0;
 err1:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("l2cap_sock", proc_affix);
#endif
	return err;
}

void __exit exit_lpf(void)
{
	DBFENTER;
	affix_sock_unregister(BTPROTO_L2CAP);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("l2cap_sock", proc_affix);
#endif
	DBFEXIT;
}

