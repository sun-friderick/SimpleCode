/* 
   BlueCore Serial Protocol (BCSP) for Linux Bluetooth stack (BlueZ).
   Copyright 2002 by Fabrizio Gennari <fabrizio.gennari@philips.com>

   Based on
       hci_h4.c  by Maxim Krasnyansky <maxk@qualcomm.com>
       ABCSP     by Carl Orsborn <cjo@csr.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation;

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
   IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) AND AUTHOR(S) BE LIABLE FOR ANY
   CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES 
   WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN 
   ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF 
   OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

   ALL LIABILITY, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PATENTS, 
   COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS, RELATING TO USE OF THIS 
   SOFTWARE IS DISCLAIMED.
*/

/*
 * $Id: btuart_bcsp.c,v 1.1 2004/02/22 18:36:31 kassatki Exp $
 *
 * Fixes & Improvements by Dmitry Kasatkin
 * - Aknownledgement fix
 * - Link Establishment
 */

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


#define	FILEBIT	DBDRV

#include <affix/uart.h>

#define CONFIG_BLUEZ_HCIUART_BCSP_TXCRC

#include <affix/bluez.h>
#include "btuart_bcsp.h"

static inline void hci_uart_tx_wakeup(struct affix_uart *btuart)
{
	btuart_xmit_wakeup(btuart);
}


/* --------------------------------------------------- */

#define VERSION "0.2-affix"

/*
 * LE packets
 */
u8 conf_pkt[4]     = { 0xad, 0xef, 0xac, 0xed };
u8 conf_rsp_pkt[4] = { 0xde, 0xad, 0xd0, 0xd0 };
u8 sync_pkt[4]     = { 0xda, 0xdc, 0xed, 0xed };
u8 sync_rsp_pkt[4] = { 0xac, 0xaf, 0xef, 0xee };


/* ---- BCSP CRC calculation ---- */

/* Table for calculating CRC for polynomial 0x1021, LSB processed first,
initial value 0xffff, bits shifted in reverse order. */

static const u16 crc_table[] = {
	0x0000, 0x1081, 0x2102, 0x3183,
	0x4204, 0x5285, 0x6306, 0x7387,
	0x8408, 0x9489, 0xa50a, 0xb58b,
	0xc60c, 0xd68d, 0xe70e, 0xf78f
};

/* Initialise the crc calculator */
#define BCSP_CRC_INIT(x) x = 0xffff

/*
   Update crc with next data byte

   Implementation note
        The data byte is treated as two nibbles.  The crc is generated
        in reverse, i.e., bits are fed into the register from the top.
*/
static void bcsp_crc_update(u16 *crc, u8 d)
{
	u16 reg = *crc;

	reg = (reg >> 4) ^ crc_table[(reg ^ d) & 0x000f];
	reg = (reg >> 4) ^ crc_table[(reg ^ (d >> 4)) & 0x000f];

	*crc = reg;
}

/*
   Get reverse of generated crc

   Implementation note
        The crc generator (bcsp_crc_init() and bcsp_crc_update())
        creates a reversed crc, so it needs to be swapped back before
        being passed on.
*/
static u16 bcsp_crc_reverse(u16 crc)
{
	u16 b, rev;

	for (b = 0, rev = 0; b < 16; b++) {
		rev = rev << 1;
		rev |= (crc & 1);
		crc = crc >> 1;
	}
	return (rev);
}

/* ---- BCSP core ---- */

static void bcsp_slip_msgdelim(struct sk_buff *skb)
{
	const char pkt_delim = 0xc0;
	memcpy(skb_put(skb, 1), &pkt_delim, 1);
}

static void bcsp_slip_one_byte(struct sk_buff *skb, u8 c)
{
	const char esc_c0[2] = { 0xdb, 0xdc };
	const char esc_db[2] = { 0xdb, 0xdd };

	switch (c) {
	case 0xc0:
		memcpy(skb_put(skb, 2), &esc_c0, 2);
		break;
	case 0xdb:
		memcpy(skb_put(skb, 2), &esc_db, 2);
		break;
	default:
		memcpy(skb_put(skb, 1), &c, 1);
	}
}

