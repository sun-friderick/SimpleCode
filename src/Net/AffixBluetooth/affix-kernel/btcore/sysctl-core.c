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
   $Id: sysctl-core.c,v 1.4 2004/07/20 16:20:46 chineape Exp $

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

extern int sysctl_l2cap_mtu;
extern int sysctl_l2cap_cl_mtu;
extern int sysctl_l2cap_flush_to;
#ifdef CONFIG_AFFIX_BT_1_2
extern int sysctl_l2cap_mps;
extern int sysctl_l2cap_modes;
extern int sysctl_l2cap_txwindow;
extern int sysctl_l2cap_max_transmit;
extern int sysctl_l2cap_ret_timeout;
extern int sysctl_l2cap_mon_timeout;
#endif

static ctl_table affix_table[] = {
	/* HCI */
	{NET_AFFIX_HCI_SNIFF, "hci_allow_promisc",
	 &sysctl_hci_allow_promisc, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_HCI_USE_INQUIRY, "hci_use_inquiry",
	 &sysctl_hci_use_inquiry, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_HCI_MAX_ATTEMPT, "hci_max_attempt",
	 &sysctl_hci_max_attempt, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_HCI_REQ_COUNT, "hci_req_count",
	 &sysctl_hci_req_count, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_HCI_REQ_TIMEOUT, "hci_req_timeout",
	 &sysctl_hci_req_timeout, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_HCI_DEFER_DISC_TIMEOUT, "hci_defer_disc_timeout",
	 &sysctl_hci_defer_disc_timeout, sizeof(int), 0644, NULL, &proc_dointvec},
	 /* L2CAP */
	{NET_AFFIX_L2CAP_MTU, "l2cap_mtu",
	 &sysctl_l2cap_mtu, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_CL_MTU, "l2cap_cl_mtu",
	 &sysctl_l2cap_cl_mtu, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_FLUSH_TO, "l2cap_flush_to",
	 &sysctl_l2cap_flush_to, sizeof(int), 0644, NULL, &proc_dointvec},
#ifdef CONFIG_AFFIX_BT_1_2
	{NET_AFFIX_L2CAP_MPS, "l2cap_mps",
	 &sysctl_l2cap_mps, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_SUPPORTED_MODES, "l2cap_extended_features",
	 &sysctl_l2cap_modes, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_TXWINDOW, "l2cap_txwindow",
	 &sysctl_l2cap_txwindow, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_MAX_TRANSMIT, "l2cap_max_transmit",
	 &sysctl_l2cap_max_transmit, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_RET_TIMEOUT, "l2cap_ret_timeout",
	 &sysctl_l2cap_ret_timeout, sizeof(int), 0644, NULL, &proc_dointvec},
	{NET_AFFIX_L2CAP_MON_TIMEOUT, "l2cap_mon_timeout",
	 &sysctl_l2cap_mon_timeout, sizeof(int), 0644, NULL, &proc_dointvec},
#endif
	 { 0 }
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static ctl_table affix_core_table[] = {
	{NET_AFFIX_CORE, "core", NULL, 0, 0555, affix_table},      
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
int affix_sysctl_register(void)
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
void affix_sysctl_unregister(void) 
{
	unregister_sysctl_table(affix_table_header);
}

/* export stuff */


