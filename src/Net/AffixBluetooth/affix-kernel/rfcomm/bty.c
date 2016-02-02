/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 - 2004 Nokia Corporation
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
   $Id: bty.c,v 1.113 2004/05/26 09:58:20 kassatki Exp $

   BTY - RFCOMM terminal driver

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
		Imre Deak <ext-imre.deak@nokia.com>
*/		

/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/smp_lock.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/tty.h>
#include <linux/serial.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <linux/skbuff.h>
#include <net/sock.h>

#define FILEBIT	DBBTY

/* Local Includes */
#include <affix/bluetooth.h>
#include <affix/btdebug.h>

#include <affix/rfcomm.h>
#include "bty.h"


/*================================================================*/
/* "Driver global" definitions */

int				sysctl_bty_wmem = BTY_SNDBUF_SIZE;
int				sysctl_bty_rmem = BTY_RCVBUF_SIZE;
int				bty_maxdev = BTY_MAX_TTY;
struct bty_state		*bty_state = NULL;
rwlock_t			bty_lock;

struct tty_driver		bty_driver;	/* our driver structure	*/
static struct tty_struct 	**tty_table;
static struct termios 		**tty_termios;
static struct termios 		**tty_termios_locked;

//static DECLARE_MUTEX(port_sem);

/*
 * BTY internal functions
 */
int bty_create(int line, struct bty_state **ret_bty)
{
	struct bty_info		*bty;
	int			err = 0;
	struct bty_state	*state;

	DBFENTER;

	*ret_bty = state = bty_state + line;
	if (down_interruptible(&state->sem)) {
		err = -ERESTARTSYS;
		goto exit;
	}

	if (!state->info) {
		state->info = bty = kmalloc(sizeof(struct bty_info), GFP_ATOMIC);
		if (!bty) {
			err = -ENOMEM;
			up(&state->sem);
			goto exit;
		}
		memset(bty, 0, sizeof(struct bty_info));

		init_waitqueue_head(&bty->open_wait);	/* should be waken on ready */
		init_waitqueue_head(&bty->delta_msr_wait);

		tasklet_init(&bty->rx_task, (void*)bty_rx_task, (unsigned long) bty);
		tasklet_init(&bty->tx_wakeup_task, (void*)bty_tx_wakeup_task, (unsigned long) bty);

		bty->closing_wait = 10 * HZ;
		bty->close_delay = 5 * HZ/10;

		bty->line = line;

	}
exit:
	DBFEXIT;
	return 0;
}

int bty_baud_rate(int baud)
{
	switch (baud) {
		case B2400:
			return RFCOMM_2400;
		case B4800:
			return RFCOMM_4800;
		case B9600:
			return RFCOMM_9600;
		case B19200:
			return RFCOMM_19200;
		case B38400:
			return RFCOMM_38400;
		case B57600:
			return RFCOMM_57600;
		case B115200:
			return RFCOMM_115200;
		case B230400:
			return RFCOMM_230400;
		default:
			return RFCOMM_9600;
	}
}

/*
   For Remote Port
 */
void bty_update_termios(struct bty_info *bty)
{
	unsigned int			cflag;
	struct rfcomm_port_param	param;

	DBFENTER;
	memset(&param, 0, sizeof(param));
	cflag = bty->tty->termios->c_cflag;
	param.bit_rate = bty_baud_rate(cflag & CBAUDEX);
	switch (cflag & CSIZE) {
		case CS5:
			param.format |= RFCOMM_CS5;
			break;
		case CS6:
			param.format |= RFCOMM_CS6;
			break;
		case CS7:
			param.format |= RFCOMM_CS7;
			break;
		case CS8:
			param.format |= RFCOMM_CS8;
			break;
		default:
			param.format |= RFCOMM_CS8;
	}
	if (cflag & CSTOPB)
		param.format |= RFCOMM_15STOP;
	if (cflag & PARENB)
		param.format |= RFCOMM_PARENB;
	if (!(cflag & PARODD))
		param.format |= RFCOMM_PAREVEN;
	if (cflag & CRTSCTS) {
		bty->flags |= ASYNC_CTS_FLOW;
		param.fc |= RFCOMM_RTR_INPUT;
		param.fc |= RFCOMM_RTR_OUTPUT;
	} else {
		bty->flags &= ~ASYNC_CTS_FLOW;
		param.fc &= ~RFCOMM_RTR_INPUT;
		param.fc &= ~RFCOMM_RTR_OUTPUT;
	}
	if (cflag & CLOCAL)
		bty->flags &= ~ASYNC_CHECK_CD;
	else
		bty->flags |= ASYNC_CHECK_CD;
	/*
	 * Set up parity check flag
	 */
	bty->read_status_mask = BTY_LSR_OE;
	if (I_INPCK(bty->tty))
		bty->read_status_mask |= BTY_LSR_FE | BTY_LSR_PE;
	if (I_BRKINT(bty->tty) || I_PARMRK(bty->tty))
		bty->read_status_mask |= BTY_LSR_BI;
	/*
	 * Characters to ignore
	 */
	bty->ignore_status_mask = 0;
	if (I_IGNPAR(bty->tty))
		bty->ignore_status_mask |= BTY_LSR_PE | BTY_LSR_FE;
	if (I_IGNBRK(bty->tty)) {
		bty->ignore_status_mask |= BTY_LSR_BI;
		/*
		 * If we're ignore parity and break indicators, ignore 
		 * overruns too.  (For real raw supcon).
		 */
		if (I_IGNPAR(bty->tty)) 
			bty->ignore_status_mask |= BTY_LSR_OE;
	}
	rfcon_set_param(bty->con, &param);
	DBFEXIT;
}

