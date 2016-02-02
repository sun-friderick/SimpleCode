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
   $Id: hci.h,v 1.151 2004/07/22 14:38:03 chineape Exp $

   Host Controller Interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/

#define HCI_BROADCAST_CHANDLE	0x0001

#ifndef _HCI_KERNEL_H
#define _HCI_KERNEL_H

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if.h>
#include <linux/init.h>
#include <net/sock.h>

#include <asm/uaccess.h>

#include <affix/hci_types.h>
#include <affix/btlist.h>
#include <affix/bluetooth.h>

#define HCIDEV_SKB_RESERVE	12
#define HCI_SKB_RESERVE		(HCIDEV_SKB_RESERVE + HCI_ACL_HDR_LEN)

/* HCI device types */
#define HCI_UART 		0
#define HCI_USB			1
#define HCI_PCCARD		2
#define HCI_UART_CS		3
#define HCI_PCI			4
#define HCI_VHCI		8
#define HCI_LOOP		9

/******* MACRO DEFINITION ******************/

#define ENTERSTATE(ch, st)	((ch)->state = st)
#define SETSTATE(ch, st)	(xchg(&(ch)->state, st))
#define STATE(ch)		((ch)->state)

#define __is_dead(con)		(STATE((con)) == DEAD)


/*  Disconnect an unused HCI connection after this time.
 *  A new connection request will cancel the disconnection
 */
#define HCC_CONN_TIMEOUT	(50 * HZ)
#define HCC_DISC_TIMEOUT	(5 * HZ)
#define HCC_AUTH_TIMEOUT	HCC_CONN_TIMEOUT

#define HCI_SCO_BAUD_RATE	8000	// baud rate
#define HCI_SCO_TIMEOUT		1	// jiffies
/* bytes per HCI_SCO_TIMEOUT */
#define HCI_SCO_CHUNK		(HCI_SCO_BAUD_RATE * HCI_SCO_TIMEOUT / HZ)


#define HCI_FLAGS_INITIATOR		0

/**************   variable declaration    ****************************/

extern struct proc_dir_entry	*proc_affix;
extern btlist_head_t		hcicons;
extern btlist_head_t		hcidevs;
extern struct semaphore		hcidev_sema;
extern btlist_head_t		btdevs;	/* Neighbor Devices list */

/* sysctl */
extern int			sysctl_hci_allow_promisc;
extern int			sysctl_hci_use_inquiry;
extern int			sysctl_hci_max_attempt;
extern int			sysctl_hci_req_count;
extern int			sysctl_hci_req_timeout;
extern int			sysctl_hci_defer_disc_timeout;

extern int			hci_promisc_count;

/*
 * important types
 */
typedef struct hci_struct	hci_struct;
typedef struct hci_con		hci_con;

static inline int test_bit64(int n, __u64 *mask)
{
	__u64	a = (1 << n);
	return (*mask & a) != 0;
}
	
static inline void _bda2str(char *str, BD_ADDR *p)
{
	__u8	*ma=(__u8*)p;

	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", 
		 ma[5], ma[4], ma[3], ma[2], ma[1], ma[0]);
}

static inline char *bda2str(BD_ADDR *bda)
{
	static unsigned char 	buf[2][18];
	static int 		num = 0; 

	num = 1 - num; /* switch buf */
	_bda2str(buf[num], bda);
	return buf[num];
}

static inline void bdacpy(void *rev_bda, void *bda)
{
	int	i;

	for (i = 0; i < 6; i++)
		((__u8*)rev_bda)[i] = ((__u8*)bda)[5-i];
}

static inline suseconds_t sub_timeval(struct timeval *t1, struct timeval *t2)
{
	time_t	dsec = (t1->tv_sec-t2->tv_sec);
	
	if (dsec != 0)
		return (1000000*dsec + t1->tv_usec - t2->tv_usec);
	else
		return (t1->tv_usec - t2->tv_usec);
}

