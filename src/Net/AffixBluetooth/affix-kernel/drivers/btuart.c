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
   $Id: btuart.c,v 1.3 2004/02/24 15:39:58 kassatki Exp $

   BTUART - physical protocol layer for UART based cards

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
		Shrirang Bhagwat <shrirangb@aftek.com>
*/


/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/serial.h>
#include <linux/ioctl.h>
#include <linux/file.h>
#include <linux/termios.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <linux/skbuff.h>

/* Local Includes */
#define	FILEBIT	DBDRV

#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/uart.h>

#if defined(CONFIG_AFFIX_UART_BCSP)
#include "btuart_bcsp.h"
#endif

#include <asm/io.h>
#include <linux/serial_reg.h>
#define AFFIX_16C950_BUG

void btuart_rx_task(struct affix_uart *btuart);

struct affix_uart_proto	btuart_protos[] = {
#if defined(CONFIG_AFFIX_UART_TLP)
{
	proto:			HCI_UART_TLP,
	hard_hdr_len:		TLP_HDR_LEN,
	enqueue:		btuart_enqueue_tlp,
	dequeue:		btuart_dequeue_tlp,
	init:			btuart_init_tlp,
	uninit:			btuart_uninit_tlp,
	recv_buf:		btuart_recv_buf_tlp,
},
#endif
#if defined(CONFIG_AFFIX_UART_BCSP)
{
	/* BCSP protocol descriptor */
	proto:			HCI_UART_BCSP,
	hard_hdr_len:		16,
	enqueue:		bcsp_enqueue,
	dequeue:		bcsp_dequeue,
	init:			bcsp_open,
	uninit:			bcsp_close,
	recv_buf:		(void*)bcsp_recv,
},
#endif
#if defined(CONFIG_AFFIX_UART_H4)
{
	/* last - default descriptor */
	proto:			HCI_UART_H4,
	hard_hdr_len:		1,
	enqueue:		btuart_enqueue_h4,
	dequeue:		btuart_dequeue_h4,
	init:			btuart_init_h4,
	uninit:			btuart_uninit_h4,
	recv_buf:		btuart_recv_buf_h4,
}
#endif
};

#define NOF_UART_DEVS (sizeof(btuart_protos) / sizeof(btuart_protos[0]))

static struct affix_uart_proto *get_by_proto(int proto)
{
	int	i;
	
	for (i = 0; i < NOF_UART_DEVS; i++) {
		if (proto == btuart_protos[i].proto)
			return &btuart_protos[i];
	}
	return NULL;
}


/*************************** FUNCTION DEFINITION SECTION **********************/

int btuart_xmit_start(struct affix_uart *btuart)
{
	int	actual;

	DBFENTER;
	for (;;) {
		if (!test_bit(BTUART_RUNNING, &btuart->flags) || !hcidev_present(btuart->hci))
			return -1;

		if (!btuart->tx_skb) {
			btuart->tx_skb = btuart->proto.dequeue(btuart);
			if (!btuart->tx_skb)
				break;
		}
		DBDUMP(btuart->tx_skb->data, btuart->tx_skb->len);
		
		actual = btuart->write(btuart, btuart->tx_skb->data, btuart->tx_skb->len);
		DBPRT("packet sent: %d of %d bytes\n", actual, btuart->tx_skb->len);
		skb_pull(btuart->tx_skb, actual);
		if (btuart->tx_skb->len)
			break;
		dev_kfree_skb_any(btuart->tx_skb);
		btuart->tx_skb = NULL;
	}
	DBFEXIT;
	return 0;
}

void btuart_xmit_wakeup(struct affix_uart *btuart)
{
	int	err = 0;

	DBFENTER;
	set_bit(BTUART_XMIT_WAKEUP, &btuart->flags);
	while (spin_trylock_bh(&btuart->xmit_lock)) {
		clear_bit(BTUART_XMIT_WAKEUP, &btuart->flags);
		err = btuart_xmit_start(btuart);
		spin_unlock_bh(&btuart->xmit_lock);
		if (err || !test_bit(BTUART_XMIT_WAKEUP, &btuart->flags))
			break;
	}
	DBFEXIT;
}

