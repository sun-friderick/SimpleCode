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
   $Id: btuart_h4.c,v 1.1 2004/02/22 18:36:31 kassatki Exp $

   btuart_h4 - UART protocol (H4)
   
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
#include <asm/io.h>
#include <linux/serial_reg.h>

/* Local Includes */
#define	FILEBIT	DBDRV

#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/uart.h>

int btuart_receive_data_h4(struct affix_uart *btuart)
{
	DBFENTER;
	DBPRT("HCI packet, len: %d\n", btuart->rx_skb->len);
	DBDUMP(btuart->rx_skb->data, btuart->rx_skb->len);
	hcidev_rx(btuart->hci, btuart->rx_skb);	/* send to upper protocol layer */
	btuart->rx_count = 0;
	btuart->rx_skb = NULL;
	DBFEXIT;
	return 0;
}

void btuart_recv_buf_h4(struct affix_uart *btuart, const unsigned char *cp, int count)
{
	int		lmin;
	
	DBFENTER;
	DBPRT("We've received %d bytes\n", count);
	DBDUMP(cp, count);

	while (count) {
		if (btuart->rx_count == 0) {	/* expect new packet */
			btuart->hdr[0] = *cp;
			btuart->hdr_len = hci_pktlen(*btuart->hdr, NULL);
			if (btuart->hdr_len) {
				btuart->rx_count = 1;
				btuart->hdr_len++;
			}
			cp++;
			count--;
			continue;
		}
		if (btuart->rx_count < btuart->hdr_len) {
			lmin = btmin(btuart->hdr_len - btuart->rx_count, count);
			memcpy(btuart->hdr + btuart->rx_count, cp, lmin);
			btuart->rx_count += lmin;
			cp += lmin;
			count -= lmin;
			if (btuart->rx_count >= btuart->hdr_len) {
				int	pkt_len;
				pkt_len = hci_pktlen(*btuart->hdr, btuart->hdr + 1);
				btuart->rx_skb = dev_alloc_skb(pkt_len);
				if (!btuart->rx_skb) {
					BTERROR("No memory for skbuff\n");
					/* No free memory */
					btuart->rx_count = 0;
					continue;
				}
				btuart->rx_skb->pkt_type = btuart->hdr[0];
				btuart_cb(btuart->rx_skb)->pkt_len = pkt_len;
				/* copy header */
				memcpy(skb_put(btuart->rx_skb, btuart->hdr_len - 1), 
						btuart->hdr + 1, btuart->hdr_len - 1);
				if (btuart->rx_skb->len >= btuart_cb(btuart->rx_skb)->pkt_len)
					btuart_receive_data_h4(btuart);
			}
			continue;
		}
		if (!btuart->rx_skb) {
			btuart->rx_count = 0;
			continue;
		}
		if (btuart->rx_skb->len < btuart_cb(btuart->rx_skb)->pkt_len) {
			lmin = btmin(btuart_cb(btuart->rx_skb)->pkt_len - btuart->rx_skb->len, count);
			memcpy(skb_put(btuart->rx_skb, lmin), cp, lmin);
			cp += lmin;
			count -= lmin;
			if (btuart->rx_skb->len >= btuart_cb(btuart->rx_skb)->pkt_len)
				btuart_receive_data_h4(btuart);
		}
	}
	DBFEXIT;
}


int btuart_enqueue_h4(struct affix_uart *btuart, struct sk_buff *skb)
{
	DBFENTER;
	*skb_push(skb, 1) = skb->pkt_type;
	DBDUMP(skb->data, skb->len);
	skb_queue_tail(&btuart->tx_q, skb);
	DBFEXIT;
	return 0;
}

struct sk_buff *btuart_dequeue_h4(struct affix_uart *btuart)
{
	return skb_dequeue(&btuart->tx_q);
}

int btuart_init_h4(struct affix_uart *btuart)
{
	DBFENTER;
	DBFEXIT;
	return 0;
}

void btuart_uninit_h4(struct affix_uart *btuart)
{
	DBFENTER;
	DBFEXIT;
}

