/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
   
   Original Author: Imre Deak <ext-imre.deak@nokia.com>

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
   $Id: btusb.c,v 1.4 2004/05/13 17:41:44 kassatki Exp $

   Physical protocol layer for USB Bluetooth devices

   Fixes:
   		Dmitry Kasatkin		:
*/		

#define __NO_VERSION__

#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>

#include <linux/skbuff.h>

#include <linux/usb.h>

#define FILEBIT DBDRV		// for sake of selective debug messages

#include <affix/btdebug.h>
#include <affix/hci.h>

#ifndef USB_ZERO_PACKET
#define USB_ZERO_PACKET		0
#endif


/* Vendor/Product codes */
#define VENDOR_ID_DIGIANSWER			0x8fd
#define DEVICE_ID_DIGIANSWER_USB_BT_DONGLE	1

/* Class, SubClass, and Protocol codes that describe a Bluetooth device */
#define WIRELESS_CLASS_CODE			0xe0
#define RF_SUBCLASS_CODE			0x01
#define BLUETOOTH_PROGRAMMING_PROTOCOL_CODE	0x01

/* Sizes */
/* ACL */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 4, 8)
#define BTUSB_ACL_RX_URB		1
#else
//#define BTUSB_ACL_RX_URB		8
#define BTUSB_ACL_RX_URB		1
#endif

#define BTUSB_ACL_RX_SIZE		2048
#define BTUSB_ACL_TX_URB		4

/* SCO RX*/
#define BTUSB_SCO_RX_URB		1
//#define BTUSB_SCO_RX_SIZE		32
#define BTUSB_SCO_RX_PACKETS		10

/* SCO TX */
#define BTUSB_SCO_TX_URB		1
//#define BTUSB_SCO_TX_SIZE		8
#define BTUSB_SCO_TX_PACKETS		10

/*  Maximum number of consecutive failed USB reads before giving up. */
#define MAX_READ_FAIL_COUNT		100

typedef struct _usb_bt_params
{
	int	uart_leading_byte;	// if there is a uart-style leading byte for packets
	__u8	control_req_type;
	int	vendor_id;
	int	device_id;
} usb_bt_params_t;

usb_bt_params_t usb_bt_list[] = {
	{
		uart_leading_byte : 1,
		control_req_type : 0x40,
		vendor_id : VENDOR_ID_DIGIANSWER,
		device_id : DEVICE_ID_DIGIANSWER_USB_BT_DONGLE
	},
        {       uart_leading_byte : 0,		// default last descriptor
                control_req_type : 0x20,
                vendor_id : 0,
                device_id : 0 
        }
};

#define NUM_OF_USB_BT_DEV (sizeof(usb_bt_list) / sizeof(usb_bt_list[0]))	

static struct usb_device_id btusb_ids [] = {
	/* Generic Bluetooth USB device */
	{ USB_DEVICE_INFO(WIRELESS_CLASS_CODE,
			RF_SUBCLASS_CODE, BLUETOOTH_PROGRAMMING_PROTOCOL_CODE)},

	/* Ericsson with non-standard id */
	{ USB_DEVICE(0x0bdb, 0x1002) },
	
	/* Bluetooth Ultraport Module from IBM */
	{ USB_DEVICE(0x04bf, 0x030a) },

	{ }
};

MODULE_DEVICE_TABLE (usb, btusb_ids);

static struct usb_device_id btusb_ignore_ids[] = {
	/* Broadcom BCM2033 without firmware */
	{ USB_DEVICE(0x0a5c, 0x2033) },

	{ }	/* Terminating entry */
};

static int btusb_probe(struct usb_interface *intf, const struct usb_device_id *id);
static void btusb_disconnect(struct usb_interface *intf);

static struct usb_driver btusb_driver =
{
	.owner		= THIS_MODULE,
	.name		= "affix_usb",
	.probe		= btusb_probe,
	.disconnect	= btusb_disconnect,
	.id_table	= btusb_ids
};

static void btusb_tx_complete(struct urb *urb, struct pt_regs *);
static void btusb_rx_complete(struct urb *urb, struct pt_regs *);


/* request data structure */
typedef struct btusb_req {
	struct btusb		*btusb;
#define BTUSB_XMIT_WAKEUP		0
#define BTUSB_XMIT_PROCESS	1
	unsigned long		flags;
	rwlock_t		callback_lock;
//	spinlock_t		xmit_lock;
	unsigned int		pipe;
	__u8			interval;

	/* rx state */
	struct sk_buff		*rx_skb;
	/* tx state */
	struct sk_buff_head	queue;
	struct sk_buff_head	pending;
	
} btusb_req_t;

typedef struct {
	struct urb	*urb;	
	btusb_req_t	*req;
	int		pkt_len;
} btusb_skb_cb;

#define btusb_cb(skb)	((btusb_skb_cb*)skb->cb)