/*
   TRUE, if t1<t2
*/
static inline int cmp_timeval(struct timeval *t1, struct timeval *t2)
{
	if (t1->tv_sec < t2->tv_sec) return 1;
	else if (t1->tv_sec > t2->tv_sec) return 0;
	else if (t1->tv_usec < t2->tv_usec) return 1;
	else return 0;
}


/*
 * remote device record
 */
struct btdev {
	struct list_head	q;	/* List Support */

	atomic_t	refcnt;		/* number of users */
	int		stamp;		/* last inquiry */
	int		state;		/* device state */
#define BTDEV_AUTHPENDING	0x0001

	BD_ADDR		bda;		/* Device Address */
	int		flags;		/* valid fields */
#define	NBT_INQUIRY	0x01
#define NBT_PIN		0x02
#define NBT_KEY		0x04
	
	/* like INQUIRY_ITEM */
	__u8		PS_Repetition_Mode;
	__u8		PS_Period_Mode;
	__u8		PS_Mode;
	__u32		Class_of_Device;
	__u16		Clock_Offset;
	
	/* Security Specific */
	__u8		Key_Type;
	__u8		Link_Key[16];
	__u8		PIN_Code_Length;
	__u8		PIN_Code[16];

	int		paired;
	int		trusted;
};


/*
 * Control structure for HCI interface
 */

enum hcidev_state_t
{
	HCIDEV_STATE_XOFF=0,
	HCIDEV_STATE_START,
	HCIDEV_STATE_RUNNING,
	HCIDEV_STATE_PRESENT,
	HCIDEV_STATE_NOCARRIER
};

struct sco_info {
	struct list_head	q;	/* for queueing */
	
	int	len;
	__u16	handle;
};

struct hci_struct {
	struct list_head	q;	/* for queueing */

	atomic_t		refcnt;
	unsigned long		state;
	void			*priv;
	char			name[IFNAMSIZ];
	BD_ADDR			bda;
	int			flags;		/* HCI_FLAGS_UP, ... */
	int			open_count;
	struct rw_semaphore	sema;		/* serialize access at some point */
	int			pid;		/* process make exclusive access */

	__u16			manfid;
	void			*devinfo;
	int			hdrlen;
	int			devnum;		/* HCI descriptor */
	__u64			lmp_features;

	/* default device attributes */
	__u16			pkt_type;	/* packet type at connection */

	/* scheduler stuff */
	__u16			acl_mtu;
	__u8			sco_mtu;
	__u16			acl_num;
	__u16			sco_num;
	atomic_t		cmd_count;
	atomic_t		acl_count;
	atomic_t		sco_count;

	/* new tx stuff */
	struct tasklet_struct 	tx_task;
	
	/* new rx stuff */
	struct tasklet_struct 	rx_task;
	struct sk_buff_head	rx_queue;

	/* command processing */
	struct sk_buff_head	cmd_queue;
	btlist_head_t		hci_locks;	/* locked commands */

	/* raw data */
	struct sk_buff_head	acl_queue;
#if defined(CONFIG_AFFIX_SCO)
	struct sk_buff_head	sco_queue;
#endif
	
	/* audio */
	__u16			audio_setting;
	int			audio_mode;


	spinlock_t		xmit_lock;
	int			xmit_lock_owner;
	spinlock_t		queue_lock;
	
	int			deadbeaf;
	unsigned char		type;			/* Selectable AUI, TP,..*/
	unsigned long		trans_start;		/* Time (in jiffies) of last Tx	*/
	struct hcidev_stats	stats;

	/* Pointers to interface service routines.	*/
	int			(*open)(hci_struct *hci);
	int			(*close)(hci_struct *hci);
	int			(*send) (hci_struct *hci, struct sk_buff *skb);
	int			(*ioctl)(hci_struct *hci, int cmd, void *arg);
	
	/* open/release and usage marking */
	struct module		*owner;

#ifdef CONFIG_AFFIX_DTL1_FIX
	int			fixit;
	struct sk_buff		*delayed_acl_skb;
#endif
};


struct hci_con {
	struct list_head	q;		/* for queueing */