int btuart_net_xmit(hci_struct *hci, struct sk_buff *skb)
{
	struct affix_uart	*btuart = hci->priv;

	DBFENTER;
	if (!test_bit(BTUART_RUNNING, &btuart->flags)) {
		DBPRT("%s: xmit call when iface is down\n", hci->name);
		dev_kfree_skb_any(skb);
		return 0;
	}
	hci->trans_start = jiffies;
	btuart->proto.enqueue(btuart, skb);
	btuart_xmit_wakeup(btuart);
	DBFEXIT;
	return 0;
}


/***********************   Network Interface Subsystem  ********************/

int btuart_open(hci_struct *hci)
{
	struct affix_uart	*btuart = hci->priv;
	int			err;

	DBFENTER;
	if (test_and_set_bit(BTUART_RUNNING, &btuart->flags))
		return -EBUSY;

	DBPRT("proto: %d, name: %s\n", btuart->proto.proto, btuart->uart.name);

	if (!btuart->proto.proto) {
		clear_bit(BTUART_RUNNING, &btuart->flags);
		return -EBUSY;
	}
	// open device
	err = btuart->open(btuart);
	if (err) {
		clear_bit(BTUART_RUNNING, &btuart->flags);
		return err;
	}

	/* now we are able to receive data */
	btuart->tx_skb = NULL;
	btuart->rx_skb = NULL;
	btuart->rx_count = 0;

	tasklet_init(&btuart->tx_task, (void*)btuart_xmit_wakeup, (unsigned long)btuart);
#ifdef CONFIG_AFFIX_UART_NEW_BH
	tasklet_init(&btuart->rx_task, (void*)btuart_rx_task, (unsigned long)btuart);
#endif

	/* protocol specific init */
	if (btuart->proto.init) {
		err = btuart->proto.init(btuart);
		if (err)
			goto ioerr;
	}

	hcidev_start_queue(btuart->hci);

	if (btuart->uart.count)
		(*btuart->uart.count)++;

	DBFEXIT;
	return 0;
ioerr:
	clear_bit(BTUART_RUNNING, &btuart->flags);
	btuart->close(btuart);
	DBFEXIT;
	return err;
}

int btuart_close(hci_struct *hci)
{
	struct affix_uart	*btuart = hci->priv;
	
	DBFENTER;
	if (!test_and_clear_bit(BTUART_RUNNING, &btuart->flags))
		return 0;

	hcidev_stop_queue(btuart->hci);

	write_lock(&btuart->lock);	/* lock */

	tasklet_kill(&btuart->tx_task);
#ifdef CONFIG_AFFIX_UART_NEW_BH
	tasklet_kill(&btuart->rx_task);
#endif
	if (btuart->proto.uninit)
		btuart->proto.uninit(btuart);

	/* cleanup resources */
	if (btuart->tx_skb)
		kfree_skb(btuart->tx_skb);
	if (btuart->rx_skb)
		kfree_skb(btuart->rx_skb);
	skb_queue_purge(&btuart->tx_q);
	
	btuart->close(btuart);

	if (btuart->uart.count)
		(*btuart->uart.count)--;

	write_unlock(&btuart->lock);	/* unlock */
	DBFEXIT;
	return 0;
}

int btuart_ioctl(hci_struct *hci, int cmd, void *arg)
{
	struct affix_uart	*btuart = (void*)hci->priv;
	int		err;

	DBFENTER;
	if (!btuart)	// sanity check
		return -ENODEV;
	switch (cmd) {
		case BTIOC_SETUP_UART:
			{
				struct open_uart	*uart = (void*)arg;
				struct affix_uart_proto	*proto;

				btuart->uart.speed = uart->speed;
				btuart->uart.flags = uart->flags;

				/* select configuration */
				proto = get_by_proto(uart->proto);
				if (!proto)
					return -EPROTONOSUPPORT;
				btuart->proto = *proto;
				btuart->hci->hdrlen = btuart->proto.hard_hdr_len;
				DBPRT("proto: %p, %d\n", proto, btuart->proto.proto);

				/* change settings */
				err = btuart->ioctl(btuart, cmd, arg);
			}
			break;

		default:
			err = btuart->ioctl(btuart, cmd, arg);
			break;
	}
	DBFEXIT;
	return err;
}

