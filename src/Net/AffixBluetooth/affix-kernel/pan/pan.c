/* -*-   Mode:C; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001, 2002 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
   Original Author: Muller Ulrich <ulrich.muller@nokia.com>

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
   $Id: pan.c,v 1.103 2004/05/17 16:11:11 kassatki Exp $

   pan.c - PAN kernel module
   
   Fixes:
   
   	Dmitry Kasatkin		- Integrated to the Affix

*/


/* kernel includes: */
#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <net/ip.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <asm/uaccess.h>
#include <linux/smp_lock.h>
#include <linux/ethtool.h>

/* other includes: */
#define FILEBIT		DBPAN

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>

#include "pan.h"
#include "bnep.h"


static int errno;

#define Connections_wanted	1 /* a pan user only wants a connection to one gan/nap */
#define Max_Num_Responses	100
#define Max_Connection_Trys	5

/* global variables */

btlist_head_t		btdevs;				/* Bluetooth devices */
int			sysctl_pan_sndbuf = 0xFFFF;

#define SigConnectCfm_Pos	1
#define SigConnectCfm_Neg	2

static int SigConnectCfm = 0;				/* set when pos/neg ConnectCfm is received */
static int SigDisconnectCfm = 0;			/* set when DisconnectCfm is received */
atomic_t	conn_count;

static DECLARE_WAIT_QUEUE_HEAD(SigConnectCfm_wait);	/* waked up when ConnectCfm/Ind is received */
static DECLARE_WAIT_QUEUE_HEAD(SigDisconnectCfm_wait);	/* waked up when DisconnectCfm/Ind is received */


/* class of device field according Bluetooth Assigned Numbers (network byte order)*/
__u32	HCI_COD_NETCOMP = 0x020100;
__u32	HCI_COD_LAP_LOAD[8] =	{0x020300, 0x020320, 0x020340, 0x020360,
				0x020380, 0x0203A0, 0x0203C0, 0x0203E0};

/* convert bluetooth CoD to string */

char *BD_CLASS2str(__u32 bdc)
{
	static unsigned char buf[2][9];
	static int num = 0;

	if (bdc == HCI_COD_NETCOMP)
		return "Computer";
	if (bdc == HCI_COD_LAP_LOAD[0])
		return "LAN Access Point/no connections";
	if (bdc == HCI_COD_LAP_LOAD[1])
		return "LAN Access Point/1 connection";
	if (bdc == HCI_COD_LAP_LOAD[2])
		return "LAN Access Point/2 connections";
	if (bdc == HCI_COD_LAP_LOAD[3])
		return "LAN Access Point/3 connections";
	if (bdc == HCI_COD_LAP_LOAD[4])
		return "LAN Access Point/4 connections";
	if (bdc == HCI_COD_LAP_LOAD[5])
		return "LAN Access Point/5 connections";
	if (bdc == HCI_COD_LAP_LOAD[6])
		return "LAN Access Point/6 connections";
	if (bdc == HCI_COD_LAP_LOAD[7])
		return "LAN Access Point/7 connections";
	num = 1 - num;
	sprintf(buf[num], "%#.6x", bdc);
	return buf[num];
}


/* compare BD_ADDR with reverse ordered ethernet addr */

int ethbdacmp(BD_ADDR *bda, void *etaddr)
{
	BD_ADDR		eth;
	
	bdacpy(&eth, etaddr);
	return !bda_equal(&eth, bda);
}

void pan_wfree(struct sk_buff *skb)
{
	struct pan_dev	*btdev = (void*)skb->sk;

	DBFENTER;
	atomic_sub(skb->truesize, &btdev->wmem_alloc);
	if ((atomic_read(&btdev->wmem_alloc) << 1) <= btdev->sndbuf)
		netif_wake_queue(btdev->net_dev);
	pan_put(btdev);
	DBFEXIT;
}

void pan_skb_set_owner_w(struct sk_buff *skb, struct pan_dev *btdev)
{
	DBFENTER;
	pan_hold(btdev);
	skb->sk = (void*)btdev;
	skb->destructor = pan_wfree;
	atomic_add(skb->truesize, &btdev->wmem_alloc);
	if (atomic_read(&btdev->wmem_alloc) >= btdev->sndbuf)
		netif_stop_queue(btdev->net_dev);
	DBFEXIT;
}


#if 0
struct pan_dev *__pan_lookup_bda(BD_ADDR *bda)
{
	struct pan_dev	*btdev;

	DBFENTER;
	btl_for_each (btdev, btdevs) {
		if (memcmp(&btdev->bdaddr, bda, 6) == 0)
			return btdev;
	}
	DBFEXIT;
	return NULL;
}

struct pan_dev *pan_lookup_bda(BD_ADDR *bda)
{
	struct pan_dev	*btdev;

	DBFENTER;
	btl_read_lock(&btdevs);
	btdev = __pan_lookup_bda(bda);
	if (btdev)
		pan_hold(btdev);
	btl_read_unlock(&btdevs);
	DBFEXIT;
	return btdev;
}
#endif

struct pan_dev *pan_lookup_devnum(int devnum)
{
	struct pan_dev	*btdev = NULL;

	DBFENTER;
	btl_read_lock(&btdevs);
	btl_for_each (btdev, btdevs) {
		if (btdev->hci->devnum == devnum)
			break;
	}
	if (btdev)
		pan_hold(btdev);
	btl_read_unlock(&btdevs);
	DBFEXIT;
	return btdev;
}
/* helper for pan_dev */

/* 
 * add new connection to btdev->connections
 * forward connection to bnep layer; when running as a gan/nap, update class of device,
 * returns 0 on success 
 */
