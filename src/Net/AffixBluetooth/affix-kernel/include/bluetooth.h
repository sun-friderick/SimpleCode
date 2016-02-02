/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
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
   $Id: bluetooth.h,v 1.154 2004/07/22 14:38:03 chineape Exp $

   bluetooth.h - main Bluetooth header file

*/

#ifndef	_AF_AFFIX_H
#define	_AF_AFFIX_H

#ifdef __KERNEL__
/* kernel */
#include <linux/tcp.h>	// TCP states
#include <linux/if.h>
#include <linux/ioctl.h>
#include <linux/net.h>
#if !defined(__OPTIMIZE__)
//#warning  You must compile this file with the correct options!
//#warning  See the last lines of the source file.
//#error You must compile this driver with "-O".
#undef ARCH_HAS_PREFETCH
#define ARCH_HAS_PREFETCH
static inline void prefetch(const void *x) {;}
#endif
#else
/* user */
#include <sys/types.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#endif

#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/socket.h>

#include <affix/hci_types.h>

#ifdef  __cplusplus
extern "C" {
#endif


/* Debug levels */
#define DBL_LOG		1
#define DBL_FUNC	2
#define DBL_MEMBER	3
#define DBL_LOCAL	4

#ifdef __PACK__
#undef __PACK__
#endif
#define __PACK__	__attribute__ ((packed))

#if defined(__i386__)

#define __get_u8(ptr)	(*(__u8*)(ptr))
#define __get_u16(ptr)	(*(__u16*)(ptr))
#define __get_u32(ptr)	(*(__u32*)(ptr))
#define __get_u64(ptr)	(*(__u64*)(ptr))

#define __put_u8(ptr, value)	(*(__u8*)(ptr) = (value))
#define __put_u16(ptr, value)	(*(__u16*)(ptr) = (value))
#define __put_u32(ptr, value)	(*(__u32*)(ptr) = (value))
#define __put_u64(ptr, value)	(*(__u64*)(ptr) = (value))

#else /* not __i386__ */

#if 0

#include <asm/unaligned.h>
#define __get_u8(ptr)	(get_unaligned((__u8*)(ptr)))
#define __get_u16(ptr)	(get_unaligned((__u16*)(ptr)))
#define __get_u32(ptr)	(get_unaligned((__u32*)(ptr)))
#define __get_u64(ptr)	(get_unaligned((__u64*)(ptr)))

#define __put_u8(ptr, value)	(put_unaligned(value, (__u8*)(ptr)))
#define __put_u16(ptr, value)	(put_unaligned(value, (__u16*)(ptr)))
#define __put_u32(ptr, value)	(put_unaligned(value, (__u32*)(ptr)))
#define __put_u64(ptr, value)	(put_unaligned(value, (__u64*)(ptr)))

#else

typedef struct { __u8	value; } __PACK__ __u8_packed;
typedef struct { __u16	value; } __PACK__ __u16_packed;
typedef struct { __u32	value; } __PACK__ __u32_packed;
typedef struct { __u64	value; } __PACK__ __u64_packed;

static inline __u8 __get_u8(void *ptr)
{
	__u8_packed	*p = (__u8_packed*)ptr;
	return p->value;
}
static inline __u16 __get_u16(void *ptr)
{
	__u16_packed	*p = (__u16_packed*)ptr;
	return p->value;
}
static inline __u32 __get_u32(void *ptr)
{
	__u32_packed	*p = (__u32_packed*)ptr;
	return p->value;
}
static inline __u64 __get_u64(void *ptr)
{
	__u64_packed	*p = (__u64_packed*)ptr;
	return p->value;
}

static inline void __put_u8(void *ptr, __u8 value)
{
	__u8_packed	*p = (__u8_packed*)ptr;
	p->value = value;
}
static inline void __put_u16(void *ptr, __u16 value)
{
	__u16_packed	*p = (__u16_packed*)ptr;
	p->value = value;
}
static inline void __put_u32(void *ptr, __u32 value)
{
	__u32_packed	*p = (__u32_packed*)ptr;
	p->value = value;
}
static inline void __put_u64(void *ptr, __u64 value)
{
	__u64_packed	*p = (__u64_packed*)ptr;
	p->value = value;
}

#endif

#endif


/* bluetooth network data types */

/* some affix releated */
#ifndef	__KERNEL__
/* user mode */

#define BT_BYTE_ORDER	__BYTE_ORDER

#else
/* kernel mode */

#ifdef __LITTLE_ENDIAN
#define BT_BYTE_ORDER	__LITTLE_ENDIAN
#endif

#ifdef __BIG_ENDIAN
#define BT_BYTE_ORDER	__BIG_ENDIAN
#endif

#endif

#define __btoh16(data)	__le16_to_cpu(data)
#define __btoh32(data)	__le32_to_cpu(data)
#define __btoh64(data)	__le64_to_cpu(data)
#define __htob16(data)	__cpu_to_le16(data)
#define __htob32(data)	__cpu_to_le32(data)
#define __htob64(data)	__cpu_to_le64(data)

#if BT_BYTE_ORDER == __LITTLE_ENDIAN
#define __btoh24(ptr)	(__btoh32(ptr))
#define __htob24(ptr)	(__htob32(ptr))
#elif BT_BYTE_ORDER == __BIG_ENDIAN
#define __btoh24(ptr)	(__btoh32(ptr)>>8)
#define __htob24(ptr)	(__htob32(ptr)>>8)
#endif

#ifdef FALSE
#undef FALSE
#endif
#ifdef TRUE
#undef TRUE
#endif
#define TRUE	1
#define FALSE	0

#ifdef btmin
#undef btmin
#endif
#define btmin(a, b)	(((a)<(b))?a:b)


extern __u32	affix_dbmask;

/* ------------------------------------------------------------------- */
/*
  These constants control which files/objects have their debugging 
  messages turned on
*/
/* Protocols */
/* core */
#define DBHCI		0x00000001
#define DBAFHCI		0x00000020
#define DBHCIMGR	0x00000100
#define DBHCISCHED	0x00000200
#define DBHCILIB	0x00000400
/* l2cap */
#define DBL2CAP		0x00000002
#define DBAFL2CAP	0x00000040
/* rfcomm */
#define DBRFCOMM	0x00000004
#define DBAFRFCOMM	0x00000080
#define DBBTY		0x00000008
/* pan */
#define DBPAN		0x00000010

/* Drivers */
#define DBDRV		0x00001000

#define DBALLPROTO	(DBHCI|DBAFHCI|DBHCIMGR|DBHCISCHED|DBHCILIB | DBL2CAP|DBAFL2CAP |\
			DBRFCOMM|DBAFRFCOMM|DBBTY | DBPAN)
