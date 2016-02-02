/*
 *
 *  Driver for bluetooth CF cards with OXCF950 UART such as BT2000E from Ambicom/Pretec
 *
 *  Copyright (C) 2002  Albert Rybalkin <albertr@iral.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation;
 *
 *  Software distributed under the License is distributed on an "AS
 *  IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *  implied. See the License for the specific language governing
 *  rights and limitations under the License.
 *
 * Portions based on the original code of pcmcia-cs by David A. Hinds,
 * Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Portions based on the original code of btuart_cs.c by Marcel Holtmann,
 * Copyright (C) 2001-2002  Marcel Holtmann <marcel@holtmann.org>. 
 *
 */

/* 
   $Id: bt950uart_cs.c,v 1.4 2004/03/02 11:53:35 kassatki Exp $

   bt950uart_cs - uart driver for pcmcia serial 950 based bluetooth cards

   Fixes:	
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/


#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/serial.h>
#include <linux/serial_reg.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>
#include <pcmcia/cisreg.h>

#include <affix/bluez.h>

/* ======================== Module parameters ======================== */

#define DRIVER_VERSION 	"0.22"
#define DRIVER_NAME	"OXCF950-based bluetooth cards driver"

/* Bit map of interrupts to choose from */
static u_int irq_mask = 0x86bc;
static int irq_list[4] = { -1 };

MODULE_PARM(irq_mask, "i");
MODULE_PARM(irq_list, "1-4i");