int bcsp_enqueue(struct affix_uart *hu, struct sk_buff *skb)
{
	struct bcsp_struct *bcsp = hu->priv;

	DBFENTER;
	if (skb->len > 0xFFF) {
		BT_ERR("Packet too long");
		kfree_skb(skb);
		return 0;
	}

	switch (skb->pkt_type) {
	case HCI_ACLDATA_PKT:
	case HCI_COMMAND_PKT:
		skb_queue_tail(&bcsp->rel, skb);
		break;

	case HCI_SCODATA_PKT:
		skb_queue_tail(&bcsp->unrel, skb);
		break;
		
	default:
		BT_ERR("Unknown packet type");
		kfree_skb(skb);
		break;
	}
	DBFEXIT;
	return 0;
}

static struct sk_buff *bcsp_prepare_pkt(struct bcsp_struct *bcsp, u8 *data,
		int len, int pkt_type)
{
	struct sk_buff	*nskb;
	u8		hdr[4], chan;
	int		rel, i;
#ifdef CONFIG_BLUEZ_HCIUART_BCSP_TXCRC
	u16 		BCSP_CRC_INIT(bcsp_txmsg_crc);
#endif

	DBFENTER;

	switch (pkt_type) {
	case HCI_ACLDATA_PKT:
		chan = 6;	/* BCSP ACL channel */
		rel = 1;	/* reliable channel */
		break;
	case HCI_COMMAND_PKT:
		chan = 5;	/* BCSP cmd/evt channel */
		rel = 1;	/* reliable channel */
		break;
	case HCI_SCODATA_PKT:
		chan = 7;	/* BCSP SCO channel */
		rel = 0;	/* unreliable channel */
		break;
	case BCSP_LE_PKT:
		chan = 1;	/* BCSP LE channel */
		rel = 0;	/* unreliable channel */
		hdr[0] = 0;
		break;
	case BCSP_ACK_PKT:
		chan = 0;	/* BCSP internal channel */
		rel = 0;	/* unreliable channel */
		break;
	default:
		BT_ERR("Unknown packet type");
		return NULL;
	}

	/* Max len of packet: (original len +4(bcsp hdr) +2(crc))*2
	   (because bytes 0xc0 and 0xdb are escaped, worst case is
	   when the packet is all made of 0xc0 and 0xdb :) )
	   + 2 (0xc0 delimiters at start and end). */

	nskb = alloc_skb((len + 6) * 2 + 2, GFP_ATOMIC);
	if (!nskb)
		return NULL;

	nskb->pkt_type = pkt_type;

	bcsp_slip_msgdelim(nskb);

	if (pkt_type != BCSP_LE_PKT) {
		/* normal packets */
		hdr[0] = bcsp->rxseq_txack << 3;
		bcsp->txack_req = 0;
		BT_DBG("Request for pkt no %u from the card", bcsp->rxseq_txack);

		if (rel) {
			hdr[0] |= 0x80 + bcsp->msgq_txseq;
			BT_DBG("Sending packet no %u to the card", bcsp->msgq_txseq);
			bcsp->msgq_txseq = (bcsp->msgq_txseq + 1) & 0x07;	//XXX:
			//bcsp->msgq_txseq = ++(bcsp->msgq_txseq) & 0x07;
		}
#ifdef  CONFIG_BLUEZ_HCIUART_BCSP_TXCRC
		hdr[0] |= 0x40;
#endif
	}
	hdr[1]  = (len << 4) & 0xFF;
	hdr[1] |= chan;
	hdr[2]  = len >> 4;
	hdr[3]  = ~(hdr[0] + hdr[1] + hdr[2]);

	/* Put BCSP header */
	for (i = 0; i < 4; i++) {
		bcsp_slip_one_byte(nskb, hdr[i]);
#ifdef  CONFIG_BLUEZ_HCIUART_BCSP_TXCRC
		bcsp_crc_update(&bcsp_txmsg_crc, hdr[i]);
#endif
	}

	/* Put payload */
	for (i = 0; i < len; i++) {
		bcsp_slip_one_byte(nskb, data[i]);
#ifdef  CONFIG_BLUEZ_HCIUART_BCSP_TXCRC
		bcsp_crc_update(&bcsp_txmsg_crc, data[i]);
#endif
	}

#ifdef CONFIG_BLUEZ_HCIUART_BCSP_TXCRC
	/* Put CRC */
	if (pkt_type != BCSP_LE_PKT) {
		bcsp_txmsg_crc = bcsp_crc_reverse(bcsp_txmsg_crc);
		bcsp_slip_one_byte(nskb, (u8) ((bcsp_txmsg_crc >> 8) & 0x00ff));
		bcsp_slip_one_byte(nskb, (u8) (bcsp_txmsg_crc & 0x00ff));
	}
#endif

	bcsp_slip_msgdelim(nskb);
	DBFEXIT;
	return nskb;
}