#define DBALLMOD	(DBALLPROTO | DBDRV)

/* details */
#define DBCTRL		0x04000000
#define DBPARSE		0x08000000
#define DBCHARDUMP	0x10000000
#define DBHEXDUMP	0x20000000
#define DBFNAME		0x40000000
#define DBFUNC		0x80000000

#define DBALLDETAIL	(DBCTRL | DBPARSE | DBCHARDUMP | DBHEXDUMP | DBFNAME | DBFUNC)

/* ----------------------------------------------------- */

/* kernel only section */
#ifdef __KERNEL__

#if !defined(KERNEL_VERSION)
#include <linux/version.h>
#endif

/*
 * error codes
 */
#define ERR_OK			0x0000
#define ERR_PENDING		0x0001	// pending...				EINPROGRESS
#define ERR_LINKTO		0x0002	// baseband link timed out		EHOSTUNREACH
#define ERR_RESOURCE		0x0003	// no resources - 			ENOMEM
#define ERR_CONNREFUSED		0x0003	// service not available (on PSM)	ECONNREFUSED
#define ERR_SECURITY		0x0004	// access not allowed	 		EACCESS
#define ERR_TIMEDOUT		0x0005	// connection timed out			ETIMEDOUT
#define ERR_CONNRESET		0x0006	// disconnection came from peer side	ECONNRESET
#define ERR_FAILURE		0x0007	// any failure
#define ERR_INVALIDARGS		0x0008	// invalid arguments (proto specific)	EINVAL
#define ERR_HARDWARE		0x0009	// hardware failure

/*
 * general connection states
 */
#if 0
enum {
  TCP_ESTABLISHED = 1,
  TCP_SYN_SENT,
  TCP_SYN_RECV,
  TCP_FIN_WAIT1,
  TCP_FIN_WAIT2,
  TCP_TIME_WAIT,
  TCP_CLOSE,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_LISTEN,
  TCP_CLOSING,	 /* now a valid state */

  TCP_MAX_STATES /* Leave at the end! */
};
#endif

typedef volatile enum {
	DEAD = 0,
	/* socket specific */
	CON_ESTABLISHED = TCP_ESTABLISHED,
	CON_CONNECTING = TCP_SYN_SENT,
	CON_CLOSED = TCP_CLOSE,
	CON_LISTEN = TCP_LISTEN,

	CON_W4_LCONRSP = TCP_MAX_STATES,
	CON_CONFIG,		/* protocol specific configuration process */
	CON_W4_DISCRSP,
	CON_W4_LCONREQ,
	CON_W4_LDISCREQ,
	CON_W4_DISCREQ,		// unsued
	CON_W4_LDISCRSP,	// packet only
	CON_W4_LINKUP,		// unused
	CON_W4_AUTHRSP,

	CON_MAX_STATES, /* Leave at the end! */

	// these must be last, otherwise enumeration will be re-odered
	CON_OPEN = CON_ESTABLISHED,
	CON_W4_CONRSP = CON_CONNECTING,
	CON_W4_CONREQ = CON_LISTEN,
	
} con_state;

int affix_sock_register(struct net_proto_family *pf, int protocol);
int affix_sock_unregister(int protocol);

#endif	/* __KERNEL__ */


static inline void bda2eth(void *eth, void *bda)
{
	int	i;

	for (i = 0; i < 6; i++)
		((__u8*)eth)[i] = ((__u8*)bda)[5-i];
}


#define AFFIX_FLOW_OFF		0x00
#define AFFIX_FLOW_ON		0x01

/* IOCTL for HCI devices */
#define SIOCHCI_SET_AUDIO	(SIOCDEVPRIVATE+1)
#define SIOCHCI_GET_AUDIO	(SIOCDEVPRIVATE+2)

/* Bypass warnings */
#ifdef AF_AFFIX
#undef AF_AFFIX
#endif

#ifdef PF_AFFIX
#undef PF_AFFIX
#endif

#define AF_AFFIX		27	// FIXME:
#define PF_AFFIX		AF_AFFIX

/* Socket Options Stuff */
#define SOL_AFFIX		277

#define BTSO_MTU		0x01
#define BTSO_SECURITY		0x02
#define BTSO_EVENT_MASK		0x03
#define BTSO_PKT_MASK		0x04
#define BTSO_PROMISC		0x05
#define BTSO_TYPE		0x10

/* socket modes */
#define AFFIX_SOCK_PACKET	0x20

/* network device definition */
#define ETH_P_BLUETOOTH		0x0027

#define BTPROTO_HCI		0
#define BTPROTO_L2CAP		1
#define BTPROTO_RFCOMM		2
#define BTPROTO_HCIACL		3
#define BTPROTO_HCISCO		4
#define BTPROTO_SCO		BTPROTO_HCISCO

#ifdef CONFIG_AFFIX_BT_1_2

#define	BTPROTO_L2CAP_RET	11
#define	BTPROTO_L2CAP_FC	12
#define BTPROTO_L2CAP_BASIC	BTPROTO_L2CAP
#define BTPROTO_MAX		13

#else

#define	BTPROTO_MAX		10	/* should be 1 greater than last proto */

#endif

//#define SOL_HCI		2
//#define SOL_L2CAP		3
//#define SOL_RFCOMM		4

/* HCI address family data types */
#define HCIDEV_ANY	0

struct sockaddr_affix {
	sa_family_t	family;
	int		devnum;
	BD_ADDR		bda;
	uint16_t	port;
};

/* sendmsg()/recvmsg() Affix flags */
#define HCI_SKIP_STATUS		0x01000000
#define HCI_SKIP_COMPLETE	0x01000000
#define HCI_REQUEST_MODE	0x02000000
#define HCI_NO_UART_ENCAP	0x04000000


/* L2CAP address family data types */

/*
 ** Protocol and service multiplexor (PSM) standards
 */