MODULE_AUTHOR("Albert Rybalkin <albertr@iral.com>");
MODULE_DESCRIPTION(DRIVER_NAME " ver." DRIVER_VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

/* ======================== Local structures ======================== */


typedef struct btuart_info_t {
	dev_link_t link;
	dev_node_t node;

	struct hci_dev hdev;

	spinlock_t lock;           /* For serializing operations */

	struct sk_buff_head txq;

	unsigned long rx_state;
	unsigned long tx_state;
	unsigned long rx_count;

	struct sk_buff *rx_skb;
} btuart_info_t;


static dev_info_t dev_info = "bt950uart_cs";
static dev_link_t *dev_list = NULL;

static void btuart_config(dev_link_t *link);
static void btuart_release(dev_link_t *link);
static int btuart_event(event_t event, int priority, event_callback_args_t *args);
static dev_link_t *btuart_attach(void);
static void btuart_detach(dev_link_t *);

static struct pcmcia_driver btuart_driver = {
	.drv		= {
		.name	= "bt950uart_cs",
	},
	.attach		= btuart_attach,
	.detach		= btuart_detach,
	.owner		= THIS_MODULE,
};


/* Receiver states */
#define RECV_WAIT_PACKET_TYPE   0
#define RECV_WAIT_EVENT_HEADER  1
#define RECV_WAIT_ACL_HEADER    2
#define RECV_WAIT_SCO_HEADER    3
#define RECV_WAIT_DATA          4

/* Transmitter states */
#define XMIT_SENDING		1

/* 950-specific stuff */
#define MAX_WAIT	0xFFFF
#define FIFO_SIZE	128

#define TR_TX_INT 	0x10	/* TTL: TX interrupt trigger level (0-127) */
#define TR_RX_INT 	0x40	/* RTL: RX interrupt trigger level (1-127) */
#define TR_CTL_LO 	0x08	/* FCL: auto flow control LOWER trigger level (0-127) */
#define TR_CTL_HI 	0x60	/* FCH: auto flow control HIGH trigger level (1-127) */

/* 950-specific registers and values we use. It should eventually go to
 *
 *  include/linux/serial_reg.h
 *
 */
#define UART_IER_CTS	0x80 /* enable CTS interrupt */
#define UART_IER_RTS	0x40 /* enable RTS interrupt */
#define UART_IER_SLP	0x10 /* enable sleep mode */
#define UART_LCR_650	0xBF /* enable 650-compatible registers access */
#define UART_LSR_DE	0x80 /* data error */
#define UART_LSR_ERR	(UART_LSR_OE|UART_LSR_PE|UART_LSR_FE|UART_LSR_BI|UART_LSR_DE)
#define UART_IIR_RXTOUT 0x0C /* RX timeout interrupt */
#define UART_IIR_CTSRTS 0x20 /* CTS or RTS change interrupt */
#define UART_IIR_RTS	0x40
#define UART_IIR_CTS	0x80
#define UART_IIR_MASK	0x3E /* interrupt mask */
#define UART_SRT	0x0D /* soft reset register */


/* ======================== Interrupt handling ======================== */

static void btuart_write(btuart_info_t *info, int fromint) {

	unsigned int iobase = info->link.io.BasePort1;
	register int i, chunk;
	struct sk_buff *skb;
	int total;

	if (!info) {
		printk(KERN_ERR "bt950uart_cs: unknown device\n");
		return;
	}

	/* activate DTR and RTS */
	outb(UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2, iobase + UART_MCR);

	/* wait for CTS */
	for (i = MAX_WAIT; i; i--)
		if (inb(iobase + UART_MSR) & UART_MSR_CTS) break;

	if (!i) {
		printk(KERN_WARNING "bt950uart_cs: timeout waiting for CTS\n");
		clear_bit(XMIT_SENDING, &(info->tx_state));
		return;
	}

	skb = skb_dequeue(&(info->txq));

	if (skb) {  

		if (fromint) {

			chunk = FIFO_SIZE - TR_TX_INT; 

		} else {

			chunk = FIFO_SIZE; 

			/* TX FIFO should be empty */
			for (i = MAX_WAIT; i; i--)
				if (inb(iobase + UART_LSR) & UART_LSR_THRE) break;

			if (!i) {
				printk(KERN_WARNING "bt950uart_cs: timeout waiting for empty TX FIFO\n");
				/* drop skb on the floor */
				dev_kfree_skb_any(skb);
				clear_bit(XMIT_SENDING, &(info->tx_state));
				return;
			}

		}

		total = skb->len;

		if (skb->len < chunk) {

			chunk = skb->len;

			/* fill up TX FIFO */
			for (i = 0; i < chunk; i++) {
				outb(skb->data[i], iobase + UART_TX);
			}

			dev_kfree_skb_any(skb);
			clear_bit(XMIT_SENDING, &(info->tx_state));

		} else {

			/* fill up TX FIFO */
			for (i = 0; i < chunk; i++) {
				outb(skb->data[i], iobase + UART_TX);
			}

			skb_pull(skb, chunk);
			skb_queue_head(&(info->txq), skb);
			set_bit(XMIT_SENDING, &(info->tx_state));
		}

		info->hdev.stat.byte_tx += chunk;

#if defined(OX950_DEBUG)
		if (fromint)
			printk(KERN_WARNING "bt950uart_cs: wrote=<%d out of %d> from int\n", chunk, total);
		else
			printk(KERN_WARNING "bt950uart_cs: wrote=<%d out of %d>\n", chunk, total);
#endif

	}

}


static inline void btuart_read(btuart_info_t *info) {

	unsigned int iobase;
	int dlen;
	struct hci_event_hdr *eh;
	struct hci_acl_hdr   *ah;
	struct hci_sco_hdr   *sh;
	unsigned long size = 0;

	if (!info) {
		printk(KERN_ERR "bt950uart_cs: receive: unknown device\n");
		return;
	}

	iobase = info->link.io.BasePort1;

start_over:

	/* Allocate packet */
	if (!(info->rx_skb) && !(info->rx_skb = bt_skb_alloc(HCI_MAX_FRAME_SIZE, GFP_ATOMIC))) {
		printk(KERN_ERR "bt950uart_cs: receive: can't allocate mem for new packet\n");
		info->rx_skb = NULL;
		return;
	}

	if (info->rx_state == RECV_WAIT_PACKET_TYPE) {

		/* BUG-BUG */
		inb(iobase + UART_MCR);

		/* get first byte */
		info->rx_skb->pkt_type = inb(iobase + UART_RX);
		info->rx_skb->dev = (void *)&(info->hdev);
		info->hdev.stat.byte_rx++;

		switch (info->rx_skb->pkt_type) {

			case HCI_EVENT_PKT:
				info->rx_state = RECV_WAIT_EVENT_HEADER;
				info->rx_count = HCI_EVENT_HDR_SIZE;
				break;

			case HCI_ACLDATA_PKT:
				info->rx_state = RECV_WAIT_ACL_HEADER;
				info->rx_count = HCI_ACL_HDR_SIZE;
				break;

			case HCI_SCODATA_PKT:
				info->rx_state = RECV_WAIT_SCO_HEADER;
				info->rx_count = HCI_SCO_HDR_SIZE;
				break;

			default:
				/* bogus packet */
				/* read the rest of the packet and discard it */
				size = 1;
				info->hdev.stat.err_rx++;
				while (inb(iobase + UART_LSR) & UART_LSR_DR) {
					inb(iobase + UART_RX);
					size++;
				}
				printk(KERN_WARNING "bt950uart_cs: receive: received bogus packet: type=0x%02X size=%ld\n", info->rx_skb->pkt_type, size);
				dev_kfree_skb_any(info->rx_skb);
				info->rx_skb = NULL;
				return;
				break;
		}

	}

get_more:

	if (info->rx_count > skb_tailroom(info->rx_skb)) {
		printk(KERN_ERR "bt950uart_cs: receive: packet is too large\n");
		info->hdev.stat.err_rx++;
		dev_kfree_skb_any(info->rx_skb);
		info->rx_skb = NULL;
		return;
	}

	if (!(info->rx_count)) {
		printk(KERN_ERR "bt950uart_cs: receive: zero packet size\n");
		info->hdev.stat.err_rx++;
		dev_kfree_skb_any(info->rx_skb);
		info->rx_skb = NULL;
		return;
	}

	size = info->rx_count;

	while ((info->rx_count) && (inb(iobase + UART_LSR) & UART_LSR_DR)) {

		/* get the next byte */
		*skb_put(info->rx_skb, 1) = inb(iobase + UART_RX);
		info->hdev.stat.byte_rx++;
		info->rx_count--;
	}


#if defined(OX950_DEBUG)
	if (!(info->rx_count))
		printk(KERN_NOTICE "bt950uart_cs: receive: received type=0x%X state=%ld size=%ld\n", info->rx_skb->pkt_type, info->rx_state, size); 
	else
		printk(KERN_NOTICE "bt950uart_cs: receive: received incomplete type=0x%X state=%ld size=%ld\n", info->rx_skb->pkt_type, info->rx_state, size); 
#endif

	if (!(info->rx_count)) {
		switch (info->rx_state) {

			case RECV_WAIT_EVENT_HEADER:
				eh = (struct hci_event_hdr *)(info->rx_skb->data);
				info->rx_state = RECV_WAIT_DATA;
				info->rx_count = eh->plen;
				goto get_more;
				break;

			case RECV_WAIT_ACL_HEADER:
				ah = (struct hci_acl_hdr *)(info->rx_skb->data);
				dlen = __le16_to_cpu(ah->dlen);
				info->rx_state = RECV_WAIT_DATA;
				info->rx_count = dlen;
				goto get_more;
				break;

			case RECV_WAIT_SCO_HEADER:
				sh = (struct hci_sco_hdr *)(info->rx_skb->data);
				info->rx_state = RECV_WAIT_DATA;
				info->rx_count = sh->dlen;
				goto get_more;
				break;

			case RECV_WAIT_DATA:
				hci_recv_frame(info->rx_skb);
				info->rx_state = RECV_WAIT_PACKET_TYPE;
				info->rx_skb = NULL;
				if (inb(iobase + UART_LSR) & UART_LSR_DR)
					/* we can save an interrupt here */
					goto start_over;
				break;
		}
	}
}


static void btuart_interrupt(int irq, void *dev_inst, struct pt_regs *regs) {

	btuart_info_t *info = dev_inst;
	unsigned int iobase;
	unsigned char lsr;
	register unsigned char iir;

	if (!info) {
		printk(KERN_ERR "bt950uart_cs: interrupt: unknown device for irq %d\n", irq);
		return;
	}

	iobase = info->link.io.BasePort1;

	spin_lock(&(info->lock));

	iir = inb(iobase + UART_IIR);

	while (!(iir & UART_IIR_NO_INT)) {

		switch (iir & UART_IIR_ID) {

			case UART_IIR_RDI:
				/* RX interrupt */
				btuart_read(info);
				break;
			case UART_IIR_THRI:
				/* TX interrupt */
				btuart_write(info, 1);
				break;
			case UART_IIR_RLSI:
				/* clear RLSI int */
				lsr = inb(iobase + UART_LSR); 
				printk(KERN_NOTICE "bt950uart_cs: interrupt: unhandled RLSI, LSR=0x%X\n", lsr);
				/* BUG-BUG we need to process error... */
				break;
			default:
				printk(KERN_NOTICE "bt950uart_cs: interrupt: unhandled IIR=0x%X\n", iir);
				break;
		}

		iir = inb(iobase + UART_IIR);
	}

	spin_unlock(&(info->lock));

}


/* ======================== HCI interface ======================== */


static int btuart_hci_flush(struct hci_dev *hdev) {

	btuart_info_t *info = (btuart_info_t *)(hdev->driver_data);

	/* Drop TX queue */
	skb_queue_purge(&(info->txq));

	return 0;

}


static int btuart_hci_open(struct hci_dev *hdev) {

	set_bit(HCI_RUNNING, &(hdev->flags));

	return 0;

}


static int btuart_hci_close(struct hci_dev *hdev) {

	if (!test_and_clear_bit(HCI_RUNNING, &(hdev->flags)))
		return 0;

	btuart_hci_flush(hdev);

	return 0;

}


static int btuart_hci_send_frame(struct sk_buff *skb) {

	unsigned long flags;
	btuart_info_t *info;
	struct hci_dev* hdev = (struct hci_dev *)(skb->dev);

	if (!hdev) {
		printk(KERN_ERR "bt950uart_cs: hci_send_frame: unknown HCI device\n");
		return -ENODEV;
	}

	info = (btuart_info_t *)(hdev->driver_data);

	switch (skb->pkt_type) {

		case HCI_COMMAND_PKT:

			hdev->stat.cmd_tx++;
			break;

		case HCI_ACLDATA_PKT:

			hdev->stat.acl_tx++;
			break;

		case HCI_SCODATA_PKT:

			hdev->stat.sco_tx++;
			break;
	};

	/* Prepend skb with frame type */
	memcpy(skb_push(skb, 1), &(skb->pkt_type), 1);
	skb_queue_tail(&(info->txq), skb);

	/* send it right away if transmitter is idle */
	if (!test_bit(XMIT_SENDING, &(info->tx_state))) {

		spin_lock_irqsave(&(info->lock), flags);

		btuart_write(info, 0);

		spin_unlock_irqrestore(&(info->lock), flags);

	}

	return 0;

}


static void btuart_hci_destruct(struct hci_dev *hdev) {

	/* nothing to do */

}


static int btuart_hci_ioctl(struct hci_dev *hdev, unsigned int cmd, unsigned long arg) {

	/* we don't provide any ioctl's */
	return -ENOIOCTLCMD;

}


/* ======================== Card services HCI interaction ======================== */


static int setup_uart(btuart_info_t *info) {

	unsigned long flags;
	unsigned int iobase = info->link.io.BasePort1;
	unsigned char lcr, ier = UART_IER_THRI | UART_IER_RDI | UART_IER_RLSI | UART_IER_SLP;
	unsigned int divisor = 8; /* divisor == 0x0C ??? */
	unsigned char id1, id2, id3, rev;
	register int i;

	spin_lock_irqsave(&(info->lock), flags);

	/* disable interrupts */
	outb(0, iobase + UART_IER); 

	/* activate RTS and OUT2 */
	/* BUG-BUG: is OUT2 used to enable interrupts? */
	outb(UART_MCR_RTS | UART_MCR_OUT2, iobase + UART_MCR);

	/* setup FIFOs */
	outb(0, iobase + UART_FCR);
	inb(iobase + UART_RX);
	outb(UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT | UART_FCR_TRIGGER_14, iobase + UART_FCR);

	/* disable divisor latch access */
	lcr = inb(iobase + UART_LCR) & 0x3F; /* mask out UART_LCR_DLAB and UART_LCR_SBC */
	outb(lcr, iobase + UART_LCR);

	/* read upto 4 bytes from RX FIFO */
	for(i = 1; i < 5; i++) {
		inb(iobase + UART_RX);
		if (!(inb(iobase + UART_LSR) & UART_LSR_DR)) break;
	}

	/* wait if CTS/DSR/DCD changing */
	for (i = 1; i < 0x3E8; i++) {
		if (!(inb(iobase + UART_MSR) & UART_MSR_ANY_DELTA)) break;
	}

	/* enable divisor latch access */
	outb(lcr | UART_LCR_DLAB, iobase + UART_LCR);

	/* setup divisor latch */
	outb(divisor & 0x00FF, iobase + UART_DLL); /* divisor latch LOW byte */
	outb((divisor & 0xFF00) >> 8, iobase + UART_DLM); /* divisor latch HIGH byte */

	/* disable divisor latch access */
	outb(lcr, iobase + UART_LCR);

	/* setup interrupts, enable sleep mode */
	outb(ier, iobase + UART_IER); /* we don't want to handle TX interrupts */

	/* skip pending interrupts */
	for (i = 1; i < 5; i++) {
		if (inb(iobase + UART_IIR) & UART_IIR_NO_INT) break;
	}

	/* 8N1 */
	lcr = UART_LCR_WLEN8;
	outb(lcr, iobase + UART_LCR); 

	/* setup CTS/RTS flow control and 950 enhanced mode */
	outb(UART_LCR_650, iobase + UART_LCR);
	outb(UART_EFR_CTS | UART_EFR_RTS | UART_EFR_ECB, iobase + UART_EFR);
	outb(lcr, iobase + UART_LCR);

	/* read core id and revision */
	outb(UART_ACR, iobase + UART_EMSR);
	outb(UART_ACR_ICRRD, iobase + UART_LSR); /* enable ICR read access, we don't need to save the old value of ACR */

	outb(UART_ID1, iobase + UART_EMSR);
	id1 = inb(iobase + UART_LSR);

	outb(UART_ID2, iobase + UART_EMSR);
	id2 = inb(iobase + UART_LSR);

	outb(UART_ID3, iobase + UART_EMSR);
	id3 = inb(iobase + UART_LSR);

	outb(UART_REV, iobase + UART_EMSR);
	rev = inb(iobase + UART_LSR);

	if (id1 != 0x16 || id2 != 0xC9 || id3 != 0x50) {
		printk(KERN_ERR "bt950uart_cs: unknown UART core found: 0x%02x%02x%02x\n", id1, id2, id3);
		spin_unlock_irqrestore(&(info->lock), flags);
		return -ENODEV;
	}

#if defined(OX950_DEBUG)
	switch (rev) {

		case 3:
			printk(KERN_DEBUG "bt950uart_cs: core is OX16C950B\n");
			break;

		case 6:
			printk(KERN_DEBUG "bt950uart_cs: core is OXCF950\n");
			break;

		case 8:
			printk(KERN_DEBUG "bt950uart_cs: core is OXCF950B\n");
			break;

		default:
			printk(KERN_DEBUG "bt950uart_cs: core revision: 0x%X\n", rev);
			break;
	}
#endif

	/* init ICR registers */
	outb(UART_TTL, iobase + UART_EMSR);
	outb(TR_TX_INT, iobase + UART_LSR); /* TX interrupt trigger level (0-127) */

	outb(UART_RTL, iobase + UART_EMSR);
	outb(TR_RX_INT, iobase + UART_LSR); /* RX interrupt trigger level (1-127) */

	outb(UART_FCL, iobase + UART_EMSR);
	outb(TR_CTL_LO, iobase + UART_LSR); /* auto flow control LOWER trigger level (0-127) */

	outb(UART_FCH, iobase + UART_EMSR);
	outb(TR_CTL_HI, iobase + UART_LSR); /* auto flow control HIGH trigger level (1-127) */

	outb(UART_ACR, iobase + UART_EMSR);
	outb(UART_ACR_TLENB, iobase + UART_LSR); /* disable ICR read access, enable trigger levels */ 

	spin_unlock_irqrestore(&(info->lock), flags);

	/* Timeout before it is safe to send the first HCI packet */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ);

	return 0;

}


