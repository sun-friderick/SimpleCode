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
   $Id: rfcomm.h,v 1.72 2004/05/25 16:02:18 kassatki Exp $

   RFCOMM - RFCOMM protocol for Bluetooth

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

#ifndef _RFCOMM_H
#define _RFCOMM_H

#include <affix/bluetooth.h>
#include <affix/l2cap.h>

#if !defined(__LITTLE_ENDIAN_BITFIELD) && !defined(__BIG_ENDIAN_BITFIELD)
#error "Bitfield endianness not defined! Check your byteorder.h"
#endif


#define RFCOMM_SKB_RESERVE		(L2CAP_SKB_RESERVE + RFCOMM_LONG_HDR_SIZE + RFCOMM_CREDIT_SIZE)

#define RFCOMM_T1_TIMEOUT		(20*HZ)
#define RFCOMM_T2_TIMEOUT		(20*HZ)
#define RFCOMM_DEFER_DISC_TIMEOUT	(2*HZ)
#define RFCOMM_CONREQ_TIMEOUT		(2*HZ)
#define RFCOMM_CFGREQ_TIMEOUT		(1*HZ)
#define RFCOMM_CONRSP_TIMEOUT		(60*HZ)	/* pin code requires user interaction */
#define RFCOMM_DISCRSP_TIMEOUT		(2*HZ)


/****************** RFCOMM protocol data types *****************/

#define RFCOMM_STATUS_SUCCESS		0
#define RFCOMM_STATUS_FAILURE		1

#define RFCOMM_SHORT_PAYLOAD_SIZE 	0x7F
#define RFCOMM_MAX_PAYLOAD_SIZE		0x7FFF
#define RFCOMM_DEFAULT_MTU		0x7F
#define RFCOMM_GUESS_MTU		1685

#define RFCOMM_SCH(dlci)		(dlci >> 1)
#define RFCOMM_DLCI(channel)		(channel << 1)

#define RFCOMM_UIH_CRC_CHECK		2
#define RFCOMM_CTRL_CRC_CHECK		3

#define RFCOMM_SHORT_HDR_SIZE		sizeof(rfcomm_hdr_t)
#define RFCOMM_LONG_HDR_SIZE		sizeof(rfcomm_long_hdr_t)

#define RFCOMM_FCS_SIZE			1
#define RFCOMM_CREDIT_SIZE		1

#define RFCOMM_MCC_CMD			1
#define RFCOMM_MCC_RSP			0

/* The values in the control field - frame types */
#define RFCOMM_SABM		0x2f
#define RFCOMM_UA		0x63
#define RFCOMM_DM		0x0f
#define RFCOMM_DISC		0x43
#define RFCOMM_UIH		0xef

#define RFCOMM_CTRL_SIZE	4

/* The values in the type field in a multiplexer command packet */
#define RFCOMM_TEST		0x08
#define RFCOMM_FCON		0x28
#define RFCOMM_FCOFF		0x18
#define RFCOMM_MSC		0x38
#define RFCOMM_RPN		0x24
#define RFCOMM_RLS		0x14
#define RFCOMM_PN		0x20
#define RFCOMM_NSC		0x04

// MSC
#define RFCOMM_RTC		0x01
#define RFCOMM_RTR		0x02
#define RFCOMM_IC		0x10
#define RFCOMM_DV		0x20
#define RFCOMM_BREAK		0x01
#define RFCOMM_BREAK_LENGTH	0xF8

/* control events */
#define RFCOMM_LINE_STATUS	0x01
#define RFCOMM_MODEM_STATUS	0x02
#define RFCOMM_BREAK_SIGNAL	0x03
#define	RFCOMM_PORT_PARAM	0x04
#define RFCOMM_WRITE_SPACE	0x05
#define RFCOMM_TX_WAKEUP	0x06
#define RFCOMM_MODEM_STATUS_BRK	0x07
#define RFCOMM_SHUTDOWN		0x08
#define RFCOMM_ESTABLISHED	0x09


