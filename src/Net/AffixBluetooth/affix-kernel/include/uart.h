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
   $Id: uart.h,v 1.11 2004/02/22 18:36:32 kassatki Exp $

   BTUART - physical protocol layer for bluetooth uart devices

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/

#ifndef _BTUART_H
#define _BTUART_H

#define BTUART_DEVNAME	"affix_uart"

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/serial.h>
#include <asm/io.h>

#include <affix/hci.h>

#define N_AFFIX		14	/* line discipline */

/* NOKIA */
#define MANFID_NOKIA		0x0124
#define PRODID_DTL1		0x0900
#define PRODID_DTL4		0x1000

/* SOCKET */
#define MANFID_SOCKET		0x0104
#define PRODID_SOCKET_BTCF1	0x009f


// Shrirang - 12th july 03
#if (!defined(CONFIG_AFFIX_UART_BCSP)) /* No use using this for H4 */
#undef CONFIG_AFFIX_UART_NEW_BH
#endif

#ifdef CONFIG_AFFIX_UART_NEW_BH

/* This buffer is used to decrease overruns on serial port. Copies data in
 * interrupt context and schedules a task which consumes data at 'safe' time */

#define BT_INBUFFER_SIZE 4000

struct btuart_rxbuf {
	u8* head;       /* Start of data buffer */
	u8* tail;       /* End of data buffer */ 
	u8* put;        /* Points to where new data is inserted */
	u8* get;        /* This is where the receive task consumes data */
	u8 data[BT_INBUFFER_SIZE];
};

#endif

struct affix_uart;

struct affix_uart_proto {
	int	proto;
	int	hard_hdr_len;
	int	(*enqueue)(struct affix_uart *btuart, struct sk_buff *skb);
	struct sk_buff*	(*dequeue)(struct affix_uart *btuart);
	int	(*init)(struct affix_uart *btuart);
	void	(*uninit)(struct affix_uart *btuart);
	void	(*recv_buf)(struct affix_uart *btuart, const unsigned char *, int);
};

struct affix_uart {
	struct list_head	q;	/* for queueing */

	hci_struct 		*hci;	// HCI

#define BTUART_RUNNING		0
#define BTUART_XMIT_WAKEUP	1
	unsigned long		flags;
	spinlock_t		xmit_lock;	/* xmit lock */
	rwlock_t		lock;
	
	/* physical/tty stuff */
	affix_uart_t		uart;
	struct affix_uart_proto	proto;
	struct serial_struct	ser;

	/* tx state */
	struct sk_buff_head	tx_q;
	struct sk_buff		*tx_skb;	/* sended packet */
	struct tasklet_struct	tx_task;
#ifdef CONFIG_AFFIX_UART_NEW_BH
	struct tasklet_struct	rx_task;
	struct btuart_rxbuf	hci_data;
#endif
	/* rx state */
	struct sk_buff		*rx_skb;
	int			hdr_len;
	int			rx_count;
	__u8			hdr[5];
	
#if defined(CONFIG_AFFIX_UART_TLP)
	// XXX: move it to ->priv
	int			flowmask;	/* tlp flow mask */
	unsigned long		sync;
	struct timeval		sync_stamp;
	int			uart_ri_latch;
#ifdef CONFIG_AFFIX_DEBUG
	int			irqcnt;
#endif
#endif
	/* protocol private */
	void			*priv;
	// driver
	void			*driver_data;

	int (*open)(struct affix_uart *uart);
	int (*close)(struct affix_uart *uart);
	int (*flush)(struct affix_uart *uart);
	int (*write)(struct affix_uart *uart, char *data, int size);
	void (*destruct)(struct affix_uart *uart);
	int (*ioctl)(struct affix_uart *uart, unsigned int cmd, void *arg);

};

/* control structure for transport driver */
struct btuart {
	struct affix_uart	uart;

	/* physical/tty stuff */
	dev_t			dev;
	struct file		*filp;		/* for tty driver */
	struct tty_struct 	*tty;
};


typedef struct {
	int		pkt_len;
} btuart_skb_cb;