#define SDP_PSM 	 	0x0001
#define RFCOMM_PSM  	 	0x0003
#define TCS_BIN_PSM 	 	0x0005
#define TCS_BIN_CORDLESS_PSM 	0x0007
#define BNEP_PSM		0x000F

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
} __PACK__ l2cap_qos_t;

#ifdef CONFIG_AFFIX_BT_1_2
/* Retransmission and Flow Control options */
typedef struct {
	__u8	mode;
	__u8	txwindow_size;
	__u8	max_transmit;
	__u16	retransmission_timeout;
	__u16	monitor_timeout;
	__u16	mps;
}__PACK__ l2cap_rfc_t;
#endif


/* cmsg_type */
#define L2CAP_PING		0
#define L2CAP_SINGLEPING	1

#define BTIOC_MAGIC		'b'

/* sioc commands for socket interface */
#define SIOCL2CAP_CONFIG	(SIOCPROTOPRIVATE+0)
#define SIOCL2CAP_FLUSH		(SIOCPROTOPRIVATE+1)


/* RFCOMM stuff */
#define SIOCRFCOMM_OPEN_BTY		_IOWR(BTIOC_MAGIC, 1, int)
#define SIOCRFCOMM_CLOSE_BTY		_IOW(BTIOC_MAGIC, 2, int)
#define SIOCRFCOMM_SETTYPE		_IOW(BTIOC_MAGIC, 3, int)

struct rfcomm_port {
	int			line;
	struct sockaddr_affix	addr;
#define RFCOMM_SOCK_BTY		0x0001
#define RFCOMM_SOCK_BOUND	0x0002
#define RFCOMM_SOCK_CONNECTED	0x0004
	int			flags;
};

#define SIOCRFCOMM_BIND_BTY		_IOWR(BTIOC_MAGIC, 4, struct rfcomm_port)

struct rfcomm_ports {
	struct rfcomm_port	*ports;	/* array ptr */
	int			size;	/* array size */
	int			count;	/* read info */
};

#define SIOCRFCOMM_GET_PORTS		_IOWR(BTIOC_MAGIC, 5, struct rfcomm_ports)

#define RFCOMM_TYPE_SOCKET	0
#define RFCOMM_TYPE_BTY		1
#define RFCOMM_BTY		RFCOMM_TYPE_BTY

#define RFCOMM_BTY_ANY		-1


/* PAN stuff */
#define SIOCSFILTERPROTOCOL	SIOCDEVPRIVATE		/* set protocol filter */
#define SIOCSFILTERMULTICAST	SIOCDEVPRIVATE + 1	/* set mulitcast filter */
#define SIOCGFILTERPROTOCOL	SIOCDEVPRIVATE + 2	/* get protocol filter */
#define SIOCGFILTERMULTICAST	SIOCDEVPRIVATE + 3	/* get mulitcast filter */


#define PROTOCOL_FILTER_MAX	16
#define MULTICAST_FILTER_MAX	16

#define F_START 0
#define F_STOP  1

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

typedef unsigned char ETH_ADDR[ETH_ALEN];

typedef struct {
	__u16		count; /* number of filter entrys, 0 = no filter */
	__u16		protocol[PROTOCOL_FILTER_MAX][2]; /* start_range - stop_range pairs in network byte order */
} protocol_filter;

typedef struct {
	__u16		count; /* number of filter entrys, 0 = no filter */
	ETH_ADDR	multicast[MULTICAST_FILTER_MAX][2]; /* start_range - stop_range pairs in network byte order */
} multicast_filter;


/* **** */
#define HCI_MAX_EVENT_SIZE		260
#define HCI_MAX_CMD_SIZE		260
#define HCI_MAX_MSG_SIZE		64

/*  Messages used to comunicate with the affix servers (affixd and btsrv) */
struct hci_msg_hdr {
	int	opcode;
	int	length;
}__PACK__;

#define HCICTL_STATE_CHANGE			0x01
#define HCICTL_CONNECT_REQ			0x02
#define HCICTL_DISCONNECT_REQ			0x03
#define HCICTL_AUTH_REQ				0x04
#define HCICTL_PAN_EVENT			0x05
#define HCICTL_UPDATECLOCKOFFSET_REQ		0x06

/* hci device events */
#define HCIDEV_UP		0x0001
#define HCIDEV_DOWN		0x0002
#define HCIDEV_CHANGE		0x0004
#define HCIDEV_REGISTER		0x0005
#define HCIDEV_UNREGISTER	0x0006
/* affix specific */
#define HCIDEV_DETACH		0x0101
#define HCIDEV_ATTACH		0x0102
/* HCI connection specific */
#define HCICON_AUTH_COMPLETE	0x0103

/* PAN events */
#define PANDEV_CONNECT_LOST	0x0001


struct hci_state_change {
	struct hci_msg_hdr	hdr;
	int			devnum;
	int			event;
}__PACK__;

struct hci_connect_req {
	struct hci_msg_hdr	hdr;
	int			id;
}__PACK__;

struct hci_disconnect_req {
	struct hci_msg_hdr	hdr;
	int			id;
	__u8			reason;
}__PACK__;

struct hci_auth_req {
	struct hci_msg_hdr	hdr;
	int			id;
}__PACK__;

#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
struct hci_updateclockoffset_req {
	struct hci_msg_hdr	hdr;
	int			id;
	__u16			chandle;
}__PACK__;
#endif

struct hci_pan_event {
	struct hci_msg_hdr	hdr;
	int			devnum;
	int			event;
}__PACK__;

/* HCI device statistic */
struct hcidev_stats
{
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_acl;			/* total ACL packets received	*/
	unsigned long	tx_acl;			/* total ACL packets transmitted*/
	unsigned long	rx_sco;			/* total SCO packets received	*/
	unsigned long	tx_sco;			/* total SCO packets transmitted*/
	unsigned long	rx_event;		/* total EVENT packets received	*/
	unsigned long	tx_cmd;			/* total CMD packets transmitted*/
	/* for device driver */
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
};


/* *****************   ioctl stuff   ********************** */

#define HCI_OPEN_NAME		1
#define HCI_OPEN_ID		2
#define HCI_OPEN_MGR		3
#define HCI_OPEN_EVENT		4

#define HCI_OPEN_MASK		0xF0
#define HCI_OPEN_SUPER		0x80

struct hci_open {
	int	cmd;
	char	name[IFNAMSIZ];
	int	devnum;
};