int pan_connection_add(struct pan_dev *btdev, l2cap_ch *ch, State state)
{
	struct bnep_con	*cl;

	DBFENTER;
	if (btdev->connections.len >= 7) { /* should not be possible */
		DBPRT("too many connections!\n");
		return -EBUSY;
	}
	cl = (struct bnep_con*)kmalloc(sizeof(struct bnep_con), GFP_ATOMIC);
	if (!cl)
		return -ENOMEM; /* out of memory */
	memset(cl, 0, sizeof(struct bnep_con));

	cl->ch = ch;
	cl->btdev = btdev;
	pan_hold(btdev);
	cl->state = state;
	/* add to head of connection list */
	btl_add_tail(&btdev->connections, cl);
	l2ca_graft(ch, cl);
	if (state == configured)
		bnep_init(cl);
	DBFEXIT;
	return 0;
}

/* 
 * change state of connection,
 * forward connection to bnep layer
 */
void pan_connection_change_state(l2cap_ch *ch, State state)
{
	struct bnep_con *cl = (struct bnep_con*) ch->priv;

	DBFENTER;
	if (!cl || cl->state == state)
		return;
	cl->state = state;
	/* we dont expect to change state from configured to unconfigured */
	if (state == configured)
		bnep_init(cl);
	DBFEXIT;
}

/* remove connection from btdev->connections
 * forward connection to bnep layer; when running as a gan/nap, update class of device
 */
void pan_connection_remove(l2cap_ch *ch)
{
	struct bnep_con		*cl = (struct bnep_con*)ch->priv;
	struct pan_dev		*btdev = cl->btdev;

	DBFENTER;
	if (!cl)
		return;
	if (cl->setup_complete)
		pan_check_link(btdev, 0);
	l2ca_orphan(ch);
	l2ca_put(ch);
	bnep_close(cl);
	btl_unlink(&btdev->connections, cl);
	pan_put(btdev);
	kfree(cl);
	DBFEXIT;
}

/*
 * close all open connections of given device
 */
void pan_close_connections(struct pan_dev *btdev)
{
	struct bnep_con		*cl;
	l2cap_ch		*ch;
	
	DBFENTER;
	while ((cl = btl_dequeue_head(&btdev->connections))) {
		ch = cl->ch;
		DBPRT("closing connection to %s\n", bda2str(&ch->bda));
		SigDisconnectCfm = 0;
		pan_connection_remove(ch);
	}
	SigDisconnectCfm = 1;
	wake_up_interruptible(&SigDisconnectCfm_wait);
	DBFEXIT;
}

struct bnep_con *pan_connection_lookup(struct pan_dev *btdev, BD_ADDR *bda)
{
	struct bnep_con *cl;

	btl_read_lock(&btdev->connections);
	btl_for_each (cl, btdev->connections) {
		if (bda_equal(&cl->ch->bda, bda))
			break;		
	}
	btl_read_unlock(&btdev->connections);
	return cl;
}


/*
 * this function set the class of device/scan activity according to the current number of connections
 */
void pan_connections_update(struct pan_dev *btdev)
{
	int counter = btdev->connections.len;

	if (counter > 7)
		counter = 7; /* class of device is defined for up to 7 connections */
	if (counter == btdev->connections_counter_old)
		return;
	if (btdev->role != AFFIX_PAN_PANU) { /* switch */
		DBPRT("setting CoD to load %d\n", counter);
		HCI_WriteClassOfDevice(btdev->fd, HCI_COD_LAP_LOAD[counter]);
		if (counter == 7) /* maximum number of slaves reached */
			HCI_WriteScanEnable(btdev->fd, HCI_SCAN_OFF); /* disable page/inquiry scan */
		else if (btdev->connections_counter_old == 7) /* maximum number of slaves left */
			HCI_WriteScanEnable(btdev->fd, HCI_SCAN_BOTH);
	}
	btdev->connections_counter_old = counter;
}

/* connection initiation */

/* 
 * thread that connects to other devices until thread_stop != 0
 */
