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
   $Id: l2cap.h,v 1.94 2004/07/16 18:58:52 chineape Exp $

   Link Layer Control and Adaptation Protocol

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#ifndef L2CAP_H
#define L2CAP_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/module.h>

#include <affix/bluetooth.h>
#include <affix/hci.h>


/* L2CAP definition */

#define	L2CAP_HDR_LEN		4

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
#define	L2CAP_GROUP_HDR_LEN	6
#define L2CAP_GROUP_SKB_RESERVE	(HCI_SKB_RESERVE + L2CAP_GROUP_HDR_LEN)
#endif

#ifdef CONFIG_AFFIX_BT_1_2

#define L2CAP_SKB_RESERVE	(HCI_SKB_RESERVE + 8)

#else

#define L2CAP_SKB_RESERVE	(HCI_SKB_RESERVE + L2CAP_HDR_LEN)

#endif

//#define L2CAP_RTX_TIMEOUT		(30*HZ)
//#define L2CAP_L2CAP_ERTX_TIMEOUT	(30*HZ)
#define L2CAP_RTX_TIMEOUT		(60*HZ)
#define L2CAP_RTX_TIMEOUT_DISC		(2*HZ)
#define L2CAP_ERTX_TIMEOUT		(300*HZ)

#define L2CAP_MTU			672
#define L2CAP_MIN_SIGMTU		48

/* Signalling channel commands' id's */

#define L2CAP_SIG_RESERVED		0x00
#define L2CAP_SIG_REJECT		0x01
#define L2CAP_SIG_CONREQ		0x02
#define L2CAP_SIG_CONRSP		0x03
#define L2CAP_SIG_CFGREQ		0x04
#define L2CAP_SIG_CFGRSP		0x05
#define L2CAP_SIG_DISCREQ		0x06
#define L2CAP_SIG_DISCRSP		0x07
#define L2CAP_SIG_ECHOREQ		0x08
#define L2CAP_SIG_ECHORSP		0x09
#define L2CAP_SIG_INFOREQ		0x0A
#define L2CAP_SIG_INFORSP		0x0B

/* some proprietary Affix commands */
#define L2CAP_SIG_SINGLEPING		0x80
#define L2CAP_SIG_DATA			0x81

/* Channels */

#define L2CAP_CID_NULL			0x0000
#define L2CAP_CID_SIGNAL		0x0001
#define L2CAP_CID_CONLESS		0x0002
#define L2CAP_CID_GROUP			L2CAP_CID_CONLESS
#define L2CAP_CID_BASE			0x0040

#ifdef CONFIG_AFFIX_BT_1_2

#if 0
#define __L2CAP_TEST_TIMERS__
#endif

#define L2CAP_AFFIX_MIN_RETRANSMISSION_TIMEOUT	400
#define L2CAP_AFFIX_MIN_MONITOR_TIMEOUT	100

#define L2CAP_RET_TIMEOUT		1000
#define L2CAP_MONITOR_TIMEOUT		1000

#define L2CAP_MAX_WINDOW_SIZE		32
#define L2CAP_MAX_MPS			0xFFFF  
#define L2CAP_INFO_EXT_FEATURES		0x0002
#define L2CAP_LOCAL_EXT_FEATURES_MASK   0x00000007 /* FLOWCONTROL AND RETRANSMISSION MODE ENABLED */
#if 0
/* NOTE: All this flags has the QoS support enabled (last bit on) */
#define L2CAP_LOCAL_EXT_FEATURES_MASK   0x00000005 /* Only FLOWCONTROL MODE ENABLED */
#define L2CAP_LOCAL_EXT_FEATURES_MASK	0x00000006 /* Only RETRANSMISSION MODE ENABLED */
#define L2CAP_LOCAL_EXT_FEATURES_MASK	0x00000004 /* Only BASIC MODE supported*/
#endif
#define L2CAP_EXT_FEATURE_FC		0
#define L2CAP_EXT_FEATURE_RET		1
#define L2CAP_EXT_FEATURE_QOS		2