static int btuart_open(btuart_info_t *info) {

	struct hci_dev *hdev;
	int ret;

	spin_lock_init(&(info->lock));

	skb_queue_head_init(&(info->txq));

	info->rx_state = RECV_WAIT_PACKET_TYPE;
	info->rx_count = 0;
	clear_bit(XMIT_SENDING, &(info->tx_state));
	info->rx_skb = NULL;

	/* setup uart */
	if ((ret = setup_uart(info)) != 0)
		return ret;

	/* Initialize and register HCI device */
	hdev = &(info->hdev);

	hdev->type = HCI_PCCARD;
	hdev->driver_data = info;

	hdev->open     = btuart_hci_open;
	hdev->close    = btuart_hci_close;
	hdev->flush    = btuart_hci_flush;
	hdev->send     = btuart_hci_send_frame;
	hdev->destruct = btuart_hci_destruct;
	hdev->ioctl    = btuart_hci_ioctl;

	if (hci_register_dev(hdev) < 0) {
		printk(KERN_ERR "bt950uart_cs: Can't register HCI device %s.\n", hdev->name);
		return -ENODEV;
	}

	return 0;

}


static void stop_uart(btuart_info_t *info) {

	unsigned long flags;
	unsigned int iobase = info->link.io.BasePort1;

	spin_lock_irqsave(&(info->lock), flags);

	/* disable interrupts */
	outb(0, iobase + UART_IER);

	outb(0, iobase + UART_MCR);

	spin_unlock_irqrestore(&(info->lock), flags);

	return;
}