/* driver data structure */
typedef struct btusb {
	struct list_head	q;	/* for queueing */
	
	int			signature;
	struct usb_device	*usb_dev;
	hci_struct		*hci;
	usb_bt_params_t		*params;
#define BTUSB_RUNNING		0x00
#define BTUSB_AUDIO_ON		0x01
	unsigned long		flags;
	
	btusb_req_t		ctrl_tx_req;
	btusb_req_t		int_rx_req;
	btusb_req_t		bulk_rx_req;
	btusb_req_t		bulk_tx_req;
#if defined(CONFIG_AFFIX_SCO)
	btusb_req_t		isoc_rx_req;
	btusb_req_t		isoc_tx_req;
	int			isoc;
#endif
	int			read_fail_cnt;
} btusb_t;


static void btusb_init_req(btusb_req_t *req, btusb_t *btusb)
{
	req->btusb = btusb;
	rwlock_init(&req->callback_lock);
//	spin_lock_init(&req->xmit_lock);
	skb_queue_head_init(&req->queue);
	skb_queue_head_init(&req->pending);
	req->rx_skb = NULL;
}

static void btusb_release_req(btusb_req_t *req)
{
	struct sk_buff	*skb;
	struct urb	*urb;
	int		err;
	unsigned long	flags;

	DBFENTER;
	write_lock_irqsave(&req->callback_lock, flags);
	write_unlock_irqrestore(&req->callback_lock, flags);
	/* purge data queue */
	DBPRT("data queue len: %d\n", skb_queue_len(&req->queue));
	while ((skb = skb_dequeue(&req->queue)))
		dev_kfree_skb_any(skb);
	/* purge urb queue */
	DBPRT("pending queue len: %d\n", skb_queue_len(&req->pending));
	while ((skb = skb_dequeue(&req->pending))) {
		urb = btusb_cb(skb)->urb;
		DBPRT("unlinking urb... ");
		err = usb_unlink_urb(urb);
		_DBPRT("done: %d\n", err);
		if (!err) {
			if (usb_pipecontrol(urb->pipe)) {
				DBPRT("deleting setup... ");
				kfree(urb->setup_packet);
				_DBPRT("done\n");
			}
			DBPRT("freeing urb... ");
			usb_free_urb(urb);
			_DBPRT("done\n");
		}
		DBPRT("deleting skb... ");
		dev_kfree_skb_any(skb);		
		_DBPRT("done\n");
	}
	if (req->rx_skb) {
		DBPRT("freeing rx_skb... ");
		dev_kfree_skb_any(req->rx_skb);
		req->rx_skb = NULL;
		_DBPRT("done\n");
	}
	DBFEXIT;
}


static int btusb_submit_urb(btusb_req_t *req, struct urb *urb, int del)
{
	int	err;
	skb_queue_tail(&req->pending, urb->context);
	err = usb_submit_urb(urb, GFP_ATOMIC);
	if (err) {
		skb_unlink(urb->context);
		if (del)
			dev_kfree_skb_any(urb->context);
		if (usb_pipecontrol(urb->pipe))
			kfree(urb->setup_packet);
		usb_free_urb(urb);
	}
	return err;
}

#if defined(CONFIG_AFFIX_SCO)
static void __usb_fill_isoc_urb(struct urb *urb, struct usb_device *dev,
		unsigned int pipe, struct sk_buff *skb, int mtu, usb_complete_t complete, int interval) 
{
	int 	k, left = skb->len, offset = 0;

	for (k = 0; left; k++) {
		urb->iso_frame_desc[k].offset = offset;
		urb->iso_frame_desc[k].length = (left > mtu) ? mtu : left;
		left -= urb->iso_frame_desc[k].length;
		offset += urb->iso_frame_desc[k].length;
	}
	spin_lock_init(&urb->lock);
	urb->dev = dev;
	urb->pipe = pipe;
	urb->complete = complete;
	urb->context = skb;
	urb->transfer_flags = URB_ISO_ASAP;
	urb->number_of_packets = k;
	urb->transfer_buffer = skb->data;
	urb->transfer_buffer_length = skb->len;
	urb->interval = interval;
}