/* Menaning of bits in ch->rfc_flags */
#define L2CAP_LOCAL_RDB			0	/* Local position of the Retransmission Diasable Bit in rfc_flags */
#define L2CAP_LOCAL_REJ_CONDITION	1

#define L2CAP_IFRAME_HDR_LEN		6
#define L2CAP_SFRAME_HDR_LEN		8
#define L2CAP_FRAME_MIN_LEN		8
#define L2CAP_FCS_LEN			2
#define L2CAP_RFC_OPTION_LEN		9

/* L2CAP Modes */
#define L2CAP_BASIC_MODE		0x00
#define L2CAP_RETRANSMISSION_MODE	0X01
#define L2CAP_FLOW_CONTROL_MODE		0X02

/* Control field */
#define L2CAP_SAR_UNSEGMENTED		0x0000
#define L2CAP_SAR_START			0x4000
#define L2CAP_SAR_END			0x8000
#define L2CAP_SAR_CONTINUATION		0xC000

#define L2CAP_S_RR			0x0000
#define	L2CAP_S_REJ			0x0004

#define L2CAP_SAR_MASK			0xC000
#define L2CAP_REQSEQ_MASK		0x3F00
#define L2CAP_RETRANSMISSION_BIT_MASK	0x0080
#define L2CAP_TXSEQ_MASK		0x007E
#define L2CAP_S_MASK			0x000C

#define L2CAP_CLEAR_REQSEQ		0xC0FF
#define L2CAP_CLEAR_TXSEQ		0xFF81

#define L2CAP_SET_RET_DISABLE_BIT	0x0080
#define L2CAP_UNSET_RET_DISABLE_BIT	0xFF7F

#define SAR(control)		(L2CAP_SAR_MASK & control)
#define SUPERVISORY(control)	(L2CAP_S_MASK & control)
#define IFRAMETYPE(control)	((L2CAP_IFRAME_TYPE_MASK & control) == L2CAP_IFRAME_TYPE)
#define SFRAMETYPE(control)	((L2CAP_SFRAME_TYPE_MASK & control) == L2CAP_SFRAME_TYPE)
#if 0
#define LOCAL_MODE(ch)		(ch->cfgin.rfc.mode)
#define REMOTE_MODE(ch)		(ch->cfgout.rfc.mode)
#endif

#define LOCAL_MODE(ch)		(ch->cfgout.rfc.mode)
#define REMOTE_MODE(ch)		(ch->cfgin.rfc.mode)


#define L2CAP_FLOW_CONTROL_ON		0x01
#define L2CAP_FLOW_CONTROL_OFF		0xFE

#define L2CAP_LOCAL_RET_ON		0x02
#define L2CAP_LOCAL_RET_OFF		0xFD

#define L2CAP_IFRAME_TYPE_MASK		0X0001
#define L2CAP_SFRAME_TYPE_MASK		0x0003

#define L2CAP_IFRAME_TYPE		0x0000
#define L2CAP_SFRAME_TYPE		0x0001

#define L2CAP_MAX_TX_WINDOW		0x20
#define L2CAP_MIN_TX_WINDOW		0X01

#endif     


/* l2cap protocol header */
/* Connection-oriented header in basic L2CAP mode */ 
typedef struct {
	__u16	length;
	__u16	cid;
	__u8	data[0];
} __PACK__ l2cap_hdr_t;

#ifdef CONFIG_AFFIX_BT_1_2

/* Connection-oriented header in retransmission/flow control modes */
/* Supervisory frame (S-frame)*/
typedef struct {
	__u16	length;
	__u16	cid;
	__u16   control;
	__u16   fcs;
}__PACK__ l2cap_sframe_hdr_t;