/* This is a rewrite of pkt_avail in ABCSP */
struct sk_buff *bcsp_dequeue(struct affix_uart *hu)
{
	struct bcsp_struct *bcsp = (struct bcsp_struct *) hu->priv;
	unsigned long flags;
	struct sk_buff *skb;
	
	DBFENTER;
	/* First of all, check for unreliable messages in the queue,
	   since they have priority */

	if ((skb = skb_dequeue(&bcsp->unrel)) != NULL) {
		struct sk_buff *nskb = bcsp_prepare_pkt(bcsp, skb->data, skb->len, skb->pkt_type);
		if (nskb) {
			kfree_skb(skb);
			DBFEXIT;
			return nskb;
		} else {
			skb_queue_head(&bcsp->unrel, skb);
			BT_ERR("Could not dequeue pkt because alloc_skb failed");
		}
	}

	/* check state */
	if (bcsp->state != BCSP_STATE_GARRULOUS) {
		/* transmission disabled for normal traffic */
		return NULL;
	}

	/* Now, try to send a reliable pkt. We can only send a
	   reliable packet if the number of packets sent but not yet ack'ed
	   is < than the winsize */

	spin_lock_irqsave(&bcsp->unack.lock, flags);

	if (bcsp->unack.qlen < BCSP_TXWINSIZE && (skb = skb_dequeue(&bcsp->rel)) != NULL) {
		struct sk_buff *nskb = bcsp_prepare_pkt(bcsp, skb->data, skb->len, skb->pkt_type);
		if (nskb) {
			__skb_queue_tail(&bcsp->unack, skb);
			mod_timer(&bcsp->tbcsp, jiffies + HZ / 4);
			spin_unlock_irqrestore(&bcsp->unack.lock, flags);
			return nskb;
		} else {
			skb_queue_head(&bcsp->rel, skb);
			BT_ERR("Could not dequeue pkt because alloc_skb failed");
		}
	}

	spin_unlock_irqrestore(&bcsp->unack.lock, flags);


	/* We could not send a reliable packet, either because there are
	   none or because there are too many unack'ed pkts. Did we receive
	   any packets we have not acknowledged yet ? */

	if (bcsp->txack_req) {
		/* if so, craft an empty ACK pkt and send it on BCSP unreliable
		   channel 0 */
		struct sk_buff *nskb = bcsp_prepare_pkt(bcsp, NULL, 0, BCSP_ACK_PKT);
		DBFEXIT;
		return nskb;
	}

	/* We have nothing to send */
	return NULL;
}

/* Remove ack'ed packets */
static void bcsp_pkt_cull(struct bcsp_struct *bcsp)
{
	unsigned long	flags;
	struct sk_buff	*skb;
	int 		pkts_to_be_removed;
	u8 		not_acked;

	spin_lock_irqsave(&bcsp->unack.lock, flags);

	not_acked = (bcsp->msgq_txseq - bcsp->rxack) & 0x07;
	pkts_to_be_removed = bcsp->unack.qlen - not_acked;

	if (pkts_to_be_removed < 0) {
		BT_ERR("Peer acked invalid packet - more than sent, txseq: %u, rxack: %u",
				bcsp->msgq_txseq, bcsp->rxack);
		pkts_to_be_removed = 0;
	}

	BT_DBG("Removing %u pkts of %u, up to seqno %u",
	       pkts_to_be_removed, bcsp->unack.qlen, (bcsp->rxack - 1) & 0x07);

	while (pkts_to_be_removed-- && (skb = __skb_dequeue(&bcsp->unack)))
		dev_kfree_skb_any(skb);

	if (bcsp->unack.qlen == 0)
		del_timer(&bcsp->tbcsp);

	spin_unlock_irqrestore(&bcsp->unack.lock, flags);
}

