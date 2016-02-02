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
   $Id: btuart_tty.c,v 1.4 2004/05/04 09:59:56 kassatki Exp $

   BTUART - physical protocol layer for UART based cards

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
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
#include <linux/tty.h>
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
#include <affix/uart.h>

#include <asm/io.h>
#include <linux/serial_reg.h>
#define AFFIX_16C950_BUG


int  btuartld_open(struct tty_struct *tty);
void btuartld_close(struct tty_struct *tty);
int  btuartld_ioctl(struct tty_struct *, void *, int, void *);
int  btuartld_receive_room(struct tty_struct *tty);
void btuartld_write_wakeup(struct tty_struct *tty);
void btuartld_receive_buf(struct tty_struct *, const unsigned char *, char *, int);


struct tty_ldisc	btuart_ldisc;
btlist_head_t		btuarts;

/*************************** FUNCTION DEFINITION SECTION **********************/

struct btuart *__btuart_lookup_device(char *name)
{
	struct btuart	*btuart;

	btl_for_each (btuart, btuarts) {
		if (strcmp(btuart->uart.uart.name, name) == 0)
			break;
	}
	return btuart;
}

struct btuart *btuart_lookup_device(char *name)
{
	struct btuart	*btuart;
	unsigned long	flags;

	read_lock_irqsave(&btuarts.lock, flags);
	btuart = __btuart_lookup_device(name);
	read_unlock_irqrestore(&btuarts.lock, flags);

	return btuart;
}

struct btuart *__btuart_lookup_dev(dev_t dev)
{
	struct btuart	*btuart;

	btl_for_each (btuart, btuarts)
		if (btuart->dev == dev)
			break;
	return btuart;
}

struct btuart *btuart_lookup_dev(struct tty_struct *tty)
{
	struct btuart	*btuart;
	unsigned long	flags;

	read_lock_irqsave(&btuarts.lock, flags);
	btuart = __btuart_lookup_dev(tty_devnum(tty));
	read_unlock_irqrestore(&btuarts.lock, flags);

	return btuart;
}


/* ***************** UART common ****************************** */

struct file *kdev_open(char *name)
{
	struct file	*file;

	DBFENTER;
	DBPRT("Opening device, path: %s\n", name);
	file = filp_open(name, 0, 0);
	if (IS_ERR(file)) {
		BTERROR("Unable to open divice: %s\n", name);
		return NULL;
	}
	DBFEXIT;
	return file;
}

int kdev_close(struct file *file)
{
	int	err;
	DBFENTER;
	err = filp_close(file, NULL);
	DBFEXIT;
	return err;
}

int kdev_ioctl(struct file *filp, int cmd, void *arg)
{
	int	err = -1;
	
	if (filp->f_op && filp->f_op->ioctl) {
		mm_segment_t	old_fs;
		lock_kernel();
		old_fs = get_fs(); set_fs(KERNEL_DS);
		err = filp->f_op->ioctl(filp->f_dentry->d_inode, filp, cmd, (unsigned long)arg);
		set_fs(old_fs);
		unlock_kernel();
	}
	return err;
}

/*
#define    B57600 0010001
#define   B115200 0010002
#define   B230400 0010003
#define   B460800 0010004
#define   B500000 0010005
#define   B576000 0010006
#define   B921600 0010007
*/

#ifdef AFFIX_16C950_BUG
/*
 * For the 16C950
 */
void serial_icr_write(struct btuart *btuart, int offset, int  value)
{
	serial_out(&btuart->uart, UART_SCR, offset);
	serial_out(&btuart->uart, UART_ICR, value);
}

#endif