int btusb_setup_isoc_rx(btusb_req_t *req)
{
	int		err;
	struct urb	*urb;
	struct sk_buff	*skb;
	int		mtu;
	unsigned long	flags;

	DBFENTER;
	write_lock_irqsave(&req->callback_lock, flags);
#ifdef BTUSB_SCO_RX_SIZE
	err = -EINVAL;
	mtu = BTUSB_SCO_RX_SIZE;
	if (mtu <= 0)
		goto exit;
#else
	mtu = usb_maxpacket(req->btusb->usb_dev, req->pipe, 0);
#endif
	DBPRT("mtu: %d\n", mtu);
	err = -ENOMEM;
	urb = usb_alloc_urb(BTUSB_SCO_RX_PACKETS, GFP_ATOMIC);
	if (!urb)
		goto exit;
	skb = dev_alloc_skb(BTUSB_SCO_RX_PACKETS * mtu);
	if (!skb) {
		usb_free_urb(urb);
		goto exit;
	}
	skb->pkt_type = HCI_SCO;
	skb_put(skb, BTUSB_SCO_RX_PACKETS * mtu);
	__usb_fill_isoc_urb(urb, req->btusb->usb_dev, req->pipe, skb, mtu, 
			btusb_rx_complete, req->interval);
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 1);
exit:
	write_unlock_irqrestore(&req->callback_lock, flags);
	DBFEXIT;
	return err;
}

int btusb_audio_mode(btusb_t *btusb, int audio_mode)
{
	int		err = 0, i;

	DBFENTER;
	if (!btusb->isoc)
		return -ENODEV;
	if (audio_mode & AFFIX_AUDIO_ON) { 
		if (!test_bit(BTUSB_RUNNING, &btusb->flags)) {
			err = -ENODEV;
			goto exit;
		}
		if (test_and_set_bit(BTUSB_AUDIO_ON, &btusb->flags))
			goto exit;
		DBPRT("Setting up audio mode...\n");
		err = usb_set_interface(btusb->usb_dev, 1, btusb->isoc - 1);
		if (err) {
			BTERROR("usb_set_interface(dev, 1, %d) failed", btusb->isoc - 1);
			goto exit;
		}
		for (i = 0; i < BTUSB_SCO_RX_URB; i++) {
			err = btusb_setup_isoc_rx(&btusb->isoc_rx_req);
			if (err) {
				BTERROR("setup isoc pipes failed with status %d\n", err);
				clear_bit(BTUSB_AUDIO_ON, &btusb->flags);
				btusb_release_req(&btusb->isoc_rx_req);
				goto exit;
			}
		}
		DBPRT("Setting up audio mode completed\n");
	} else if (test_and_clear_bit(BTUSB_AUDIO_ON, &btusb->flags)) {
		btusb_release_req(&btusb->isoc_rx_req);
		btusb_release_req(&btusb->isoc_tx_req);
	}
exit:
	DBFEXIT;
	return err;
}
#endif

int btusb_setup_int(btusb_req_t *req)
{
	int		err;
	struct urb	*urb;
	struct sk_buff	*skb;
	int		mtu;
	unsigned long	flags;

	DBFENTER;
	write_lock_irqsave(&req->callback_lock, flags);
	err = -EINVAL;
	mtu = usb_maxpacket(req->btusb->usb_dev, req->pipe, 0);
	if (mtu <= 0)
		goto exit;
	err = -ENOMEM;
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		goto exit;
	skb = dev_alloc_skb(mtu);
	if (!skb) {
		usb_free_urb(urb);
		goto exit;
	}
	skb->pkt_type = HCI_EVENT;
	skb_put(skb, mtu);
	usb_fill_int_urb(urb, req->btusb->usb_dev, req->pipe,
			skb->data, skb->len,
			btusb_rx_complete, skb, req->interval);
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 1);
exit:
	write_unlock_irqrestore(&req->callback_lock, flags);
	DBFEXIT;
	return err;
}

int btusb_setup_bulk_rx(btusb_req_t *req)
{
	int		err;
	struct urb	*urb;
	struct sk_buff	*skb;
	int		mtu;
	unsigned long	flags;

	DBFENTER;
	write_lock_irqsave(&req->callback_lock, flags);
#if BTUSB_ACL_RX_URB > 2
	err = -EINVAL;
	mtu = usb_maxpacket(req->btusb->usb_dev, req->pipe, 0);
	if (mtu <= 0)
		goto exit;
#else
	mtu = BTUSB_ACL_RX_SIZE;
#endif
	err = -ENOMEM;
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		goto exit;
	skb = dev_alloc_skb(mtu);
	if (!skb) {
		usb_free_urb(urb);
		goto exit;
	}
	skb->pkt_type = HCI_ACL;
	skb_put(skb, mtu);
	usb_fill_bulk_urb(urb, req->btusb->usb_dev, req->pipe,
			skb->data, skb->len,
			btusb_rx_complete, skb);
	urb->transfer_flags = URB_NO_INTERRUPT;	// BULK
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 1);
exit:
	write_unlock_irqrestore(&req->callback_lock, flags);
	DBFEXIT;
	return err;
}

/* -------------------------- XMIT ------------------------- */

int btusb_xmit_bulk(btusb_req_t *req, struct sk_buff *skb)
{
	int		err;
	struct urb	*urb;

	DBFENTER;
	if (!test_bit(BTUSB_RUNNING, &req->btusb->flags))
		return -1;
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		return -ENOMEM;
	usb_fill_bulk_urb(urb, req->btusb->usb_dev, req->pipe,
			skb->data, skb->len, btusb_tx_complete, skb);
	urb->transfer_flags = URB_NO_INTERRUPT | URB_ZERO_PACKET;
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 0);
	DBFEXIT;
	return err;
}