int bty_startup(struct bty_state *state)
{
	struct sock		*sk;
	struct bty_info		*bty = state->info;
	int			err;

	DBFENTER;

	if (bty->flags & ASYNC_INITIALIZED)
		return 0;

	write_lock_bh(&bty_lock);
	sk = state->sk;
	if (!sk) {
		// no connection
		write_unlock_bh(&bty_lock);
		return -ENOTCONN;
	}
	sock_hold(sk);
	write_unlock_bh(&bty_lock);

	bty->sk = sk;
	write_lock_bh(&sk->sk_callback_lock);
	rpf_get(sk)->bty = state;
	write_unlock_bh(&sk->sk_callback_lock);

	err = rpf_connect_bty(sk, &bty->open_wait);	// may block like socket connect()
	if (err) {
		write_lock_bh(&sk->sk_callback_lock);
		rpf_get(sk)->bty = NULL;
		write_unlock_bh(&sk->sk_callback_lock);
		rpf_disconnect_bty(sk);
		sock_put(xchg(&bty->sk, NULL));		// release sock
		return err;
	}
	bty->con = rpf_get(sk)->con;
	rfcon_hold(bty->con);
	sk->sk_sndbuf = sysctl_bty_wmem;
	sk->sk_rcvbuf = sysctl_bty_rmem;

	/* MCR & MSR */
	rfcon_set_rxfc(bty->con, AFFIX_FLOW_ON);
	bty->msr = rfcon_get_msr(bty->con);
	bty->mcr = rfcon_get_mcr(bty->con);

	clear_bit(TTY_IO_ERROR, &bty->tty->flags);
	bty->flags |= ASYNC_INITIALIZED;

	bty_update_termios(bty);	//XXX: needed here?

	DBFEXIT;
	return 0;
}

void bty_shutdown(struct bty_info *bty)
{
	struct sock	*sk = bty->sk;

	DBFENTER;

	if (!(bty->flags & ASYNC_INITIALIZED))
		return;

	bty->flags &= ~ASYNC_INITIALIZED;

	/* disable tasklets */
	tasklet_kill(&bty->rx_task);
	tasklet_kill(&bty->tx_wakeup_task);

	/* wake up ioctl.. */
	wake_up_interruptible(&bty->delta_msr_wait);

	/* clear DTR and RTS */
	if (!bty->tty || (bty->tty->termios->c_cflag & HUPCL))
		bty->mcr &= ~(BTY_MCR_DTR|BTY_MCR_RTS|BTY_MCR_DCD);

	if (sk) {
		write_lock_bh(&sk->sk_callback_lock);
		rpf_get(sk)->bty = NULL;
		write_unlock_bh(&sk->sk_callback_lock);
	}
	if (bty->con) {
		rfcon_set_rxfc(bty->con, AFFIX_FLOW_OFF);
		rfcon_put(xchg(&bty->con, NULL));	// release con
	}
	if (sk) {
		rpf_disconnect_bty(sk);
		sock_put(xchg(&bty->sk, NULL));		// release sock
	}

	if (bty->tty)
		set_bit(TTY_IO_ERROR, &bty->tty->flags);

	DBFEXIT;
}

void bty_write_to_tty(struct bty_info *bty)
{
	rfcomm_con		*con;
	struct tty_struct	*tty;
	struct sk_buff		*skb;
	int			room, flag;
	int			status;

	DBFENTER;

	if (!bty || !(bty->flags & ASYNC_INITIALIZED) || 
			!(con = bty->con) || !(tty = bty->tty))
		return;
#if 0	
	if ((tty->termios->c_cflag & CRTSCTS) && !(bty->mcr & BTY_MCR_RTS))
		return;
#endif

	DBPRT("Queue length: %d\n", skb_queue_len(&bty->sk->sk_receive_queue));

	if (skb_queue_len(&bty->sk->sk_receive_queue) == 0)
		goto exit;
#if 0
	if (tty->flip.count >= TTY_FLIPBUF_SIZE)
		tty->flip.tqueue.routine((void *) tty);
#endif
	for(;;) {
		if (test_bit(TTY_THROTTLED, &tty->flags))
			goto exit;
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto exit;
		skb = skb_dequeue(&bty->sk->sk_receive_queue);
		if (!skb)
			break;
		DBDUMP(skb->data, skb->len);
		DBDUMPCHAR(skb->data, skb->len);
		status = rfcon_get_line_status(con) & bty->read_status_mask;
		if (status & bty->ignore_status_mask) {
			BTERROR("line status error: ignore characters.\n");
			dev_kfree_skb(skb);
			continue;
		}
		if (con->peer_break_signal.b & RFCOMM_BREAK) {
			con->peer_break_signal.b &= ~RFCOMM_BREAK;
			DBPRT("handling break....\n");
			flag = TTY_BREAK;
			if (bty->flags & ASYNC_SAK)
				do_SAK(tty);

		} else if (status & BTY_LSR_PE) flag = TTY_PARITY;
		else if (status & BTY_LSR_FE) flag = TTY_FRAME;
		else if (status & BTY_LSR_OE) flag = TTY_OVERRUN;
		else flag = TTY_NORMAL;

		room = btmin(TTY_FLIPBUF_SIZE-tty->flip.count, skb->len);
		DBPRT("RECV, room: %d, flag: %#02x\n", room, flag);
		memset(tty->flip.flag_buf_ptr, flag, room);
		memcpy(tty->flip.char_buf_ptr, skb->data, room);
		tty->flip.char_buf_ptr += room;
		tty->flip.flag_buf_ptr += room;
		tty->flip.count += room;
		skb_pull(skb, room);

		if (skb->len)
			skb_queue_head(&bty->sk->sk_receive_queue, skb);
		else
			dev_kfree_skb(skb);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			tty_flip_buffer_push(tty);
	}
	tty_flip_buffer_push(tty);
exit:
	if (!skb_queue_empty(&bty->sk->sk_receive_queue))
		tasklet_schedule(&bty->rx_task);
	else if (rfcon_disconnect_pend(con))
		tty_hangup(tty);
	DBFEXIT;
}

void bty_rx_task(void *data)
{
	DBFENTER;
	bty_write_to_tty(data);
	DBFEXIT;
}

void bty_data_ind(struct sock *sk)
{
	struct bty_state	*state = rpf_get(sk)->bty;
	DBFENTER;
	if (!state)
		return;
	if (!(state->info->flags & ASYNC_INITIALIZED))
		goto exit;
	if (tasklet_trylock(&state->info->rx_task)) {
		bty_write_to_tty(state->info);
		tasklet_unlock(&state->info->rx_task);
	} else
		tasklet_schedule(&state->info->rx_task);
exit:
	DBFEXIT;

}