	con_state		state;		/* object state */
	atomic_t		refcnt;		/* number of users */
	unsigned long		flags;
	struct timer_list	timer;
	int			id;

	int			attempt;	/* retry counter */

	hci_struct		*hci;		/* device */

	BD_ADDR			bda;		/* remote side address */
	__u16			chandle;
	__u8			link_type;
	__u8			encryp_mode;
	__u16			pkt_type;

	struct sk_buff		*rx_skb;	/* for reassembling */

	int			mtu;
	atomic_t		pending;
	struct sk_buff_head	tx_queue;

	/* security block */
	struct btdev		*btdev;

#if defined(CONFIG_AFFIX_SCO)
	/* SCO flow control */
	btlist_head_t		sco_pending;
	struct timer_list	sco_timer;
#endif
};

static inline int __is_acl(hci_con *con)
{
	return con->link_type == HCI_LT_ACL;
}

/*
 * skb->cb control block data structure
 */
typedef struct {
	int	devnum;
	int	conid;
	int	pb;
} hci_skb_cb;

#define hci_skb(skb)	((hci_skb_cb*)(skb->cb))

/**************   functions declaration    ****************************/

/* Connection management */
hci_con *hcc_lookup_chandle(hci_struct *hci, __u16 chandle);
hci_con *hcc_lookup_acl(hci_struct *hci, BD_ADDR *bda);
hci_con *hcc_lookup_id(int id);
hci_con *hcc_lookup_chandle_devnum(int devnum, __u16 chandle);
hci_con *hcc_alloc(void);
hci_con *hcc_create(hci_struct *hci, BD_ADDR *bda);
void __hcc_destroy(hci_con *con);
void hcc_bind(hci_con *con, hci_struct *hci);
int hcc_release(hci_con *con);
void hcc_close(hci_con *con);

static inline void hcc_hold(hci_con *con)
{
	atomic_inc(&con->refcnt);
}

void hcc_put(hci_con *con);

static inline void __hcc_put(hci_con *con)
{
	if (atomic_dec_and_test(&con->refcnt))
		__hcc_destroy(con);
}

/* HCC */
static inline int __hcc_linkup(hci_con *con)
{
	return (STATE(con) == CON_OPEN);
}


static inline void hcc_start_timer(hci_con *con, unsigned long timeout)
{
	if (!del_timer(&con->timer))
		hcc_hold(con);
	mod_timer(&con->timer, jiffies + timeout);
}

/*  called when someone wants to use the HCI connection before the timer above
 *  would expire.
 */
static inline void hcc_stop_timer(hci_con *con)
{
	if (del_timer(&con->timer))
		hcc_put(con);
}

static inline int hcc_authenticated(hci_con *con)
{
	return con->btdev->paired;
}

/*
 * HCI management 
 */
hci_struct *hci_select(void);
hci_struct *__hci_lookup_bda(BD_ADDR *bda);
hci_struct *hci_lookup_bda(BD_ADDR *bda);
hci_struct *__hci_lookup_devnum(int devnum);
hci_struct *hci_lookup_devnum(int devnum);
hci_struct *__hci_lookup_name(char *name);
hci_struct *hci_lookup_name(char *name);
void hci_destroy(hci_struct *hci);

static inline void hci_hold(hci_struct *hci)
{
	atomic_inc(&hci->refcnt);
}

static inline void hci_put(hci_struct *hci)
{
	if (atomic_dec_and_test(&hci->refcnt))
		hci_destroy(hci);
}

/* L2CAP support layer */
struct sk_buff *lp_reassemble_packet(hci_con *con, __u8 first, struct sk_buff *skb);
int lp_receive_data(hci_con *con, __u8 first, __u8 bc, struct sk_buff *skb);
int lp_connect_ind(hci_con *con);
int lp_connect_cfm(hci_con *con, int status);
int lp_disconnect_ind(hci_con *con);
int lp_qos_violation_ind(hci_con *con);
int lp_connect_req(hci_struct *hci, BD_ADDR *bda, hci_con **con);
int lp_add_sco(hci_struct *hci, BD_ADDR *bda, hci_con **ret);
int lp_connect_rsp(hci_con *con, __u8 status);
int lp_disconnect_req(hci_con *con, int reason);
int lp_auth_req(hci_con *con);
int lp_send_data(hci_con *con, struct sk_buff *skb);

