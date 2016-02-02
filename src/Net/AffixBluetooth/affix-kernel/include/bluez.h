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
   $Id: bluez.h,v 1.29 2004/02/22 18:36:32 kassatki Exp $

   BlueZ -> Affix adoptation layer

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/


#ifndef _AFFIX_BLUEZ_H
#define _AFFIX_BLUEZ_H

#ifndef FILEBIT
#define FILEBIT	DBDRV
#endif

#include <linux/netdevice.h>

#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/bluetooth.h>

#define BT_INFO(fmt, arg...) printk(KERN_INFO "Bluetooth: " fmt "\n" , ## arg)
#define BT_ERR(fmt, args...)	printk(KERN_ERR "%s: " fmt "\n", __FUNCTION__ , ##args)

#ifdef CONFIG_AFFIX_DEBUG
	#define BT_DBG(fmt, args...) \
	{ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & (DBCTRL))) { \
			if (affix_dbmask & DBFNAME) \
				printk(KERN_DEBUG "%s: " fmt "\n", __FUNCTION__ , ##args); \
			else \
				printk(KERN_DEBUG fmt "\n" , ##args); \
		} \
	}
#else
#define BT_DBG(fmt, args...)
#endif /* CONFIG_AFFIX_DEBUG */

/* HCI device states */
enum hci_dev_states_t
{
	HCI_RUNNING = 1,
};

#define BLUEZ_SKB_RESERVE	8
#define BT_SKB_RESERVE       8

#define HCI_MAX_FRAME_SIZE	2048

/* Skb helpers */
struct bt_skb_cb {
	int    incoming;
};
#define bt_cb(skb) ((struct bt_skb_cb *)(skb->cb)) 


/* HCI Packet types */
#define HCI_COMMAND_PKT		0x01
#define HCI_ACLDATA_PKT 	0x02
#define HCI_SCODATA_PKT 	0x03
#define HCI_EVENT_PKT		0x04

/* --------  HCI Packet structures  -------- */
#define HCI_TYPE_LEN		1
#define HCI_COMMAND_HDR_SIZE 	3
#define HCI_EVENT_HDR_SIZE 	2
#define HCI_ACL_HDR_SIZE 	4
#define HCI_SCO_HDR_SIZE 	3

struct hci_command_hdr {
	__u16 	opcode;		/* OCF & OGF */
	__u8 	plen;
} __attribute__ ((packed));

struct hci_event_hdr {
	__u8 	evt;
	__u8 	plen;
} __attribute__ ((packed));

struct hci_acl_hdr {
	__u16 	handle;		/* Handle & Flags(PB, BC) */
	__u16 	dlen;
} __attribute__ ((packed));

struct hci_sco_hdr {
	__u16 	handle;
	__u8 	dlen;
} __attribute__ ((packed));



/* ----- HCI Devices ----- */

struct hci_dev_stats {
	__u32 err_rx;
	__u32 err_tx;
	__u32 cmd_tx;
	__u32 evt_rx;
	__u32 acl_tx;
	__u32 acl_rx;
	__u32 sco_tx;
	__u32 sco_rx;
	__u32 byte_rx;
	__u32 byte_tx;
};

struct hci_dev {
	char			name[8];
	void			*driver_data;
	unsigned long 		flags;
	__u8	 		type;
	struct hci_dev_stats 	stat;
	struct module		*owner;
	
	int (*open)(struct hci_dev *hdev);
	int (*close)(struct hci_dev *hdev);
	int (*flush)(struct hci_dev *hdev);
	int (*send)(struct sk_buff *skb);

	void (*destruct)(struct hci_dev *hdev);
	int (*ioctl)(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);

	/* Affix */
	hci_struct		*hci;
};

static inline int hci_dev_open(hci_struct *hci)
{
	int		err;
	struct hci_dev	*hdev = hci->priv;

	DBFENTER;
	DBPRT("opening device ...\n");
	err = hdev->open(hdev);
	DBPRT("device opened: %d\n", err);
	if (err)
		return err;
	hcidev_start_queue(hci);
	DBFEXIT;
	return 0;
}

static inline int hci_dev_stop(hci_struct *hci)
{
	struct hci_dev	*hdev = hci->priv;
	int		err;

	hcidev_stop_queue(hci);
	err = hdev->close(hdev);
	return err;
}

static inline int hci_dev_ioctl(hci_struct *hci, int cmd, void *arg)
{
	DBFENTER;
	switch (cmd) {
	default:
		return -ENOIOCTLCMD;
	}
	DBFEXIT;
	return 0;
}


static inline int hci_dev_xmit(hci_struct *hci, struct sk_buff *skb)
{
	struct hci_dev	*hdev = hci->priv;
	int		err;

	DBFENTER;
	hci->trans_start = jiffies;
	skb->dev = (void*)hdev;
	err = hdev->send(skb);
	DBPRT("packet sent to driver: %d\n", err);
	DBFEXIT;
        return err;
}

/* Register HCI device */
static inline int hci_register_dev(struct hci_dev *hdev)
{
	hci_struct	*hci;
	int		err;

	DBFENTER;
	hci = hcidev_alloc();
	if (hci == NULL)
		return -ENOMEM;
	/* bluez stuff */
	memset(&hdev->stat, 0, sizeof(struct hci_dev_stats));
	hdev->flags = 0;
	hdev->hci = hci;

	/* Affix stuff */
	hci->priv = hdev;	/* set private pointer */
	hci->open = hci_dev_open;
	hci->close = hci_dev_stop;
	hci->ioctl = hci_dev_ioctl;
	hci->send = hci_dev_xmit;
	hci->hdrlen = BLUEZ_SKB_RESERVE;
	hci->type = hdev->type;
	hci->owner = THIS_MODULE;
	err = hcidev_register(hci, NULL);
	DBFEXIT;
	return (err)? err : hci->devnum;
}

/* Unregister HCI device */
static inline int hci_unregister_dev(struct hci_dev *hdev)
{
	DBFENTER;
	hcidev_unregister(hdev->hci);
	hdev->hci = NULL;
	DBFEXIT;
	return 0;
}

/* Receive frame from HCI drivers */
static inline int hci_recv_frame(struct sk_buff *skb)
{
	struct hci_dev *hdev = (struct hci_dev *) skb->dev;

	DBFENTER;
	if (!hdev) {
		kfree_skb(skb);
		return -1;
	}
	hcidev_rx(hdev->hci, skb);	/* send to upper protocol layer */
	DBFEXIT;
	return 0;
}

static inline struct sk_buff *bt_skb_alloc(unsigned int len, int how)
{
	struct sk_buff *skb;

	if ((skb = alloc_skb(len + BT_SKB_RESERVE, how))) {
		skb_reserve(skb, BT_SKB_RESERVE);
		bt_cb(skb)->incoming  = 0;
	}
	return skb;
}


#endif