int init_uart(struct btuart *btuart)
{
	int			err;
	struct termios		term;

	DBFENTER;

	/* get current settings */
	err = kdev_ioctl(btuart->filp, TCGETS, &term);
	if (err)
		goto ioerr;
	//term.c_iflag |= IGNPAR;
	if (btuart->uart.uart.flags & CRTSCTS)
		term.c_cflag |= CRTSCTS;
	else
		term.c_cflag &= ~CRTSCTS;
	err = kdev_ioctl(btuart->filp, TCSETS, &term);
	if (err) {
		BTERROR("Unable to set CRTSCTS: %d\n", err);
		goto ioerr;
	}

	if (btuart->uart.uart.flags & CSTOPB)
		term.c_cflag |= CSTOPB;	/* 1 stop bit */
	else
		term.c_cflag &= ~CSTOPB;/* 1.5 stop bits */
	err = kdev_ioctl(btuart->filp, TCSETS, &term);
	if (err) {
		BTERROR("Unable to set CSTOPB: %d\n", err);
		goto ioerr;
	}

	if (btuart->uart.uart.flags & PARENB) {
		term.c_cflag |= PARENB;
		if (btuart->uart.uart.flags & PARODD)
			term.c_cflag |= PARODD;
		else
			term.c_cflag &= ~PARODD;
	} else
		term.c_cflag &= ~PARENB;

	err = kdev_ioctl(btuart->filp, TCSETS, &term);
	if (err) {
		BTERROR("Unable to set PARITY: %d\n", err);
		goto ioerr;
	}
	
	DBPRT("Setting speed: %o\n", btuart->uart.uart.speed);
	if (btuart->uart.uart.speed) {
		int	baud = btuart->uart.uart.speed;
#ifdef AFFIX_16C950_BUG
		int	tcr = 0;
		if (btuart->uart.ser.type == PORT_16C950 
				&& btuart->uart.ser.baud_base <= 115200) {
			BTINFO("Working around 16C950 bug in 8250.c ...\n");
			if (baud <= B115200)
				tcr = 0;
			else if (baud <= B230400) {
				tcr = 0x8;	//230400
				baud = B115200;
			} else if (baud <= B460800) {
				tcr = 0x4;	//460800
				baud = B115200;
			} else
				tcr = 0;
		}
#endif
		term.c_cflag = (term.c_cflag & ~CBAUD) | baud;
		err = kdev_ioctl(btuart->filp, TCSETS, &term);
		if (err) {
			BTERROR("Unable to set speed: %d\n", err);
			goto ioerr;
		}
#ifdef AFFIX_16C950_BUG
		if (btuart->uart.ser.type == PORT_16C950)
			serial_icr_write(btuart, UART_TCR, tcr);
#endif
	}
ioerr:
	DBFEXIT;
	return err;
}

/***********************   UART Interface Subsystem  ********************/

int btuart_tty_write(struct affix_uart *uart, char *data, int size)
{
	struct btuart	*btuart = (void*)uart;
	int		actual;

	DBFENTER;
	//btuart->tty->flags |= (1 << TTY_DO_WRITE_WAKEUP);
	set_bit(TTY_DO_WRITE_WAKEUP, &btuart->tty->flags);
	actual = btuart->tty->driver->write(btuart->tty, 0, data, size);
	DBFEXIT;
	return actual;
}

int btuart_tty_open(struct affix_uart *uart)
{
	int		disc = N_AFFIX, err;
	struct btuart	*btuart = (void*)uart;

	DBFENTER;

	btuart->filp = kdev_open(btuart->uart.uart.name);
	if (!btuart->filp)
		return -EBUSY;

	/* get kdev */
	btuart->dev = btuart->filp->f_dentry->d_inode->i_rdev;
	
	err = kdev_ioctl(btuart->filp, TIOCGSERIAL, &btuart->uart.ser);
	if (err)
		goto ioerr;

	err = init_uart(btuart);
	if (err)
		goto ioerr;

	err = kdev_ioctl(btuart->filp, TIOCSETD, &disc);
	if (err)
		goto ioerr;

	DBFEXIT;
	return 0;
ioerr:
	DBPRT("Unable to set discipline/terminal parameters: err: %d\n", err);
	kdev_close(btuart->filp);
	btuart->filp = NULL;
	return err;
}

int btuart_tty_close(struct affix_uart *uart)
{
	struct btuart	*btuart = (void*)uart;
	DBFENTER;
	kdev_close(btuart->filp);
	btuart->filp = NULL;
	DBFEXIT;
	return 0;
}

int btuart_tty_ioctl(struct affix_uart *uart, unsigned int cmd, void *arg)
{
	struct btuart	*btuart = (void*)uart;
	int		err = 0;

	DBFENTER;
	
	if (!btuart)	// sanity check
		return -ENODEV;

	if (_IOC_TYPE(cmd) == 'T') {
		/* terminal ioctl */

		if (!btuart->filp)
			return -ENOTTY;
		err = btuart->filp->f_op->ioctl(btuart->filp->f_dentry->d_inode, 
						btuart->filp, cmd, (unsigned long)arg);
		//err = kdev_ioctl(btuart->filp, cmd, arg);
	} else {
		switch (cmd) {
			case BTIOC_SETUP_UART:
				if (btuart->filp)
					err = init_uart(btuart);
				break;
			default:
				return -ENOIOCTLCMD;
		}
	}
	DBFEXIT;
	return err;
}