#ifdef CONFIG_AFFIX_HCI_BROADCAST
int lp_connect_broadcast(hci_struct *, BD_ADDR *, hci_con **,hci_struct **);
int lp_broadcast_data(hci_con *, struct sk_buff *);
#endif

/* receving part */
int hci_receive_event(hci_struct *hci, struct sk_buff *skb);
int hci_receive_acl(hci_struct *hci, struct sk_buff *skb);
void hci_receive_data(hci_struct *hci, struct sk_buff *skb);
void hci_rx_task(unsigned long arg);


/* network device susbsystem */
static inline void hcidev_lock(void)
{
	down(&hcidev_sema);
}
static inline void hcidev_unlock(void)
{
	up(&hcidev_sema);
}

hci_struct *hcidev_alloc(void);
int hcidev_register(hci_struct *hci, void *param);
void hcidev_unregister(hci_struct *hci);
int hcidev_open(hci_struct *hci);
void hcidev_close(hci_struct *hci);
int hcidev_ioctl(hci_struct *hci, int cmd, void *arg);
int hcidev_rx(hci_struct *hci, struct sk_buff *skb);
void hcidev_state_change(hci_struct *hci, int event);
void hci_run_hotplug(hci_struct *hci, char *verb);
int hci_queue_xmit(hci_struct *hci, hci_con *con, struct sk_buff *skb);
void hci_tx_task(unsigned long arg);

/* inline functions */

static inline int hci_running(hci_struct *hci)
{
	return test_bit(HCIDEV_STATE_START, &hci->state);
}

static inline int hcidev_running(hci_struct *hci)
{
	return test_bit(HCIDEV_STATE_RUNNING, &hci->state);
}

static inline void hcidev_wake_queue(hci_struct *hci)
{
	if (test_and_clear_bit(HCIDEV_STATE_XOFF, &hci->state))
		tasklet_hi_schedule(&hci->tx_task);
}

static inline void hcidev_stop_queue(hci_struct *hci)
{
	set_bit(HCIDEV_STATE_XOFF, &hci->state);
}

static inline void hcidev_start_queue(hci_struct *hci)
{
	clear_bit(HCIDEV_STATE_XOFF, &hci->state);
}

static inline int hcidev_queue_stopped(hci_struct *hci)
{
	return test_bit(HCIDEV_STATE_XOFF, &hci->state);
}

static inline void hcidev_attach(hci_struct *hci)
{
	if (!test_and_set_bit(HCIDEV_STATE_PRESENT, &hci->state) &&
			test_bit(HCIDEV_STATE_RUNNING, &hci->state)) {
		hcidev_wake_queue(hci);
		hcidev_state_change(hci, HCIDEV_ATTACH);
	}
}

static inline void hcidev_detach(hci_struct *hci)
{
	if (test_and_clear_bit(HCIDEV_STATE_PRESENT, &hci->state) &&
			test_bit(HCIDEV_STATE_RUNNING, &hci->state)) {
		hcidev_stop_queue(hci);
		hcidev_state_change(hci, HCIDEV_DETACH);
	}
}

static inline int hcidev_present(hci_struct *hci)
{
	return test_bit(HCIDEV_STATE_PRESENT, &hci->state);
}

static inline void hcidev_schedule(hci_struct *hci)
{
	if (!test_bit(HCIDEV_STATE_XOFF, &hci->state))
		tasklet_hi_schedule(&hci->tx_task);
}

/* Command lock object */
typedef struct {
	struct list_head	q;		/* for queueing */

	atomic_t		refcnt;		/* holders */
	struct semaphore	sema;		/* sleepers */
	hci_struct		*hci;
	__u16			opcode;		/* pending command */
} hci_lock_t;