/* Information frame (I-frame)*/
/* NOTES: L2CAP SDU length only present if SAR = 01 (16 bits)
 * 	  FCS at the end of the information payload (16 bits) */
typedef struct {
	__u16	length;
	__u16	cid;
	__u16	control;
	__u8 data[0];	//[L2CAP SDU length when SAR=01] + Information payload + FCS.
}__PACK__ l2cap_iframe_hdr_t;

#endif

/* Connection less header */
/* Connectionless data channel in basic L2CAP mode */
typedef struct {
	__u16	length;
	__u16	cid;
	__u16	psm;
	__u8	data[0];
} __PACK__ l2cap_grp_hdr_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u8	data[0];
} __PACK__ l2cap_cmd_t;


typedef struct {
	__u8	code;
	__u8	id;
	__u16	length; 
	__u16	reason;
#define	L2CAP_CMDREJ_NOTSUP		0x0000
#define	L2CAP_CMDREJ_MTUEXCEED		0x0001
#define	L2CAP_CMDREJ_INVCID		0x0002
	__u8	data[0];
} __PACK__ l2cap_cmdrej_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	psm;
	__u16	scid;
} __PACK__ l2cap_conreq_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	dcid;
	__u16	scid;
	__u16	result;
#define L2CAP_CONRSP_SUCCESS		0x0000
#define L2CAP_CONRSP_PENDING		0x0001
#define L2CAP_CONRSP_PSM		0x0002
#define L2CAP_CONRSP_SECURITY		0x0003
#define L2CAP_CONRSP_RESOURCE		0x0004
	__u16	status;
#define	L2CAP_CONRSP_NOINFO		0x0000
#define	L2CAP_CONRSP_AUTHEN		0x0001
#define	L2CAP_CONRSP_AUTHOR		0x0002
} __PACK__ l2cap_conrsp_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	dcid;
	__u16	flags;
#define L2CAP_CFGREQ_MORE		0x0001
#define L2CAP_CFGREQ_INTERNAL		0x8000		/* auto response */
	__u8	options[0];
} __PACK__ l2cap_cfgreq_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	scid;
	__u16	flags;
#define L2CAP_CFGRSP_MORE		0x0001
#define L2CAP_CFGRSP_INTERNAL		0x8000		/* auto response */
	__u16	result;
#ifdef CONFIG_AFFIX_BT_1_2
#define	L2CAP_CFGRSP_SUCCESS		0x0000
#define L2CAP_CFGRSP_PARAMETERS		0x0001
#define L2CAP_CFGRSP_REJECT		0x0002
#define L2CAP_CFGRSP_UNKNOWN_OPT	0x0003
#else
#define	L2CAP_CFGRSP_SUCCESS		0x0000
#define L2CAP_CFGRSP_PARAMETERS		0x0001
#define L2CAP_CFGRSP_REJECT		0x0002
#define L2CAP_CFGRSP_INVALID_CID	0x0003
#define L2CAP_CFGRSP_UNKNOWN_OPT	0x0004
#endif
	__u8	options[0];
} __PACK__ l2cap_cfgrsp_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	dcid;
	__u16	scid;
} __PACK__ l2cap_discreq_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	dcid;
	__u16	scid;
} __PACK__ l2cap_discrsp_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	type;
#define L2CAP_INFO_CL_MTU		0x0001
} __PACK__ l2cap_inforeq_t;

typedef struct {
	__u8	code;
	__u8	id;
	__u16	length;
	__u16	type;
	__u16	result;
#define L2CAP_INFORSP_SUCCESS		0x0000
#define L2CAP_INFORSP_NOTSUPPORTED	0x0001
	__u8	data[0];
/* here is MTU - 2 bytes */
} __PACK__ l2cap_inforsp_t;