static int btuart_close(btuart_info_t *info) {

	struct hci_dev *hdev = &(info->hdev);

	btuart_hci_close(hdev);

	/* stop hardware */
	stop_uart(info);

	if (hci_unregister_dev(hdev) < 0)
		printk(KERN_ERR "bt950uart_cs: Can't unregister HCI device %s.\n", hdev->name);

	return 0;

}



/* ======================== Card services ======================== */


static dev_link_t *btuart_attach(void) {

	btuart_info_t *info;
	client_reg_t client_reg;
	dev_link_t *link;
	int i, ret;


	/* Create new info device */
	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return NULL;
	memset(info, 0, sizeof(*info));


	link = &info->link;
	link->priv = info;

	link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;
	link->io.NumPorts1 = 8;
	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE | IRQ_HANDLE_PRESENT;
	link->irq.IRQInfo1 = IRQ_INFO2_VALID | IRQ_LEVEL_ID;

	if (irq_list[0] == -1)
		link->irq.IRQInfo2 = irq_mask;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << irq_list[i];

	link->irq.Handler = btuart_interrupt;
	link->irq.Instance = info;

	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.IntType = INT_MEMORY_AND_IO;
	link->conf.Present = PRESENT_OPTION | PRESENT_STATUS | PRESENT_PIN_REPLACE | PRESENT_COPY;

	/* Register with Card Services */
	link->next = dev_list;
	dev_list = link;
	client_reg.dev_info = &dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
	client_reg.EventMask =  CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
		CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
		CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &btuart_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;

	ret = pcmcia_register_client(&link->handle, &client_reg);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, RegisterClient, ret);
		btuart_detach(link);
		return NULL;
	}

	return link;

}