/* Handle BCSP link-establishment packets. When we
   detect a "sync" packet, symptom that the BT module has reset,
   we do nothing :) (yet) */
static void bcsp_handle_le_pkt(struct affix_uart *hu)
{
	struct bcsp_struct *bcsp = hu->priv;

	DBFENTER;
	BT_DBG("%s", bcsp_state(bcsp));
	
	/* spot "conf" pkts and reply with a "conf rsp" pkt */
	if (bcsp->rx_skb->data[1] >> 4 != 4 || bcsp->rx_skb->data[2] != 0) {
		/* probably not config packet - not 4 bytes len */
		return;
	}
	/* Spot "sync" pkts. If we find one...disaster! */
	if (!memcmp(&bcsp->rx_skb->data[4], sync_pkt, 4)) {
		BT_DBG("Found a LE sync pkt");
		if (bcsp->state == BCSP_STATE_GARRULOUS) {
			BTINFO("Found a LE sync pkt, card has reset\n");
#if 0			
			bcsp->state = BCSP_STATE_SHY;
			bcsp_send_le_pkt(hu, sync_pkt, Tshy);
#endif
		} else {
			bcsp_send_le_pkt(hu, sync_rsp_pkt, 0);
		}
	} else if (!memcmp(&bcsp->rx_skb->data[4], sync_rsp_pkt, 4)) {
		BT_DBG("Found a LE sync_rsp pkt");
		if (bcsp->state == BCSP_STATE_SHY) {
			del_timer(&bcsp->tbcsp);
			bcsp->conf_count = conf_count_limit;
			bcsp->state = BCSP_STATE_CURIOUS;
			mod_timer(&bcsp->tbcsp, jiffies + Tconf);
		}
	} else if (!memcmp(&bcsp->rx_skb->data[4], conf_pkt, 4)) {
		BT_DBG("Found a LE conf pkt");
		if (bcsp->state == BCSP_STATE_SHY) {
			/* ignore it */
		} else {
			bcsp_send_le_pkt(hu, conf_rsp_pkt, 0);
		}
	} else if (!memcmp(&bcsp->rx_skb->data[4], conf_rsp_pkt, 4)) {
		BT_DBG("Found a LE conf_rsp pkt");
		if (bcsp->state == BCSP_STATE_CURIOUS) {
			del_timer(&bcsp->tbcsp);
			bcsp->state = BCSP_STATE_GARRULOUS;
			bcsp->rxseq_txack = 0;
			bcsp->msgq_txseq = 0;
			bcsp->rxack = 0;
			BT_DBG("%s", bcsp_state(bcsp));
			/* wakeup queue */
			hci_uart_tx_wakeup(hu);
		}
	}
	DBFEXIT;
}

static inline void bcsp_unslip_one_byte(struct bcsp_struct *bcsp, unsigned char byte)
{
	const u8 c0 = 0xc0, db = 0xdb;
	
	//DBFENTER;
	switch (bcsp->rx_esc_state) {
	case BCSP_ESCSTATE_NOESC:
		switch (byte) {
		case 0xdb:
			bcsp->rx_esc_state = BCSP_ESCSTATE_ESC;
			break;
		default:
			memcpy(skb_put(bcsp->rx_skb, 1), &byte, 1);
			if ((bcsp->rx_skb-> data[0] & 0x40) != 0 && 
					bcsp->rx_state != BCSP_W4_CRC)
				bcsp_crc_update(&bcsp->message_crc, byte);
			bcsp->rx_count--;
		}
		break;

	case BCSP_ESCSTATE_ESC:
		switch (byte) {
		case 0xdc:
			memcpy(skb_put(bcsp->rx_skb, 1), &c0, 1);
			if ((bcsp->rx_skb-> data[0] & 0x40) != 0 && 
					bcsp->rx_state != BCSP_W4_CRC)
				bcsp_crc_update(&bcsp->message_crc, 0xc0);
			bcsp->rx_esc_state = BCSP_ESCSTATE_NOESC;
			bcsp->rx_count--;
			break;

		case 0xdd:
			memcpy(skb_put(bcsp->rx_skb, 1), &db, 1);
			if ((bcsp->rx_skb-> data[0] & 0x40) != 0 && 
					bcsp->rx_state != BCSP_W4_CRC) 
				bcsp_crc_update(&bcsp->message_crc, 0xdb);
			bcsp->rx_esc_state = BCSP_ESCSTATE_NOESC;
			bcsp->rx_count--;
			break;

		default:
			BT_ERR ("Invalid byte %02x after esc byte", byte);
			kfree_skb(bcsp->rx_skb);
			bcsp->rx_skb = NULL;
			bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
			bcsp->rx_count = 0;
		}
	}
	//DBFEXIT;
}