/* Configuration parameter options */
typedef struct {
	__u8	type;
#define L2CAP_CFGOPT_MTU		0x01
#define L2CAP_CFGOPT_FLUSHTO		0x02
#define L2CAP_CFGOPT_QOS		0x03
#define L2CAP_CFGOPT_LINKTO		0x04
#define	L2CAP_CFGOPT_SKIP		0x80
#define L2CAP_CFGOPT_MTU_SKIP		0x81
#define L2CAP_CFGOPT_FLUSHTO_SKIP	0x82
#define L2CAP_CFGOPT_QOS_SKIP		0x83
#define L2CAP_CFGOPT_LINKTO_SKIP	0x84
#define L2CAP_CFGOPT_RFC		0x04
#define L2CAP_CFGOPT_RFC_SKIP		0x84
	__u8	length;
	__u8	data[0];
} __PACK__ l2cap_cfgopt_t;

#ifdef CONFIG_AFFIX_BT_1_2
/* Retransmission and Flow Control options */
typedef struct {
	__u8	mode;
	__u8	txwindow_size;
	__u8	max_transmit;
	__u16	retransmission_timeout;
	__u16	monitor_timeout;
	__u16	mps;
}__PACK__ __l2cap_rfc_t;

#endif

/* QoS options */
typedef struct {
	__u8	flags;
	__u8	service_type;
#define L2CAP_QOS_NO_TRAFFIC		0x00
#define L2CAP_QOS_BEST_EFFORT		0x01
#define L2CAP_QOS_GUARANTEED		0x02
	__u32	token_rate;
	__u32	token_size;
	__u32	bandwidth;
	__u32	latency;
	__u32	delay_variation;
} __PACK__ __l2cap_qos_t;


/**************************************************************************************/

#if CONFIG_AFFIX_BT_1_2
typedef struct {
	unsigned long	flags;	/* what should be negotiated */
	__u16		mtu;
	__u16		flush_to;
	l2cap_qos_t	qos;
	l2cap_rfc_t	rfc;	/* Retransmission and Flow control options */
	__u16		link_to;
} l2cap_cfg_opt;
#else
typedef struct {
	unsigned long	flags;	/* what should be negotiated */
	__u16		mtu;
	__u16		flush_to;
	l2cap_qos_t	qos;
	__u16		link_to;
} l2cap_cfg_opt;
#endif

typedef struct l2cap_ch  l2cap_ch;

typedef struct {
	struct	module	*owner;
	int (*data_ind)(l2cap_ch *ch, struct sk_buff *skb);
	int (*connect_ind)(l2cap_ch *ch);
	int (*connect_cfm)(l2cap_ch *ch, int result, int status);
	int (*config_ind)(l2cap_ch *ch);
	int (*config_cfm)(l2cap_ch *ch, int result);
	int (*disconnect_ind)(l2cap_ch *ch);
	int (*disconnect_cfm)(l2cap_ch *ch);
	int (*control_ind)(l2cap_ch *ch, int event, void *arg);	// any event notifer
} l2cap_proto_ops;

#define L2CAP_EVENT_TIMEOUT		0x01
#define L2CAP_EVENT_QOS_VIOLATION	0x02
#define L2CAP_EVENT_PING		0x03

typedef struct {
	struct list_head	q;
	__u16			psm;
	l2cap_proto_ops		*ops;
	int			disabled;
} l2cap_proto;

/*
   l2cap_ch describe the L2CAP channel
*/
struct l2cap_ch {
	struct list_head	q;		/* for queueing */
	
	atomic_t		refcnt;		/* number of users */
	spinlock_t		callback_lock;
	int			callback_cpu;
	con_state		state;		/* State of the channel	*/
	int			security;

	void			*priv;		/* Private field for upper layer */

	/* Upper protocol stuff */
	BD_ADDR			bda;
	__u16			psm;
	l2cap_proto_ops		*ops;

	/* L2CAP Protocol stuff */
	__u16			lcid;		/* local CID		*/
	__u16			rcid;		/* remote CID		*/

	/* request management */
	struct timer_list	timer;
	__u8			rspid;		/* command response id	*/