void bty_handle_msc(struct tty_struct *tty, struct bty_info *bty)
{
	__u8	delta_msr;
	
	DBFENTER;
	if (!bty->con)
		return;
	delta_msr = bty->msr;
	bty->msr = rfcon_get_msr(bty->con);
	delta_msr ^= bty->msr;

	if (delta_msr) {
		if (bty->msr & BTY_MSR_CTS) bty->icount.cts++;
		if (bty->msr & BTY_MSR_DSR) bty->icount.dsr++;
		if (bty->msr & BTY_MSR_RI)  bty->icount.rng++;
		if (bty->msr & BTY_MSR_DCD) bty->icount.dcd++;
		wake_up_interruptible(&bty->delta_msr_wait);
	}
	if ((bty->flags & ASYNC_CHECK_CD) && (delta_msr & BTY_MSR_DCD)) {
		DBPRT("CD now %s...\n", (bty->msr & BTY_MSR_DCD) ? "on" : "off");
		if (bty->msr & BTY_MSR_DCD) {
			/* DCD raised! */
			wake_up_interruptible(&bty->open_wait);
		} else {
			/* DCD falled */
			DBPRT("hangup..\n");
			tty_hangup(tty);
		}
	}
	/* like ASYNC_CTS_FLOW */
	if (bty->flags & ASYNC_CTS_FLOW) {
		if (tty->hw_stopped) {
			if (bty->msr & BTY_MSR_CTS) {
				DBPRT("CTS tx start...\n");
				tty->hw_stopped = bty_disabled(bty);
				if (tty->hw_stopped)
					goto exit;
				tasklet_schedule(&bty->tx_wakeup_task);
				wake_up_interruptible(&tty->write_wait);
			}
		} else {
			if (!(bty->msr & BTY_MSR_CTS)) {
				DBPRT("CTS tx stop...\n");
				tty->hw_stopped = 1;
			}
		}
	}
exit:
	DBFEXIT;
}

void bty_control_ind(struct sock *sk, int event)
{
	struct bty_state	*state = rpf_get(sk)->bty;
	struct bty_info		*bty;
	struct tty_struct	*tty;

	DBFENTER;
	DBPRT("event: %d\n", event);

	if (!state || !(bty = state->info) || !(tty = bty->tty))
		goto exit;

	switch (event) {
		case RFCOMM_ESTABLISHED:
			wake_up_interruptible(&bty->open_wait);
			break;
		case RFCOMM_SHUTDOWN:
			wake_up_interruptible(&bty->open_wait);
			tty_hangup(tty);
			break;
		default:
			goto next;
	}
	goto exit;
next:
	if (!(bty->flags & ASYNC_INITIALIZED))
		goto exit;

	switch (event) {
		case RFCOMM_TX_WAKEUP:
			tty->hw_stopped = bty_disabled(bty);
			if (tty->hw_stopped)
				goto exit;
			/* fall through */
		case RFCOMM_WRITE_SPACE:
			tasklet_schedule(&bty->tx_wakeup_task);
			wake_up_interruptible(&tty->write_wait);
			break;
		case RFCOMM_MODEM_STATUS:
		case RFCOMM_MODEM_STATUS_BRK:
			bty_handle_msc(tty, bty);
			break;
		case RFCOMM_PORT_PARAM:
			BTDEBUG("Port Parameters (RPN) is not handled!!!\n");
			break;
	}
exit:
	DBFEXIT;
}

void bty_tx_wakeup_task(void *data)
{
	struct bty_info		*bty = data;
	struct tty_struct	*tty = bty->tty;

	DBFENTER;
	if (!tty)
		return;
	if (test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags) && tty->ldisc.write_wakeup)
		tty->ldisc.write_wakeup(tty);
	DBFEXIT;
}

/*
   it seems it is called from BH
   */
int bty_register_sock(struct sock *sk, int line)
{
	DBFENTER;
	DBPRT("Register line: %d\n", line);
	write_lock_bh(&bty_lock);
	if (line == RFCOMM_BTY_ANY) {
		for (line = 0; line < bty_maxdev; line++)
			if (!bty_state[line].sk)
				break;
		if (line >= bty_maxdev) {
			line = -EBUSY;
			goto exit;
		}
	} else {
		if (line < 0 || line >= bty_maxdev) {
			line = -EINVAL;
			goto exit;
		} else if (bty_state[line].sk) {
			line = -EBUSY;
			goto exit;
		}
	}
	sock_hold(sk);	/* hold it */
	bty_state[line].sk = sk;
	rpf_get(sk)->port.line = line;
	DBPRT("Line registered: %d\n", line);
exit:
	write_unlock_bh(&bty_lock);
	DBFEXIT;
	return line;
}

/*
 * keeps sk hold -> pass ownership to af_rfcomm.c
 */
struct sock *bty_unregister_sock(int line)
{
	struct sock		*sk;
	struct bty_state	*state;

	DBFENTER;
	if (line < 0 || line >= bty_maxdev)
		return NULL;
	state = bty_state + line;
	write_lock_bh(&bty_lock);
	sk = state->sk;
	if (!sk) {
		write_unlock_bh(&bty_lock);
		return NULL;
	}
	state->sk = NULL;
	if (state->info && state->info->tty)
		tty_hangup(state->info->tty);
	write_unlock_bh(&bty_lock);
	DBFEXIT;
	return sk;
}


/*********************   CALLBACKS   *******************/

/*
 * Block the open until the port is ready.  We must be called with
 * the per-port semaphore held.
 */