int pan_panu_thread(void *p)
{
	struct pan_dev	*btdev = p;
	INQUIRY_ITEM	inquiry_list[Max_Num_Responses];	/* list of inquiry results */
	int		connect_counter[Max_Num_Responses];	/* list of connection trys to each inquiry device */

	DBFENTER;

	lock_kernel();
        daemonize("panu");
	snprintf(current->comm, sizeof(current->comm), "%s", btdev->net_dev->name);
	unlock_kernel();

	btdev->fd = hci_open_id(btdev->hci->devnum);
	if (btdev->fd < 0) {
		BTERROR("cannot open BT device %s\n", btdev->hci->name);
		btdev->thread_stop = 1;
	}

	up(&btdev->thread_sem); /* started - unlock */

	/* set class of device and disable page/inquiry scan until net device is "up" */
	HCI_WriteScanEnable(btdev->fd, HCI_SCAN_OFF);
	HCI_WriteClassOfDevice(btdev->fd, HCI_COD_NETCOMP);

	while (!btdev->thread_stop) { /* loop until thread_stop is set */
		DECLARE_WAITQUEUE(wait, current);
		__u8 Num_Responses_real;
		int i, ret, counter, Num_Responses;

		BTINFO("PAN: inquiry...\n");
		ret = HCI_Inquiry(btdev->fd, 10, Max_Num_Responses, inquiry_list, &Num_Responses_real);

		if (ret != 0) {
			BTERROR("HCI inquiry error\n");
			btdev->thread_stop = 1;
			break;
		}
		/* filter inquiry result with CoD */
		i = 0;
		Num_Responses = Num_Responses_real;
		while (i < Num_Responses) {
			int ok = 0;
			int j;
			__u32	cod = inquiry_list[i].Class_of_Device;

			for (j = 0; j < 8; j++)
				if ((cod & HCI_COD_LAP_LOAD[j]) == HCI_COD_LAP_LOAD[j])
					ok = 1;
			if (!ok) {
				DBPRT("skipping %s/%s\n", bda2str(&inquiry_list[i].bda), 
						BD_CLASS2str(inquiry_list[i].Class_of_Device));
				memcpy(&inquiry_list[i], &inquiry_list[i + 1], (Num_Responses - i) * sizeof(INQUIRY_ITEM));
				Num_Responses--;
			} else {
				i++;
			}
		}
		BTINFO("PAN: found %d devices (%d total)\n", Num_Responses, Num_Responses_real);
		memset(&connect_counter, 0, sizeof(connect_counter));

		/* now try to connect to up to Connections_wanted devices */
		i = 0;
		counter = 0; /* counts connection trys */
		while (!btdev->thread_stop && (i < Num_Responses ) && 
				(btdev->connections.len < Connections_wanted)) {
			DBPRT("connecting to %s/%s (device %d/%d, try %d/%d)\n", 
					bda2str(&inquiry_list[i].bda), 
					BD_CLASS2str(inquiry_list[i].Class_of_Device), i+1, Num_Responses,
					connect_counter[i] + 1, Max_Connection_Trys);

			SigConnectCfm = 0;
			ret = pan_connect_req(btdev, &inquiry_list[i].bda, BNEPPSM);
			if (ret == 0) { /* now wait until connection is confirmed or cancelled */
				DBPRT("waiting for SigConnectCfm\n");
				add_wait_queue(&SigConnectCfm_wait, &wait);
				set_current_state(TASK_INTERRUPTIBLE);
				while(!btdev->thread_stop && !SigConnectCfm) {
					if (signal_pending(current)) {
						btdev->thread_stop = 1;
						break;
					}
					schedule();
					set_current_state(TASK_INTERRUPTIBLE);
				}
				set_current_state(TASK_RUNNING);
				remove_wait_queue(&SigConnectCfm_wait, &wait);
				DBPRT("waiting finished\n");
			}
			pan_connections_update(btdev);
			/* determine next connection partner */
			if (SigConnectCfm == SigConnectCfm_Pos)
				connect_counter[i] = Max_Connection_Trys;
			else
				connect_counter[i]++;
			do {
				i++;
			} while (i < Num_Responses && connect_counter[i] == Max_Connection_Trys);
			if (i == Num_Responses) {
				i = 0;
				while (i < Num_Responses && connect_counter[i] == Max_Connection_Trys)
					i++;
			}
		}
		/* 
		 * Now we have tried to connect to all devices from last inquiry 
		 * OR have connected to Connections_wanted devices.
		 * Before we start the next inquiry, we wait until all connections are down.
		 */
		DBPRT("waiting for all connections to close\n");
		add_wait_queue(&SigDisconnectCfm_wait, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		while (!btdev->thread_stop && (btdev->connections.len)) {
			if (signal_pending(current)) {
				btdev->thread_stop = 1;
				break;
			}
			schedule();
			pan_connections_update(btdev);
			set_current_state(TASK_INTERRUPTIBLE);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&SigDisconnectCfm_wait, &wait);
		DBPRT("no connections available\n");
	}
	hci_close(btdev->fd);
	up(&btdev->thread_sem); /* thread going down */
	DBFEXIT;
	return 0;
}

/*
 * thread that updates CoD/scan activity until thread_stop != 0
 */
int pan_nap_thread(void *p)
{
	struct pan_dev		*btdev = p;
	DECLARE_WAITQUEUE(wait1, current);
	DECLARE_WAITQUEUE(wait2, current);

	DBFENTER;

	lock_kernel();
        daemonize("nap");
	snprintf(current->comm, sizeof(current->comm), "%s", btdev->net_dev->name);
	unlock_kernel();

	btdev->fd = hci_open_id(btdev->hci->devnum);
	if (btdev->fd < 0) {
		BTERROR("cannot open BT device %s\n", btdev->hci->name);
		btdev->thread_stop = 1;
	}

	up(&btdev->thread_sem); /* started: unlock */
	
	/* enable Inquiry Scan + Page Scan and wait for panu to connect */
	HCI_WriteScanEnable(btdev->fd, HCI_SCAN_BOTH);
	HCI_WriteClassOfDevice(btdev->fd, HCI_COD_LAP_LOAD[0]); /* no connections */

	add_wait_queue(&SigConnectCfm_wait, &wait1);
	add_wait_queue(&SigDisconnectCfm_wait, &wait2);
	set_current_state(TASK_INTERRUPTIBLE);
	while(!btdev->thread_stop) {	
		if (signal_pending(current)) {
			btdev->thread_stop = 1;
			break;
		}
		schedule();
		pan_connections_update(btdev);
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&SigConnectCfm_wait, &wait1);
	remove_wait_queue(&SigDisconnectCfm_wait, &wait2);

	HCI_WriteScanEnable(btdev->fd, HCI_SCAN_OFF);
	hci_close(btdev->fd);

	up(&btdev->thread_sem); /* thread going down */
	DBFEXIT;
	return 0;
}


/*
 * start + stop panu/access point
 */
int pan_thread_start(struct pan_dev *btdev)
{
	void	*func;

	if (!(btdev->mode & AFFIX_PAN_AUTO))
		return 0;

	if (btdev->role == AFFIX_PAN_PANU)
		func = pan_panu_thread;
	else
		func = pan_nap_thread;
	btdev->thread_stop = 0;
	down(&btdev->thread_sem); /* assume thread is running: set thread running lock */
	if (kernel_thread(func, btdev, CLONE_FS | CLONE_FILES) < 0) {
		up(&btdev->thread_sem); /* unlock */
		BTERROR("PAN: unable to start kernel thread\n");
		return -1;
	} else {
		down(&btdev->thread_sem);	/* wait for thread started */
		if (btdev->thread_stop) {
			BTERROR("PAN: unable to start kernel thread\n");
			return -1;
		}
	}
	return 0;
}

void pan_thread_stop(struct pan_dev *btdev)
{
	if (!(btdev->mode & AFFIX_PAN_AUTO))
		return;
	if (btdev->thread_stop)
		return;
	/* stop thread */
	btdev->thread_stop = 1;				/* signal thread to stop */
	wake_up_interruptible(&SigConnectCfm_wait);	/* wake thread if it is sleeping */
	wake_up_interruptible(&SigDisconnectCfm_wait);	/* wake thread if it is still sleeping */
	down(&btdev->thread_sem);			/* wait for thread to finish */
	up(&btdev->thread_sem);				/* but dont block restart */
}

/*
 * net device functions: called from network layer
 */
int pan_net_open(struct net_device *dev)
{
	DBFENTER;
	/* set device flag 'up' */
	netif_start_queue(dev);
	DBFEXIT;
	return 0;
}

int pan_net_close(struct net_device *dev)
{
	struct pan_dev	*btdev = (struct pan_dev*) dev->priv;
	DBFENTER;
	/* set device flag 'down' */
	netif_stop_queue(dev);
	while (atomic_read(&btdev->wmem_alloc) != 0)
		yield();
	DBFEXIT;
	return 0;
}

int pan_ethtool_ioctl(struct net_device *dev, void __user *useraddr)
{
	struct ethtool_value	edata;
	struct ethtool_drvinfo	info;

	DBFENTER;

	if (copy_from_user(&edata.cmd, useraddr, sizeof (edata.cmd)))
		return -EFAULT;

	switch (edata.cmd) {
		case ETHTOOL_GLINK:
			edata.cmd = ETHTOOL_GLINK;
			edata.data = netif_running(dev) && netif_carrier_ok(dev);
			if (copy_to_user(useraddr, &edata, sizeof(edata)))
				return -EFAULT;
			break;
		case ETHTOOL_GDRVINFO:
			memset(&info, 0, sizeof(info));
			info.cmd = ETHTOOL_GDRVINFO;
			strcpy(info.driver, "affix_pan");
			strcpy(info.version, AFFIX_VERSION);
			strcpy(info.fw_version, "N/A");
			strcpy(info.bus_info, "N/A");

			if (copy_to_user(useraddr, &info, sizeof(info)))
				return -EFAULT;
			break;

		default:
			return -EOPNOTSUPP;
	}
	DBFEXIT;
	return 0;
}


int pan_net_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct pan_dev	*btdev = (struct pan_dev*) dev->priv;

	switch (cmd) {
		case SIOCETHTOOL:
			return pan_ethtool_ioctl(dev, ifr->ifr_data);
		case SIOCSFILTERPROTOCOL: 
			{ /* set protocol filter */
				protocol_filter pf;
				if (copy_from_user(&pf, ifr->ifr_ifru.ifru_data, sizeof(pf)))
					return -EFAULT;
				if (pf.count > PROTOCOL_FILTER_MAX)
					return -ENOMEM;
				bnep_set_filter_protocol(btdev, &pf);
			}
			break;
		case SIOCSFILTERMULTICAST: 
			{ /* set multicast filter */
				multicast_filter mf;
				if (copy_from_user(&mf, ifr->ifr_ifru.ifru_data, sizeof(mf)))
					return -EFAULT;
				if (mf.count > MULTICAST_FILTER_MAX)
					return -ENOMEM;
				bnep_set_filter_multicast(btdev, &mf);
			}
			break;
		case SIOCGFILTERPROTOCOL: 
			/* get protocol filter */
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &btdev->pf, sizeof(protocol_filter)))
				return -EFAULT;
			break;
		case SIOCGFILTERMULTICAST: /* get multicast filter */
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &btdev->mf, sizeof(multicast_filter)))
				return -EFAULT;
			break;

		default:
			return -EOPNOTSUPP;
	}
	return 0;
}