	/* Configuration stuff */
	atomic_t		cfgreq_count;	/* Request path	*/
	l2cap_cfg_opt		cfgout;		/* to remote device */

	//atomic_t		cfgrsp_fail_count;	/* Response path */
	l2cap_cfg_opt		cfgin;		/* from remote device */

	unsigned long		flags;		/* configuration flags 	*/
#define L2CAP_FLAGS_SERVER		0
#define L2CAP_FLAGS_CONFIG		1
#define L2CAP_FLAGS_CFGIN_DONE		2
#define L2CAP_FLAGS_CFGOUT_DONE		3
#define L2CAP_FLAGS_INFOREQ_DONE	4
	
	/* HCI related */
	hci_struct		*hci;		/* device handle */
	hci_con			*con;		/* contains BD_ADDR */
	
	struct sk_buff_head	req_queue;
	//struct sk_buff_head	write_queue;

#ifdef CONFIG_AFFIX_BT_1_2
	/* Transmitting buffer */
	struct sk_buff_head 	tx_buffer;
	struct sk_buff 		*next_tx_frame;		 /* Points to the sk_buff(L2CAP packet) to be transmited next. */
	__u16			expected_ack_seq;
	__u16			next_tx_seq;	
	/* Receiving buffer */
	struct sk_buff_head 	rx_buffer;
	__u16			buffer_seq;	/* Points to the first sk_buff (L2CAP packet) waiting to be pull out from the upper layer */
	__u16			expected_tx_seq;/* Sequence number of the next packet to be received */ 
	
	unsigned long		rfc_flags;	/* Retransmission & Flow control flags */
	__u8			transmit_counter;/* Number of times the current packet has been retransmited (Retransmission mode only) */	
	
	/* L2CAP 1.2 Timers */
	struct timer_list	ret_timer;	/* Retransmission time-out. THINK about use one timer for retransmissionas and monitor !? */
	struct timer_list	monitor_timer;	/* Monitor time-out. */

	spinlock_t		xmit_lock;	/* Tx lock for l2cap_try_to_send */
	
	/* Reassembly */
	struct sk_buff		*lframe;			/* Points to the packet that it is currently being reassembled. */
	__u16			expected_reassembly_seq;	/* Keeps track of the TxSeq numbers when reassembling a sdu .*/
#endif
};

struct l2cap_skb_cb {
	int			ident;
	l2cap_ch		*ch;
	struct timer_list	timer;
	unsigned long		timeout;
};

#ifdef CONFIG_AFFIX_L2CAP_GROUPS
typedef struct {
	struct list_head	q;		/* for queueing */
	l2cap_ch *channel;
	int	 psm;
	atomic_t refcnt;
	btlist_head_t	bdas;		        /* list of BD_ADDR */
} __PACK__ l2cap_group_t;			/* L2CAP group struct */

/* Group data struct */
typedef struct {
	hci_con *bchndl;
	btlist_head_t	l2cap_groups;		/* L2CAP groups list */
	rwlock_t lock;
} __PACK__ l2cap_groups_t;


l2cap_group_t * __l2cap_group_lookup(__u16 );
l2cap_group_t * l2cap_group_lookup(__u16 );
void l2ca_group_put(l2cap_group_t *);
int l2ca_create_group(int , l2cap_proto_ops *, l2cap_ch **);
int l2ca_remove_group(int );
int l2ca_send_data_group(int psm, struct sk_buff *);
#endif

#define l2cap_cb(skb)	((struct l2cap_skb_cb*)(skb)->cb)


/* protocol management */
l2cap_proto * l2cap_proto_lookup(__u16 psm);
int l2ca_register_protocol(__u16 psm, l2cap_proto_ops *pops);
void l2ca_unregister_protocol(__u16 psm);
void l2ca_disable_protocol(__u16 psm, int disable);