static int bty_block_til_ready(struct bty_state *state, struct file *filp)
{
	struct bty_info		*bty = state->info;
	DECLARE_WAITQUEUE(wait, current);

	DBFENTER;
	
	bty->blocked_open++;
	state->count--;

	add_wait_queue(&bty->open_wait, &wait);
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		/*
		 * If we have been hung up, tell userspace/restart open.
		 */
		if (tty_hung_up_p(filp) || bty->tty == NULL)
			break;

		/*
		 * If the port has been closed, tell userspace/restart open.
		 */
		if (!(bty->flags & ASYNC_INITIALIZED))
			break;

		/*
		 * If non-blocking mode is set, or CLOCAL mode is set,
		 * we don't want to wait for the modem status lines to
		 * indicate that the port is ready.
		 *
		 * Also, if the port is not enabled/configured, we want
		 * to allow the open to succeed here.  Note that we will
		 * have set TTY_IO_ERROR for a non-existant port.
		 */
		DBPRT("CLOCAL: %d\n", bty->tty->termios->c_cflag & CLOCAL);
		if ((filp->f_flags & O_NONBLOCK) ||
	            (bty->tty->termios->c_cflag & CLOCAL) || 
		    test_bit(TTY_IO_ERROR, &bty->tty->flags)) {
			break;
		}

		/*
		 * and wait for the carrier to indicate that the
		 * modem is ready for us.
		 */
		if (bty->msr & BTY_MSR_DCD)
			break;

		up(&state->sem);
		schedule();
		down(&state->sem);

		if (signal_pending(current))
			break;
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&bty->open_wait, &wait);

	state->count++;
	bty->blocked_open--;

	DBFEXIT;

	if (signal_pending(current))
		return -ERESTARTSYS;

	if (!bty->tty || tty_hung_up_p(filp))
		return -EAGAIN;

	return 0;
}

/*
   open and friends
   */
int bty_open(struct tty_struct * tty, struct file * filp)
{
	struct bty_state	*state;
	struct bty_info		*bty;
	__u32 			line;
	int			err;

	DBFENTER;
	DBPRT("CLOCAL: %d\n", tty->termios->c_cflag & CLOCAL);
	line = __tty_to_line(tty);
	if (line >= bty_maxdev)
		return -ENODEV;

	err = bty_create(line, &state);
	if (err) {
		BTERROR("bty structure create err: %d\n", err);
		return err;
	}

	bty = state->info;
	
	/* set pointer to driver structures */
	DBPRT("tty: %p, tty->driver_data: %p, bty: %p, bty->tty: %p\n", 
			tty, tty->driver_data, bty, bty->tty);
	tty->driver_data = state;
	bty->tty = tty;
	tty->low_latency = 1;//(bty->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	state->count++;	// get it
	
	if (tty_hung_up_p(filp)) {
		// hangup in open -> it's not first opening...
		// first open never has hangup
		DBPRT("Hang up...\n");
		err = -EAGAIN;
		goto exit;
	}
	
	err = bty_startup(state);
	if (!err)
		err = bty_block_til_ready(state, filp);

	if (!err && !(bty->flags & ASYNC_NORMAL_ACTIVE)) {
		bty->flags |= ASYNC_NORMAL_ACTIVE;
	}
	
	if (!err)
		tty->hw_stopped = bty_disabled(bty);
exit:
	up(&state->sem);
	DBFEXIT;
	return err;
}

/*
 * ----------------------------------------------------------------------
 * bty_close() and friends
 *
 * most of this function is stolen from serial.c
 * ----------------------------------------------------------------------
 */

//#define ___BUG_ON(condition) do { if (unlikely((condition)!=0)) printk("BUG()\n"); else printk("no BUG()\n"); } while(0)

void bty_close(struct tty_struct * tty, struct file * filp)
{
	struct bty_state 	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;

	DBFENTER;

	BUG_ON(!kernel_locked());	// should be locked, otherwise tty_hung_up_p(filp) not valid

	DBPRT("tty: %p, data: %p\n", tty, tty->driver_data);

	if (!state) {
		BTERROR("state == NULL\n");
		return;
	}
	
	down(&state->sem);

	if (!(bty = state->info)) {
		DBPRT("bty == NULL\n");
		goto exit;
	}
	
	if (tty_hung_up_p(filp) && state->count == 0) {
		/*
		 * state->count == 0 ????? shoud be because kernel_locked()
		 * is it possible to come here but state->count != 0 ??
		 */
		/*
		 * upper tty layer caught a HUP signal and called bty_hangup()
		 * before. so we do nothing here.
		 */
		DBPRT("Hang up..., count: %d\n", state->count);
		goto exit;
	}
	if ((tty->count == 1) && (state->count != 1)) {
		BTERROR("uart_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		BTERROR("driver count is negative: %d\n", state->count);
		state->count = 0;
	}		
	if (state->count) { 	/* do nothing */
		DBPRT("driver still in use: %d\n", state->count);
		goto exit;
	}

	bty->flags |= ASYNC_CLOSING;

	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (bty->closing_wait != ASYNC_CLOSING_WAIT_NONE){
		DBPRT("calling tty_wait_until_sent()\n");
		tty_wait_until_sent(tty, bty->closing_wait);
	}
	
	bty_shutdown(bty);

	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	if (tty->driver->flush_buffer) 
		tty->driver->flush_buffer(tty);  
	// or bty_flush_buffer(tty) instead??
	tty->closing = 0;
	bty->tty = NULL;
	if (bty->blocked_open) {/* used to block till ready */
		if (bty->close_delay) {
			/* kill time */
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(bty->close_delay);
		}
	}
	bty->flags &= ~(ASYNC_CLOSING | ASYNC_NORMAL_ACTIVE);
	wake_up_interruptible(&bty->open_wait);
exit:
	if (tty->count <= 1 && state->info) {
		// TTY will dissapiar
		write_lock_bh(&bty_lock);	// lock it
		kfree(state->info);
		state->info = NULL;
		write_unlock_bh(&bty_lock);	// unlock it
	}
	up(&state->sem);
	DBPRT("%s[%d] opened, state->count: %d, tty->count: %d\n", 
			tty->driver->name, __tty_to_line(tty), state->count, tty->count);
	DBFEXIT;
}

/*
 * ------------------------------------------------------------
 * bty_hangup()
 * This routine notifies that tty layer have got HUP signal
 * ------------------------------------------------------------
 */

void bty_hangup(struct tty_struct *tty)
{
	struct bty_state	*state = tty->driver_data;
	struct bty_info		*bty;

	DBFENTER;
	down(&state->sem);
	bty = state->info;
	if (bty && bty->flags & ASYNC_NORMAL_ACTIVE) {
		bty_flush_buffer(tty);
		bty_shutdown(bty);
		bty->tty = NULL;
		state->count = 0;
		wake_up_interruptible(&bty->delta_msr_wait);
		wake_up_interruptible(&bty->open_wait);
		bty->flags &= ~ASYNC_NORMAL_ACTIVE;
	}
	up(&state->sem);
	DBPRT("tty->count: %d, state->count: %d\n", tty->count, state->count);
	DBFEXIT;
}



/*
 * ----------------------------------------------------------------------
 * bty_write() and friends
 * This routine will be called when something data are passed from
 * kernel or user.
 * ----------------------------------------------------------------------
 */

int bty_write(struct tty_struct * tty, int from_user, const unsigned char *buf, int count)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;
	int			room, wrote = 0;
	__u8			*data;
	struct sk_buff		*txbuff;

	DBFENTER;
	DBPRT("Transmit: %d bytes\n", count);
	DBDUMP((void*)buf, count);
	DBDUMPCHAR(buf, count);

	if (!state || !(bty = state->info) || !(bty->flags & ASYNC_INITIALIZED))
		return -ENODEV;
	if (!count)
		return 0;
	txbuff = skb_dequeue_tail(&bty->sk->sk_write_queue);
	if (txbuff && rfcomm_skb_tailroom(txbuff) == 0) {
		/* no space */
		skb_queue_tail(&bty->sk->sk_write_queue, txbuff);
		txbuff = NULL;
	}
	while (count) {
		if (!txbuff) {
			/* allocate new one */
			txbuff = rpf_wmalloc(bty->sk, rfcon_getmtu(bty->con), 0, GFP_ATOMIC);
			if (!txbuff)
				goto exit;
		}
		room = btmin(count, rfcomm_skb_tailroom(txbuff));
		DBPRT("room: %d\n", room);
		data = skb_put(txbuff, room);
		if (from_user)
			copy_from_user(data, buf, room);
		else
			memcpy(data, buf, room);
		buf += room;
		wrote += room;
		count -= room;
		skb_queue_tail(&bty->sk->sk_write_queue, txbuff);
		txbuff = NULL;
		rpf_send_data(bty->sk);
	}
exit:
	DBPRT("wrote: %d\n", wrote);
	DBFEXIT;
	return wrote;
}