/* 
 * Set or clear the multicast filter for this device
 */
void pan_net_set_multicast_list(struct net_device *dev)
{
	struct pan_dev		*btdev = (struct pan_dev*) dev->priv;
	multicast_filter	mf;
	struct 			dev_mc_list *dmi = dev->mc_list;
	ETH_ADDR		*addr;
	int			i;

	mf.count = 0;
	if ((dev->flags & IFF_PROMISC) || (dev->flags & IFF_ALLMULTI) || (dev->mc_count > MULTICAST_FILTER_MAX)) {
		DBPRT("all multicast mode\n");
		bnep_set_filter_multicast(btdev, &mf);
		return;
	}

	/* Add addresses */
	DBPRT("%d multicast addresses:\n", dev->mc_count);
	for (i = 0; i < dev->mc_count; i++) {
		addr = (ETH_ADDR*) dmi->dmi_addr;
		DBPRT("- %s\n", ETH_ADDR2str(addr));

		memcpy(&mf.multicast[i][0], addr, 6);
		memcpy(&mf.multicast[i][1], addr, 6); /* start range == stop range */
		dmi = dmi->next;
	}
	mf.count = dev->mc_count;
	bnep_set_filter_multicast(btdev, &mf);
}

struct net_device_stats *pan_net_get_stats(struct net_device *dev)
{
	struct pan_dev *btdev = (struct pan_dev*) dev->priv;