hci_lock_t *hci_lock_create(hci_struct *hci, __u16 opcode);
void hci_lock_destroy(hci_lock_t *lock);

static inline void hci_lock_put(hci_lock_t *lock)
{
	if (atomic_dec_and_test(&lock->refcnt))
		hci_lock_destroy(lock);
}

static inline hci_lock_t *hci_lock_cmd(hci_struct *hci, __u16 opcode)
{
	hci_lock_t	*lock;

	lock = hci_lock_create(hci, opcode);
	if (lock)
		down(&lock->sema);
	return lock;
}

static inline void hci_unlock_cmd(hci_lock_t *lock)
{
	up(&lock->sema);
	hci_lock_put(lock);
}

int hci_deliver_msg(void *msg, int size);

typedef struct {
	struct module	*owner;
	char	name[AFFIX_UART_PATHLEN];
	int	manfid, prodid;
	int	proto;
	int	flags;
	int	speed;
	int	*count;
} affix_uart_t;

struct affix_uart_operations {
	struct module	*owner;
	int (*attach)(affix_uart_t *uart);
	int (*detach)(char *name);
	void (*suspend)(char *name);
	void (*resume)(char *name);
};

extern struct affix_uart_operations	affix_uart_ops;
void affix_set_uart(struct affix_uart_operations *ops);
int affix_open_uart(struct open_uart *line);
int affix_close_uart(struct open_uart *line);

/* -------------- l2cap socket ------------------- */

#define AFFIX_ERR_TIMEOUT		0xEEEE

typedef struct {
	struct list_head	q;

	struct sock	*sk;
	int		dport, sport;
} affix_sock_t;

typedef int (*affix_do_connect_t)(struct sock *sk, struct sockaddr_affix *addr);
typedef void (*affix_do_destroy_t)(struct sock *sk);
typedef void (*affix_sock_destruct_t)(struct sk_buff *skb);


int affix_register_notifier(struct notifier_block *nb);
int affix_unregister_notifier(struct notifier_block *nb);

int affix_sock_wait_for_state_sleep(struct sock *sk, int state, int nonblock, wait_queue_head_t *sleep);

static inline int affix_sock_wait_for_state(struct sock *sk, int state, int nonblock)
{
	return affix_sock_wait_for_state_sleep(sk, state, nonblock, sk->sk_sleep);
}

struct sk_buff *affix_sock_dequeue(struct sock *sk, int noblock, int *err);
int affix_sock_flush(struct sock *sk);
int affix_sock_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags);
int affix_sock_listen(struct socket *sock, int backlog);
int affix_sock_accept(struct socket *sock, struct socket *newsock, int flags);
int affix_sock_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags, affix_do_connect_t func);
void affix_sock_reset(struct sock *sk, int reason, affix_do_destroy_t func);
int affix_sock_ready(struct sock *sk);
int affix_sock_rx(struct sock *sk, struct sk_buff *skb, affix_sock_destruct_t destruct);

static inline int affix_sock_connection_based(struct sock *sk)
{
	return sk->sk_type == SOCK_SEQPACKET || sk->sk_type == SOCK_STREAM;
}

struct affix_pan_operations {
	struct module	*owner;
	int (*ioctl)(unsigned int cmd,  unsigned long arg);
};


extern struct affix_pan_operations	affix_pan_ops;
extern struct affix_pan_operations      affix_hidp_ops;
int affix_set_pan(struct affix_pan_operations *ops);
int affix_set_hidp(struct affix_pan_operations *ops);


/* from hci_mgr.h */

extern int	affix_ctl_mode;


/* Manager Section */
int hci_start_manager(void);
void hci_stop_manager(void);

/* some commands */
int hci_state_change(hci_struct *hci, int event);
int hci_connect_req(hci_con *con);
int hci_auth_req(hci_con *con);
int hci_disconnect_req(hci_con *con, __u8 reason);
int hci_updateclockoffset_req(hci_con *con, __u16 chandle);