/*
 * Function bty_put_char (tty, ch)
 *
 *    This routine is called by the kernel to pass a single character.
 *    If we exausted our buffer,we can ignore the character!
 *
 */
void bty_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;
	struct sk_buff		*txbuff;
	__u8			*ptr;

	DBFENTER;
	if (!state || !(bty = state->info) || !(bty->flags & ASYNC_INITIALIZED))
		return;
	txbuff = skb_dequeue_tail(&bty->sk->sk_write_queue);
	if (txbuff && rfcomm_skb_tailroom(txbuff) == 0) {
		/* no space */
		skb_queue_tail(&bty->sk->sk_write_queue, txbuff);
		txbuff = NULL;
	}
	if (!txbuff) {
		/* rs_put_char does not do it but we do */
		/* allocate new one */
		txbuff = rpf_wmalloc(bty->sk, rfcon_getmtu(bty->con), 1, GFP_ATOMIC);
		if (!txbuff)
			return;
	}
	ptr = skb_put(txbuff, 1);
	*ptr = ch;
	skb_queue_tail(&bty->sk->sk_write_queue, txbuff);
	DBFEXIT;
}


void bty_flush_chars(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;
	DBFENTER;
	if (!state || !(bty = state->info) || !(bty->flags & ASYNC_INITIALIZED))
		return;
	rpf_send_data(bty->sk);
	DBFEXIT;
}


/*
 * Function bty_write_room (tty)
 *
 *    This routine returns the room that our buffer has now.
 *
 */
int bty_write_room(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;
	DBFENTER;
	if (!state || !(bty = state->info) || !(bty->flags & ASYNC_INITIALIZED))
		return 0;
	return sock_wspace(bty->sk);
}


/*
 * Function bty_chars_in_buffer (tty)
 *
 *    This function returns how many characters which have not been sent yet 
 *    are still in buffer.
 *
 */
int bty_chars_in_buffer(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty;
	if (!state || !(bty = state->info) || !(bty->flags & ASYNC_INITIALIZED))
		return 0;
	return atomic_read(&bty->sk->sk_wmem_alloc);
}

void bty_flush_buffer(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;

	DBFENTER;
	if (bty->sk)
		skb_queue_purge(&bty->sk->sk_write_queue);
	wake_up_interruptible(&tty->write_wait);
	if (test_bit(TTY_DO_WRITE_WAKEUP, &tty->flags) && tty->ldisc.write_wakeup)
		tty->ldisc.write_wakeup(tty);
	DBFEXIT;
}

int bty_tiocmset(struct tty_struct *tty, struct file *file, 
		unsigned int set, unsigned int clear)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;

	DBFENTER;
	if (set && clear) {
		bty->mcr = bty->mcr & ~(BTY_MCR_RTS | BTY_MCR_DTR | BTY_MCR_DCD | BTY_MCR_RI);
		goto doset;
	} else if (set && !clear) {
doset:
		if (set & TIOCM_RTS) bty->mcr |= BTY_MCR_RTS;
		if (set & TIOCM_DTR) bty->mcr |= BTY_MCR_DTR;
		if (set & TIOCM_CAR) bty->mcr |= BTY_MCR_DCD;
		if (set & TIOCM_RNG) bty->mcr |= BTY_MCR_RI;
	} else if (!set && clear) {
		if (clear & TIOCM_RTS) bty->mcr &= ~BTY_MCR_RTS;
		if (clear & TIOCM_DTR) bty->mcr &= ~BTY_MCR_DTR;
		if (clear & TIOCM_CAR) bty->mcr &= ~BTY_MCR_DCD;
		if (clear & TIOCM_RNG) bty->mcr &= ~BTY_MCR_RI;
	}
	rfcon_set_mcr(bty->con, bty->mcr);
	DBFEXIT;
	return 0;
}