/*  parameters of DTELINE_STATE  --  MCR */
#define BTY_MCR_DTR		RFCOMM_RTC
#define BTY_MCR_RTS		RFCOMM_RTR
#define BTY_MCR_DCD		RFCOMM_DV
#define BTY_MCR_RI		RFCOMM_IC

/*  parameters of DCELINE_STATE  --  MSR */
#define BTY_MSR_CTS		RFCOMM_RTR
#define BTY_MSR_DSR		RFCOMM_RTC
#define BTY_MSR_RI		RFCOMM_IC
#define BTY_MSR_DCD		RFCOMM_DV

/*  parameters of LINE_STATUS */

#define BTY_LSR_BI		0x01    /* Break interrupt indicator */
#define BTY_LSR_OE		0x02    /* Overrun error indicator */
#define BTY_LSR_PE		0x04    /* Parity error indicator */
#define BTY_LSR_FE		0x08    /* Frame error indicator */

/* ----------------- Data types ------------------ */

typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 		ea:1;
	__u8 		cr:1;
	__u8		dlci:6;
#else
	__u8		dlci:6;
	__u8		cr:1;
	__u8		ea:1;
#endif
	__u8 		control;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		lea:1;
	__u8		len:7;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		len:7;
	__u8		lea:1;
#endif
	__u8 		data[0];		/* if no data then one byte of FCS */
}__PACK__ rfcomm_hdr_t;

typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 		ea:1;
	__u8 		cr:1;
	__u8		dlci:6;
#else
	__u8		dlci:6;
	__u8		cr:1;
	__u8		ea:1;
#endif
	__u8 		control;
	__u16		len;
	__u8		data[0];
}__PACK__ rfcomm_long_hdr_t;


typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		ea:1;
	__u8		cr:1;
	__u8		type:6;
	__u8		lea:1;
	__u8		len:7;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		type:6;
	__u8		cr:1;
	__u8		ea:1;
	__u8		len:7;
	__u8		lea:1;
#endif
	__u8		data[0];
}__PACK__ rfcomm_cmd_hdr_t;


/* Multiplexer Control Channel Commands */

/* MSC-command */

struct rfcomm_modem {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	ea:1;
	__u8	fc:1;
	__u8	mr:6;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	mr:6;
	__u8	fc:1;
	__u8	ea:1;
#endif
}__PACK__;

struct rfcomm_break {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	ea:1;
	__u8	b:3;
	__u8	len:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	len:4;
	__u8	b:3;
	__u8	ea:1;
#endif
}__PACK__;

struct rfcomm_msc {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 		ea:1;
	__u8 		cr:1;
	__u8		dlci:6;
#else
	__u8		dlci:6;
	__u8		cr:1;
	__u8		ea:1;
#endif
	struct rfcomm_modem 	sig;
	__u8			brk[0];
}__PACK__;

struct rfcomm_msc_full {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 		ea:1;
	__u8 		cr:1;
	__u8		dlci:6;
#else
	__u8		dlci:6;
	__u8		cr:1;
	__u8		ea:1;
#endif
	struct rfcomm_modem 	sig;
	struct rfcomm_break 	brk;
}__PACK__;


/* RPN command */

#define RFCOMM_MASK_BIT_RATE	0x0001
#define RFCOMM_MASK_DATA_BITS	0x0002
#define RFCOMM_MASK_STOP_BIT	0x0004
#define RFCOMM_MASK_PARITY	0x0008
#define RFCOMM_MASK_PARITY_TYPE	0x0010
#define RFCOMM_MASK_XON_CHAR	0x0020
#define RFCOMM_MASK_XOFF_CHAR	0x0040
#define RFCOMM_MASK_XON_INPUT	0x0100
#define RFCOMM_MASK_XON_OUTPUT	0x0200
#define RFCOMM_MASK_RTR_INPUT	0x0400
#define RFCOMM_MASK_RTR_OUTPUT	0x0800
#define RFCOMM_MASK_RTC_INPUT	0x1000
#define RFCOMM_MASK_RTC_OUTPUT	0x2000