/* ****************** attachment stuff ***************************** */

int btuart_register_hci(struct affix_uart *btuart)
{
	hci_struct	*hci;
	int		err;

	DBFENTER;	
	hci = hcidev_alloc();
	if (!hci)
		return -ENOMEM;
	hci->priv = btuart;	/* set private pointer */
	hci->open = btuart_open;
	hci->close = btuart_close;
	hci->ioctl = btuart_ioctl;
	hci->send = btuart_net_xmit;
	hci->hdrlen = btuart->proto.hard_hdr_len;
	hci->type = btuart->uart.manfid ? HCI_UART_CS : HCI_UART;
	hci->owner = btuart->uart.owner;
	err = hcidev_register(hci, &btuart->uart);
	if (!err)
		btuart->hci = hci;
	DBFEXIT;
	return err;
}

static inline void btuart_unregister_hci(struct affix_uart *btuart)
{
	DBFENTER;
	if (btuart->hci) {
		hcidev_unregister(btuart->hci);
		btuart->hci = NULL;
	}
	DBFEXIT;
}


int affix_uart_register(struct affix_uart *btuart)
{
	int			err;
	struct affix_uart_proto	*proto;
	
	DBFENTER;

#ifdef CONFIG_AFFIX_UART_NEW_BH
	BTINFO("Using BT Inbuffers [%d Bytes]\n", BT_INBUFFER_SIZE);
	btuart->hci_data.head = &btuart->hci_data.data[0];
	btuart->hci_data.tail = &btuart->hci_data.data[BT_INBUFFER_SIZE-1];
	btuart->hci_data.put = btuart->hci_data.head;
	btuart->hci_data.get = btuart->hci_data.head;;
#endif

	rwlock_init(&btuart->lock);
	spin_lock_init(&btuart->xmit_lock);
	skb_queue_head_init(&btuart->tx_q);

	/* select configuration */
	proto = get_by_proto(btuart->uart.proto);
	if (!proto)
		btuart->proto.proto = 0;	/* no proto */
	else
		btuart->proto = *proto;

	err = btuart_register_hci(btuart);

	DBFEXIT;
	return err;
}

int affix_uart_unregister(struct affix_uart *btuart)
{
	DBFENTER;
	if (!btuart)
		return -ENODEV;
	btuart_unregister_hci(btuart);
	DBFEXIT;
	return 0;
}

void affix_uart_suspend(struct affix_uart *btuart)
{
	DBFENTER;
	if (btuart->hci)
		hcidev_detach(btuart->hci);
	DBFEXIT;
	return;
}

void affix_uart_resume(struct affix_uart *btuart)
{
	DBFENTER;
	if (btuart->hci)
		hcidev_attach(btuart->hci);
	DBFEXIT;
	return;
}


#ifdef CONFIG_AFFIX_UART_NEW_BH
void btuart_rx_task(struct affix_uart *btuart)
{
	s32 size_end;
	s32 size_start;
	u8* getTemp;

	cli();
	if (btuart->hci_data.get == btuart->hci_data.put) {
		sti();
		return;
	} else if (btuart->hci_data.get > btuart->hci_data.put) {
		/* buffer is wrapped */
		size_end = btuart->hci_data.tail - btuart->hci_data.get + 1;
		size_start = btuart->hci_data.put - btuart->hci_data.head;
		getTemp = btuart->hci_data.get;

		/* Indicate that all data has been fetched (or soon will be) 
		 * 		   by setting get == put. */
		btuart->hci_data.get = btuart->hci_data.put;
		sti();

		btuart->proto.recv_buf(btuart, getTemp, size_end);
		btuart->proto.recv_buf(btuart, btuart->hci_data.head, size_start);
	} else {
		/* no wrapped buffer */
		size_end = btuart->hci_data.put - btuart->hci_data.get;
		getTemp = btuart->hci_data.get;
		/* Indicate that all data has been fetched (or soon will be)
		 * 		   by setting get == put. */
		btuart->hci_data.get = btuart->hci_data.put;
		sti();

		btuart->proto.recv_buf(btuart, getTemp, size_end);
	}
}