	return &btdev->stats;
}

/* receive a packet from upper layer */
int pan_net_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct pan_dev	*btdev = (struct pan_dev*) dev->priv;
	int 		result, len;

	DBFENTER;
	DBPRT("len: %d, pkt_type: %x, protocol: %x\n", skb->len, skb->pkt_type, skb->protocol); 
	DBDUMP(skb->data, skb->len);
	len = skb->len;
	result = bnep_process_upper(btdev, skb);
	if (result == 0) {
		btdev->stats.tx_packets++;
		btdev->stats.tx_bytes += len;
		btdev->net_dev->trans_start = jiffies;
	} else {
		btdev->stats.tx_dropped++;
	}
	DBFEXIT;
	return 0;
}

int pan_net_set_mac_addr(struct net_device *dev, void *arg)
{
	BTDEBUG("%s", dev->name);
	return 0;
}

void pan_net_timeout(struct net_device *dev)
{
	BTDEBUG("net_timeout");
	netif_wake_queue(dev);
}

int pan_net_init(struct net_device *dev)
{
	struct pan_dev *btdev = (struct pan_dev*) dev->priv;

	DBFENTER;
	DBPRT("%s\n", dev->name);

	if (btdev->role == AFFIX_PAN_PANU) /* get multicast filter from net device */
		dev->set_multicast_list = pan_net_set_multicast_list;

	DBFEXIT;
	return 0;
}

/*
 * called under dev_hold().. do does dev_put()
 */
void pan_net_uninit(struct net_device *dev)
{
	struct pan_dev *btdev = (struct pan_dev*) dev->priv;

	DBFENTER;
	btl_unlink(&btdevs, btdev);
	pan_thread_stop(btdev);
	/* close all open connections */
	pan_close_connections(btdev);
	hci_put(btdev->hci);		// XXX: should be in pan_net_destructor?
	dev_put(dev);
	BTINFO("PAN device %s unregistered, refcnt:%d\n", dev->name, atomic_read(&dev->refcnt));
	DBFEXIT;
}



/* called from ourself to transmit packet to net device */
void pan_net_receive(struct sk_buff *skb, struct pan_dev *btdev)
{
	DBFENTER;
#if 0
	if (!netif_running(btdev->net_dev)) {
		kfree_skb(skb);
		return;
	}
#endif
	memset(skb->cb, 0, sizeof(skb->cb));
	skb->pkt_type = PACKET_HOST;
	skb->protocol = eth_type_trans(skb, btdev->net_dev);
	skb->dev = btdev->net_dev;
	skb->dev->last_rx = jiffies;
	btdev->stats.rx_packets++;
	btdev->stats.rx_bytes += skb->len;
	DBPRT("received, len: %d, pkt_type: %x, protocol: %x\n", skb->len, skb->pkt_type, skb->protocol); 
	DBDUMP(skb->data, skb->len);
	netif_rx(skb);   /* Eat it! */	// netif_receive_skb(skb);   /* Eat it! */
	DBFEXIT;
}

/* initialize virtual network device 'panx' */
void pan_net_setup(struct net_device *dev)
{
	DBFENTER;
	ether_setup(dev); 	/* register some kernel internal default ethernet functions (net_init.c) */

	dev->init		= pan_net_init;
	dev->uninit		= pan_net_uninit;
	dev->destructor 	= free_netdev;
	dev->open		= pan_net_open;
	dev->stop		= pan_net_close;
	dev->do_ioctl		= pan_net_ioctl;
	dev->get_stats		= pan_net_get_stats;
	dev->hard_start_xmit	= pan_net_xmit;
	dev->set_mac_address 	= pan_net_set_mac_addr;
	dev->watchdog_timeo	= HZ * 2;
	dev->tx_timeout		= pan_net_timeout;
	dev->tx_queue_len	= 25; // default (1000) is too heavy

	DBFEXIT;
}

/* PAN L2CAP upper protocol layer */

/*
 * Upper layers to l2cap
 */
int pan_connect_req(struct pan_dev *btdev, BD_ADDR *bdaddr, __u16 psm)
{
	int		err;
	l2cap_ch	*ch;

	if (!btdev)
		return -ENODEV;
	ch = l2ca_create(&l2cap_ops);
	if (ch == NULL) {
		DBPRT("l2ca_create() failed\n");
		return -ENOMEM;
	}
	hci_hold(btdev->hci);
	ch->hci = btdev->hci;
	/* channel configuration */
	if (pan_connection_add(btdev, ch, unconfigured) != 0)
		goto fail;
	l2ca_set_mtu(ch, PAN_MTU);
	err = l2ca_connect_req(ch, bdaddr, psm);
	return err;
fail:
	l2ca_put(ch);
	return -1;
}

int pan_DataWriteBroadcast(__u16 psm, struct sk_buff *skb)
{
	dev_kfree_skb(skb);
	return -EFAULT;
}