#define BTIOC_OPEN_HCI		_IOW(BTIOC_MAGIC, 2, struct hci_open)
#define BTIOC_LOCK_HCI		_IOW(BTIOC_MAGIC, 3, int)
#define BTIOC_START_DEV		_IOW(BTIOC_MAGIC, 4, int)

#define BTIOC_DBMGET		_IOR(BTIOC_MAGIC, 5, __u32)
#define BTIOC_DBMSET		_IOW(BTIOC_MAGIC, 6, __u32)

#define BTIOC_ADDPINCODE	_IOW(BTIOC_MAGIC, 9, struct PIN_Code)
#define BTIOC_REMOVEPINCODE	_IO(BTIOC_MAGIC, 10)
#define BTIOC_REMOVELINKKEY	_IO(BTIOC_MAGIC, 11)

struct link_key {
	BD_ADDR		bda;
	__u8		key_type;
	__u8		key[16];
};

#define BTIOC_ADDLINKKEY	_IOW(BTIOC_MAGIC, 12, struct link_key)

/* mask to enable TLP protocol on UART */
#define AFFIX_UART_RI		0x01000000
#define AFFIX_UART_LOW		0x02000000

/* max path len */
#define AFFIX_UART_PATHLEN	32

struct open_uart {
	char	dev[AFFIX_UART_PATHLEN];
	int	type;
	int	proto;
	int	flags;	
	int	speed;
};

#define BTIOC_SETUP_UART	_IOW(BTIOC_MAGIC, 13, struct open_uart)
#define BTIOC_OPEN_UART		_IOWR(BTIOC_MAGIC, 14, struct open_uart)
#define BTIOC_CLOSE_UART	_IOWR(BTIOC_MAGIC, 15, struct open_uart)

#define HCI_MAX_DEVS		16
#define BTIOC_GETDEVS		_IOR(BTIOC_MAGIC, 16, int[HCI_MAX_DEVS])

#define BTIOC_HCI_DISC		_IOW(BTIOC_MAGIC, 21, struct sockaddr_affix)

struct affix_conn_info {
	uint32_t	proto;
	int		devnum;
	BD_ADDR		bda;
	uint16_t	psm;
	uint32_t	sport;
	uint32_t	dport;
};

#define BTIOC_GET_CONN		_IOWR(BTIOC_MAGIC, 22, struct affix_conn_info)


#define HCI_ATTR_ALL		0xFF

struct hci_dev_attr {
	int			devnum;
	char			name[IFNAMSIZ];
	BD_ADDR			bda;
	int			flags;
	int			pkt_type;
	struct hcidev_stats	stats;
};

#define BTIOC_SET_ATTR		_IOW(BTIOC_MAGIC, 23, struct hci_dev_attr)
#define BTIOC_GET_ATTR		_IOWR(BTIOC_MAGIC, 24, struct hci_dev_attr)

#define HCI_FLAGS_UP			0x00000001

#define HCI_FLAGS_ROLE			0x000000F0		
#define HCI_ROLE_ALLOW_SWITCH		0x00000000
#define HCI_ROLE_DENY_SWITCH		0x00000010
#define HCI_ROLE_REMAIN_SLAVE		0x00000000
#define HCI_ROLE_BECOME_MASTER		0x00000020

#define HCI_FLAGS_SECURITY		0x00FFFF00
#define HCI_SECURITY_OPEN		0x00000100
#define HCI_SECURITY_SERVICE		0x00000200
#define HCI_SECURITY_LINK		0x00000400
#define HCI_SECURITY_PAIRABLE		0x00000800
/* levels */
#define HCI_SECURITY_AUTH		0x00010000
#define	HCI_SECURITY_ENCRYPT		0x00020000
#define HCI_SECURITY_AUTHOR		0x00040000
#define HCI_SECURITY_OUT_AUTH		0x00100000
#define	HCI_SECURITY_OUT_ENCRYPT	0x00200000
#define HCI_SECURITY_OUT_AUTHOR		0x00400000
/* connection less traffic */
#define HCI_SECURITY_CL			0x00800000
/* scan */
#define HCI_FLAGS_SCAN_BITS		24
#define HCI_FLAGS_SCAN			0x0F000000
#define HCI_FLAGS_SCAN_INQUIRY		0x01000000
#define HCI_FLAGS_SCAN_PAGE		0x02000000
#define HCI_FLAGS_SCAN_BOTH		0x03000000

#define AFFIX_FLAGS_PROMISC		0x00000001
#define AFFIX_FLAGS_SUPER		0x00000002
#define AFFIX_FLAGS_LOCK		0x00000004
#define AFFIX_FLAGS_W4_STATUS		0x00000010
#define AFFIX_FLAGS_CMD_PENDING		0x00000020
#define AFFIX_FLAGS_CTLMASK		0x0000FF00
#define AFFIX_FLAGS_PIN			0x00000100
#define AFFIX_FLAGS_KEY			0x00000200
/* for compatibility */
#define AFFIX_MODE_PIN			AFFIX_FLAGS_PIN
#define AFFIX_MODE_KEY			AFFIX_FLAGS_KEY


#define BTIOC_SET_CTL			_IOW(BTIOC_MAGIC, 25, int)
#define BTIOC_GET_CTL			_IOWR(BTIOC_MAGIC, 26, int)

#define AFFIX_AUDIO_ON			0x0001
#define AFFIX_AUDIO_ASYNC		0x0010		// Bluetooth Module SCO flow control
#define AFFIX_AUDIO_SYNC		0x0020		// Affix SCO flow control
#define AFFIX_AUDIO_GETALT(mode)	(((mode)>>8) & 0x0F)
#define AFFIX_AUDIO_SETALT(mode, alt)	(((mode) & ~0x0F00) | ((alt)<<8))

struct affix_audio {
	int	mode;
	__u16	setting;
};

#define BTIOC_SET_AUDIO		_IOW(BTIOC_MAGIC, 27, struct affix_audio)
#define BTIOC_GET_AUDIO		_IOWR(BTIOC_MAGIC, 28, struct affix_audio)

struct affix_version {
	int	version;
};

#define BTIOC_GET_VERSION	_IOR(BTIOC_MAGIC, 29, struct affix_version)