void btuart_schedule_rx(struct affix_uart *btuart, const __u8 *data, s32 count)
{
	s32		free;

	/* Check if there is data that hasn't been passed up yet. 
	 * In that case there is a task scheduled for this and we shouldn't 
	 * add another one. */

	if (btuart->hci_data.put == btuart->hci_data.get) {
		tasklet_hi_schedule(&btuart->rx_task);
	}

	if (btuart->hci_data.put >= btuart->hci_data.get) {
		/* check for overruns... */
		if (btuart->hci_data.put + count - BT_INBUFFER_SIZE >= btuart->hci_data.get) {
			printk("btuart.c : Buffer overrun!\n");
		} else {
			/* Calculate how much space there is left at the end 
			 * 			   of the buffer */
			free = btuart->hci_data.tail - btuart->hci_data.put + 1;

			/* normal case, data fits in buffer */
			if (count < free) {
				memcpy(btuart->hci_data.put, (u8*)data, count);
			} else {
				/* wrap buffer */
				memcpy(btuart->hci_data.put, (u8*)data, free);
				memcpy(btuart->hci_data.head, (u8*)(data + free), count - free);    
			}
		}
	} else { 
		/* hci_data.put < hci_data.get */
		/* check for overruns ... */
		if (btuart->hci_data.put + count >= btuart->hci_data.get) {
			printk("btuart.c : Buffer overrun!\n");
		} else {
			/* Copy the data to the buffer */
			memcpy(btuart->hci_data.put, (u8*)data, count);
		}
	}

	btuart->hci_data.put += count;
	if (btuart->hci_data.put > btuart->hci_data.tail)
		btuart->hci_data.put -= BT_INBUFFER_SIZE; 
}
#endif

void affix_uart_recv_buf(struct affix_uart *btuart, const char *data, int count)
{
	if (!btuart || !test_bit(BTUART_RUNNING, &btuart->flags) || !hcidev_present(btuart->hci))
		return;
	read_lock(&btuart->lock);
#ifdef CONFIG_AFFIX_UART_NEW_BH
	/* store in bt inbuffer and schedule task if none is started  */
	btuart_schedule_rx(btuart, data, count);
#else
	if (btuart->proto.recv_buf)
		btuart->proto.recv_buf(btuart, data, count);
#endif
	read_unlock(&btuart->lock);
}

void affix_uart_write_wakeup(struct affix_uart *btuart)
{

	DBFENTER;
	if (!btuart || !test_bit(BTUART_RUNNING, &btuart->flags) || !hcidev_present(btuart->hci))
		return;
	read_lock(&btuart->lock);
	btuart_xmit_wakeup(btuart);
	read_unlock(&btuart->lock);
	DBFEXIT;
}

int init_btuart_tty(void);
void exit_btuart_tty(void);

int __init init_affix_uart(void)
{
	int	err = 0;

	DBFENTER;

	printk("Affix UART Bluetooth driver loaded (affix_uart)\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");

	err = init_btuart_tty();

	DBFEXIT;
	return err;
}

void __exit exit_affix_uart(void)
{
	DBFENTER;
	exit_btuart_tty();
	DBFEXIT;
}


/*  If we are resident in kernel we want to call init_btuart_cs manually. */
module_init(init_affix_uart);
module_exit(exit_affix_uart);

MODULE_AUTHOR("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>");
MODULE_DESCRIPTION("Affix UART driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(affix_uart_register);
EXPORT_SYMBOL(affix_uart_unregister);
EXPORT_SYMBOL(affix_uart_recv_buf);
EXPORT_SYMBOL(affix_uart_write_wakeup);
EXPORT_SYMBOL(affix_uart_suspend);
EXPORT_SYMBOL(affix_uart_resume);

