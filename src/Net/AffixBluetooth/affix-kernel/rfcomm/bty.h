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
   $Id: bty.h,v 1.35 2004/02/19 16:54:12 kassatki Exp $

   BTY - RF Virutal terminal Driver for RFCOMM

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/		


#ifndef _BTY_H
#define _BTY_H

#define	BTY_MAGIC		0x94765768
#define BTY_MAX_TTY		8

#include <linux/tty.h>
#include <linux/serial.h>

#include <affix/bluetooth.h>

#define BTY_SNDBUF_SIZE		0x7FFF
#define BTY_RCVBUF_SIZE		0x7FFF

#define BTY_MAJOR		60
#define BTY_MINOR		0

#define DO_RESTART
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

/* like async_struct */
struct bty_info {

	/* RFCOMM part */
	struct sock		*sk;
	rfcomm_con		*con;

	/* Linux tty part */
	int			flags;
	struct tty_struct	*tty;

	int			line;
	int 			blocked_open;
	wait_queue_head_t	open_wait;
	wait_queue_head_t	delta_msr_wait;

	struct tasklet_struct	rx_task;
	struct tasklet_struct	tx_wakeup_task;

	unsigned short		closing_wait;
	unsigned short		close_delay;
	struct async_icount	icount;

	int			read_status_mask;
	int			ignore_status_mask;

	/* and here status information about virtual con */
	/* msr, lsr */
	__u8			msr;
	__u8			mcr;
	
};

/*
 * persistant across opens
 */
struct bty_state {
	int 			count;                /* open count */
	struct semaphore	sem;
	struct bty_info		*info;
	struct sock		*sk;
};

extern int			bty_maxdev;
extern struct bty_state		*bty_state;
extern rwlock_t			bty_lock;


/* /dev/btyXX */
int bty_open(struct tty_struct * tty, struct file * filp);
void bty_close(struct tty_struct * tty, struct file * filp);
int bty_write(struct tty_struct * tty, int from_user, const unsigned char *buf, int count);
void bty_put_char(struct tty_struct *tty, unsigned char ch);
void bty_flush_chars(struct tty_struct *tty);
int bty_write_room(struct tty_struct *tty);
int bty_chars_in_buffer(struct tty_struct *tty);
void bty_flush_buffer(struct tty_struct *tty);
int bty_ioctl(struct tty_struct *tty, struct file * file, unsigned int cmd, unsigned long arg);
void bty_throttle(struct tty_struct *tty);
void bty_unthrottle(struct tty_struct *tty);
void bty_set_termios(struct tty_struct *tty, struct termios * old_termios);
void bty_stop(struct tty_struct *tty);
void bty_start(struct tty_struct *tty);
void bty_hangup(struct tty_struct *tty);
void bty_send_xchar(struct tty_struct *tty, char ch);
void bty_break(struct tty_struct *tty, int break_state);
void bty_wait_until_sent(struct tty_struct *tty, int timeout);
int bty_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *unused);


int __init bty_init(void);
void bty_exit(void);
void bty_destroy(struct bty_info *bty);
int bty_register_sock(struct sock *sk, int line);
struct sock *bty_unregister_sock(int line);
void bty_write_to_tty(struct bty_info *bty);
void bty_data_ind(struct sock *sk);
void bty_control_ind(struct sock *sk, int event);
void bty_rx_task(void *data);
void bty_tx_wakeup_task(void *data);


static inline __u32 __tty_to_line(struct tty_struct *tty)
{
	return tty->index;
}

static inline struct sock *__bty_get_sock(int line)
{
	if (line >= 0 && line < bty_maxdev)
		return bty_state[line].sk;
	return NULL;
}

static inline struct sock *bty_get_sock(int line)
{
	struct sock	*sk = NULL;
	write_lock_bh(&bty_lock);
	sk = __bty_get_sock(line);
	if (sk)
		sock_hold(sk);
	write_unlock_bh(&bty_lock);
	return sk;
}

static inline int bty_disabled(struct bty_info *bty)
{
	return !rfcon_get_txfc(bty->con);
	//(!con->peer_param.rtr_output || con->peer_modem.mr & RFCOMM_RTR)
}

void rpf_send_data(struct sock *sk);
struct sk_buff *rpf_wmalloc(struct sock *sk, unsigned long size, int force, int priority);
int rpf_connect_bty(struct sock *sk, wait_queue_head_t *wq);
void rpf_disconnect_bty(struct sock *sk);

#endif