static inline void bcsp_complete_rx_pkt(struct affix_uart *hu)
{
	struct bcsp_struct *bcsp = hu->priv;
	int pass_up;
	
	DBFENTER;
	DBPRT("got packet: %d bytes\n", bcsp->rx_skb->len);
	DBDUMP(bcsp->rx_skb->data, bcsp->rx_skb->len);

	/* check pkt types */
	if ((bcsp->rx_skb->data[1] & 0x0f) == 6 &&
			bcsp->rx_skb->data[0] & 0x80) {
		bcsp->rx_skb->pkt_type = HCI_ACLDATA_PKT;
		pass_up = 1;
	} else if ((bcsp->rx_skb->data[1] & 0x0f) == 5 &&
			bcsp->rx_skb->data[0] & 0x80) {
		bcsp->rx_skb->pkt_type = HCI_EVENT_PKT;
		pass_up = 1;
	} else if ((bcsp->rx_skb->data[1] & 0x0f) == 7) {
		bcsp->rx_skb->pkt_type = HCI_SCODATA_PKT;
		pass_up = 1;
	} else if ((bcsp->rx_skb->data[1] & 0x0f) == 1 &&
			!(bcsp->rx_skb->data[0] & 0x80)) {
		bcsp->rx_skb->pkt_type = BCSP_LE_PKT;
		bcsp_handle_le_pkt(hu);
		pass_up = 0;
	} else {
		bcsp->rx_skb->pkt_type = 0;
		pass_up = 0;
	}

	if (bcsp->rx_skb->pkt_type != BCSP_LE_PKT) {
		if (bcsp->rx_skb->data[0] & 0x80) {	/* reliable pkt */
			BT_DBG("Received packet no %u from the card", bcsp->rxseq_txack);
			bcsp->rxseq_txack = (bcsp->rxseq_txack + 1) & 0x07;	//XXX:
			//bcsp->rxseq_txack++;
			bcsp->txack_req = 1;
			/* If needed, transmit an ack pkt */
			hci_uart_tx_wakeup(hu);
		}

		bcsp->rxack = (bcsp->rx_skb->data[0] >> 3) & 0x07;
		BT_DBG("Request for pkt no %u to the card", bcsp->rxack);

		/* handle acknowledgment */
		bcsp_pkt_cull(bcsp);
	}

	if (!pass_up) {
		if ((bcsp->rx_skb->data[1] & 0x0f) != 0 &&
				(bcsp->rx_skb->data[1] & 0x0f) != 1) {
			BT_ERR ("Packet for unknown channel (%u %s)",
					bcsp->rx_skb->data[1] & 0x0f,
					bcsp->rx_skb->data[0] & 0x80 ? 
					"reliable" : "unreliable");
		}
		kfree_skb(bcsp->rx_skb);
	} else {
		/* Pull out BCSP hdr */
		skb_pull(bcsp->rx_skb, 4);
		hcidev_rx(hu->hci, bcsp->rx_skb);	//XXX:
	}
	bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
	bcsp->rx_skb = NULL;
	DBFEXIT;
}