#if defined(CONFIG_AFFIX_SCO)
int btusb_xmit_isoc(btusb_req_t *req, struct sk_buff *src_skb)
{
	int		err;
	struct urb	*urb;
	struct sk_buff	*skb = NULL;
	int		num_packets, len, mtu;

	DBFENTER;
#ifdef BTUSB_SCO_TX_SIZE
	mtu = BTUSB_SCO_TX_SIZE;
#else
	mtu = usb_maxpacket(req->btusb->usb_dev, req->pipe, 1);
	DBPRT("mtu: %d\n", mtu);
#endif
	if (mtu <= 0)
		return -EINVAL;
	num_packets = src_skb->len / mtu;
	if (src_skb->len % mtu)
		num_packets++;
	if (num_packets > BTUSB_SCO_TX_PACKETS) {
		num_packets = BTUSB_SCO_TX_PACKETS;
		len = num_packets * mtu;
		skb = dev_alloc_skb(len);
		if (!skb)
			return -ENOMEM;
		skb->pkt_type = HCI_SCO;
		memcpy(skb_put(skb, len), src_skb->data, len);
	} else {
		skb = skb_get(src_skb);
	}
	urb = usb_alloc_urb(num_packets, GFP_ATOMIC);
	if (!urb) {
		kfree_skb(skb);
		return -ENOMEM;
	}
	__usb_fill_isoc_urb(urb, req->btusb->usb_dev, req->pipe, skb, mtu, 
			btusb_tx_complete, req->interval);
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 1);
	if (!err)
		skb_pull(src_skb, skb->len);	//skb->len == 0 if src_skb == skb
	DBFEXIT;
	return err;
}
#endif

int btusb_xmit_ctrl(btusb_req_t *req, struct sk_buff *skb)
{
	int		err;
	struct urb	*urb;
	struct usb_ctrlrequest	*dr;	/* for setup packets */
	DBFENTER;
	if (!test_bit(BTUSB_RUNNING, &req->btusb->flags))
		return -1;
	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb)
		return -ENOMEM;
	dr = kmalloc(sizeof(*dr), GFP_ATOMIC);
	if (!dr) {
		usb_free_urb(urb);
		return -ENOMEM;
	}
	memset(dr, 0, sizeof(*dr));
	dr->bRequestType = req->btusb->params->control_req_type;
	dr->wLength = cpu_to_le16(skb->len);
	usb_fill_control_urb(urb, req->btusb->usb_dev, req->pipe,
			(unsigned char*)dr, skb->data, skb->len, 
			btusb_tx_complete, skb);
	btusb_cb(skb)->urb = urb;
	btusb_cb(skb)->req = req;
	err = btusb_submit_urb(req, urb, 0);
	DBFEXIT;
	return err;
}

/* it's called under spinlock */
static inline int btusb_xmit_start(btusb_req_t *req)
{
	int		err = 0;
	struct sk_buff	*skb = NULL;

	DBFENTER;
	if (usb_pipebulk(req->pipe)) {
		while (!err && skb_queue_len(&req->pending) <= BTUSB_ACL_TX_URB && 
				(skb = skb_dequeue(&req->queue))) {
			err = btusb_xmit_bulk(req, skb);
		}
	} else if (usb_pipecontrol(req->pipe)) {
		if (skb_queue_empty(&req->pending) &&
				(skb = skb_dequeue(&req->queue)))
			err = btusb_xmit_ctrl(req, skb);
	}
#if defined(CONFIG_AFFIX_SCO)
	else {
		DBPRT("xmit_isoc: pending queue len: %d\n", skb_queue_len(&req->pending));
		while (!err && skb_queue_len(&req->pending) < BTUSB_SCO_TX_URB && 
				(skb = skb_dequeue(&req->queue))) {
			err = btusb_xmit_isoc(req, skb);
			if (err)
				break;
			if (skb->len)
				skb_queue_head(&req->queue, skb);
			else
				dev_kfree_skb_any(skb);
		}
	}
#endif
	if (err) {
		BTERROR("usb_submit_urb failed (%d)\n", err);
		skb_queue_head(&req->queue, skb);
	} else if (skb_queue_empty(&req->queue))
		hcidev_wake_queue(req->btusb->hci);

	DBFEXIT;
	return err;
}