int pan_connect_ind(l2cap_ch *ch)
{
	struct pan_dev *btdev;

	BTINFO("PAN: connection request from %s\n", bda2str(&ch->bda));

	btdev = pan_lookup_devnum(ch->hci->devnum);
	if (!btdev)
		goto fail0;
	if (btdev->role == AFFIX_PAN_PANU) /* I am not a gan/nap */
		goto fail;
#if 0
	/* if gan/nap is not "up", refuse connection */
	if (!test_bit(__LINK_STATE_START, &btdev->net_dev->state)) {
		DBPRT("connection refused - net device is not running\n");
		goto fail;
	}
#endif
	/* not needed anymore */
	if (pan_connection_add(btdev, ch, unconfigured) != 0)
		goto fail;
	l2ca_hold(ch);
	/* accept connection but do the channel configuration before */
	l2ca_set_mtu(ch, PAN_MTU);
	/* accept connection */
	l2ca_connect_rsp(ch, L2CAP_CONRSP_SUCCESS, 0);
	/* signal connect thread  */
	wake_up_interruptible(&SigConnectCfm_wait); /* waked up when ConnectionEvent is set */
	pan_put(btdev);
	return 0;
fail:
	pan_put(btdev);
fail0:
	l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
	return 0;
}

int pan_connect_cfm(l2cap_ch *ch, int result, int status)
{
	DBFENTER;
	switch (result) {
		case L2CAP_CONRSP_SUCCESS:
			BTINFO("PAN: connection to %s successful\n", bda2str(&ch->bda));
			pan_connection_change_state(ch, configured);
			SigConnectCfm = SigConnectCfm_Pos;
			break;

		case L2CAP_CONRSP_PENDING:
			DBPRT("connection to %s is pending\n", bda2str(&ch->bda));
			return 0;
			break;

		default:
			BTERROR("PAN: connection to %s failed\n", bda2str(&ch->bda));
			SigConnectCfm = SigConnectCfm_Neg;
			pan_connection_remove(ch);
	}
	/* signal connect thread  */
	wake_up_interruptible(&SigConnectCfm_wait); /* waked up when ConnectionEvent is set */
	DBFEXIT;
	return 0;
}

int pan_config_ind(l2cap_ch *ch)
{
	DBFENTER;

	if (l2ca_get_mtu(ch) < PAN_MTU) { /* wrong MTU */
		DBPRT("wrong MTU (%d instead %d), sending negative ConfigRsp\n", l2ca_get_mtu(ch), PAN_MTU);
		l2ca_set_mtu(ch, PAN_MTU);
		l2ca_config_rsp(ch, L2CAP_CFGRSP_PARAMETERS);
		return 0;
	}

	DBPRT("sending positive ConfigRsp\n");
	/* the responder of a positive ConfigReq must always return the MTU it will use */
	l2ca_set_mtu(ch, PAN_MTU);
	l2ca_config_rsp(ch, L2CAP_CFGRSP_SUCCESS);

	//pan_connection_change_state(con, configured);

	DBFEXIT;
	return 0;
}

int pan_config_cfm(l2cap_ch *ch, int result)
{
	//if (result != L2CAP_CFGRSP_SUCCESS)
	//pan_connection_change_state(con, configured);
	return 0;
}

/*
 * check if we have at least one connection then link is up
 */
void pan_check_link(struct pan_dev *btdev, int added)
{
	DBFENTER;
	if (added) {
		atomic_inc(&btdev->conn_count);
		// there are connections..
		if (!netif_carrier_ok(btdev->net_dev)) {
			netif_carrier_on(btdev->net_dev);
			BTINFO("%s link is up\n", btdev->net_dev->name);
		}
	} else {
		if (atomic_dec_and_test(&btdev->conn_count)) {
			if (netif_carrier_ok(btdev->net_dev)) {
				netif_carrier_off(btdev->net_dev);
				BTINFO("%s link is down\n", btdev->net_dev->name);
			}
		}
	}
	DBFEXIT;
}

int pan_disconnect_ind(l2cap_ch *ch)
{
	struct bnep_con		*cl = (struct bnep_con*)ch->priv;
	struct pan_dev		*btdev = cl->btdev;

	DBFENTER;
	BTINFO("PAN: connection to %s closed\n", bda2str(&ch->bda));
	pan_deliver_event(btdev, PANDEV_CONNECT_LOST);
	/* signal disconnect task that connection is closed, se comment in pan_close_connections */
	DBPRT("sending SigDisconnectCfm\n");
	SigDisconnectCfm = 1;
	wake_up_interruptible(&SigDisconnectCfm_wait);
	pan_connection_remove(ch);
	DBFEXIT;
	return 0;
}

int pan_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	struct bnep_con *cl = (struct bnep_con*) ch->priv;
	bnep_process_lower(cl, skb);
	return 0;
}

/* Callback functions for L2CAP layer */
l2cap_proto_ops l2cap_ops = {
	.owner = THIS_MODULE,
	.data_ind = pan_data_ind,
	.connect_ind = pan_connect_ind,
	.connect_cfm = pan_connect_cfm,
	.config_ind = pan_config_ind,
	.config_cfm = pan_config_cfm,
	.disconnect_ind = pan_disconnect_ind
};


int pan_exit_netdev(hci_struct *hci)
{
	struct pan_dev	*btdev;
	
	DBFENTER;
	btdev = pan_lookup_devnum(hci->devnum);
	if (!btdev)
		return -ENODEV;
	unregister_netdev(btdev->net_dev);
	DBFEXIT;
	return 0;
}

int pan_event(struct notifier_block *nb, unsigned long event, void *arg)
{
	hci_struct	*hci = arg;

	switch (event) {
		case HCIDEV_DOWN:
			pan_exit_netdev(hci);
			break;
		default:
			break;
	}
	return NOTIFY_DONE;
}

