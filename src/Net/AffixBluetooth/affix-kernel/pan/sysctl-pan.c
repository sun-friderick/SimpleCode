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
   $Id: sysctl-pan.c,v 1.3 2004/02/24 15:39:58 kassatki Exp $

   AF_AFFIX - HCI Protocol Address family for socket interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>
#include <linux/version.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>

/* Local Includes */
//#define FILEBIT	DBAFHCI

#include <affix/bluetooth.h>
#include <affix/hci.h>


int	sysctl_l2cap_mtu = 0x1FFF;

#if 0
int proc_dointvec(ctl_table *table, int write, struct file *filp,
		     void *buffer, size_t *lenp)
{
    return do_proc_dointvec(table,write,filp,buffer,lenp,1,OP_SET);
}

#endif

static ctl_table affix_table[] = {
	{NET_AFFIX_PAN_MTU, "pan_mtu",
	 &sysctl_l2cap_mtu, sizeof(int), 0644, NULL, &proc_dointvec},
	 { 0 }
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static ctl_table affix_core_table[] = {
	{NET_AFFIX_PAN, "pan", NULL, 0, 0555, affix_table},      
	{ 0 }
};
#endif

static ctl_table affix_net_table[] = {
	{NET_AFFIX, "affix", NULL, 0, 0555, affix_table},      
	{ 0 }
};

/* The parent directory */
static ctl_table affix_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, affix_net_table},
	{ 0 }
};

static struct ctl_table_header *affix_table_header;

/*
 * Function affix_sysctl_register (void)
 *
 *    Register our sysctl interface
 *
 */
int pan_sysctl_register(void)
{
	affix_table_header = register_sysctl_table(affix_root_table, 0);
	if (!affix_table_header)
		return -ENOMEM;
	return 0;
}

/*
 * Function affix_sysctl_unregister (void)
 *
 *    Unregister our sysctl interface
 *
 */
void pan_sysctl_unregister(void) 
{
	unregister_sysctl_table(affix_table_header);
}