int btusb_xmit_wakeup(btusb_req_t *req)
{
	int	err = 0;

	DBFENTER;
	set_bit(BTUSB_XMIT_WAKEUP, &req->flags);
	while (!test_and_set_bit(BTUSB_XMIT_PROCESS, &req->flags)) {
//	while (spin_trylock_bh(&req->xmit_lock)) {
		clear_bit(BTUSB_XMIT_WAKEUP, &req->flags);
		err = btusb_xmit_start(req);
//		spin_unlock_bh(&req->xmit_lock);
		clear_bit(BTUSB_XMIT_PROCESS, &req->flags);
		if (err || !test_bit(BTUSB_XMIT_WAKEUP, &req->flags))
			break;
	}
	DBFEXIT;
	return err;
}


/*----------------------------------------------------------------------------------------------
				USB callback functions
  ----------------------------------------------------------------------------------------------
*/
static inline btusb_t *get_btusb(btusb_req_t *req, const char *function)
{
	if (req == NULL || req->btusb == NULL || req->btusb->signature != 0xDEADBEAF) {
		BTERROR("%s - invalid btusb\n", function);
		return NULL;
	}
	return req->btusb;
}

static void btusb_tx_complete(struct urb *urb, struct pt_regs *regs)
{
	struct sk_buff	*skb = urb->context;
	btusb_req_t	*req = btusb_cb(skb)->req;
	btusb_t		*btusb = get_btusb(req, __FUNCTION__);

	DBFENTER;
	if (skb->pkt_type == HCI_SCO && !test_bit(BTUSB_AUDIO_ON, &req->btusb->flags))
		return;
	if (!test_bit(BTUSB_RUNNING, &btusb->flags))
		return;
	read_lock(&req->callback_lock);
	if (urb->status) {
		BTDEBUG("nonzero write status: %d\n", urb->status);
	}
	/* clear state */
	if (usb_pipecontrol(urb->pipe)) {
		kfree(urb->setup_packet);
	}
	skb_unlink(skb);
	dev_kfree_skb_any(skb);
	usb_free_urb(urb);
	btusb_xmit_wakeup(req);
	read_unlock(&req->callback_lock);
	DBFEXIT;
}

static void btusb_rx_data(btusb_req_t *req, int pkt_type, unsigned char *data, int count)
{
	int	copy_bytes;

	DBFENTER;
	DBPRT("%s pkt. len=%d\n", hci_pkttype(pkt_type), count);
	DBDUMP(data, count);
	while (count) {	
		if (req->rx_skb == NULL) {	/* new packet */
			int	pkt_len;
			if (req->btusb->params->uart_leading_byte) {
				count--;
				data++;
			}
			/* allocate skb */
			if (count < (0x06 - pkt_type)) {//hci_pktlen(pkt_type, NULL)
				BTDEBUG("too small read packet\n");
				return;
			}
			pkt_len = hci_pktlen(pkt_type, data);
			if (!pkt_len)
				return;
			req->rx_skb = dev_alloc_skb(pkt_len);
			if (!req->rx_skb) {
				BTERROR("dev_alloc_skb() failed\n");
				return;
			}
			req->rx_skb->pkt_type = pkt_type;
			btusb_cb(req->rx_skb)->pkt_len = pkt_len;
		}
		copy_bytes = btmin(btusb_cb(req->rx_skb)->pkt_len - req->rx_skb->len, count);
		memcpy(skb_put(req->rx_skb, copy_bytes), data, copy_bytes);
		if (req->rx_skb->len == btusb_cb(req->rx_skb)->pkt_len) {
			hcidev_rx(req->btusb->hci, req->rx_skb);
			req->rx_skb = NULL;
		}
		data += copy_bytes;
		count -= copy_bytes;
	}			
	DBFEXIT;
}

static void btusb_rx_complete(struct urb *urb, struct pt_regs *regs)
{
	struct sk_buff	*skb = urb->context;
	btusb_req_t	*req = btusb_cb(skb)->req;
	int		len = urb->actual_length;

	if (len) DBFENTER;
	if (skb->pkt_type == HCI_SCO && !test_bit(BTUSB_AUDIO_ON, &req->btusb->flags))
		return;
	if (!test_bit(BTUSB_RUNNING, &req->btusb->flags))
		return;
	read_lock(&req->callback_lock);
	if (!get_btusb(req, __FUNCTION__))
		goto exit;
	skb_unlink(skb);
	if (urb->status) {
		BTDEBUG("nonzero read status received: %d\n", urb->status);
		if (++req->btusb->read_fail_cnt > MAX_READ_FAIL_COUNT) {
			BTDEBUG("Maximum read failure count reached. Give up.\n");
			goto exit;
		}
		goto resubmit;
	}
	req->btusb->read_fail_cnt = 0;
	if (!len)
		goto resubmit;
#if defined(CONFIG_AFFIX_SCO)
	if (skb->pkt_type == HCI_SCO) {
		int	i;
		for (i = 0; i < urb->number_of_packets; i++) {
			if (urb->iso_frame_desc[i].status) {
				BTDEBUG("isoc frame %u status %d\n", i, urb->iso_frame_desc[i].status);
				continue;
			}
			btusb_rx_data(req, skb->pkt_type, urb->transfer_buffer + urb->iso_frame_desc[i].offset,
					urb->iso_frame_desc[i].actual_length);
			urb->iso_frame_desc[i].status = 0;

		}
	} else 
#endif
		btusb_rx_data(req, skb->pkt_type, urb->transfer_buffer, len);
resubmit:
	urb->dev = req->btusb->usb_dev;
	if (btusb_submit_urb(req, urb, 1))
		BTERROR("bulk_in resubmit failed\n");
exit:
	read_unlock(&req->callback_lock);
	if (len) DBFEXIT;
}