struct notifier_block	pan_notifier_block = {
	.notifier_call = pan_event,
};


int pan_init(struct pan_init *arg)
{
	hci_struct		*hci = NULL;
	int			err;
	int			role, i;
	struct pan_dev		*btdev;
	struct net_device	*dev;
	char			name[IFNAMSIZ];

	DBFENTER;
	hci = hci_lookup_name(arg->name);
	if (!hci)
		return -ENODEV;
	/* evaluate "role" */
	role = arg->mode & AFFIX_PAN_ROLE;
	if (role == 0) {
		err =  pan_exit_netdev(hci);
		goto out;
	}
	btdev = pan_lookup_devnum(hci->devnum);
	if (btdev) {
		err = -EEXIST;
		pan_put(btdev);
		goto out;
	}

	if (sscanf(hci->name, "bt%d", &i) > 0 ) {
		sprintf(name, "pan%d", i);
	} else {
		strcpy(name, "pan%d");
	}

	dev = alloc_netdev(sizeof(struct pan_dev), name, pan_net_setup);
	if (!dev) {
		err = -ENOMEM;
		goto out;
	}

	btdev = dev->priv;
	btdev->net_dev = dev;

	btl_head_init(&btdev->connections);
	init_MUTEX(&btdev->thread_sem);
	btdev->hci = hci;
	btdev->role = role;
	btdev->peer_role = pan_default_peer_role(role);
	btdev->bdaddr = hci->bda;
	btdev->fd = -1;
	btdev->mode = arg->mode & ~AFFIX_PAN_ROLE;

	bda2eth(dev->dev_addr, &btdev->bdaddr);

	atomic_set(&btdev->wmem_alloc, 0);
	btdev->sndbuf = sysctl_pan_sndbuf;

	atomic_set(&btdev->conn_count, 0);

	netif_stop_queue(dev);
	netif_carrier_off(btdev->net_dev);	// default state

	err = register_netdev(dev);	/* register network device */
	if (err) {
		BTERROR("unable to register network device %s\n", dev->name);
		goto out_free;
	}
	BTINFO("PAN net device %s registered, hwaddr: %s\n", dev->name, bda2str(&btdev->bdaddr));
	btl_add_tail(&btdevs, btdev);	/* add to list */
	err = pan_thread_start(btdev);
	if (err) {
		dev_hold(dev);	// requirement for uninit()
		unregister_netdev(dev);
	}
	DBFEXIT;
	return err;
out_free:
	free_netdev(dev);
out:
	hci_put(hci);
	return err;
}