/* Recv data */
int bcsp_recv(struct affix_uart *hu, void *data, int count)
{
	struct bcsp_struct *bcsp = hu->priv;
	register unsigned char *ptr;

	DBFENTER;
	DBPRT("%d bytes received\n", count);
	DBDUMP(data, count);

	BT_DBG("hu %p count %d rx_state %d rx_count %lu", 
		hu, count, bcsp->rx_state, bcsp->rx_count);

	ptr = data;
	while (count) {
		if (bcsp->rx_count) {
			if (*ptr == 0xc0) {
				BT_ERR("Short BCSP packet");
				kfree_skb(bcsp->rx_skb);
				bcsp->rx_state = BCSP_W4_PKT_START;
				bcsp->rx_count = 0;
			} else
				bcsp_unslip_one_byte(bcsp, *ptr);

			ptr++; count--;
			continue;
		}

		switch (bcsp->rx_state) {
		case BCSP_W4_BCSP_HDR:
			if ((0xff & (u8) ~ (bcsp->rx_skb->data[0] + bcsp->rx_skb->data[1] +
					bcsp->rx_skb->data[2])) != bcsp->rx_skb->data[3]) {
				BT_ERR("Error in BCSP hdr checksum");
				kfree_skb(bcsp->rx_skb);
				bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
				bcsp->rx_count = 0;
				continue;
			}
			/* check state */
			if (!((bcsp->rx_skb->data[1] & 0x0f) == 1 &&
				!(bcsp->rx_skb->data[0] & 0x80)) && 
					bcsp->state != BCSP_STATE_GARRULOUS) {
				/* not LE pkt at non GARRULOUS state */
				kfree_skb(bcsp->rx_skb);
				bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
				bcsp->rx_count = 0;
				continue;
			}
			/* may be move this code to bcsp_complete_rx_pkt ?? */
			if (bcsp->rx_skb->data[0] & 0x80	/* reliable pkt */
			    		&& (bcsp->rx_skb->data[0] & 0x07) != bcsp->rxseq_txack) {
				BT_ERR ("Out-of-order packet arrived: got %u, expected %u",
					bcsp->rx_skb->data[0] & 0x07, bcsp->rxseq_txack);

				kfree_skb(bcsp->rx_skb);
				bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
				bcsp->rx_count = 0;
#if 1
				//XXX:
				bcsp->txack_req = 1;
				/* transmit an ack pkt for expected rxseq_txack */
				hci_uart_tx_wakeup(hu);
#endif
				continue;
			}
			bcsp->rx_state = BCSP_W4_DATA;
			bcsp->rx_count = (bcsp->rx_skb->data[1] >> 4) + 
					(bcsp->rx_skb->data[2] << 4);	/* May be 0 */
			continue;

		case BCSP_W4_DATA:
			if (bcsp->rx_skb->data[0] & 0x40) {	/* pkt with crc */
				bcsp->rx_state = BCSP_W4_CRC;
				bcsp->rx_count = 2;
			} else
				bcsp_complete_rx_pkt(hu);
			continue;

		case BCSP_W4_CRC:
			if (bcsp_crc_reverse(bcsp->message_crc) !=
					(bcsp->rx_skb->data[bcsp->rx_skb->len - 2] << 8) +
					bcsp->rx_skb->data[bcsp->rx_skb->len - 1]) {

				BT_ERR ("Checksum failed: computed %04x received %04x",
					bcsp_crc_reverse(bcsp->message_crc),
				     	(bcsp->rx_skb-> data[bcsp->rx_skb->len - 2] << 8) +
				     	bcsp->rx_skb->data[bcsp->rx_skb->len - 1]);

				kfree_skb(bcsp->rx_skb);
				bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
				bcsp->rx_count = 0;
				continue;
			}
			skb_trim(bcsp->rx_skb, bcsp->rx_skb->len - 2);
			bcsp_complete_rx_pkt(hu);
			continue;

		case BCSP_W4_PKT_DELIMITER:
			switch (*ptr) {
			case 0xc0:
				bcsp->rx_state = BCSP_W4_PKT_START;
				break;
			default:
				/*BT_ERR("Ignoring byte %02x", *ptr);*/
				break;
			}
			ptr++; count--;
			break;

		case BCSP_W4_PKT_START:
			switch (*ptr) {
			case 0xc0:
				ptr++; count--;
				break;

			default:
				bcsp->rx_state = BCSP_W4_BCSP_HDR;
				bcsp->rx_count = 4;
				bcsp->rx_esc_state = BCSP_ESCSTATE_NOESC;
				BCSP_CRC_INIT(bcsp->message_crc);
				
				/* Do not increment ptr or decrement count
				 * Allocate packet. Max len of a BCSP pkt= 
				 * 0xFFF (payload) +4 (header) +2 (crc) */

				bcsp->rx_skb = dev_alloc_skb(0x1005);	//XXX:
				if (!bcsp->rx_skb) {
					BT_ERR("Can't allocate mem for new packet");
					bcsp->rx_state = BCSP_W4_PKT_DELIMITER;
					bcsp->rx_count = 0;
					return 0;
				}
				//bcsp->rx_skb->dev = (void*)hu->hci;	//XXX:
				break;
			}
			break;
		}
	}
	DBFEXIT;
	return count;
}