/*----------------------------------------------------------------------------------------------
				Network interface functions.
  ----------------------------------------------------------------------------------------------
*/
static int btusb_hci_open(hci_struct *hci)
{
	btusb_t		*btusb = hci->priv;
	int		err, i;

	DBFENTER;
	if (test_and_set_bit(BTUSB_RUNNING, &btusb->flags)) {
		BTERROR("device already open\n");
		return -EBUSY;
	}
	err = btusb_setup_int(&btusb->int_rx_req);
	if (err) {
		BTERROR("int setup failed: %d\n", err);
		goto err1;
	}
	for (i = 0; i < BTUSB_ACL_RX_URB; i++) {
		err = btusb_setup_bulk_rx(&btusb->bulk_rx_req);
		if (err) {
			BTERROR("bulk setup failed: %d\n", err);
			goto err1;
		}
	}
#if 0
	err = btusb_audio_mode(btusb, AFFIX_AUDIO_ON);	//try to switch it on
	if (err)
		goto err2;
#endif
	hcidev_start_queue(hci);
	DBFEXIT;
	return 0;
#if 0
err2:
	btusb_audio_mode(btusb, 0);
#endif
err1:
	clear_bit(BTUSB_RUNNING, &btusb->flags);
	btusb_release_req(&btusb->bulk_rx_req);
	btusb_release_req(&btusb->int_rx_req);
	return err;
}

static int btusb_hci_stop(hci_struct *hci)
{
	btusb_t 	*btusb = hci->priv;

	DBFENTER;

	if (!test_and_clear_bit(BTUSB_RUNNING, &btusb->flags))
		return 0;

	hcidev_stop_queue(hci);
	/* switch off audio */
#if defined(CONFIG_AFFIX_SCO)
	btusb_audio_mode(btusb, 0);
#endif
	/* 
	 * unlinking urb means calling the completion routine of any pending transaction
	 * it's not a problem since there we will check btusb->active.
	 */
	btusb_release_req(&btusb->int_rx_req);
	btusb_release_req(&btusb->ctrl_tx_req);
	btusb_release_req(&btusb->bulk_rx_req);
	btusb_release_req(&btusb->bulk_tx_req);
	DBFEXIT;
	return 0;
}

static inline btusb_req_t *__get_req(btusb_t *btusb, int pkt_type)
{
	switch (pkt_type) {
		case HCI_COMMAND:
			return &btusb->ctrl_tx_req;
		case HCI_ACL:
			return &btusb->bulk_tx_req;
#if defined(CONFIG_AFFIX_SCO)
		case HCI_SCO:
			return &btusb->isoc_tx_req;
#endif
		default:
			return NULL;
	}
}

static int btusb_hci_xmit(hci_struct *hci, struct sk_buff *skb)
{
	btusb_t		*btusb = hci->priv;
	btusb_req_t	*req = NULL;
	int		res = 0;

	DBFENTER;
	if (!test_bit(BTUSB_RUNNING, &btusb->flags)) {
		BTERROR("device not opened\n");
		res = -EINVAL;
		goto exit;
	}
	DBDUMP(skb->data, skb->len);
	if (btusb->params->uart_leading_byte)
		*skb_push(skb, 1) = skb->pkt_type;
	DBPRT("send HCI %s pkt. len=%d\n", hci_pkttype(skb->pkt_type), skb->len);
	req = __get_req(btusb, skb->pkt_type);
	if (!req) {
		dev_kfree_skb_any(skb);
		goto exit;
	}
	skb_queue_tail(&req->queue, skb);
	hci->trans_start = jiffies;
	btusb_xmit_wakeup(req);
exit:
	DBFEXIT;
	return res;
} 

static int btusb_hci_ioctl(hci_struct *hci, int cmd, void *arg)
{
        btusb_t		*btusb = hci->priv;
        int		res = 0;

	DBFENTER;
	if (btusb == NULL || btusb->signature != 0xDEADBEAF) {
		BTERROR("invalid btusb\n");
		res = -ENODEV;
		goto exit;
	} 
	switch (cmd) {
#if defined(CONFIG_AFFIX_SCO)
		case SIOCHCI_SET_AUDIO:
			res = btusb_audio_mode(btusb, hci->audio_mode);
			break;
#endif
		default:
			res = -ENOIOCTLCMD;
			goto exit;
	}
exit: 
 	DBFEXIT;
        return res;
}