/* channel management */
l2cap_ch *l2ca_create(l2cap_proto_ops *pops);
void __l2cap_destroy(l2cap_ch *ch);
void l2ca_put(l2cap_ch *ch);

void l2cap_default_cfgopt(l2cap_cfg_opt *opt);
int l2cap_unpack_cfgopt(struct sk_buff *skb, l2cap_cfg_opt *opt, int req);
int l2cap_pack_cfgopt(__u8 *options, l2cap_cfg_opt *opt);

/* For upper layer */
int __l2ca_connect_req(l2cap_ch *ch, BD_ADDR *bda);
int l2ca_connect_req(l2cap_ch *ch, BD_ADDR *bdaddr, __u16 psm);
int l2ca_connect_rsp(l2cap_ch *ch, __u16 response, __u16 status);
int l2ca_config_req(l2cap_ch *ch);
int l2ca_config_rsp(l2cap_ch *ch, __u16 result);
int l2ca_disconnect_req(l2cap_ch *ch);
int l2ca_disconnect_rsp(l2cap_ch *ch);
int l2ca_send_data(l2cap_ch *ch, struct sk_buff *skb);
int l2ca_ping(l2cap_ch *ch, struct sk_buff *skb);
int l2ca_singleping(l2cap_ch *ch, struct sk_buff *skb);
int __l2ca_ping(l2cap_ch *ch, BD_ADDR *bda, struct sk_buff *skb);

/* to upper layer */
int l2ca_data_ind(l2cap_ch *ch, struct sk_buff *skb);
int l2ca_connect_ind(l2cap_ch *ch);
int l2ca_connect_cfm(l2cap_ch *ch, int result, int status);
int l2ca_config_ind(l2cap_ch *ch);
int l2ca_config_cfm(l2cap_ch *ch, int result);
int l2ca_disconnect_ind(l2cap_ch *ch);
int l2ca_disconnect_cfm(l2cap_ch *ch);
int l2ca_control_ind(l2cap_ch *ch, int event, void *arg);

/* signalling requests */
int l2cap_connect_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 psm, __u16 scid);
int l2cap_config_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 dcid, __u16 flags, __u8 olen, __u8 *options);
int l2cap_disconnect_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 dcid, __u16 scid);
int l2cap_echo_req(l2cap_ch *ch, unsigned long timeout, __u8 code, __u8 id, void *data, int len);
int l2cap_info_req(l2cap_ch *ch, unsigned long timeout, __u8 id, __u16 infotype);
/* signalling responses */
int l2cap_connect_rsp(hci_con *con, __u8 id, __u16 scid, __u16 dcid, __u16 result, __u16 status);
int l2cap_command_rej(hci_con *con, __u8 id, __u16 reason, __u16 length, void *data);
int l2cap_config_rsp(hci_con *con, __u8 id, __u16 dcid, __u16 flags, __u16 result, __u8 olen, __u8 *options);
int l2cap_disconnect_rsp(hci_con *con, __u8 id, __u16 scid, __u16 dcid);
int l2cap_echo_rsp(hci_con *con, __u8 id, __u16 length, __u8 *data);
int l2cap_info_rsp(hci_con *con, __u8 id, __u16 type, __u16 result, __u16 length, __u8 *data);


/*
  inline functions
*/
static inline void l2ca_hold(l2cap_ch *ch)
{
	atomic_inc(&ch->refcnt);
}

static inline void l2ca_graft(l2cap_ch *ch, void *parent)
{
	if (ch->callback_cpu != smp_processor_id())
		spin_lock_bh(&ch->callback_lock);
	ch->priv = parent;
	if (ch->callback_cpu != smp_processor_id())
		spin_unlock_bh(&ch->callback_lock);
}