int pan_deliver_event(struct pan_dev *btdev, int event)
{
	struct hci_pan_event	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_PAN_EVENT;
	msg.devnum = btdev->hci->devnum;
	msg.event = event;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int pan_connect_nap(struct pan_connect *arg)
{
	int			err = 0;
	struct pan_dev		*btdev = NULL;
	struct bnep_con		*cl;
	struct sockaddr_affix   *sa = &arg->saddr;
	DECLARE_WAITQUEUE(wait, current);

	DBFENTER;
	
	btdev = pan_lookup_devnum(sa->devnum);
	if (!btdev)
		return -ENODEV;
		
	if (bda_zero(&sa->bda)) {
		pan_close_connections(btdev);
		goto exit;
	}

	/* connecting only if PANU */
	if (btdev->role != AFFIX_PAN_PANU) {
		err = -EINVAL;
		goto exit;
	}
	
	cl = pan_connection_lookup(btdev, &sa->bda);
	if (cl) {
		/* already connected */
		goto exit;
	} else {
		/* disconnect existing */
		pan_close_connections(btdev);
	}
	btdev->peer_role = arg->peer_role & AFFIX_PAN_ROLE;
	DBPRT("connecting to %s ...\n", bda2str(&sa->bda));
	SigConnectCfm = 0;
	err = pan_connect_req(btdev, &sa->bda, BNEPPSM);
	if (err == 0) { /* now wait until connection is confirmed or cancelled */
		DBPRT("waiting for SigConnectCfm\n");
		add_wait_queue(&SigConnectCfm_wait, &wait);
		set_current_state(TASK_INTERRUPTIBLE);
		while(!SigConnectCfm) {
			if (signal_pending(current))
				break;
			schedule();
			set_current_state(TASK_INTERRUPTIBLE);
		}
		set_current_state(TASK_RUNNING);
		remove_wait_queue(&SigConnectCfm_wait, &wait);
		DBPRT("waiting finished\n");
	}
	pan_connections_update(btdev);
	/* determine next connection partner */
	if (SigConnectCfm == SigConnectCfm_Pos) {
		BTINFO("PAN: connected to NAP: %s\n", bda2str(&sa->bda));
		if (!netif_carrier_ok(btdev->net_dev)) {
			netif_carrier_on(btdev->net_dev);
			BTINFO("%s link is on\n", btdev->net_dev->name);
		}
	} else {
		DBPRT("connection failed\n");
		err = -ECONNREFUSED;
	}
exit:
	pan_put(btdev);
	DBFEXIT;
	return err;
}

int pan_ioctl(unsigned int cmd,  unsigned long arg)
{
	int	err = -1;

	switch (cmd) {
		case BTIOC_PAN_INIT:
			err = pan_init((struct pan_init*)arg);
			break;
		case BTIOC_PAN_CONNECT:
			err = pan_connect_nap((struct pan_connect*)arg);
			break;
		default:
			return -ENOIOCTLCMD;
	}
	return err;
}

	
struct affix_pan_operations	pan_ops = {
	owner:	THIS_MODULE,
	ioctl:	pan_ioctl,
};


#ifdef CONFIG_PROC_FS

/* report information when user reads /proc/net/affix/pan */
static int pan_proc_read(char *buf, char **start, off_t offset, int len)
{
	struct pan_dev	*btdev;
	int 		i, count = 0;

	btl_read_lock(&btdevs);
	btl_for_each (btdev, btdevs) {
		/* create ouput */
		/* - role */
		count += sprintf(buf + count, "role: ");
		if (btdev->role == AFFIX_PAN_PANU) {
			count += sprintf(buf + count, "PAN User\n");
		} else if (btdev->role == AFFIX_PAN_NAP) {
			count += sprintf(buf + count, "Network Access Point\n");
		} else
			count += sprintf(buf + count, "Group Ad-hoc Network\n");

		/* - local filters */
		if (btdev->pf.count) {
			count += sprintf(buf + count, "local protocol filter:\n");
			for (i = 0; i < btdev->pf.count; i++)
				count += sprintf(buf + count, "- 0x%04x - 0x%04x\n",
						btdev->pf.protocol[i][0], 
						btdev->pf.protocol[i][1]);
		} else {
			count += sprintf(buf + count, "no local protocol filter\n");
		}
		if (btdev->mf.count) {
			count += sprintf(buf + count, "local multicast filter:\n");
			for (i = 0; i < btdev->mf.count; i++)
				count += sprintf(buf + count, "- %s - %s\n", 
						ETH_ADDR2str(&btdev->mf.multicast[i][0]), 
						ETH_ADDR2str(&btdev->mf.multicast[i][1]));
		} else {
			count += sprintf(buf + count, "no local multicast filter\n");
		}

		/* remote devices */
		if (btdev->connections.len) {
			struct bnep_con *cl;
			count += sprintf(buf + count, "connections:\n");
			btl_read_lock(&btdev->connections);
			btl_for_each (cl, btdev->connections) {
				count += sprintf(buf + count, "- %s\n", bda2str(&cl->ch->bda));

				if (!cl->setup_complete) {
					count += sprintf(buf + count, "  connection is not setup by BNEP\n");
				} else {
					/* - remote filters */
					if (cl->pf.count) {
						count += sprintf(buf + count, "  protocol filter:\n");
						for (i = 0; i < cl->pf.count; i++)
							count += sprintf(buf + count, "  - 0x%04x - 0x%04x\n",
									cl->pf.protocol[i][0],
									cl->pf.protocol[i][1]);
					} else {
						count += sprintf(buf + count, "  no protocol filter\n");
					}
					if (cl->mf.count) {
						count += sprintf(buf + count, "  multicast filter:\n");
						for (i = 0; i < cl->mf.count; i++)
							count += sprintf(buf + count, "  - %s - %s\n", 
									ETH_ADDR2str(&cl->mf.multicast[i][0]),
									ETH_ADDR2str(&cl->mf.multicast[i][1]));
					} else {
						count += sprintf(buf + count, "  no multicast filter\n");
					}
				}
			}
			btl_read_unlock(&btdev->connections);
		} else {
			count += sprintf(buf + count, "no connections\n");
		}
	}
	btl_read_unlock(&btdevs);
	return count;
}

struct proc_dir_entry		*pan_proc;	/* proc file system entry operations */

#endif

int pan_sysctl_register(void);
int pan_sysctl_unregister(void);

int __init init_pan(void)
{
	int	err = -EINVAL;

	DBFENTER;
	
	printk("Affix Bluetooth PAN/BNEP protocol loaded (affix_pan)\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Muller Ulrich <ulrich.muller@nokia.com>\n");
	printk("and Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");

	btl_head_init(&btdevs);
	/* register upper protocol to L2CAP */
	err = l2ca_register_protocol(BNEPPSM, &l2cap_ops);
	if (err < 0) {
		BTERROR("PAN: error registering L2CAP protocol");
		goto err1;
	}
#ifdef CONFIG_PROC_FS
	/* install proc file entry */
	pan_proc = create_proc_info_entry("pan", 0, proc_affix, pan_proc_read);
	if (!pan_proc)
		goto err2;
#endif
#if defined(CONFIG_SYSCTL)
	err = pan_sysctl_register();
	if (err)
		goto err3;
#endif
	affix_register_notifier(&pan_notifier_block);
	affix_set_pan(&pan_ops);

	DBFEXIT;
	return 0; /* success */
err3:
#ifdef CONFIG_PROC_FS
	remove_proc_entry("pan", proc_affix);
err2:
#endif
	l2ca_unregister_protocol(BNEPPSM);
err1:
	BTERROR("init_module failed\n");
	return err; /* error */
}

void __exit exit_pan(void)
{
	struct pan_dev	*btdev;
	
	DBFENTER;
	/* stop devices first */
	while ((btdev = btl_dequeue_head(&btdevs))) {
		dev_hold(btdev->net_dev);	// requirement for uninit()
		unregister_netdev(btdev->net_dev);
	}
	affix_set_pan(NULL);
	affix_unregister_notifier(&pan_notifier_block);
#if defined(CONFIG_SYSCTL)
	pan_sysctl_unregister();
#endif
#ifdef CONFIG_PROC_FS
	remove_proc_entry("pan", proc_affix);
#endif
	l2ca_unregister_protocol(BNEPPSM);
	DBFEXIT;
}

module_init(init_pan);
module_exit(exit_pan);

MODULE_AUTHOR("Muller Ulrich, Dmitry Kasatkin");
MODULE_DESCRIPTION("Affix PAN Profile");
MODULE_LICENSE("GPL");