/*----------------------------------------------------------------------------------------------
				USB init and shutdown functions
  ----------------------------------------------------------------------------------------------
*/
static int btusb_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	btusb_t		*btusb = NULL;
	int		btnum;
	int		i, s, ep, ep_found, vendor_id, device_id;
	btusb_req_t	*req = NULL;
	struct usb_device		*usb_dev = interface_to_usbdev(intf);
	struct usb_host_interface	*iface;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
	unsigned int ifnum = intf->altsetting[intf->act_altsetting].desc.bInterfaceNumber;
#else
	struct usb_host_interface       *alt = intf->cur_altsetting;
	unsigned int ifnum = alt->desc.bInterfaceNumber;
#endif
	
#if defined(CONFIG_AFFIX_SCO)
	int	pkt_size = 0;
#endif
	DBFENTER;
	DBPRT("usb_dev: %p, ifnum: %d\n", usb_dev, ifnum);
		
	/* Check our black list */
	if (usb_match_id(intf, btusb_ignore_ids))
		return -EIO;
	if (ifnum) {
		return -ENODEV;
	}
	for (btnum = 0; btnum < NUM_OF_USB_BT_DEV; btnum++) {
		vendor_id = usb_bt_list[btnum].vendor_id;
		device_id = usb_bt_list[btnum].device_id;
		if( (vendor_id == usb_dev->descriptor.idVendor &&
		    device_id == usb_dev->descriptor.idProduct) || 
				(vendor_id == 0 && device_id == 0) )
			break;
	}
	if (btnum == NUM_OF_USB_BT_DEV)	{
		BTINFO("bluetooth device not supported\n");
		goto failed;
	}

	/* allocating device object */
	btusb = kmalloc(sizeof(btusb_t), GFP_KERNEL);
	if (btusb == NULL) {
		BTERROR("failed to reserve memory for dev\n");
		goto failed;
	}
	memset(btusb, 0, sizeof(btusb_t));

	btusb->signature = 0xDEADBEAF;
	btusb->usb_dev = usb_dev;
	btusb->params = &usb_bt_list[btnum];

	/* initialize some data */
	btusb_init_req(&btusb->bulk_tx_req, btusb);
	btusb_init_req(&btusb->bulk_rx_req, btusb);
	btusb_init_req(&btusb->ctrl_tx_req, btusb);
	btusb_init_req(&btusb->int_rx_req, btusb);
#if defined(CONFIG_AFFIX_SCO)
	btusb_init_req(&btusb->isoc_tx_req, btusb);
	btusb_init_req(&btusb->isoc_rx_req, btusb);
#endif

#define EP(x) (iface->endpoint[x])
	ep_found = 0;
	DBPRT("Checking interfaces...\n");
	DBPRT("Number of Interfaces: %d\n", usb_dev->actconfig->desc.bNumInterfaces);
	for (i = 0; i < usb_dev->actconfig->desc.bNumInterfaces; i++) {
		DBPRT("   Interface: %d\n", i);
		DBPRT("   Number of altsettings: %d\n", usb_dev->actconfig->interface[i]->num_altsetting);
		for (s = 0; s < usb_dev->actconfig->interface[i]->num_altsetting; s++) {
			iface = &usb_dev->actconfig->interface[i]->altsetting[s];
			DBPRT("      Altsetting: %d\n", s);
			DBPRT("      Interface number: %d\n", iface->desc.bInterfaceNumber);
			DBPRT("      Number of endpoints: %d\n", iface->desc.bNumEndpoints);
			for (ep = 0; ep < iface->desc.bNumEndpoints; ep++) {
				DBPRT("        EndpointAddress: %#x, Attributes: %#x, Interval: %d\n",
				EP(ep).desc.bEndpointAddress, EP(ep).desc.bmAttributes, EP(ep).desc.bInterval); 
				switch (EP(ep).desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) {
					case USB_ENDPOINT_XFER_BULK:
						if (EP(ep).desc.bEndpointAddress & USB_DIR_IN) {
							req = &btusb->bulk_rx_req;
							req->pipe = usb_rcvbulkpipe(usb_dev, EP(ep).desc.bEndpointAddress);
							ep_found |= 0x01;
						} else {
							req = &btusb->bulk_tx_req;
							req->pipe = usb_sndbulkpipe(usb_dev, EP(ep).desc.bEndpointAddress);
							ep_found |= 0x02;
						}
						break;
					case USB_ENDPOINT_XFER_INT:
						if (EP(ep).desc.bEndpointAddress & USB_DIR_IN) {
							req = &btusb->int_rx_req;
							req->pipe = usb_rcvintpipe(usb_dev, EP(ep).desc.bEndpointAddress);
							req->interval = EP(ep).desc.bInterval;
							ep_found |= 0x04;
						}
						break;
#if defined(CONFIG_AFFIX_SCO)
					case USB_ENDPOINT_XFER_ISOC:
						if (EP(ep).desc.wMaxPacketSize < pkt_size)
							break;
						pkt_size = EP(ep).desc.wMaxPacketSize;	// get max MTU
						btusb->isoc = s + 1;
						if (EP(ep).desc.bEndpointAddress & USB_DIR_IN) {
							req = &btusb->isoc_rx_req;
							req->pipe = usb_rcvisocpipe(usb_dev, EP(ep).desc.bEndpointAddress);
							req->interval = EP(ep).desc.bInterval;
							ep_found |= 0x10;
						} else {
							req = &btusb->isoc_tx_req;
							req->pipe = usb_sndisocpipe(usb_dev, EP(ep).desc.bEndpointAddress);
							req->interval = EP(ep).desc.bInterval;
							ep_found |= 0x20;
						}
						break;
#endif
					default:
						req = NULL;
				}
			}
		}
	}
	if ((ep_found & 0x0f) != 0x07) {
		BTERROR("invalid endpoint address / attribute\n");
		goto failed;
	}
	btusb->ctrl_tx_req.pipe = usb_sndctrlpipe(usb_dev, 0);