static void btuart_detach(dev_link_t *link) {

	btuart_info_t *info = link->priv;
	dev_link_t **linkp;
	int ret;

	/* Locate device structure */
	for (linkp = &dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link)
			break;

	if (*linkp == NULL)
		return;

	btuart_release(link);

	if (link->handle) {

		ret = pcmcia_deregister_client(link->handle);
		if (ret != CS_SUCCESS)
			cs_error(link->handle, DeregisterClient, ret);
	}

	/* Unlink device structure, free bits */
	*linkp = link->next;

	kfree(info);
}


static int first_tuple(client_handle_t handle, tuple_t * tuple, cisparse_t * parse)
{
	int i;
	i = pcmcia_get_first_tuple(handle, tuple);
	if (i != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;
	i = pcmcia_get_tuple_data(handle, tuple);
	if (i != CS_SUCCESS)
		return i;
	return pcmcia_parse_tuple(handle, tuple, parse);
}

static int next_tuple(client_handle_t handle, tuple_t * tuple, cisparse_t * parse)
{
	int i;
	i = pcmcia_get_next_tuple(handle, tuple);
	if (i != CS_SUCCESS)
		return CS_NO_MORE_ITEMS;
	i = pcmcia_get_tuple_data(handle, tuple);
	if (i != CS_SUCCESS)
		return i;
	return pcmcia_parse_tuple(handle, tuple, parse);
}

static void btuart_config(dev_link_t *link) {

	static ioaddr_t base[4] = { 0x2f8, 0x3e8, 0x2e8, 0x0 };
	client_handle_t handle = link->handle;
	btuart_info_t *info = link->priv;
	tuple_t tuple;
	u_short buf[256];
	cisparse_t parse;
	cistpl_cftable_entry_t *cf = &parse.cftable_entry;
	config_info_t config;
	int i, j, try, last_ret, last_fn;


	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0;
	tuple.TupleDataMax = 255;
	tuple.Attributes = 0;

	/* Get configuration register information */
	tuple.DesiredTuple = CISTPL_CONFIG;
	last_ret = first_tuple(handle, &tuple, &parse);

	if (last_ret != CS_SUCCESS) {
		last_fn = ParseTuple;
		goto cs_failed;
	}

	link->conf.ConfigBase = parse.config.base;
	link->conf.Present = parse.config.rmask[0];

	/* Configure card */
	link->state |= DEV_CONFIG;
	i = pcmcia_get_configuration_info(handle, &config);
	link->conf.Vcc = config.Vcc;


	/* First pass: look for a config entry that looks normal. */
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleOffset = 0; tuple.TupleDataMax = 255;
	tuple.Attributes = 0;
	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;

	/* Two tries: without IO aliases, then with aliases */
	for (try = 0; try < 2; try++) {

		i = first_tuple(handle, &tuple, &parse);

		while (i != CS_NO_MORE_ITEMS) {

			if (i != CS_SUCCESS)
				goto next_entry;

			if (cf->vpp1.present & (1<<CISTPL_POWER_VNOM))
				link->conf.Vpp1 = link->conf.Vpp2 = cf->vpp1.param[CISTPL_POWER_VNOM]/10000;

			if ((cf->io.nwin > 0) && (cf->io.win[0].len == 8) && (cf->io.win[0].base != 0)) {
				link->conf.ConfigIndex = cf->index;
				link->io.BasePort1 = cf->io.win[0].base;
				link->io.IOAddrLines = (try == 0) ? 16 : cf->io.flags & CISTPL_IO_LINES_MASK;

				i = pcmcia_request_io(link->handle, &link->io);
				if (i == CS_SUCCESS)
					goto found_port;
			}
next_entry:

			i = next_tuple(handle, &tuple, &parse);
		}
	}

	/* Second pass: try to find an entry that isn't picky about
	   its base address, then try to grab any standard serial port
	   address, and finally try to get any free port. */
	i = first_tuple(handle, &tuple, &parse);

	while (i != CS_NO_MORE_ITEMS) {
		if ((i == CS_SUCCESS) && (cf->io.nwin > 0) && ((cf->io.flags & CISTPL_IO_LINES_MASK) <= 3)) {
			link->conf.ConfigIndex = cf->index;

			for (j = 0; j < 5; j++) {
				link->io.BasePort1 = base[j];
				link->io.IOAddrLines = base[j] ? 16 : 3;

				i = pcmcia_request_io(link->handle, &link->io);
				if (i == CS_SUCCESS)
					goto found_port;
			}
		}

		i = next_tuple(handle, &tuple, &parse);
	}

found_port:

	if (i != CS_SUCCESS) {
		printk(KERN_ERR "bt950uart_cs: No usable port range found. Giving up.\n");
		cs_error(link->handle, RequestIO, i);
		goto failed;
	}

	i = pcmcia_request_irq(link->handle, &link->irq);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestIRQ, i);
		link->irq.AssignedIRQ = 0;
	}

	i = pcmcia_request_configuration(link->handle, &link->conf);
	if (i != CS_SUCCESS) {
		cs_error(link->handle, RequestConfiguration, i);
		goto failed;
	}

	if (btuart_open(info) != 0)
		goto failed;

	strcpy(info->node.dev_name, info->hdev.name);
	link->dev = &info->node;
	link->state &= ~DEV_CONFIG_PENDING;

	return;