int bty_set_modem_info(struct tty_struct *tty, struct file *file, 
		unsigned int cmd, unsigned int *arg)
{ 
	int		err;
	unsigned int	set, clear, val;

	DBFENTER;
	err = get_user(val, arg);
	if(err)
		return err;

	set = clear = 0;
	switch (cmd) {
		case TIOCMBIS:
			set = val;
			break;
		case TIOCMBIC:
			clear = val;
			break;
		case TIOCMSET:
			set = val;
			clear = ~val;
			break;
		default:
			return -EINVAL;
	}
	DBFEXIT;
	return bty_tiocmset(tty, file, set, clear);
}

int bty_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	int 			result = 0;

	DBFENTER;
	if (bty->mcr & BTY_MCR_RTS) result |= TIOCM_RTS;
	if (bty->mcr & BTY_MCR_DTR) result |= TIOCM_DTR;

	if (bty->msr & BTY_MSR_DCD) result |= TIOCM_CAR;
	if (bty->msr & BTY_MSR_RI) result |= TIOCM_RNG;
	if (bty->msr & BTY_MSR_DSR) result |= TIOCM_DSR;
	if (bty->msr & BTY_MSR_CTS) result |= TIOCM_CTS;
	DBFEXIT;
	return result;
}

int bty_get_modem_info(struct tty_struct *tty, struct file *file, unsigned int *value)
{
	int 	result = bty_tiocmget(tty, file);

	return put_user(result, value);
}

/*
 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
 * - mask passed in arg for lines of interest
 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
 * Caller should use TIOCGICOUNT to see which one it was
 */
static int bty_wait_modem_status(struct bty_info *bty, unsigned long arg)
{
	DECLARE_WAITQUEUE(wait, current);
	struct async_icount	cnow, cprev;
	int ret;

	/*
	 * note the counters on entry
	 */
	write_lock_bh(&bty_lock);
	memcpy(&cprev, &bty->icount, sizeof(struct async_icount));
	/*
	 * Force modem status interrupts on
	 */
	//port->ops->enable_ms(port);
	write_unlock_bh(&bty_lock);

	add_wait_queue(&bty->delta_msr_wait, &wait);
	for (;;) {
		write_lock_bh(&bty_lock);
		memcpy(&cnow, &bty->icount, sizeof(struct async_icount));
		write_unlock_bh(&bty_lock);

		set_current_state(TASK_INTERRUPTIBLE);

		if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
		    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
		    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
		    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
		    	ret = 0;
		    	break;
		}

		schedule();

		/* see if a signal did it */
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		cprev = cnow;
	}

	current->state = TASK_RUNNING;
	remove_wait_queue(&bty->delta_msr_wait, &wait);

	return ret;
}

/*
 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
 * Return: write counters to the user passed counter struct
 * NB: both 1->0 and 0->1 transitions are counted except for
 *     RI where only 0->1 is counted.
 */
static int bty_get_icount(struct bty_info *bty, struct serial_icounter_struct *icnt)
{
	struct serial_icounter_struct icount;
	struct async_icount cnow;

	write_lock_bh(&bty_lock);
	memcpy(&cnow, &bty->icount, sizeof(struct async_icount));
	write_unlock_bh(&bty_lock);

	icount.cts         = cnow.cts;
	icount.dsr         = cnow.dsr;
	icount.rng         = cnow.rng;
	icount.dcd         = cnow.dcd;
	icount.rx          = cnow.rx;
	icount.tx          = cnow.tx;
	icount.frame       = cnow.frame;
	icount.overrun     = cnow.overrun;
	icount.parity      = cnow.parity;
	icount.brk         = cnow.brk;
	icount.buf_overrun = cnow.buf_overrun;

	return copy_to_user(icnt, &icount, sizeof(icount)) ? -EFAULT : 0;
}

/*
 * ----------------------------------------------------------------------
 * bty_ioctl() and friends
 * This routine allows us to implement device-specific ioctl's.
 * If passed ioctl number (i.e.cmd) is unknown one, we should return 
 * ENOIOCTLCMD.
 *
 * TODO: we can't use setserial on tty because some ioctls are not implemented.
 * we should add some ioctls and make some tool which is resemble to setserial.
 * ----------------------------------------------------------------------
 */

int bty_ioctl(struct tty_struct *tty, struct file * file, unsigned int cmd, unsigned long arg)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	int 			err = 0;

	//DBFENTER;
	switch (cmd) {
		case TIOCMGET:
			return bty_get_modem_info(tty, file, (unsigned int *) arg);
		case TIOCMBIS:
		case TIOCMBIC:
		case TIOCMSET:
			return bty_set_modem_info(tty, file, cmd, (unsigned int *) arg);
		case TIOCMIWAIT:
			return bty_wait_modem_status(bty, arg);
		case TIOCGICOUNT:
			return bty_get_icount(bty, (struct serial_icounter_struct *)arg);

			/* ioctls which are imcompatible with serial.c */
		case TIOCGSERIAL:
			DBPRT("TIOCGSERIAL is not supported\n");
			return -ENOIOCTLCMD;  
			//return get_serial_info(driver, (struct serial_struct *) arg);
		case TIOCSSERIAL:
			DBPRT("TIOCSSERIAL is not supported\n");
			return -ENOIOCTLCMD;  
			//return set_serial_info(driver, (struct serial_struct *) arg);
		case TIOCSERGSTRUCT:
			DBPRT("TIOCSERGSTRUCT is not supported\n");
			return -ENOIOCTLCMD;  
		case TIOCSERGETLSR:
			DBPRT("TIOCSERGETLSR is not supported\n");
			return -ENOIOCTLCMD;  
		case TIOCSERCONFIG:
			DBPRT("TIOCSERCONFIG is not supported\n");
			return -ENOIOCTLCMD;  
		default:
			//DBPRT("Command ignored: %#04x\n", cmd);
			return -ENOIOCTLCMD;  /* ioctls which we must ignore */
	}
	//DBFEXIT;
	return err;
}