/* Arrange to retransmit all messages in the relq. */
static void bcsp_timed_event(unsigned long arg)
{
	struct affix_uart		*hu = (struct affix_uart *) arg;
	struct bcsp_struct	*bcsp = (struct bcsp_struct *) hu->priv;
	struct sk_buff		*skb;
	unsigned long		flags;

	DBFENTER;
	BT_DBG("%s", bcsp_state(bcsp));

	if (bcsp->state == BCSP_STATE_SHY) {
		/* sync timeout */
		BT_DBG("Sync Timeout");
		bcsp_send_le_pkt(hu, sync_pkt, Tshy);
	} else if (bcsp->state == BCSP_STATE_CURIOUS) {
		/* conf timeout */
		BT_DBG("Conf Timeout");
		if (bcsp->conf_count--)
			bcsp_send_le_pkt(hu, conf_pkt, Tconf);
		else
			BTINFO("Conf not succeded\n");
	} else  if (bcsp->state == BCSP_STATE_GARRULOUS) {
		/* normal traffic */
		BT_DBG("Timeout, retransmitting %u pkts", bcsp->unack.qlen);
		spin_lock_irqsave(&bcsp->unack.lock, flags);
		while ((skb = __skb_dequeue_tail(&bcsp->unack)) != NULL) {
			bcsp->msgq_txseq = (bcsp->msgq_txseq - 1) & 0x07;	//XXX:
			skb_queue_head(&bcsp->rel, skb);
		}
		spin_unlock_irqrestore(&bcsp->unack.lock, flags);
		hci_uart_tx_wakeup(hu);
	}
	DBFEXIT;
}

int bcsp_open(struct affix_uart *hu)
{
	struct bcsp_struct 	*bcsp;
	int			err;
	
	DBFENTER;
	BT_DBG("hu %p", hu);

	bcsp = kmalloc(sizeof(*bcsp), GFP_ATOMIC);
	if (!bcsp)
		return -ENOMEM;
	memset(bcsp, 0, sizeof(*bcsp));

	hu->priv = bcsp;
	skb_queue_head_init(&bcsp->unack);
	skb_queue_head_init(&bcsp->rel);
	skb_queue_head_init(&bcsp->unrel);

	init_timer(&bcsp->tbcsp);
	bcsp->tbcsp.function = bcsp_timed_event;
	bcsp->tbcsp.data     = (u_long) hu;

	bcsp->rx_state = BCSP_W4_PKT_DELIMITER;

	/* start initialization - shy state */
	bcsp->state = BCSP_STATE_SHY;
	BT_DBG("%s", bcsp_state(bcsp));
	err = bcsp_send_le_pkt(hu, sync_pkt, Tshy);
	DBFEXIT;
	return err;
}

void bcsp_close(struct affix_uart *hu)
{
	struct bcsp_struct	*bcsp = hu->priv;
	
	DBFENTER;
	hu->priv = NULL;
	BT_DBG("hu %p", hu);
	skb_queue_purge(&bcsp->unack);
	skb_queue_purge(&bcsp->rel);
	skb_queue_purge(&bcsp->unrel);
	del_timer(&bcsp->tbcsp);
	kfree(bcsp);
	DBFEXIT;
}

/* -------- extra section ----------- */

/* send sync pkt */
int bcsp_send_le_pkt(struct affix_uart *hu, u8 *pkt, unsigned long timeout)
{
	struct bcsp_struct	*bcsp = hu->priv;
	struct sk_buff		*skb;
	
	skb = alloc_skb(4, GFP_ATOMIC);
	if (!skb)
		return -ENOMEM;
	memcpy(skb_put(skb, 4), pkt, 4);
	skb->pkt_type = BCSP_LE_PKT;
	skb_queue_head(&bcsp->unrel, skb);
	if (timeout)
		mod_timer(&bcsp->tbcsp, jiffies + timeout);
	hci_uart_tx_wakeup(hu);
	return 0;
}