#if defined(CONFIG_AFFIX_SCO)
	if ((ep_found & 0xF0) == 0x30) {
		DBPRT("Found ISOC interface: %d\n", btusb->isoc - 1);
#if 0
		if (usb_set_interface(usb_dev, 1, btusb->isoc - 1)) {
			BTERROR("usb_set_interface(dev, 1, %d) failed", btusb->isoc - 1);
			btusb->isoc = 0;
		}
#endif
		if (btusb->isoc) {
			usb_driver_claim_interface(&btusb_driver, usb_dev->actconfig->interface[1], btusb);
		}
	} else
		btusb->isoc = 0;
#endif

#undef EP
	/* setting up hci inetrface */
	if ((btusb->hci = hcidev_alloc()) == NULL)
		goto failed;
	btusb->hci->priv = btusb;
	btusb->hci->open = btusb_hci_open;
	btusb->hci->close = btusb_hci_stop;
	btusb->hci->send = btusb_hci_xmit;
	btusb->hci->ioctl = btusb_hci_ioctl;
	btusb->hci->hdrlen = btusb->params->uart_leading_byte;
	btusb->hci->type = HCI_USB;
	btusb->hci->owner = THIS_MODULE;
	if (hcidev_register(btusb->hci, usb_dev) != 0) {
		BTERROR("register_netdev failed\n");
		goto failed3;
	}
	BTINFO("affix_usb bound to device\n");
	DBFEXIT;
	usb_set_intfdata(intf, btusb);
	return 0;
failed3:
	kfree(btusb->hci);
failed:
	if (btusb) 
		kfree(btusb);
	return -EIO;
}

static void btusb_disconnect(struct usb_interface *intf)
{
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	btusb_t *btusb = usb_get_intfdata(intf);
	DBFENTER;
	DBPRT("usb_dev: %p\n", usb_dev);
	if (btusb == NULL || btusb->signature != 0xDEADBEAF) {
		BTERROR("invalid btusb\n");
		return;
	}
	if (btusb->isoc) {
		usb_driver_release_interface(&btusb_driver, usb_dev->actconfig->interface[1]);
	}
	usb_set_intfdata(intf, NULL);
	hcidev_unregister(btusb->hci);
	kfree(btusb);
	BTINFO("BTUSB driver disconnected from device\n");
	DBFEXIT;
}

/*----------------------------------------------------------------------------------------------
				Module init and shutdown functions
  ----------------------------------------------------------------------------------------------
*/
int __init init_btusb(void)
{
	int err;

	DBFENTER;
	printk("Affix USB Bluetooth driver loaded (affix_usb)\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Imre Deak <ext-imre.deak@nokia.com> and\n");
	printk("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");

	err = usb_register(&btusb_driver);
	if (err < 0) {
		BTERROR("usb_register failed for the USB bluetooth driver. Error number %d\n", err);
		DBFEXIT;
		return err;
	}
	BTINFO("affix_usb registered\n");
	DBFEXIT;
	return 0;
}

void __exit exit_btusb(void)
{
	DBFENTER;
	usb_deregister(&btusb_driver);
	BTINFO("affix_usb deregistered\n");
	DBFEXIT;
}

/* ------------------------------------------------------------------------------------------ */


module_init(init_btusb);
module_exit(exit_btusb);

MODULE_AUTHOR("Imre Deak/Dmitry Kasatkin");
MODULE_DESCRIPTION("Affix USB driver for Bluetooth devices");
MODULE_LICENSE("GPL");