static inline void l2ca_orphan(l2cap_ch *ch)
{
	if (ch->callback_cpu != smp_processor_id())
		spin_lock_bh(&ch->callback_lock);
	ch->priv = NULL;
	if (ch->ops) {
                module_put(ch->ops->owner);
		ch->ops = NULL;
	}
	if (ch->callback_cpu != smp_processor_id())
		spin_unlock_bh(&ch->callback_lock);
}

/*
 * l2cap timers
 */
static inline void _l2cap_start_timer(l2cap_ch *ch, unsigned long timeout)
{
	if (timeout <= jiffies)
		return;
	if (!mod_timer(&ch->timer, timeout))
		l2ca_hold(ch);
}

static inline void l2cap_start_timer(l2cap_ch *ch, unsigned long len)
{
	_l2cap_start_timer(ch, len + jiffies);
}

static inline void l2cap_stop_timer(l2cap_ch *ch)
{
	if (del_timer(&ch->timer))
		l2ca_put(ch);
}


static inline void l2ca_set_mtu(l2cap_ch *ch, int mtu)
{
	ch->cfgout.mtu = mtu;
	set_bit(L2CAP_CFGOPT_MTU, &ch->cfgout.flags);
}

static inline int l2ca_get_mtu(l2cap_ch *ch)
{
	return ch->cfgin.mtu;
}

#ifdef CONFIG_AFFIX_BT_1_2
static inline void __l2ca_set_mps(l2cap_ch *ch, int mps)
{
	ch->cfgout.rfc.mps = mps;
}

static inline void l2ca_set_mps(l2cap_ch *ch, int mps)
{
	ch->cfgout.rfc.mps = mps;
	set_bit(L2CAP_CFGOPT_RFC,&ch->cfgout.flags);
}

static inline int l2ca_get_mps(l2cap_ch *ch)
{
	return ch->cfgin.rfc.mps;
}

static inline void __l2cap_set_local_mode(l2cap_ch *ch,int protocol) 
{
	switch (protocol) {
		case BTPROTO_L2CAP_RET:
			ch->cfgout.rfc.mode = L2CAP_RETRANSMISSION_MODE;
			break;
		case BTPROTO_L2CAP_FC:
			ch->cfgout.rfc.mode = L2CAP_FLOW_CONTROL_MODE;
			break;
		case BTPROTO_L2CAP_BASIC: /* == BTPROTO_L2CAP */
		default:
			ch->cfgout.rfc.mode = L2CAP_BASIC_MODE;
			break;
	}
}

#endif

static inline void l2ca_set_qos(l2cap_ch *ch, l2cap_qos_t *qos)
{
	ch->cfgout.qos = *qos;
	set_bit(L2CAP_CFGOPT_QOS, &ch->cfgout.flags);
}

static inline void l2ca_set_flushto(l2cap_ch *ch, int to)
{
	ch->cfgout.flush_to = to;
	set_bit(L2CAP_CFGOPT_FLUSHTO, &ch->cfgout.flags);
}

static inline void l2ca_setlinkto(l2cap_ch *ch, int to)
{
	ch->cfgout.link_to = to;
	set_bit(L2CAP_CFGOPT_LINKTO, &ch->cfgout.flags);
}

static inline int l2ca_server(l2cap_ch *ch)
{
	return test_bit(L2CAP_FLAGS_SERVER, &ch->flags);
}

#ifdef CONFIG_AFFIX_BT_1_2
static inline void l2ca_set_rfc(l2cap_ch *ch, l2cap_rfc_t *rfc)
{
	ch->cfgout.rfc = *rfc;
	set_bit(L2CAP_CFGOPT_RFC, &ch->cfgout.flags);
}
#endif

/* -------------- l2cap socket ------------------- */

typedef struct {
	affix_sock_t	base;	// sock base
	/* l2cap options */
	l2cap_ch	*ch;
	int		mtu, mru;
} l2cap_sock_t;

static inline l2cap_sock_t *lpf_get(struct sock *sk)
{
	return (l2cap_sock_t*)sk->sk_prot;
}

#endif