#define RFCOMM_PARAM_MASK	(RFCOMM_MASK_BIT_RATE | RFCOMM_MASK_DATA_BITS | RFCOMM_MASK_STOP_BIT | \
		RFCOMM_MASK_PARITY | RFCOMM_MASK_PARITY_TYPE | \
		RFCOMM_MASK_RTR_INPUT | RFCOMM_MASK_RTR_OUTPUT | RFCOMM_MASK_RTC_INPUT | RFCOMM_MASK_RTC_OUTPUT)


// bit rate
#define RFCOMM_2400		0x00
#define RFCOMM_4800		0x01
#define RFCOMM_7200		0x02
#define RFCOMM_9600		0x03
#define RFCOMM_19200		0x04
#define RFCOMM_38400		0x05
#define RFCOMM_57600		0x06
#define RFCOMM_115200		0x07
#define RFCOMM_230400		0x08

// data format
#define RFCOMM_CS		0x03
#define RFCOMM_CS5		0x00
#define RFCOMM_CS6		0x01
#define RFCOMM_CS7		0x02
#define RFCOMM_CS8		0x03

// stop bits
#define RFCOMM_STOP		0x04
#define RFCOMM_1STOP		0x00
#define RFCOMM_15STOP		0x04
// parity
#define RCOMMM_PARITY		0x08
#define RFCOMM_PARDIS		0x00
#define RFCOMM_PARENB		0x08
// parity type
#define RFCOMM_PARITY_TYPE	0x30
#define RFCOMM_PARODD		0x00
#define RFCOMM_PAREVEN		0x10
#define RFCOMM_PARMARK		0x20
#define RFCOMM_PARSPC		0x30
// flow control
#define RFCOMM_XON_INPUT	0x01
#define RFCOMM_XON_OUTPUT	0x02
#define RFCOMM_RTR_INPUT	0x04
#define RFCOMM_RTR_OUTPUT	0x08
#define RFCOMM_RTC_INPUT	0x10
#define RFCOMM_RTC_OUTPUT	0x20

struct rfcomm_port_param {
	__u8	bit_rate;
	__u8	format;
	__u8	fc;
	__u8	xon_char;
	__u8	xoff_char;
	__u16	mask;
}__PACK__;


struct rfcomm_rpn {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 		ea:1;
	__u8 		cr:1;
	__u8		dlci:6;
#else
	__u8		dlci:6;
	__u8		cr:1;
	__u8		ea:1;
#endif
	struct rfcomm_port_param	param;
}__PACK__;

/* RLS-command */  
struct rfcomm_rls {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8 	ea:1;
	__u8 	cr:1;
	__u8	dlci:6;
	__u8	line_status:4;
	__u8	res:4;
#else
	__u8	dlci:6;
	__u8	cr:1;
	__u8	ea:1;
	__u8	res:4;
	__u8	line_status:4;
#endif
}__PACK__;

/* PN-command */
struct rfcomm_pn {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	dlci:6;			/* DLCI */
	__u8	res1:2;
	__u8	unused_frame_type:4;
	__u8	flow:4;			/* Credit based flow control */
	__u8	priority:6;			/* XXX set this */
	__u8	res2:2;
	__u8	unused_ack_timer;
	__u16	frame_size;			/* payload MTU */
	__u8	unused_max_num_retrans;
	__u8	credit:3;			/* number of credits */
	__u8	res3:5;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	res1:2;
	__u8	dlci:6;
	__u8	flow:4;
	__u8	unused_frame_type:4;
	__u8	res2:2;
	__u8	priority:6;
	__u8	unused_ack_timer;
	__u16	frame_size;			/* payload MTU */
	__u8	unused_max_num_retrans;
	__u8	res3:5;			/* number of credits */
	__u8	credit:3;
#endif

}__PACK__;

/* NSC-command */
struct rfcomm_nsc {
	__u8	cmd_type;
}__PACK__;

/****************** LOCAL FUNCTION DECLARATION SECTION **********************/

typedef struct _rfcomm_con rfcomm_con;

typedef struct {
	int (*data_ind)(rfcomm_con *con, struct sk_buff *skb);
	int (*connect_ind)(rfcomm_con *con);
	int (*connect_cfm)(rfcomm_con *con, int status);
	int (*disconnect_ind)(rfcomm_con *con);
	int (*control_ind)(rfcomm_con *con, int type);
} rfcomm_proto_ops;

