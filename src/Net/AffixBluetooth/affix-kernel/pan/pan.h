/* -*-   Mode:C; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Muller Ulrich <ulrich.muller@nokia.com>

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
  $Id: pan.h,v 1.35 2004/05/17 16:11:11 kassatki Exp $
 
  pan.c - PAN kernel module

   This module implements a PAN User, Group Ad-Hoc Network and Network Access Point.
   The role is setup at startup. Functionality:
   PAN User:
   - registers net device "panx"
   - register BNEP layer as upper layer of L2CAP of Bluetooth device "bt0"
   - create connection to GAN/NAP
   - pass all packets from BNEP to net device and vica versa
   Group Ad-Hoc Network/Network Access Point:
   - registers net device "panx"
   - register BNEP layer as upper layer of L2CAP of Bluetooth device "bt0"
   - accept connections from PAN Users
   - route packets between net device and all BNEP connections
   A NAP passes all packets to the net device that are not addressed to a member
   of the Piconet; in contract, a GAN dropps these packets.

   The module gets initialized and closed with the init_module() and cleanup_module()
   fuctions at the bottom. It registeres an upper L2CAP layer (pan_* functions) immediatley.
   The interface to the net device are the pan_net_* functions. btd_* functions are used to
   manage the connections. Connections are initiated by the connect_thread(). The main
   reason to put the connection initiation into a thread is that it runs in normal context,
   whereas so_* and pan_net_* functions are usually called in restricted context, e.g. interrupts
   disabled. Thus the connection thread can have more functionality, e.g. call HCI functions.

*/

/* includes */

#ifndef _PAN_H
#define _PAN_H

#include <linux/if.h>
#include <linux/netdevice.h>

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>

/* constants */
#define BNEPPSM			0x000F	// added according to Bluetooth Assign numbers
#define PAN_MTU			1691
#define PAN_LINK_TIMEOUT	0x7D00	/* default value, 20 secs */

#define pan_hard_header_len	16

extern l2cap_proto_ops l2cap_ops;


/* constants */

/* class of device field according Bluetooth Assigned Numbers (network byte order)*/
extern __u32 HCI_COD_NETCOMP;
extern __u32 HCI_COD_LAP_LOAD[8]; /* a LAN access point with 0-7 connections */


/* typedefs */

/* PAN devices: PAN User, Network Access Point, Group Ad-hoc Network */

typedef enum {unconfigured, configured} State;
/* status of L2CAP connection */

/* This data structure represents a PAN bluetooth interface */
struct pan_dev {
	struct list_head	q;			/* for queueing */
	
	hci_struct		*hci;			/* handle to access device */
	int			fd;
	BD_ADDR			bdaddr;			/* hardware device address */
	int			role;			/* role in PAN */
	int                     peer_role;              /* expected role of peers */
	int			mode;
	//struct sock		sk;
	int			sndbuf;			/* Size of send buffer in bytes		*/
	atomic_t		wmem_alloc;		/* Transmit queue bytes committed	*/

	struct net_device	*net_dev;		/* network device */
	struct net_device_stats	stats;			/* statistics of network device */

	atomic_t		conn_count;		// active connection counter
	btlist_head_t		connections;		/* list of all active connections */
	int			connections_counter_old;/* counter of all active connections at last CoD/Scan update */

	int			thread_stop;		/* indicates inquiry thread to terminate */
	struct semaphore	thread_sem;		/* indicates if inquiry thread is running */

	protocol_filter		pf;			/* settings of local protocol filter */
	multicast_filter	mf;			/* settings of local multicast filter */
};

/* double linked list of all active connections, required for a clean shutdown */
struct bnep_con {
	struct list_head	q;		/* for queueing */
	
	l2cap_ch		*ch;		/* L2CAP connection handle to identify connection */
	struct pan_dev		*btdev;		/* link to device that links to this connection list */
	State			state;		/* connection state */
	void			*priv;		/* internal data of bnep layer */
	
	/* BNEP stuff */

	struct timer_list	timer_setup;			/* detect setup control timeout */
	struct timer_list	timer_filter;			/* detect filter control timeout */

	/* the local filter settings are stored in the pan_dev struct of the local bluetooth device.
	   local filters are never applied, but transfered to remote devices.
	   filter settings for each remote device are stored here: */
	protocol_filter		pf;				/* remote protocol filter */
	multicast_filter	mf;				/* remote multicast filter */
	/* according to the spec, we are allowed to reject a remote filter request "due to security reasones".
	   this is currently always allowded: */
	int			filter_protocol_admitted;	/* remote side is allowed to set protocol filter */
	int			filter_multicast_admitted;	/* remote side is allowed to set multicast filter */
	/* if the local filter settings are changed, they must be sent to all remote devices. while waiting for the response,
	   they may get updated again. here we store the state of the remote filter settings: */
	int			filter_protocol_pending;	/* remote side has not yet accepted protocol filter */
	int			filter_multicast_pending;	/* remote side has not yet accepted multicast filter */
	/* possible states are: */
#define filter_done		0	/* filter is set */
#define filter_error		1	/* filter could not be set and was reset */
#define filter_pending		2	/* we are waiting for filter response */
#define filter_updated  	3	/* filter has changed and must be updated */
#define filter_unsupported	4	/* remote device does not support filters */

	int			setup_complete;			/* connection is setup, so we can send data */
};


struct bcast_list {
	__u16		psm;		/* destination PSM for broadcast */
	int		counter;	/* number of connection entrys in use */
	l2cap_ch	*con[7];	/* list of connection handles to target devices */
};

struct pan_skb_cb {
	int	outgoing;
};

#define pan_cb(skb)	((struct pan_skb_cb*)(skb)->cb)

/* functions */

/* called to transmit packet to net device */
void pan_net_receive(struct sk_buff *skb, struct pan_dev *btdev);

int pan_connect_req(struct pan_dev *btdev, BD_ADDR *bdaddr, __u16 psm);
int pan_DataWriteBroadcast(__u16 psm, struct sk_buff *skb);

/* convert bluetooth addr to string */
char *bda2str(BD_ADDR *bda);

/* convert bluetooth CoD to string */
char *BD_CLASS2str(__u32 bdc);

/* compare BD_ADDR with reverse ordered ethernet addr */
int ethbdacmp(BD_ADDR *bda, void *eta);

int pan_deliver_event(struct pan_dev *btdev, int event);
void pan_skb_set_owner_w(struct sk_buff *skb, struct pan_dev *btdev);

static inline int pan_default_peer_role(int role)
{
	switch (role) {
	case AFFIX_PAN_PANU:
		return AFFIX_PAN_NAP;
	case AFFIX_PAN_NAP:
	case AFFIX_PAN_GN:
		return AFFIX_PAN_PANU;
	default:
		return 0;
	}
}

static inline void pan_hold(struct pan_dev *btdev)
{
	dev_hold(btdev->net_dev);
}

static inline void pan_put(struct pan_dev *btdev)
{
	dev_put(btdev->net_dev);
}

void pan_check_link(struct pan_dev *btdev, int added);

#endif