#define BTIOC_SET_PKTTYPE	_IOW(BTIOC_MAGIC, 34, int)
#define BTIOC_GET_PKTTYPE	_IOR(BTIOC_MAGIC, 35, int)

#define BTIOC_SET_SECMODE	_IOW(BTIOC_MAGIC, 36, int)
#define BTIOC_SET_ROLE		_IOW(BTIOC_MAGIC, 37, int)
#define BTIOC_SET_SCAN		_IOW(BTIOC_MAGIC, 38, int)


/* **************   PAN   **************************** */
#define AFFIX_PAN_ROLE		0x03
#define AFFIX_PAN_PANU		0x01
#define AFFIX_PAN_NAP		0x02
#define AFFIX_PAN_GN		0x03
#define AFFIX_PAN_AUTO		0x04	/* auto connect mode */

#define AFFIX_PAN_UUID_PANU     0x1115
#define AFFIX_PAN_UUID_NAP      0x1116
#define AFFIX_PAN_UUID_GN       0x1117

struct pan_init {
	char	name[IFNAMSIZ];
	int	mode;
};

struct pan_connect {
        struct sockaddr_affix   saddr;
        int                     peer_role;
};

#define BTIOC_PAN_INIT		_IOWR(BTIOC_MAGIC, 64, struct pan_init)

#define BTIOC_PAN_CONNECT	_IOW(BTIOC_MAGIC, 65, struct pan_connect)

/* ************************ HIDP ************************* */

struct hidp_conn_info {
        __u16                   parser;
        __u16                   rd_size;
        __u8                    *rd_data;
        __u8                    country;
        __u16                   vendor;
        __u16                   product;
        __u16                   version;
        __u32                   flags;
        __u32                   idle_to;
        char                    name[128];
};

struct hidp_ioc {
        struct sockaddr_affix   saddr;
        struct hidp_conn_info   conn_info;
        __u16                   status;
#define HIDP_STATUS_ACTIVE_ADD  0x1
#define HIDP_STATUS_CONNECTED   0x2
        __u16                   cnum;
};

#define HIDP_MAX_DEVICES        20
struct hidp_ioc_getlist {
        int                     count;       /* # of devices in list */
        int                     left;        /* !=0 if more than count devices in list */
        struct hidp_ioc         list[0];     /* first element of list */
};

#define BTIOC_HIDP_MODIFY       _IOW(BTIOC_MAGIC, 72, struct hidp_ioc)
#define BTIOC_HIDP_DELETE       _IOW(BTIOC_MAGIC, 73, struct hidp_ioc)
#define BTIOC_HIDP_GET_LIST     _IOWR(BTIOC_MAGIC, 74, struct hidp_ioc_getlist)
/* XXX: check if 72, 73, 74 is ok... */


/* ******************************************************* */
int hci_open_dev(struct hci_open *dev);
int hci_exec_cmd(int fd, __u16 opcode, void *cmd, int len, __u64 mask, int flags, void *event, int elen);
int hci_exec_cmd0(int fd, __u16 opcode, __u64 mask, int flags, void *event, int elen);
int hci_exec_cmd1(int fd, __u16 opcode, void *cmd, int len, __u64 mask, int flags);

#ifdef __KERNEL__
#include <linux/poll.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#include <affix/hci_types.h>

static inline _syscall3(int,ioctl,int,fd,unsigned int,cmd, void*,arg);
static inline _syscall3(int,poll,struct pollfd*,ufds,unsigned int,nfds,int,timeout);
static inline _syscall2(int,socketcall,int,call,unsigned long*,args);

static inline int btsys_socket(int domain, int type, int protocol)
{
	unsigned long		a[3];
	int			fd;
	mm_segment_t		old_fs;

	a[0] = domain;
	a[1] = type;
	a[2] = protocol;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	fd = socketcall(SYS_SOCKET, a);
	set_fs(old_fs);
	return fd;
}

static inline int btsys_setsockopt(int fd, int level, int optname, const void *optval, int optlen)
{
	unsigned long		a[5];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = level;
	a[2] = optname;
	a[3] = (unsigned long)optval;
	a[4] = optlen;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_SETSOCKOPT, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_getsockopt(int fd, int level, int optname, void *optval, int *optlen)
{
	unsigned long		a[5];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = level;
	a[2] = optname;
	a[3] = (unsigned long)optval;
	a[4] = (unsigned long)optlen;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_GETSOCKOPT, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_ioctl(int fd, int cmd, void *arg)
{
	int		err = 0;
	mm_segment_t	old_fs;

	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = ioctl(fd, cmd, arg);
	set_fs(old_fs);

	return err;
}

static inline int btsys_recvmsg(int fd, struct msghdr *msg, int flags)
{
	unsigned long		a[3];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)msg;
	a[2] = flags;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_RECVMSG, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_recv(int fd, void *buf, size_t len, int flags)
{
	unsigned long		a[4];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)buf;
	a[2] = len;
	a[3] = flags;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_RECV, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_recvfrom(int fd, void *buf, size_t len, int flags, 
						struct sockaddr *from, int *fromlen)
{
	unsigned long		a[6];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)buf;
	a[2] = len;
	a[3] = flags;
	a[4] = (unsigned long)from;
	a[5] = (unsigned long)fromlen;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_RECVFROM, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_sendmsg(int fd, struct msghdr *msg, int flags)
{
	unsigned long		a[3];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)msg;
	a[2] = flags;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_SENDMSG, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_send(int fd, const void *buf, size_t len, int flags)
{
	unsigned long		a[4];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)buf;
	a[2] = len;
	a[3] = flags;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_SEND, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_sendto(int fd, void *buf, size_t len, int flags, 
						struct sockaddr *to, int tolen)
{
	unsigned long		a[6];
	int			err;
	mm_segment_t		old_fs;

	a[0] = fd;
	a[1] = (unsigned long)buf;
	a[2] = len;
	a[3] = flags;
	a[4] = (unsigned long)to;
	a[5] = tolen;
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = socketcall(SYS_SENDTO, a);
	set_fs(old_fs);
	return err;
}

static inline int btsys_poll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	int		err;
	mm_segment_t	old_fs;
	
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = poll(ufds, nfds, timeout);
	set_fs(old_fs);
	return err;
}

static inline int btsys_close(int fd)
{
	int		err;
	mm_segment_t	old_fs;
	
	old_fs = get_fs(); set_fs(KERNEL_DS);
	err = close(fd);
	set_fs(old_fs);
	return err;
}


static inline int hci_poll(struct pollfd *ufds, unsigned int nfds, int timeout)
{
	return btsys_poll(ufds, nfds, timeout);
}

#else	/* not __KERNEL__ */

static inline int btsys_socket(int domain, int type, int protocol)
{
	return socket(domain, type, protocol);
}

static inline int btsys_ioctl(int fd, int cmd, void *arg)
{
	return ioctl(fd, cmd, arg);
}

static inline int btsys_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	return setsockopt(fd, level, optname, optval, optlen);
}