typedef struct {
	struct list_head	q;
	__u8			channel;
	rfcomm_proto_ops	*ops;
} rfcomm_proto;


#define RFCOMM_FLAGS_INITIATOR		0
#define RFCOMM_FLAGS_CFC		1
#define RFCOMM_FLAGS_RX_THROTTLED	2
#define RFCOMM_FLAGS_TX_THROTTLED	3
#define RFCOMM_FLAGS_SABM_RECV		5

typedef struct {
	struct list_head	list;

	atomic_t		refcnt;
	con_state		state;
	unsigned long		flags;

	struct timer_list	timer;

	BD_ADDR			bda;
	l2cap_ch		*ch;

	btlist_head_t		cons;		/* virtual cons */

	/* flow control members */
	__u8			peer_fc;	/* agregatible fc */
	__u8			fc;

	/* credit based flow control */
	int			rx_credit;	/* Initial */
	int			tx_credit;	/* Initial */
} rfcomm_sn;


struct _rfcomm_con {
	struct list_head	list;	/* for queueing purpose */

	atomic_t			refcnt;
	spinlock_t			callback_lock;
	int				callback_cpu;
	rfcomm_proto_ops		*ops;
	con_state			state;
	unsigned long			flags;
	int				security;

	void				*priv;			/* "sock" structure */
	
	rfcomm_sn			*sn;			/* session */

	struct timer_list		timer;

	hci_struct			*hci;
	BD_ADDR				bda;
	__u8				dlci;

	/* credit based flow control stuff*/
	int				power;
	int				rx_credit_actual;	/* local */
	atomic_t			rx_credit;		/* I can receive */
	atomic_t			tx_credit;		/* I can send */

	/* parametes */
	__u16				frame_size;
	__u8				priority;

	/* port parameters */
	struct rfcomm_port_param	peer_param;
	struct rfcomm_port_param	param;

	/* modem status */
	struct rfcomm_modem		peer_modem;		/* like MSR */
	struct rfcomm_modem		modem;			/* like MCR */
	struct rfcomm_break		peer_break_signal;
	struct rfcomm_break		break_signal;

	/* line status */
	__u8				peer_line_status;
	__u8				line_status;

};

/* Function Declarations */
int rfcomm_register_protocol(__u16 *channel, rfcomm_proto_ops *ops, void *param);
void rfcomm_unregister_protocol(__u16 channel);
int __rfcomm_destroy(rfcomm_sn *sn);

void rfcon_timer(unsigned long data);
rfcomm_con *rfcon_create(BD_ADDR *bda, __u8 dlci, rfcomm_proto_ops *ops);
void __rfcon_destroy(rfcomm_con *con);
void rfcon_put(rfcomm_con *con);
int rfcon_connect_req(rfcomm_con *con);
int rfcon_connect_rsp(rfcomm_con *con, int status);
int rfcon_disconnect_req(rfcomm_con *con);

void rfcon_set_rxfc(rfcomm_con *con, int flow);
void rfcon_set_rxspace(rfcomm_con *con, int size);
void rfcon_init_cfc(rfcomm_con *con);
void rfcon_update_txfc(rfcomm_con *con);
void rfcon_set_mcr(rfcomm_con *con, __u8 mcr);
void rfcon_set_param(rfcomm_con *con, struct rfcomm_port_param *p);
void rfcon_set_break(rfcomm_con *con, __u8 b);
void rfcomm_put(rfcomm_sn *sn);
int rfcomm_send_data(rfcomm_con *con, struct sk_buff *skb);

static inline void rfcon_hold(rfcomm_con *con)
{
	atomic_inc(&con->refcnt);
}

static inline void __rfcon_put(rfcomm_con *con)
{
	if (atomic_dec_and_test(&con->refcnt))
		__rfcon_destroy(con);
}

static inline void rfcon_graft(rfcomm_con *con, void *parent)
{
	if (con->callback_cpu != smp_processor_id())
		spin_lock_bh(&con->callback_lock);
	con->priv = parent;
	if (con->callback_cpu != smp_processor_id())
		spin_unlock_bh(&con->callback_lock);
}