/* ****************** attachment stuff ***************************** */

struct btuart *btuart_create(void)
{
	struct btuart	*btuart;
	unsigned long	flags;
	
	btuart = kmalloc(sizeof(*btuart), GFP_KERNEL);
	if (!btuart)
		return NULL;
	memset(btuart, 0, sizeof(*btuart));

	write_lock_irqsave(&btuarts.lock, flags);
	__btl_add_tail(&btuarts, btuart);
	write_unlock_irqrestore(&btuarts.lock, flags);

	return btuart;
}

void btuart_destroy(struct btuart *btuart)
{
	unsigned long	flags;

	write_lock_irqsave(&btuarts.lock, flags);
	__btl_unlink(&btuarts, btuart);
	kfree(btuart);
	write_unlock_irqrestore(&btuarts.lock, flags);
}


int affix_uart_tty_attach(affix_uart_t *uart)
{
	int			err;
	struct btuart		*btuart;
	
	DBFENTER;
	btuart = btuart_lookup_device(uart->name);
	if (btuart)
		return -EBUSY;

	btuart = btuart_create();
	if (!btuart)
		return -ENOMEM;

	btuart->uart.open = btuart_tty_open;
	btuart->uart.close = btuart_tty_close;
	btuart->uart.write = btuart_tty_write;
	btuart->uart.ioctl = btuart_tty_ioctl;

	/* set uart info */
	btuart->uart.uart = *uart;

	err = affix_uart_register(&btuart->uart);
	if (err) {
		btuart_destroy(btuart);
	}
	DBFEXIT;
	return err;
}

int affix_uart_tty_detach(char *name)
{
	struct btuart *btuart;

	DBFENTER;
	btuart = btuart_lookup_device(name);
	if (!btuart)
		return -ENODEV;
	affix_uart_unregister(&btuart->uart);
	btuart_destroy(btuart);
	DBFEXIT;
	return 0;
}

void affix_uart_tty_suspend(char *name)
{
	struct btuart *btuart;

	DBFENTER;
	btuart = btuart_lookup_device(name);
	if (!btuart)
		return;
	affix_uart_suspend(&btuart->uart);
	DBFEXIT;
	return;
}

void affix_uart_tty_resume(char *name)
{
	struct btuart *btuart;

	DBFENTER;
	btuart = btuart_lookup_device(name);
	if (!btuart)
		return;
	affix_uart_resume(&btuart->uart);
	DBFEXIT;
	return;
}

struct affix_uart_operations btuart_ops = {
	.owner = THIS_MODULE,
	.attach = affix_uart_tty_attach,
   	.detach = affix_uart_tty_detach,
	.suspend = affix_uart_tty_suspend,
	.resume = affix_uart_tty_resume
};
	 
/* ************************  Line Discipline  **************************** */

/*
 * Line discipline section
 */
int btuartld_open(struct tty_struct *tty)
{
	struct btuart	*btuart;
	int		err = -EEXIST;

	DBFENTER;
	/* First make sure we're not already connected. */
	btuart = (struct btuart*)tty->disc_data;
	if (btuart) {
		BTERROR("Already opened\n");
		goto err1;
	}

	btuart = btuart_lookup_dev(tty);
	if (!btuart) {
		BTERROR("Device not found\n");
		err = -ENODEV;
		goto err1;
	}

  	btuart->tty = tty;
	tty->disc_data = btuart;

	/* flush all internal buffers */
	if (tty->driver->flush_buffer)
		tty->driver->flush_buffer(tty);

	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
  
	tty->low_latency = (btuart->uart.uart.flags & AFFIX_UART_LOW)? 1: 0;

	DBFEXIT;
	return 0;
 err1:
	tty->disc_data = NULL;
	return err;
}