static inline int btsys_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
	return getsockopt(fd, level, optname, optval, optlen);
}

static inline int btsys_recvmsg(int fd, struct msghdr *msg, int flags)
{
	return recvmsg(fd, msg, flags);
}

static inline int btsys_recv(int fd, void *buf, size_t len, int flags)
{
	return recv(fd, buf, len, flags);
}

static inline int btsys_recvfrom(int fd, void *buf, size_t len, int flags, 
						struct sockaddr *from, socklen_t *fromlen)
{
	return recvfrom(fd, buf, len, flags, from, fromlen);
}

static inline int btsys_sendmsg(int fd, struct msghdr *msg, int flags)
{
	return sendmsg(fd, msg, flags);
}

static inline int btsys_send(int fd, const void *buf, size_t len, int flags)
{
	return send(fd, buf, len, flags);
}

static inline int btsys_sendto(int fd, const void *buf, size_t len, int flags,
						const struct sockaddr *to, socklen_t tolen)
{
	return sendto(fd, buf, len, flags, to, tolen);
}

static inline int btsys_close(int fd)
{
	return close(fd);
}

#endif

/* 
 * HCI stuff
 */

static inline int hci_close(int fd)
{
	return btsys_close(fd);
}

static inline int hci_open(char *name)
{
	struct hci_open	dev;
	
	if (!name) {
		errno = EINVAL;
		return -1;
	}
	dev.cmd = HCI_OPEN_NAME;
	strncpy(dev.name, name, IFNAMSIZ);
	return hci_open_dev(&dev);
}

static inline int _hci_open(char *name)
{
	struct hci_open	dev;
	
	if (!name) {
		errno = EINVAL;
		return -1;
	}
	dev.cmd = HCI_OPEN_NAME | HCI_OPEN_SUPER;
	strncpy(dev.name, name, IFNAMSIZ);
	return hci_open_dev(&dev);
}

static inline int hci_open_id(int devnum)
{
	struct hci_open	dev;

	dev.cmd = HCI_OPEN_ID;
	dev.devnum = devnum;
	return hci_open_dev(&dev);
}

static inline int _hci_open_id(int devnum)
{
	struct hci_open	dev;

	dev.cmd = HCI_OPEN_ID | HCI_OPEN_SUPER;
	dev.devnum = devnum;
	return hci_open_dev(&dev);
}

static inline int hci_lock(int fd, int lock)
{
	return btsys_ioctl(fd, BTIOC_LOCK_HCI, &lock);
}

static inline int hci_open_event(void)
{
	struct hci_open	dev;
	
	dev.cmd = HCI_OPEN_EVENT;
	return hci_open_dev(&dev);
}

static inline int hci_open_mgr(void)
{
	struct hci_open	dev;

	dev.cmd = HCI_OPEN_MGR;
	return hci_open_dev(&dev);
}

static inline int hci_event_mask(int fd, __u64 mask)
{
	return btsys_setsockopt(fd, SOL_AFFIX, BTSO_EVENT_MASK, &mask, sizeof(mask));
}

static inline int hci_pkt_mask(int fd, unsigned int mask)
{
	return btsys_setsockopt(fd, SOL_AFFIX, BTSO_PKT_MASK, &mask, sizeof(mask));
}

static inline int hci_recv_event(int fd, void *event, int size, long timeout)
{
	struct timeval	tv = { tv_sec: timeout, tv_usec: 0 };

	/* 0 means infinite */
	btsys_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	return  btsys_recv(fd, event, size, HCI_NO_UART_ENCAP);
}

static inline int hci_set_audio(int fd, int mode, __u16 setting)
{
	struct affix_audio	audio;

	audio.mode = mode;
	audio.setting = setting;
	return btsys_ioctl(fd, BTIOC_SET_AUDIO, &audio);
}

static inline int hci_set_scan(int fd, int scan)
{
	return btsys_ioctl(fd, BTIOC_SET_SCAN, &scan);
}

static inline int hci_set_secmode(int fd, int secmode)
{
	return btsys_ioctl(fd, BTIOC_SET_SECMODE, &secmode);
}

int hci_recv_event_any(int fd, int *devnum, void *event, int size);


// HCI commands - from the spec

// Link Control
// __HCI_xx - unwaitable version of the command. returns after command status event
//
int HCI_Inquiry(int fd, __u8 Inquiry_Length, __u8 Max_Num_Responses,
		INQUIRY_ITEM *Items, __u8 *Num_Responses);
int HCI_InquiryCancel(int fd);
int HCI_PeriodicInquiryMode(int fd, __u16 Max_Period_Length, __u16 Min_Period_Length, 
			    __u16 Inquiry_Length, __u8 Max_Num_Responses);
int HCI_ExitPeriodicInquiryMode(int fd);
int HCI_CreateConnection(int fd, INQUIRY_ITEM *dev, __u16 Packet_Type, __u8 Allow_Role_Switch,
			 __u16 *Connection_Handle, __u8 *Link_Type, __u8 *Encryption_Mode);
int __HCI_CreateConnection(int fd, INQUIRY_ITEM *dev, __u16 Packet_Type, __u8 Allow_Role_Switch);
int HCI_Disconnect(int fd, __u16 Connection_Handle, __u8 Reason);
int __HCI_Disconnect(int fd, __u16 Connection_Handle, __u8 Reason);
int __HCI_AddSCOConnection(int fd, __u16 Connection_Handle, __u16 Packet_Type);
int HCI_AcceptConnectionRequest(int fd, BD_ADDR *bda, __u8 Role, __u16 *Connection_Handle,
				__u8 *Link_Type, __u8 *Encryption_Mode);