#define btuart_cb(skb)	((btuart_skb_cb*)skb->cb)


/* H4 */
#if defined(CONFIG_AFFIX_UART_H4)
int  btuart_init_h4(struct affix_uart *btuart);
void  btuart_uninit_h4(struct affix_uart *btuart);
void btuart_recv_buf_h4(struct affix_uart *, const unsigned char *, int);
int btuart_enqueue_h4(struct affix_uart *btuart, struct sk_buff *skb);
struct sk_buff *btuart_dequeue_h4(struct affix_uart *btuart);
#endif

#if defined(CONFIG_AFFIX_UART_TLP)
/* support for the Nokia Connectivity Card */

int  btuart_init_tlp(struct affix_uart *btuart);
void  btuart_uninit_tlp(struct affix_uart *btuart);
void btuart_recv_buf_tlp(struct affix_uart *, const unsigned char *, int);
int btuart_enqueue_tlp(struct affix_uart *btuart, struct sk_buff *skb);
struct sk_buff *btuart_dequeue_tlp(struct affix_uart *btuart);
/* the structure of the TLP protocol header */
typedef struct {
	__u8	type;
	__u8	zero;
	__u16	length;
} __PACK__ tlp_hdr_t;

#define TLP_HDR_LEN		4

#define TLP_CONTROL		0x80
#define TLP_HCI_COMMAND		0x81
#define TLP_HCI_ACL		0x82
#define TLP_HCI_SCO		0x83
#define TLP_HCI_EVENT		0x84

#define TLP_PAD_LEN(len)	(len & 0x0001)
#define TLP_FCS_LEN		2

#define NCC_SYNC		0x80
#define NCC_SYNC_TIME		(5000)

#define	TLP_CMD_ON		0x0001
#define	TLP_ACL_ON		0x0002
#define	TLP_SCO_ON		0x0004

#endif	/* CONFIG_AFFIX_UART_TLP */

void btuart_receive_buf(void *data);
int btuartld_init(struct affix_uart *btuart);
void btuartld_uninit(struct affix_uart *btuart);


int affix_uart_register(struct affix_uart *btuart);
int affix_uart_unregister(struct affix_uart *btuart);
void affix_uart_recv_buf(struct affix_uart *btuart, const char *data, int count);
void affix_uart_write_wakeup(struct affix_uart *btuart);
void affix_uart_suspend(struct affix_uart *btuart);
void affix_uart_resume(struct affix_uart *btuart);

int affix_uart_tty_attach(affix_uart_t *serial);
int affix_uart_tty_detach(char *name);
void affix_uart_tty_suspend(char *name);
void affix_uart_tty_resume(char *name);

int kdev_ioctl(struct file *filp, int cmd, void *arg);
void btuart_xmit_wakeup(struct affix_uart *btuart);

static inline unsigned int serial_in(struct affix_uart *btuart, int offset)
{
	switch (btuart->ser.io_type) {
#ifdef CONFIG_HUB6
		case SERIAL_IO_HUB6:
			outb(ibtuart->ser.hub6 - 1 + offset, btuart->ser.port);
			return inb(btuart->ser.port + 1);
#endif
		case SERIAL_IO_MEM:
			offset <<= btuart->ser.iomem_reg_shift;
			return readb((unsigned long) btuart->ser.iomem_base + offset);
		default:
			return inb(btuart->ser.port + offset);
	}
}

static inline void serial_out(struct affix_uart *btuart, int offset, int value)
{
	switch (btuart->ser.io_type) {
#ifdef CONFIG_HUB6
		case SERIAL_IO_HUB6:
			outb(btuart->ser.hub6 - 1 + offset, btuart->ser.port);
			outb(value, btuart->ser.port + 1);
			break;
#endif
		case SERIAL_IO_MEM:
			offset <<= btuart->ser.iomem_reg_shift;
			writeb(value, (unsigned long) btuart->ser.iomem_base + offset);
			break;
		default:
			outb(value, btuart->ser.port + offset);
	}
}


#endif