cs_failed:
	cs_error(link->handle, last_fn, last_ret);
failed:
	btuart_release(link);
	link->state &= ~DEV_CONFIG_PENDING;

}


static void btuart_release(dev_link_t *link) {

	btuart_info_t *info = link->priv;

	link->state &= ~DEV_PRESENT;
	if (link->state & DEV_CONFIG) {
		btuart_close(info);

		link->dev = NULL;

		pcmcia_release_configuration(link->handle);
		pcmcia_release_io(link->handle, &link->io);
		pcmcia_release_irq(link->handle, &link->irq);

		link->state &= ~DEV_CONFIG;
	}
}


static int btuart_event(event_t event, int priority, event_callback_args_t *args) {

	dev_link_t *link = args->client_data;
	btuart_info_t *info = link->priv;


	switch (event) {
		case CS_EVENT_CARD_REMOVAL:
			btuart_release(link);
			break;
		case CS_EVENT_CARD_INSERTION:
			link->state |= DEV_PRESENT | DEV_CONFIG_PENDING;
			btuart_config(link);
			break;
		case CS_EVENT_PM_SUSPEND:
			link->state |= DEV_SUSPEND;
			/* Fall through... */
		case CS_EVENT_RESET_PHYSICAL:
			if (link->state & DEV_CONFIG) {
				stop_uart(info);
				pcmcia_release_configuration(link->handle);
			}
			break;
		case CS_EVENT_PM_RESUME:
			link->state &= ~DEV_SUSPEND;
			/* Fall through... */
		case CS_EVENT_CARD_RESET:
			if (link->state & DEV_CONFIG) {
				pcmcia_request_configuration(link->handle, &link->conf);
				setup_uart(info);
			}
			break;
	}
	return 0;

}



/* ======================== Module initialization ======================== */


int __init init_bt950uart_cs(void) {

	servinfo_t serv;
	int err;

	printk(KERN_ERR "bt950uart_cs: %s ver.%s\n", DRIVER_NAME, DRIVER_VERSION);

	pcmcia_get_card_services_info(&serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_NOTICE "affix_uart_cs: Card Services release does not match!\n");
		return -1;
	}
	
	err = pcmcia_register_driver(&btuart_driver);
	return err;
}


void __exit exit_bt950uart_cs(void) {

	pcmcia_unregister_driver(&btuart_driver);

	while (dev_list != NULL)
		btuart_detach(dev_list);

}


module_init(init_bt950uart_cs);
module_exit(exit_bt950uart_cs);