/*
 * ----------------------------------------------------------------------
 * bty_throttle,bty_unthrottle
 *   These routines will be called when we have to pause sending up data to tty.
 *   We use RTS virtual signal when servicetype is NINE_WIRE
 * ----------------------------------------------------------------------
 */

void bty_throttle(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	rfcomm_con		*con = bty->con;

	DBFENTER;
	if (I_IXOFF(tty))
		bty_send_xchar(tty, STOP_CHAR(tty));
	if (tty->termios->c_cflag & CRTSCTS) {
		bty->mcr &= ~BTY_MCR_RTS; 
		rfcon_set_mcr(con, bty->mcr);
	}
	DBFEXIT;
}


void bty_unthrottle(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	rfcomm_con		*con = bty->con;

	DBFENTER;
	if (I_IXOFF(tty))
		bty_send_xchar(tty, START_CHAR(tty));
	if (tty->termios->c_cflag & CRTSCTS) {
		bty->mcr |= BTY_MCR_RTS;
		rfcon_set_mcr(con, bty->mcr);
	}
	DBFEXIT;
}


/*
 * ----------------------------------------------------------------------
 * bty_set_termios()
 * This is called when termios is changed.
 * If things that changed is significant for us,(i.e. changing baud rate etc.)
 * send something to peer device.
 * ----------------------------------------------------------------------
 */

void bty_set_termios(struct tty_struct *tty, struct termios * old_termios)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;

	DBFENTER;
	if( (tty->termios->c_cflag == old_termios->c_cflag) &&
			(RELEVANT_IFLAG(tty->termios->c_iflag) == RELEVANT_IFLAG(old_termios->c_iflag)) )
		return;
	bty_update_termios(bty);
	/* handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) && !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = bty_disabled(bty);
		/* bty_start(tty); */       /* we don't need this */
	}
	DBFEXIT;
}

/*
 * ------------------------------------------------------------
 * bty_stop() and bty_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable an interrupt which means "transmitter-is-ready"
 * in serial.c, but  I think these routine are not necessary for us. 
 * ------------------------------------------------------------
 */

#if 0
void bty_stop(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info; 

	DBFENTER;
	DBFEXIT;
}

void bty_start(struct tty_struct *tty)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;

	DBFENTER;
	DBFEXIT;
}
#endif


void bty_send_xchar(struct tty_struct *tty, char ch)
{
	DBFENTER;
	bty_put_char(tty, ch);
	DBFEXIT;
}

/*
 * Function bty_break (tty, break_state)
 *
 *    Routine which turns the break handling on or off
 *
 */
void bty_break_ctl(struct tty_struct *tty, int break_state)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	rfcomm_con		*con = bty->con;

	DBFENTER;
	//write_lock_bh(&bty_lock);
	if (break_state == -1)
		rfcon_set_break(con, 1);
	else
		rfcon_set_break(con, 0);
	//write_unlock_bh(&bty_lock);
	DBFEXIT;
}


int bty_proc_read(char *buf, char **start, off_t offset, int len);

int bty_read_proc(char *buf, char **start, off_t offset, int len, int *eof, void *unused)
{
	return bty_proc_read(buf, start, offset, len);
}


/*
 * Function bty_wait_until_sent (tty, timeout)
 *
 *    wait until Tx queue of BTY is empty 
 *
 */
void bty_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct bty_state	*state = (struct bty_state*)tty->driver_data;
	struct bty_info		*bty = state->info;
	unsigned long		orig_jiffies;

	DBFENTER;
	DBPRT("chars in buffer: %d\n", bty_chars_in_buffer(tty));
	if (!tty->closing || !bty->con)
		return;   /* nothing to do */
	orig_jiffies = jiffies;
	while (rfcon_disconnect_pend(bty->con)) {
		DBPRT("wait..\n");
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);
		if (signal_pending(current))
			break;
		if (timeout && time_after(jiffies, orig_jiffies + timeout))
			break;
	}
	set_current_state(TASK_RUNNING);
	DBFEXIT;
}

/*
 * Function bty_register_ttydriver(void)
 *
 *   we register "port emulation entity"(see Bty specification) here
 *   as a tty device.
 */