static inline void rfcon_orphan(rfcomm_con *con)
{
	if (con->callback_cpu != smp_processor_id())
		spin_lock_bh(&con->callback_lock);
	con->priv = NULL;
	if (con->callback_cpu != smp_processor_id())
		spin_unlock_bh(&con->callback_lock);
}

static inline void rfcon_start_timer(rfcomm_con *con, unsigned long timeout)
{
	if (!del_timer(&con->timer))
		rfcon_hold(con);
	mod_timer(&con->timer, jiffies+timeout);
}

static inline void rfcon_stop_timer(rfcomm_con *con)
{
	if (del_timer(&con->timer))
		rfcon_put(con);
}

static inline void rfcomm_hold(rfcomm_sn *sn)
{
	atomic_inc(&sn->refcnt);
}

static inline void rfcomm_start_timer(rfcomm_sn *sn, unsigned long timeout)
{
	if (!del_timer(&sn->timer))
		rfcomm_hold(sn);
	mod_timer(&sn->timer, jiffies+timeout);
}

static inline void rfcomm_stop_timer(rfcomm_sn *sn)
{
	if (del_timer(&sn->timer))
		rfcomm_put(sn);
}

static inline int rfcomm_connected(rfcomm_sn *sn)
{
	if (STATE(sn) == CON_OPEN)
		return 1;
	return 0;
}

static inline int rfcon_ready(rfcomm_con *con)
{
	if (STATE(con) == CON_OPEN)
		return 1;

	return 0;
}

static inline int rfcon_get_txfc(rfcomm_con *con)
{
	return !test_bit(RFCOMM_FLAGS_TX_THROTTLED, &con->flags);
}

static inline __u8 rfcon_get_msr(rfcomm_con *con)
{
	return con->peer_modem.mr;
}

static inline __u8 rfcon_get_mcr(rfcomm_con *con)
{
	return con->modem.mr;
}


static inline int rfcon_disconnect_pend(rfcomm_con *con)
{
	return (STATE(con) == CON_W4_DISCRSP);
}

static inline __u8 rfcon_get_line_status(rfcomm_con *con)
{
	return con->peer_line_status;
}

static inline __u16 rfcon_getmtu(rfcomm_con *con)
{
	int	mtu;

	mtu = btmin(con->frame_size, l2ca_get_mtu(con->sn->ch) - RFCOMM_LONG_HDR_SIZE - RFCOMM_FCS_SIZE);
	if (test_bit(RFCOMM_FLAGS_CFC, &con->flags))
		mtu--;
	return (mtu < 0) ? 0 : mtu;
}

static inline void rfcon_bind_session(rfcomm_con *con, rfcomm_sn *sn)
{
	if (con->sn)
		rfcomm_put(con->sn);
	rfcomm_hold(sn);
	con->sn = sn;
	btl_add_tail(&sn->cons, con);
}

static inline void rfcon_setframesize(rfcomm_con *con, int size)
{
	int	n;

	con->frame_size = size;
	for (n=0; size !=0; size>>=1,n++);
	con->power = n;
}


/* ----------- RFCOMM socket ************ */

typedef struct {
	affix_sock_t		base;

	rfcomm_con		*con;
	int			security;	/* for authentication purpose */

#define RFCOMM_XMIT_WAKEUP	1
#define RFCOMM_XMIT_BUSY	2
	unsigned long		xmit_flags;
	struct tasklet_struct	tx_task;

	// BTY
	struct rfcomm_port	port;
	void			*bty;
} rfcomm_sock_t;

static inline rfcomm_sock_t *rpf_get(struct sock *sk)
{
	return (rfcomm_sock_t*)sk->sk_prot;
}

struct rfcomm_skb_cb {
	int	size;
};

#define rfcomm_cb(skb)	((struct rfcomm_skb_cb*)skb->cb)

static inline int rfcomm_skb_tailroom(struct sk_buff *skb)
{
	int	mtu =  rfcomm_cb(skb)->size - skb->len;
	return (mtu < 0) ? 0 : mtu;
}


#endif