int __HCI_AcceptConnectionRequest(int fd, BD_ADDR *bda, __u8 Role);
int HCI_RejectConnectionRequest(int fd, BD_ADDR *bda, __u8 Reason);
int __HCI_RejectConnectionRequest(int fd, BD_ADDR *bda, __u8 Reason);
int HCI_LinkKeyRequestReply(int fd, BD_ADDR *bda, __u8 *Link_Key);
int HCI_LinkKeyRequestNegativeReply(int fd, BD_ADDR *bda);
int HCI_PINCodeRequestReply(int fd, BD_ADDR *bda, __u8 PIN_Code_Length, __u8 *PIN_Code);
int HCI_PINCodeRequestNegativeReply(int fd, BD_ADDR *bda);
int HCI_ChangeConnectionPacketType(int fd, __u16 Connection_Handle, __u16 Packet_Type);
int HCI_AuthenticationRequested(int fd, __u16 Connection_Handle);
int __HCI_AuthenticationRequested(int fd, __u16 Connection_Handle);
int HCI_SetConnectionEncryption(int fd, __u16 Connection_Handle, __u8 Encryption_Enable);
int HCI_ChangeConnectionLinkKey(int fd, __u16 Connection_Handle,
				BD_ADDR *bda, __u8 *Link_Key, __u8 *Key_Type);
int HCI_MasterLinkKey(int fd, __u8 Key_Flag, __u16 *Connection_Handle);
int HCI_RemoteNameRequest(int fd, INQUIRY_ITEM *dev, char *Name);

int HCI_ReadClockOffset(int fd, __u16 Connection_Handle, __u16 *ClockOffset);
int __HCI_ReadClockOffset(int fd, __u16 Connection_Handle);

// Link Policy Commands
int HCI_HoldMode(int fd, __u16 Connection_Handle, __u16 Hold_Mode_Max_Interval, __u16 Hold_Mode_Min_Interval);
int HCI_SniffMode(int fd, __u16 Connection_Handle, __u16 Sniff_Max_Interval, __u16 Sniff_Min_Interval, __u16 Sniff_Attempt, __u16 Sniff_Timeout);
int HCI_ExitSniffMode(int fd, __u16 Connection_Handle, __u8 *Current_Mode, __u16 *Interval);
int HCI_ParkMode(int fd, __u16 Connection_Handle, __u16 Beacon_Max_Interval, __u16 Beacon_Min_Interval);
int HCI_ExitParkMode(int fd, __u16 Connection_Handle, __u8 *Current_Mode, __u16 *Interval);
int HCI_QoS_Setup(int fd, __u16 Connection_Handle, struct HCI_QoS *Requested_QoS, struct HCI_QoS *Completed_QoS);
int HCI_RoleDiscovery(int fd, __u16 Connection_Handle, __u8 *Current_Role);
int HCI_SwitchRole(int fd, BD_ADDR *bda, __u8 Role);
int HCI_WriteLinkPolicySettings(int fd, __u16 Connection_Handle, __u8 Link_Policy_Settings);
int HCI_ReadLinkPolicySettings(int fd, __u16 Connection_Handle, __u8 *Link_Policy_Settings);

int HCI_Read_Num_Broadcast_Retransmissions(int fd, __u8 *Num);
int HCI_Write_Num_Broadcast_Retransmissions(int fd, __u8 Num);

// HC & BB commands
int HCI_SetEventMask(int fd, __u64 mask);
int __HCI_Reset(int fd);
int HCI_Reset(int fd);
int HCI_SetEventFilter(int fd, __u8 Filter_Type, __u8 Filter_Condition_Type, __u8 *Condition, __u8 Condition_Length);
int HCI_ReadPINType(int fd, __u8 *PIN_Type);
int HCI_WritePINType(int fd, __u8 PIN_Type);
int HCI_CreateNewUnitKey(int fd);
int HCI_ReadStoredLinkKey(int fd, BD_ADDR *bda, __u8 Read_All_Flag, __u16 *Max_Num_Keys, __u16 *Num_Keys, struct Link_Key *Link_Keys);
int HCI_WriteStoredLinkKey(int fd, __u8 Num_Keys_To_Write, struct Link_Key *Link_Keys, __u8 *Num_Keys_Written);
int HCI_DeleteStoredLinkKey(int fd, BD_ADDR *bda, __u8 Delete_All_Flag, __u16 *Num_Keys_Deleted);
int HCI_ReadTransmitPowerLevel(int fd, __u16 Connection_Handle, __u8 Type, __s8 *Transmit_Power_Level);
int HCI_ChangeLocalName(int fd, char *Name);
int HCI_ReadLocalName(int fd, char *Name);
int HCI_ReadPageTimeout(int fd, __u16 *Page_Timeout);
int HCI_WritePageTimeout(int fd, __u16 Page_Timeout);
int HCI_ReadScanEnable(int fd, __u8 *Scan_Enable);
int HCI_WriteScanEnable(int fd, __u8 Scan_Enable);
int HCI_ReadPageScanActivity(int fd,  __u16 *Page_Scan_Interval, __u16 *Page_Scan_Window);
int HCI_WritePageScanActivity(int fd, __u16 Page_Scan_Interval, __u16 Page_Scan_Window);
int HCI_ReadInquiryScanActivity(int fd, __u16 *Inquiry_Scan_Interval, __u16 *Inquiry_Scan_Window);
int HCI_WriteInquiryScanActivity(int fd, __u16 Inquiry_Scan_Interval, __u16 Inquiry_Scan_Window);
int HCI_ReadAuthenticationEnable(int fd, __u8 *Authentication_Enable);
int HCI_WriteAuthenticationEnable(int fd, __u8 Authentication_Enable);
int HCI_ReadEncryptionMode(int fd, __u8 *Encryption_Mode);
int HCI_WriteEncryptionMode(int fd, __u8 Encryption_Mode);
int HCI_ReadClassOfDevice(int fd, __u32 *Class_of_Device);
int HCI_WriteClassOfDevice(int fd, __u32 Class_of_Device);
int HCI_ReadVoiceSetting(int fd, __u16 *Voice_Setting);
int HCI_WriteVoiceSetting(int fd, __u16 Voice_Setting);
int HCI_ReadSCOFlowControlEnable(int fd, __u8 *Flow_Control);
int HCI_WriteSCOFlowControlEnable(int fd, __u8 Flow_Control);
int HCI_ReadHoldModeActivity(int fd, __u8 *Hold_Mode_Activity);
int HCI_WriteHoldModeActivity(int fd, __u8 Hold_Mode_Activity);
int HCI_ReadLinkSupervisionTimeout(int fd, __u16 Connection_Handle, __u16 *Link_Supervision_Timeout);
int HCI_WriteLinkSupervisionTimeout(int fd, __u16 Connection_Handle, __u16 Link_Supervision_Timeout);
int HCI_ReadNumberOfSupportedIAC(int fd, __u8 *Num_Supported_IAC);
int HCI_ReadCurrentIACLAP(int fd, __u8 *Num_Current_IAC, __u32 *IAC_LAP);
int HCI_WriteCurrentIACLAP(int fd, __u8 Num_Current_IAC, __u32 *IAC_LAP);
int HCI_ReadPageScanPeriodMode(int fd, __u8 *Page_Scan_Period_Mode);
int HCI_WritePageScanPeriodMode(int fd, __u8 Page_Scan_Period_Mode);
int HCI_ReadPageScanMode(int fd, __u8 *Page_Scan_Mode);
int HCI_WritePageScanMode(int fd, __u8 Page_Scan_Mode);