int bty_register_ttydriver(void)
{
	int	err;
	
	DBFENTER;
	/* allocate memory for data structures */
	err = -ENOMEM;
	tty_table = (struct tty_struct**)kmalloc(sizeof(void*)*bty_maxdev, GFP_KERNEL);
	if (!tty_table) {
		BTERROR("Unable to allocate memory for data structures\n");
		goto err1;
	}
	memset(tty_table, 0, sizeof(void*)*bty_maxdev);

	tty_termios = (struct termios**)kmalloc(sizeof(void*)*bty_maxdev, GFP_KERNEL);
	if (!tty_termios) {
		BTERROR("Unable to allocate memory for data structures\n");
		goto err2;
	}
	memset(tty_termios, 0, sizeof(void*)*bty_maxdev);

	tty_termios_locked = (struct termios**)kmalloc(sizeof(void*)*bty_maxdev, GFP_KERNEL);
	if (!tty_termios_locked) {
		BTERROR("Unable to allocate memory for data structures\n");
		goto err3;
	}
	memset(tty_termios_locked, 0, sizeof(void*)*bty_maxdev);

	/* setup virtual serial port device */

	/* Initialize the tty_driver structure ,which is defined in 
	   tty_driver.h */

	memset(&bty_driver, 0, sizeof(struct tty_driver));
	bty_driver.magic = TTY_DRIVER_MAGIC;
	bty_driver.driver_name = "affix_rfcomm";
	bty_driver.devfs_name = "bty/";
	bty_driver.name = "bty";
	bty_driver.major = BTY_MAJOR;
	bty_driver.minor_start = BTY_MINOR;
	bty_driver.num = bty_maxdev;
	bty_driver.type = TTY_DRIVER_TYPE_SERIAL;  /* see tty_driver.h */
	bty_driver.subtype = SERIAL_TYPE_NORMAL;  /* see tty_driver.h */


	/*
	 * see drivers/char/tty_io.c and termios(3)
	 */

	bty_driver.init_termios = tty_std_termios;
	bty_driver.init_termios.c_cflag = B115200 | CS8 | CREAD | HUPCL | CLOCAL;
	bty_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_RESET_TERMIOS;   /* see tty_driver.h */
	/* pointer to the tty data structures */
	bty_driver.ttys = tty_table;  
	bty_driver.owner = THIS_MODULE;
	bty_driver.termios = tty_termios;
	bty_driver.termios_locked = tty_termios_locked;

	/*
	 * Interface table from the kernel(tty driver) to the bty
	 * layer
	 */

	bty_driver.open = bty_open;
	bty_driver.close = bty_close;
	bty_driver.write = bty_write;
	bty_driver.put_char = bty_put_char;
	bty_driver.flush_chars = bty_flush_chars;
	bty_driver.write_room = bty_write_room;
	bty_driver.chars_in_buffer = bty_chars_in_buffer; 
	bty_driver.flush_buffer = bty_flush_buffer;
	bty_driver.ioctl = bty_ioctl; 
	bty_driver.throttle = bty_throttle;
	bty_driver.unthrottle = bty_unthrottle;
	bty_driver.set_termios = bty_set_termios;
	bty_driver.stop = NULL;				/* bty_stop */
	bty_driver.start = NULL;				/* bty_start */
	bty_driver.hangup = bty_hangup;
	bty_driver.send_xchar = bty_send_xchar;
	bty_driver.break_ctl = bty_break_ctl;
	bty_driver.read_proc = bty_read_proc;
	bty_driver.wait_until_sent = bty_wait_until_sent;
	bty_driver.tiocmget = bty_tiocmget;
	bty_driver.tiocmset = bty_tiocmset;

	if ((err = tty_register_driver(&bty_driver))) {
		goto err4;
	}
	DBPRT("done.\n");

	DBFEXIT;
	return 0;
err4:
	kfree(tty_termios_locked);
err3:
	kfree(tty_termios);
err2:
	kfree(tty_table);
err1:
	return err;
}


/*
 * Function bty_unregister_ttydriver(void) 
 *   it will be called when you rmmod
 */

int bty_unregister_ttydriver(void)
{
	int err;	
	DBFENTER;

	/* unregister tty device   */
	err = tty_unregister_driver(&bty_driver);
	if (err)
		BTERROR("BTY: failed to unregister vtd driver(%d)\n", err);
	kfree(tty_termios_locked);
	kfree(tty_termios);
	kfree(tty_table);
	return err;
}

/* Proc file system services */
#ifdef CONFIG_PROC_FS

#define LV(v)	((v) != 0)
int bty_proc_read(char *buf, char **start, off_t offset, int len)
{
	struct sock		*sk;
	rfcomm_con		*con;
	struct bty_state	*state;
	struct bty_info		*bty;
	int 			count = 0, i;

	DBFENTER;
	read_lock_bh(&bty_lock);
	for (i = 0; i < bty_maxdev; i++) {
		state = bty_state + i;
		count += sprintf(buf+count, "line: %d, count: %d, info: %p\n", i, state->count, state->info);
		// sk
		if ((sk = state->sk) && (con = rpf_get(sk)->con)) {
			count += sprintf(buf+count, "\tMSR: credit: %d, fc: %d, use rtr: %d, "
					"rtr: %d, rtc: %d, dv: %d, ic: %d\n", 
					atomic_read(&con->tx_credit), con->peer_modem.fc, LV(con->peer_param.fc & RFCOMM_RTR_OUTPUT),
					LV(con->peer_modem.mr&RFCOMM_RTR), LV(con->peer_modem.mr&RFCOMM_RTC),
					LV(con->peer_modem.mr&RFCOMM_DV), LV(con->peer_modem.mr&RFCOMM_IC));
			count += sprintf(buf+count, "\tMCR: credit: %d, fc: %d, use rtr: %d, "
					"rtr: %d, rtc: %d, dv: %d, ic: %d\n", 
					atomic_read(&con->rx_credit), con->modem.fc, LV(con->param.fc & RFCOMM_RTR_OUTPUT),
					LV(con->modem.mr&RFCOMM_RTR), LV(con->modem.mr&RFCOMM_RTC),
					LV(con->modem.mr&RFCOMM_DV), LV(con->modem.mr&RFCOMM_IC));
		}
		// bty
		if ((bty = state->info)) {
			//count += sprintf(buf+count, "\n\tbty count: %d\n\n", state->count);
		}
	}
	read_unlock_bh(&bty_lock);
	DBFEXIT;
	return count;
}

struct proc_dir_entry	*bty_proc;

#endif

int __init bty_init(void)
{
	int	err = -ENOMEM, i;

	DBFENTER;

	rwlock_init(&bty_lock);

	bty_state = (struct bty_state*)kmalloc(sizeof(struct bty_state) * bty_maxdev, GFP_KERNEL);
	if (!bty_state) {
		BTERROR("kmalloc failed!\n");
		goto err1;
	}
	memset(bty_state, 0, sizeof(struct bty_state) * bty_maxdev);
	for (i = 0 ; i < bty_maxdev; i++) {
		struct bty_state	*state = &bty_state[i];

		init_MUTEX(&state->sem);
	}
#ifdef CONFIG_PROC_FS
	bty_proc = create_proc_info_entry("bty", 0, proc_affix, bty_proc_read);
	if (!bty_proc) {
		BTERROR("Unable to register proc fs entry\n");
		goto err2;
	}
#endif
	err = bty_register_ttydriver();
	if (err){
		BTERROR("Error in rdvtd_register_device\n");
		goto err3;
	}
	DBFEXIT;
	return 0;
err3:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("bty", proc_affix);
err2:
#endif
	kfree(bty_state);
err1:
	return err;
}

void __exit bty_exit(void)
{
	DBFENTER;
	bty_unregister_ttydriver();
#ifdef CONFIG_PROC_FS
	remove_proc_entry("bty", proc_affix);
#endif
	kfree(bty_state);	// must be not NULL
	DBFEXIT;
}

/* 
 * - no reason to lock *con* if sock is locked
 *
 */
