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
   $Id: btuart_tlp.c,v 1.1 2004/02/22 18:36:31 kassatki Exp $

   BTUART - physical protocol layer for Nokia Connectivity Card

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
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
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/termios.h>
#include <linux/serial_reg.h>
#include <linux/serial.h>

/* Local Includes */
#define	FILEBIT	DBDRV

#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/uart.h>

int  btuart_init_tlp(struct affix_uart *btuart);
void  btuart_uninit_tlp(struct affix_uart *btuart);
void btuart_recv_buf_tlp(struct affix_uart *, const unsigned char *, int);
int btuart_enqueue_tlp(struct affix_uart *btuart, struct sk_buff *skb);
struct sk_buff *btuart_dequeue_tlp(struct affix_uart *btuart);

void inline btuart_wait_sync(struct affix_uart *btuart)
{
	struct timeval		ctime;
	suseconds_t		diff;

	DBPRT("btuart_wait_sync\n");
	
	if (test_and_clear_bit(0, &btuart->sync)) {
		do_gettimeofday(&ctime);
		diff = sub_timeval(&ctime, &btuart->sync_stamp);
		
		if (diff < NCC_SYNC_TIME) {
			DBPRT("udelay: %ld\n", NCC_SYNC_TIME - diff);

			udelay(NCC_SYNC_TIME - diff);
		}
	}
}

/*  Check if RI has changed. If it has the card can accept the next
    TLP packet, so we have to reenable the network sending queue.
    This type of flow control is Nokia specific. 
*/
irqreturn_t btuart_isr(int irq_num, void *dev_inst, struct pt_regs *regs)
{
#define UART_ISRMSG_CNT		200
	struct affix_uart	*btuart = dev_inst;
	unsigned char	uart_msr;

	if (!test_bit(BTUART_RUNNING, &btuart->flags))
		return IRQ_NONE;
	uart_msr = serial_in(btuart, UART_MSR);
#ifdef CONFIG_AFFIX_DEBUG
	if (++btuart->irqcnt % UART_ISRMSG_CNT == 1)
		DBPRT("cnt=%d  RI=%d\n", 
			btuart->irqcnt, 
			uart_msr & UART_MSR_RI ? 1 : 0);
#endif
	if (btuart->uart_ri_latch ^ (uart_msr & UART_MSR_RI)) {
		DBPRT("Reenable sending queue\n");
		btuart->uart_ri_latch = uart_msr & UART_MSR_RI;
		btuart->flowmask = TLP_CMD_ON | TLP_ACL_ON | TLP_SCO_ON;
		tasklet_schedule(&btuart->tx_task);
	}
	return IRQ_HANDLED;
}

/*
   if we change status from disable to enable.. -> mark_bh
*/
int btuart_receive_tlp_control(struct affix_uart *btuart, struct sk_buff *skb)
{
	__u8	flowmask = *(__u8*)skb->data;

	DBFENTER;

	DBPRT("-->> CONTROL packet\n");

	/* transition to active state */
	if ((btuart->flowmask & 0x07) == 0 && (flowmask & 0x07) != 0)
		hcidev_wake_queue(btuart->hci);

	btuart->flowmask = flowmask;

	if (flowmask & NCC_SYNC) {
		/* wait SYNC_TIME */
		set_bit(0, &btuart->sync);
		do_gettimeofday(&btuart->sync_stamp);
	}
	//netdev_state_change(btuart->dev);	/* just let HCI check state */
	dev_kfree_skb_any(skb);
	DBFEXIT;
	return 0;
}