// Informational
int HCI_ReadLocalVersionInformation(int fd, __u8 *HCI_Version, __u16 *HCI_Revision,__u8 *LMP_Version, __u16 *Manufacture_Name, __u16 *LMP_Subversion);
int HCI_ReadLocalSupportedFeatures(int fd, __u64 *LMP_Features);
int HCI_ReadBufferSize(int fd, __u16 *HC_ACL_Data_Packet_Length, __u8 *HC_SCO_Data_Packet_Length, __u16 *Total_Num_ACL_Data_Packets, __u16 *Total_Num_SCO_Data_Packets);
int HCI_ReadCountryCode(int fd, int *Country_Code);
int HCI_ReadBDAddr(int fd, BD_ADDR *bda);
int HCI_GetLinkQuality(int fd, __u16 Connection_Handle, __u8 *Link_Quality);
int HCI_ReadRSSI(int fd, __u16 Connection_Handle, __s8 *RSSI);

// Testing
int HCI_ReadLoopbackMode(int fd, __u8 *mode);
int HCI_WriteLoopbackMode(int fd, __u8 mode);
int HCI_EnableDeviceUnderTestMode(int fd);

// Ericsson Specific
int HCI_EricssonWritePCMSettings(int fd, __u8 Settings);
int HCI_EricssonSetSCODataPath(int fd, __u8 Path);

/* Affix specific */
int HCI_WriteAudioSetting(int fd, int mode, __u16 setting);
int HCI_WriteSecurityMode(int fd, int Security_Mode);

#ifdef CONFIG_AFFIX_BT_1_2

int HCI_Iquiry_RSSI(int fd, __u8 Inquiry_Length, __u8 Max_Num_Responses, INQUIRY_RSSI_ITEM *Items, __u8 *Num_Responses);

// Link Control 
int HCI_CreateConnectionCancel(int fd, BD_ADDR* bda);
int HCI_RemoteNameRequestCancel(int fd, BD_ADDR* bda);
int HCI_ReadLMPHandle(int fd, __u16 Connection_Handle, __u8* LMP_Handle);
int HCI_ReadRemoteExtendedFeatures(int fd, __u16 Connection_Handle, __u8 Page_Number, __u8* Maximum_Page_Number, __u8* Extended_LMP_Features);
int HCI_SetupSynchronousConnectionCreate(int fd, __u16 Connection_Handle, SYNC_CON_REQ *param, SYNC_CON_RES *res);
int HCI_AcceptSynchronousConnectionRequest(int fd, ACCEPT_SYNC_CON_REQ* req, SYNC_CON_RES* res);
int HCI_RejectSynchronousConnectionRequest(int fd, BD_ADDR* bda, __u8 Reason);
int __HCI_SetupSynchronousConnectionCreate(int fd, __u16 Connection_Handle, SYNC_CON_REQ *param);
int __HCI_AcceptSynchronousConnectionRequest(int fd, ACCEPT_SYNC_CON_REQ* req);
int __HCI_RejectSynchronousConnectionRequest(int fd, BD_ADDR* bda, __u8 Reason);


// Link Policy Commands
int HCI_ReadDefaultLinkPolicySettings(int fd, __u16* Default_Link_Policy_Settings);
int HCI_WriteDefaultLinkPolicySettings(int fd, __u16 Default_Link_Policy_Settings);
int HCI_Flow_Specification(int fd, __u16 Connection_Handle, HCI_FLOW* Requested_Flow, HCI_FLOW* Completed_Flow);
int __HCI_Flow_Specification(int fd, __u16 Connection_Handle, HCI_FLOW* Requested_Flow);

// Controller & Baseban Commands
int HCI_SetAFHHostChannelClassification(int fd, __u8* AFH_Host_Channel_Classification);
int HCI_ReadInquiryScanType(int fd, __u8* Inquiry_Scan_Type);
int HCI_WriteInquiryScanType(int fd, __u8 Scan_Type);
int HCI_ReadInquiryMode(int fd, __u8* Inquiry_Mode);
int HCI_WriteInquiryMode(int fd, __u8 Inquiry_Mode);
int HCI_ReadPageScanType(int fd, __u8* Page_Scan_Type);
int HCI_WritePageScanType(int fd, __u8 Page_Scan_Type);
int HCI_ReadAFHChannelAssessmentMode(int fd, __u8* AFH_Channel_Assessment_Mode);
int HCI_WriteAFHChannelAssessmentMode(int fd, __u8 AFH_Channel_Assessment_Mode);

//Informational Parameters
int HCI_ReadLocalExtendedFeatures(int fd, __u8* Page_Number,__u8* Maximum_Page_Number,__u64* Extended_LMP_Features);

//Status Parameters
int HCI_ReadAFHChannelMap(int fd, __u16 Connection_Handle, __u8* AFH_Mode,__u8* AFH_Channel_Map);
int HCI_ReadClock(int fd, __u16 Connection_Handle, __u8 Which_Clock, __u32* Clock, __u16* Accuracy);

#endif // CONFIG_AFFIX_BT_1_2

#ifdef  __cplusplus
} 
#endif


#endif