/* Neighbour Device management */
struct btdev *btdev_lookup_bda(BD_ADDR *bda);
struct btdev *btdev_create(BD_ADDR *bda);
void __btdev_destroy(struct btdev *btdev);
void btdev_flush(void);

static inline void btdev_hold(struct btdev *btdev)
{
	atomic_inc(&btdev->refcnt);
}

static inline void btdev_put(struct btdev *btdev)
{
	if (atomic_dec_and_test(&btdev->refcnt)) {
		btl_write_lock(&btdevs);
		if (atomic_read(&btdev->refcnt) == 0)
			__btdev_destroy(btdev);
		btl_write_unlock(&btdevs);
	}
}

static inline void __btdev_put(struct btdev *btdev)
{
	if (atomic_dec_and_test(&btdev->refcnt))
		__btdev_destroy(btdev);
}


/*
 * Security block
 */
int affix_add_pin(struct PIN_Code *pin);
int affix_remove_pin(BD_ADDR *bda);
int affix_add_key(struct link_key *key);
int affix_remove_key(BD_ADDR *bda);


int hpf_connect_cfm(hci_con *con, int status);
int hpf_disconnect_ind(hci_con *con);
int hpf_recv_raw(hci_struct *hci, hci_con *con, struct sk_buff *skb);

int hpf_get_devs(int *devs);

/* SYSCTL */

/* /proc/sys/net/affix */
enum
{	
	/* HCI */
	NET_AFFIX_HCI_SNIFF = 0x01,
	NET_AFFIX_HCI_USE_INQUIRY,
	NET_AFFIX_HCI_MAX_ATTEMPT,
	NET_AFFIX_HCI_REQ_COUNT,
	NET_AFFIX_HCI_REQ_TIMEOUT,
	NET_AFFIX_HCI_DEFER_DISC_TIMEOUT,
	/* L2CAP */
	NET_AFFIX_L2CAP_MTU = 0x10,
	NET_AFFIX_L2CAP_CL_MTU,
	NET_AFFIX_L2CAP_FLUSH_TO,
#ifdef CONFIG_AFFIX_BT_1_2
	NET_AFFIX_L2CAP_MPS,
	NET_AFFIX_L2CAP_SUPPORTED_MODES,
	NET_AFFIX_L2CAP_TXWINDOW,
	NET_AFFIX_L2CAP_MAX_TRANSMIT,
	NET_AFFIX_L2CAP_RET_TIMEOUT,
	NET_AFFIX_L2CAP_MON_TIMEOUT,
#endif
	/* RFCOMM */
	NET_AFFIX_RFCOMM_MTU = 0x20,
	NET_AFFIX_RFCOMM_WMEM,
	NET_AFFIX_BTY_WMEM,
	NET_AFFIX_RFCOMM_RMEM,
	NET_AFFIX_BTY_RMEM,
	/* PAN */
	NET_AFFIX_PAN_MTU = 0x30
};

/* CTL_NET names: */
enum
{
	NET_AFFIX = PF_AFFIX,	// random
	NET_AFFIX_CORE,
	NET_AFFIX_PAN,
	NET_AFFIX_RFCOMM		
};

static inline char *hci_pkttype(int pkt_type)
{
	switch (pkt_type) {
		case HCI_ACL:
			return "ACL";
		case HCI_SCO:
			return "SCO";
		case HCI_COMMAND:
			return "CMD";
		case HCI_EVENT:
			return "EVENT";
		default:
			return "unknown";
	}
}

static inline int hci_pktlen(int pkt_type, unsigned char *data)
{
	switch (pkt_type) {
		case HCI_ACL:
			return HCI_ACL_HDR_LEN + (data ? __btoh16(((hci_acl_hdr_t*)data)->Length) : 0);
		case HCI_SCO:
			return HCI_SCO_HDR_LEN + (data ? __btoh16(((hci_sco_hdr_t*)data)->Length) : 0);
		case HCI_EVENT:
			return HCI_EVENT_HDR_LEN + (data ? ((hci_event_hdr_t*)data)->Length : 0);
		default:
			return 0;
	}
}
#endif