/*
  should be used with low_latency == 1
*/
void btuart_recv_buf_tlp(struct affix_uart *btuart, const unsigned char *cp, int count)
{
	int		lmin;
	tlp_hdr_t	*tlp = (void*)btuart->hdr;

	DBFENTER;
	//DBPRT("We've received %d bytes\n", count);
	while (count) {
		if (btuart->rx_count <  TLP_HDR_LEN) {
			lmin = btmin(TLP_HDR_LEN - btuart->rx_count, count);
			memcpy(btuart->hdr + btuart->rx_count, cp, lmin);
			btuart->rx_count += lmin;
			cp += lmin;
			count -= lmin;
			
			if (btuart->rx_count >= TLP_HDR_LEN) {
				int	pkt_len;
				tlp->length = __btoh16(tlp->length);
				pkt_len = TLP_HDR_LEN + tlp->length + TLP_PAD_LEN(tlp->length);
				btuart->rx_skb = dev_alloc_skb(pkt_len);
				if (btuart->rx_skb == NULL) {
					BTERROR("No memory for skbuff\n");
					/* No free memory */
					btuart->rx_count = 0;
					return;
				}
				//btuart->rx_skb->pkt_type = btuart->hdr[0];
				btuart_cb(btuart->rx_skb)->pkt_len = pkt_len;
				/* copy tlp header */
				memcpy(skb_put(btuart->rx_skb, TLP_HDR_LEN), tlp, TLP_HDR_LEN);
			}
		} else {
			if (!btuart->rx_skb) {
				btuart->rx_count = 0;
				continue;
			}
			if (btuart->rx_skb->len < btuart_cb(btuart->rx_skb)->pkt_len) {
				lmin = btmin(btuart_cb(btuart->rx_skb)->pkt_len - btuart->rx_skb->len, count);
				memcpy(skb_put(btuart->rx_skb, lmin), cp, lmin);
				cp += lmin;
				count -= lmin;
				if (btuart->rx_skb->len >= btuart_cb(btuart->rx_skb)->pkt_len) {
					/* send skb to upper layer */
					/* remove PAD byte if it exists */
					if (TLP_PAD_LEN(tlp->length)) {
						btuart->rx_skb->tail--;
						btuart->rx_skb->len--;
					}
					if (tlp->type == TLP_CONTROL) {
						skb_pull(btuart->rx_skb, TLP_HDR_LEN);
						btuart_receive_tlp_control(btuart, btuart->rx_skb);
					} else {
						DBPRT("TLP packet received, length: %d\n",
								btuart->rx_skb->len);
						DBDUMP(btuart->rx_skb->data, btuart->rx_skb->len);
						/* remove TLP header */
						skb_pull(btuart->rx_skb, TLP_HDR_LEN);
						btuart->rx_skb->pkt_type = tlp->type & 0x7F;
						hcidev_rx(btuart->hci, btuart->rx_skb);/* send to upper protocol layer */
					}
					btuart->rx_skb = NULL;
					btuart->rx_count = 0;
				}
			}
		}
	}
}

int btuart_enqueue_tlp(struct affix_uart *btuart, struct sk_buff *skb)
{
	tlp_hdr_t	*tlp;
	__u16		dlen = skb->len;

	DBFENTER;

	if (TLP_PAD_LEN(dlen)) {
		if (!skb_tailroom(skb)) {
			struct sk_buff	*s;
			s = dev_alloc_skb(TLP_HDR_LEN + dlen + 1);
			skb_reserve(s, TLP_HDR_LEN);
			memcpy(skb_put(s, dlen), skb->data, dlen);
			dev_kfree_skb_any(skb);
			skb = s;
		}
		*skb_put(skb, 1) = 0;
	}
	tlp = (tlp_hdr_t*)skb_push(skb, TLP_HDR_LEN);
	tlp->zero = 0;
	tlp->length = __htob16(dlen);
	tlp->type = skb->pkt_type | 0x80;
	DBPRT("Transmit TLP packet, length: %d\n", skb->len);
	DBDUMP(skb->data, skb->len);
	skb_queue_tail(&btuart->tx_q, skb);
	DBFEXIT;
	return 0;
}

struct sk_buff *btuart_dequeue_tlp(struct affix_uart *btuart)
{
	struct sk_buff	*skb;
	
	if (!btuart->flowmask)
		return NULL;
	skb =  skb_dequeue(&btuart->tx_q);
	if (skb && (btuart->uart.flags & AFFIX_UART_RI)) {
		btuart->flowmask = 0;
	}
	return skb;
}

int btuart_init_tlp(struct affix_uart *btuart)
{
	int			err;

	DBFENTER;

	/*  Setup an interrupt service routine to handle the "RI" flow control.
	    This type of flow control is Nokia specific.
	    Modem Status Change interrupts are enabled by the serial driver
	*/
	if (btuart->uart.flags & AFFIX_UART_RI) {
		/* read RI state */
		btuart->uart_ri_latch = serial_in(btuart, UART_MSR) & UART_MSR_RI;
		/* set IRQ handler */
		err = request_irq(btuart->ser.irq, btuart_isr, SA_SHIRQ, BTUART_DEVNAME, btuart);
		if (err)
			goto ioerr;
	}
	btuart->flowmask = TLP_CMD_ON | TLP_ACL_ON | TLP_SCO_ON;
	DBFEXIT;
	return 0;
ioerr:
	DBPRT("Unable to set discipline/terminal parameters: err: %d\n", err);
	return err;
}

void btuart_uninit_tlp(struct affix_uart *btuart)
{
	DBFENTER;
	btuart->flowmask = 0;
	if (btuart->uart.flags & AFFIX_UART_RI)
		free_irq(btuart->ser.irq, btuart);
	DBFEXIT;
}