void btuartld_close(struct tty_struct *tty)
{
	struct btuart	*btuart = (struct btuart *)tty->disc_data;
	
	DBFENTER;
	if (!btuart)
		return;
	/* Stop tty */
	//tty->flags &= ~(1 << TTY_DO_WRITE_WAKEUP);
	clear_bit(TTY_DO_WRITE_WAKEUP, &btuart->tty->flags);
	tty->disc_data = 0;
	btuart->tty = NULL;
	DBFEXIT;
}

/*
 * Function btuartld_ioctl (tty, file, cmd, arg)
 */
int  btuartld_ioctl(struct tty_struct *tty, void *file, int cmd, void *arg)
{
	struct btuart	*btuart = (struct btuart *)tty->disc_data;
	int		err;

	DBFENTER;
	if (!btuart)
		return -ENODEV;
	DBPRT("ioctl, cmd: %#x\n", cmd);
	err = n_tty_ioctl(tty, (struct file *) file, cmd, (unsigned long) arg);
	DBFEXIT;
	return err;	
}

/*
 * Function btuartld_receive_room (tty)
 *
 *    Used by the TTY to find out how much data we can receive at a time
 * 
*/
int  btuartld_receive_room(struct tty_struct *tty)
{
	DBFENTER;
	return 65536;  /* We can handle an infinite amount of data. :-) */
}

void btuartld_receive_buf(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct btuart	*btuart = tty->disc_data;

	if (!btuart)
		return;
#ifndef CONFIG_AFFIX_UART_NEW_BH
	{
		int	i;
		for (i = 0; i < count; i++) {
			if (fp[i])
				BTDEBUG("FLAG ERROR: %#02x\n", fp[i]);
		}
	}
#endif
	affix_uart_recv_buf(&btuart->uart, cp, count);
}

/*
 * Function btuartld_write_wakeup (tty)
 *
 *    Called by the driver when there's room for more data.  If we have
 *    more packets to write, we write them here.
 *
 */
void btuartld_write_wakeup(struct tty_struct *tty)
{
	struct btuart 	*btuart = (struct btuart*)tty->disc_data;

	DBFENTER;
	if (!btuart)
		return;
	affix_uart_write_wakeup(&btuart->uart);
	DBFEXIT;
}

/*
 * btuart_init register line discipline for serial tty
 */
int __init init_btuart_tty(void)
{
	int	err;

	DBFENTER;

	btl_head_init(&btuarts);
	/* Fill in our line protocol discipline, and register it */
	memset(&btuart_ldisc, 0, sizeof(btuart_ldisc));

	btuart_ldisc.magic = TTY_LDISC_MAGIC;
	btuart_ldisc.name  = "n_affix";
	btuart_ldisc.flags = 0;
	btuart_ldisc.open  = btuartld_open;
	btuart_ldisc.close = btuartld_close;
	btuart_ldisc.read  = NULL;
	btuart_ldisc.write = NULL;
	btuart_ldisc.ioctl = (void*)btuartld_ioctl;
	btuart_ldisc.poll  = NULL;
	btuart_ldisc.receive_buf  = btuartld_receive_buf;
	btuart_ldisc.receive_room = btuartld_receive_room;
	btuart_ldisc.write_wakeup = btuartld_write_wakeup;
	btuart_ldisc.owner = THIS_MODULE;

	err = tty_register_ldisc(N_AFFIX, &btuart_ldisc);
	if (err) {
		BTERROR("Can't register line discipline (err = %d)\n", err);
		goto exit;
	}
	
	affix_set_uart(&btuart_ops);

	printk("Affix UART TTY Bluetooth driver loaded (affix_uart)\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");
	
exit:	
	DBFEXIT;
	return err;
}

void __exit exit_btuart_tty(void)
{
	int err;
  
	DBFENTER;
	affix_set_uart(NULL);
	if ((err = tty_register_ldisc(N_AFFIX, NULL)))	{
		BTERROR("can't unregister line discipline (err = %d)\n", err);
	}
	DBFEXIT;
}


/*  If we are resident in kernel we want to call init_btuart_cs manually. */
//module_init(init_btuart_tty);
//module_exit(exit_btuart_tty);

MODULE_AUTHOR("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>");
MODULE_DESCRIPTION("Affix UART TTY driver");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(affix_uart_tty_attach);
EXPORT_SYMBOL(affix_uart_tty_detach);
EXPORT_SYMBOL(affix_uart_tty_suspend);
EXPORT_SYMBOL(affix_uart_tty_resume);

