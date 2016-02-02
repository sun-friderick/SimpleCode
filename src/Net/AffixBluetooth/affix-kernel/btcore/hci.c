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
   $Id: hci.c,v 1.256 2004/07/22 14:38:03 chineape Exp $

   Host Controller Interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/		

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
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/notifier.h>
#include <linux/poll.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/termios.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <linux/skbuff.h>

#if defined(CONFIG_USB) || defined(CONFIG_USB_MODULE)
#include <linux/usb.h>
#endif

/* Local Includes */
#define FILEBIT	DBHCI

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>

/* Without this we get an unresolved symbol when loading the module */
int				errno;

__u32				affix_dbmask = CONFIG_AFFIX_DBMASK;
struct proc_dir_entry		*proc_affix;
btlist_head_t			hcidevs;
btlist_head_t			hcicons;
/* sysctl */
int				sysctl_hci_allow_promisc = 0;
int				sysctl_hci_use_inquiry = 1;
int				sysctl_hci_max_attempt = 5;
int				sysctl_hci_req_count = 1;
int				sysctl_hci_req_timeout = 5;		//sec
int				sysctl_hci_defer_disc_timeout = 10;	//sec

int				hci_promisc_count = 0;	/* count promisc sockets */
struct notifier_block		*affix_chain;
struct affix_uart_operations	affix_uart_ops;
static hci_struct		*loop;


DECLARE_MUTEX(hcidev_sema);

void hcc_sco_timer(unsigned long p);

/***************************  CONNECTION MANAGEMENT ********************************/

void hcc_timer(unsigned long p)
{
	hci_con *con = (hci_con *)p;
	
	DBFENTER;
	switch (STATE(con)) {
		case CON_OPEN:
			if (atomic_read(&con->refcnt) == 1) {
				lp_disconnect_req(con, 0x13);
			} else {
				if (con->btdev) {
					if (con->btdev->state == BTDEV_AUTHPENDING) {
						con->btdev->state = 0;
						con->btdev->paired = 0;
						notifier_call_chain(&affix_chain, HCICON_AUTH_COMPLETE, con);
					}
				}
			}
			break;
		case CON_W4_CONRSP:
			BTDEBUG("No response from %s, it may be buggy\n", con->hci->name); 
		case CON_W4_LCONREQ:
			hcc_release(con);
			hpf_connect_cfm(con, HCI_ERR_HARDWARE_FAILURE);
			break;
		default:
			break; /* DEAD | CON_W4_DISCRSP  - just release it */
	}
	hcc_put(con);
	DBFEXIT;
}

hci_con *__hcc_lookup_acl(hci_struct *hci, BD_ADDR *bda)
{
	hci_con	*con;

	btl_for_each (con, hcicons) {
		if (STATE(con) != DEAD && STATE(con) != CON_W4_DISCRSP && __is_acl(con) &&
				(!hci || con->hci == hci) && !memcmp(&con->bda, bda, 6))
				return con;
	}
	return NULL;
}

hci_con *hcc_lookup_acl(hci_struct *hci, BD_ADDR *bda)
{
	hci_con		*con;

	DBFENTER;
	btl_read_lock(&hcicons);
	con = __hcc_lookup_acl(hci, bda);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return con;
}

hci_con *__hcc_lookup_sco(hci_struct *hci, BD_ADDR *bda)
{
	hci_con	*con;

	btl_for_each (con, hcicons) {
		if (STATE(con) != DEAD && STATE(con) != CON_W4_DISCRSP && !__is_acl(con) &&
				con->hci == hci && !memcmp(&con->bda, bda, 6))
				return con;
	}
	return NULL;
}

hci_con *hcc_lookup_sco(hci_struct *hci, BD_ADDR *bda)
{
	hci_con		*con;

	DBFENTER;
	btl_read_lock(&hcicons);
	con = __hcc_lookup_sco(hci, bda);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return con;
}


hci_con *__hcc_lookup_chandle(hci_struct *hci, __u16 chandle)
{
	hci_con	*con;
	
	btl_for_each (con, hcicons) {
		if (STATE(con) != DEAD &&
				con->hci == hci && con->chandle == chandle)
				return con;
	}
	return NULL;
}

hci_con *hcc_lookup_chandle(hci_struct *hci, __u16 chandle)
{
	hci_con		*con;

	DBFENTER;
	btl_read_lock(&hcicons);
	con = __hcc_lookup_chandle(hci, chandle);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return con;
}

hci_con *__hcc_lookup_chandle_devnum(int devnum, __u16 chandle)
{
	hci_con	*con;
	
	btl_for_each (con, hcicons) {
		if (STATE(con) != DEAD)
			if ((con->hci && con->hci->devnum == devnum) && con->chandle == chandle)
			break;
	}
	return con;
}

hci_con *hcc_lookup_chandle_devnum(int devnum, __u16 chandle)
{
	hci_con		*con;

	DBFENTER;
	btl_read_lock(&hcicons);
	con = __hcc_lookup_chandle_devnum(devnum, chandle);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return con;
}

#if 0

hci_con *__hcc_lookup_hdbda(int devnum, BD_ADDR *bda)
{
	hci_con	*con;

	btl_for_each (con, hcicons) {
		if (STATE(con) != DEAD && STATE(con) != CON_W4_DISCRSP)
			if ((con->hci && con->hci->devnum == devnum) && memcmp(&con->bda, bda, 6) == 0)
			break;
	}
	return con;
}

hci_con *hcc_lookup_hdbda(int devnum, BD_ADDR *bda)
{
	hci_con		*con;

	DBFENTER;
	btl_read_lock(&hcicons);
	con = __hcc_lookup_hdbda(devnum, bda);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return con;
}

#endif

hci_con *__hcc_lookup_id(int id)
{
	hci_con	*con;

	btl_for_each (con, hcicons)
		if (STATE(con) != DEAD && con->id == id)
			return con;
	return NULL;
}

hci_con *hcc_lookup_id(int id)
{
	hci_con		*con;

	DBFENTER;

	btl_read_lock(&hcicons);
	con = __hcc_lookup_id(id);
	if (con)
		hcc_hold(con);
	btl_read_unlock(&hcicons);

	DBFEXIT;
	return con;
}


void hcc_bind(hci_con *con, hci_struct *hci)
{
	hci_hold(hci);
	con->hci = hci;
	con->mtu = __is_acl(con) ? hci->acl_mtu : hci->sco_mtu;
}
	
hci_con *hcc_alloc(void)
{
	hci_con		*con = NULL;
	static int	hcc_desc = 1;
	
	DBFENTER;
	con = kmalloc(sizeof(hci_con), GFP_ATOMIC);
	if (!con)
		return NULL;
	memset(con, 0, sizeof(hci_con));

	con->id = hcc_desc++;
	atomic_set(&con->refcnt, 1);
	set_bit(HCI_FLAGS_INITIATOR, &con->flags);

	/* by default it's an ACL connection */
	con->attempt = 0;
	con->link_type = HCI_LT_ACL;

	skb_queue_head_init(&con->tx_queue);

	init_timer(&con->timer);
	con->timer.data = (unsigned long)con;
	con->timer.function = hcc_timer;

#if defined(CONFIG_AFFIX_SCO)
	btl_head_init(&con->sco_pending);
	init_timer(&con->sco_timer);
	con->sco_timer.data = (unsigned long)con;
	con->sco_timer.function = hcc_sco_timer;
#endif
	DBFEXIT;
	return con;
}

/*
    Create new connection object. In fact should work atomicaly
    If the object exists it returns existing
*/
hci_con *hcc_create(hci_struct *hci, BD_ADDR *bda)
{
	hci_con	*con;

	DBFENTER;

	btl_write_lock(&hcicons);
	con = __hcc_lookup_acl(hci, bda);
	if (!con) {
		con = hcc_alloc();
		if (!con)
			goto exit;
		con->btdev = btdev_create(bda);
		if (!con->btdev) {
			kfree(con);
			con = NULL;
			goto exit;
		}
		memcpy(&con->bda, bda, 6);
		if (hci)
			hcc_bind(con, hci);
		ENTERSTATE(con, CON_CLOSED);	/* SET initial state */
		__btl_add_tail(&hcicons, con);
	} else
		hcc_hold(con);	/* inc refcnt */
exit:
	btl_write_unlock(&hcicons);

	DBFEXIT;
	return con;
}

hci_con *hcc_create_sco(hci_struct *hci, BD_ADDR *bda)
{
	hci_con		*con;

	DBFENTER;

	btl_write_lock(&hcicons);
	con = hcc_alloc();
	if (con) {
		con->link_type = HCI_LT_SCO;
		con->btdev = btdev_create(bda);
		if (!con->btdev) {
			kfree(con);
			con = NULL;
			goto exit;
		}
		memcpy(&con->bda, bda, 6);
		if (hci)
			hcc_bind(con, hci);
		ENTERSTATE(con, CON_CLOSED);	/* SET initial state */
		__btl_add_tail(&hcicons, con);
	}
exit:
	btl_write_unlock(&hcicons);

	DBFEXIT;
	return con;
}


void __hcc_destroy(hci_con *con)
{
	DBFENTER;
	__btl_unlink(&hcicons, con);
	skb_queue_purge(&con->tx_queue);
	if (con->rx_skb)
		kfree_skb(con->rx_skb);
	if (con->hci)
		hci_put(con->hci);
	if (con->btdev)
		btdev_put(con->btdev);
	kfree(con);
	DBFEXIT;
}

void hcc_put(hci_con *con)
{
	DBFENTER;
	if (__hcc_linkup(con) && atomic_read(&con->refcnt) == 1) {
		if (__is_acl(con))
			hcc_start_timer(con, sysctl_hci_defer_disc_timeout * HZ);
		else
			lp_disconnect_req(con, 0x13);
	}
	if (atomic_dec_and_test(&con->refcnt)) {
		btl_write_lock(&hcicons);
		if (atomic_read(&con->refcnt) == 0)
			__hcc_destroy(con);
		btl_write_unlock(&hcicons);
	}
	DBFEXIT;
}

void hcc_close(hci_con *con)
{
	DBFENTER;
	if (!__is_acl(con)) {
		// SCO
		lp_disconnect_req(con, 0x13);
	}
	hcc_put(con);
	DBFEXIT;
}

int hcc_release(hci_con *con)
{
	int	state;
	
	state = SETSTATE(con, DEAD);
	hcc_stop_timer(con);
#if defined(CONFIG_AFFIX_SCO)
	if (del_timer(&con->sco_timer))
		hcc_put(con);
	btl_purge(&con->sco_pending);
#endif
	return state;
}

/*************************   ---- HCI ----    *******************************/


/*
 * destruct HCI object
 */
void hci_destroy(hci_struct *hci)
{
	DBFENTER;
	btl_write_lock(&hcidevs);
	if (atomic_read(&hci->refcnt) == 0) {
		if (!hci->deadbeaf) {
			BTERROR("Freeing alive device %p, %s\n", hci, hci->name);
			goto exit;
		}
		__btl_unlink(&hcidevs, hci);
		kfree(hci);
	}
exit:
	btl_write_unlock(&hcidevs);
	DBFEXIT;
}

/*
  search for a hci devices
*/
hci_struct *__hci_lookup_bda(BD_ADDR *bda)
{
	hci_struct	*hci;

	DBFENTER;
	btl_for_each (hci, hcidevs) {
		if (hci_running(hci) && memcmp(&hci->bda, bda, 6) == 0)
			return hci;
	}
	DBFEXIT;
	return NULL;
}

hci_struct *hci_lookup_bda(BD_ADDR *bda)
{
	hci_struct	*hci;

	DBFENTER;
	btl_read_lock(&hcidevs);
	hci = __hci_lookup_bda(bda);
	if (hci)
		hci_hold(hci);
	btl_read_unlock(&hcidevs);
	DBFEXIT;
	return hci;
}

hci_struct *__hci_lookup_devnum(int devnum)
{
	hci_struct	*hci;

	DBFENTER;
	btl_for_each (hci, hcidevs) {
		if (hci_running(hci) && hci->devnum == devnum)
			return hci;
	}
	DBFEXIT;
	return NULL;
}

hci_struct *hci_lookup_devnum(int devnum)
{
	hci_struct	*hci;

	DBFENTER;
	btl_read_lock(&hcidevs);
	hci = __hci_lookup_devnum(devnum);
	if (hci)
		hci_hold(hci);
	btl_read_unlock(&hcidevs);
	DBFEXIT;
	return hci;
}

hci_struct *__hci_lookup_name(char *name)
{
	hci_struct	*hci;

	DBFENTER;
	btl_for_each (hci, hcidevs) {
		if (hci_running(hci) && (strncmp(hci->name, name, IFNAMSIZ) == 0))
			return hci;
	}
	DBFEXIT;
	return NULL;
}

hci_struct *hci_lookup_name(char *name)
{
	hci_struct	*hci;

	DBFENTER;
	btl_read_lock(&hcidevs);
	hci = __hci_lookup_name(name);
	if (hci)
		hci_hold(hci);
	btl_read_unlock(&hcidevs);
	DBFEXIT;
	return hci;
}


/*
  search for any hci devices
*/
hci_struct *hci_select(void)
{
	hci_struct	*hci;

	DBFENTER;
	btl_read_lock(&hcidevs);
	btl_for_each (hci, hcidevs) {
		if (hci_running(hci)) {
			hci_hold(hci);
			break;
		}
	}
	btl_read_unlock(&hcidevs);
	DBFEXIT;
	return hci;
}


/*
    **************       HCI EVENT SUBSYSTEM            *****************
*/

/*
    Check Number of Completed Packets event 
*/
int event_number_completed_packets(hci_struct *hci, struct sk_buff *skb)
{
	struct Number_Of_Completed_Packets	*event = (void*)skb->data;
	__u8		num = event->Number_Of_Handles;
	int		i;
	__u16		pnum;
	hci_con		*con;

	DBFENTER;
	for (i = 0; i < num; i++) {
		con = hcc_lookup_chandle(hci, HCI_HANDLE(event->Result[i].Connection_Handle));
		if (con) {
			pnum = __btoh16(event->Result[i].HC_Number_Of_Completed_Packets);
			atomic_sub(pnum, &con->pending);
			atomic_add(pnum, __is_acl(con) ? &hci->acl_count : &hci->sco_count);
			hcc_put(con);
			DBPRT("Completed %d packets\n", pnum);
		} else {
			DBPRT("Connection does not exist\n");
		}
		DBPRT("FREE SPACE: acl - %d, sco - %d\n", 
		atomic_read(&hci->acl_count), atomic_read(&hci->sco_count));
	}	
	DBFEXIT;
	return 0;
}

int event_flush_occurred(hci_struct *hci, struct sk_buff *skb)
{
	struct Flush_Occured_Event	*event = (void*)skb->data;
	hci_con				*con;
		
	DBFENTER;
	
	con = hcc_lookup_chandle(hci, HCI_HANDLE(event->Connection_Handle));
	if (!con) {
		DBPRT("Unknown connection !!!\n");
		return 0;
	}
	atomic_add(atomic_read(&con->pending), __is_acl(con) ? &hci->acl_count : &hci->sco_count);
	atomic_set(&con->pending, 0);	/* we can send new */
	hcc_put(con);
	DBFEXIT;
	return 0;
}

int event_connection_request(hci_struct *hci, struct sk_buff *skb)
{
	struct Connection_Request_Event	*event = (void*)skb->data;
	hci_con		*con;
	int		ret = 0;

	DBFENTER;
	
	if (event->Link_Type == HCI_LT_ACL)
		con = hcc_create(hci, &event->bda);
	else
		con = hcc_create_sco(hci, &event->bda);

	if (!con) {
		return -ENOMEM;
	}
	clear_bit(HCI_FLAGS_INITIATOR, &con->flags);
	ENTERSTATE(con, CON_W4_LCONRSP);
	/* start timer to prevent stale connections - accept timeout */
	hcc_start_timer(con, HCC_CONN_TIMEOUT);
	hcc_put(con);
	
	DBFEXIT;
	return ret;
}

int event_connection_complete(hci_struct *hci, struct sk_buff *skb)
{
	struct Connection_Complete_Event	*evt = (void*)skb->data;
	hci_con		*con;
	int		ret = 0;

	DBFENTER;
	DBPRT("Connection Complete, Status: 0x%02x, handle: %d, type: %s!!!\n",
			evt->Status, evt->Connection_Handle, (evt->Link_Type == HCI_LT_ACL)?"ACL":"SCO");

	if (evt->Link_Type == HCI_LT_SCO) {
		con = hcc_lookup_sco(hci, &evt->bda);
	} else {
		con = hcc_lookup_acl(hci, &evt->bda);
	}
	if (!con) {
		DBPRT("Unknown connection!!!\n");
		return -ENOTCONN;
	}
	/*
	 * check the state ??? CON_W4_CONRSP
	 */
	hcc_stop_timer(con);
	if (evt->Status) {
		if (test_bit(HCI_FLAGS_INITIATOR, &con->flags)) {
			if (++con->attempt < sysctl_hci_max_attempt) {
				// try again
				DBPRT("connection failed, try again...: %d\n", con->attempt);
				ENTERSTATE(con, CON_W4_LCONREQ);
				hci_connect_req(con);
				/* start timer here to prevent stale connections */
				hcc_start_timer(con, HCC_CONN_TIMEOUT);
				goto exit;
			}
		}
		hcc_release(con);
		hpf_connect_cfm(con, evt->Status);
		goto exit;
	}

#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
	// ACL connection successful; update the clock offset
	if (evt->Link_Type == HCI_LT_ACL)
		hci_updateclockoffset_req(con,con->chandle);
#endif
			
	con->btdev->paired = 0;		// non paired
	con->chandle = HCI_HANDLE(evt->Connection_Handle);
	con->encryp_mode = evt->Encryption_Mode;
	ENTERSTATE(con, CON_OPEN);
	hpf_connect_cfm(con, 0);
exit:
	hcc_put(con);
	DBFEXIT;
	return ret;
}

int event_disconnection_complete(hci_struct *hci, struct sk_buff *skb)
{
	hci_con		*con;
	struct Disconnection_Complete_Event	*evt = (void*)skb->data;
	int		ret = 0;

	DBFENTER;
	con = hcc_lookup_chandle(hci, HCI_HANDLE(evt->Connection_Handle));
	if (!con) { 
		DBPRT("Unknown connection!!!\n");
		return -ENOTCONN;
	}
	hcc_release(con);
	atomic_add(atomic_read(&con->pending), __is_acl(con) ? &hci->acl_count : &hci->sco_count);
	atomic_set(&con->pending, 0);
	hpf_disconnect_ind(con);
	hcc_put(con);
	DBFEXIT;
	return ret;
}

int event_packet_type_changed(hci_struct *hci, struct sk_buff *skb)
{
	struct Connection_Packet_Type_Changed_Event	*event = (void*)skb->data;
	hci_con		*con;
		
	DBFENTER;
	
	con = hcc_lookup_chandle(hci, HCI_HANDLE(event->Connection_Handle));
	if (!con)
		return 0;
		
	if (event->Status)
		goto exit;
	
	con->pkt_type = event->Packet_Type;
	DBPRT("Packet type: %x\n", con->pkt_type);
exit:
	hcc_put(con);
	DBFEXIT;
	return 0;
}

static inline int event_command_complete(hci_struct *hci, struct sk_buff *skb)
{
	struct Command_Complete_Event	*c = (void*)skb->data;
	
	atomic_set(&hci->cmd_count, c->Num_HCI_Command_Packets);
	return 0;
}

static inline int event_command_status(hci_struct *hci, struct sk_buff *skb)
{
	struct Command_Status_Event	*cs = (void*)skb->data;

	DBFENTER;
	atomic_set(&hci->cmd_count, cs->Num_HCI_Command_Packets);
	DBFEXIT;
	return 0;
}

int event_link_key_notification(hci_struct *hci, struct sk_buff *skb)
{
	struct Link_Key_Notification_Event	*evt = (void*)skb->data;
	struct btdev	*btdev;

	DBFENTER;

	btdev = btdev_create(&evt->bda);
	if (btdev == NULL)
		return -ENOMEM;
	btdev->Key_Type = evt->Key_Type;
	memcpy(btdev->Link_Key, evt->Link_Key, 16);
	btdev->flags |= NBT_KEY;
	btdev_put(btdev);
	
	DBFEXIT;
	return 0;
}

int event_inquiry_result(hci_struct *hci, struct sk_buff *skb)
{
	struct Inquiry_Result_Event	*evt = (void*)skb->data;
	int		i;
	struct btdev	*btdev;

	DBFENTER;

	for (i = 0; i < evt->Num_Responses; i++) {
		INQUIRY_ITEM	*item = &evt->Results[i];
		btdev = btdev_create(&item->bda);
		if (btdev == NULL)
			return -ENOMEM;
		/* set inquiry results */
		btdev->stamp = jiffies;
		btdev->PS_Repetition_Mode = item->PS_Repetition_Mode;
		btdev->PS_Period_Mode = item->PS_Period_Mode;
		btdev->PS_Mode = item->PS_Mode;
		btdev->Class_of_Device = __btoh24(item->Class_of_Device);
		btdev->Clock_Offset = __btoh16(item->Clock_Offset);
		btdev->flags |= NBT_INQUIRY;
		btdev_put(btdev);
	}

	DBFEXIT;
	return 0;
}

int event_authentication_complete(hci_struct *hci, struct sk_buff *skb)
{
	struct Authentication_Complete_Event	*evt = (void*)skb->data;
	hci_con			*con;

	DBFENTER;

	con = hcc_lookup_chandle(hci, evt->Connection_Handle);
	if (!con) {
		return 0;
	}
	if (!con->btdev) {
		hcc_put(con);
		return -ENOTCONN;
	}
	con->btdev->state = 0;
	if (evt->Status == 0)
		con->btdev->paired = 1;
	else
		con->btdev->paired = 0;

	notifier_call_chain(&affix_chain, HCICON_AUTH_COMPLETE, con);
	hcc_put(con);

	DBFEXIT;
	return 0;
}

int event_qos_violation(hci_struct *hci, struct sk_buff *skb)
{
	struct QoS_Violation_Event	*evt = (void*)skb->data;
	hci_con				*con;

	DBFENTER;
	con = hcc_lookup_chandle(hci, evt->Connection_Handle);
	if (!con) {
		return 0;
	}
	lp_qos_violation_ind(con);
	hcc_put(con);
	DBFEXIT;
	return 0;
}



/*
 * events goes from BH only
 */
int hci_receive_event(hci_struct *hci, struct sk_buff *skb)
{
	HCI_Event_Packet_Header	*hdr = (HCI_Event_Packet_Header*)skb->data;
	__u8		event = hdr->EventCode;
	int		err = 0;
	int		flag = 0;

	DBFENTER;

	if (skb->len < HCI_EVENT_HDR_LEN && skb->len < (hdr->Length + HCI_EVENT_HDR_LEN)) {
		BTDEBUG("EVENT packet too small, drop it...\n");
		kfree_skb(skb);
		return -EINVAL;
	}

	DBPRT("HCI event received: 0x%02X\n", event);
	DBPARSEHCI(skb->data, skb->len, HCI_EVENT, FROM_HOSTCTRL);	
	DBDUMP(skb->data, skb->len);

	/* process events */
	switch (event) {
		case HCI_E_COMMAND_COMPLETE:
			event_command_complete(hci, skb);
			break;
		case HCI_E_COMMAND_STATUS:
			event_command_status(hci, skb);
			break;
		case HCI_E_NUMBER_COMPLETED_PACKETS:
			event_number_completed_packets(hci, skb);
			flag = 1;
			break;
		case HCI_E_FLUSH_OCCURRED:
			event_flush_occurred(hci, skb);
			flag = 1;
			break;
		case HCI_E_CONNECTION_REQUEST:
			event_connection_request(hci, skb);
			break;
		case HCI_E_CONNECTION_COMPLETE:
			event_connection_complete(hci, skb);
			flag = 1;	// l2cap signals pending
			break;
		case HCI_E_DISCONNECTION_COMPLETE:
			event_disconnection_complete(hci, skb);
			break;
		case HCI_E_CONNECTION_PACKET_TYPE_CHANGED:
			event_packet_type_changed(hci, skb);
			break;
		case HCI_E_LINK_KEY_NOTIFICATION:
			event_link_key_notification(hci, skb);
			break;
		case HCI_E_INQUIRY_RESULT:
			event_inquiry_result(hci, skb);
			break;
		case HCI_E_AUTHENTICATION_COMPLETE:
			event_authentication_complete(hci, skb);
			break;
		case HCI_E_QOS_VIOLATION:
			event_qos_violation(hci, skb);
			break;
		default:
			break;
	}
	/* Wakeup Packet Scheduler if transmit is possible ... */
	if (flag)
		hcidev_schedule(hci);
	hpf_recv_raw(hci, NULL, skb);
	DBFEXIT;
	return err;
}

/*
    Receive acl packet..
*/
int hci_receive_acl(hci_struct *hci, struct sk_buff *skb)
{
	hci_con			*con;
	HCI_ACL_Packet_Header	*hdr = (void*)skb->data;

	DBFENTER;
	DBPRT("HCI ACL packet received\n");
	DBPARSEHCI(skb->data, skb->len, HCI_ACL, FROM_HOSTCTRL);	
	DBDUMP(skb->data, skb->len);

	if (skb->len < HCI_ACL_HDR_LEN && skb->len < (__btoh16(hdr->Length) + HCI_ACL_HDR_LEN)) {
		BTDEBUG("ACL packet too small, drop it...\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	con = hcc_lookup_chandle(hci, HCI_HANDLE(hdr->Connection_Handle));
	if (!con) {	/* No resources available	*/
		DBPRT("Unknown connection\n");
#ifdef CONFIG_AFFIX_DTL1_FIX
		if (hci->fixit) {
			if (!hci->delayed_acl_skb) {
				BTDEBUG("Delay ACL packet.\n");
				skb_unlink(skb);
				hci->delayed_acl_skb = skb;
				return -EEXIST;
			} else {
				BTDEBUG("Delayed ACL packet already buffered.\n");
				if (hci->delayed_acl_skb != skb)
					kfree_skb(hci->delayed_acl_skb);
				hci->delayed_acl_skb = NULL;
			}
		}
#endif	/* CONFIG_AFFIX_DTL1_FIX */
		kfree_skb(skb);
		return -EINVAL;
	}
#ifdef CONFIG_AFFIX_DTL1_FIX
	if (hci->fixit) {
		if (hci->delayed_acl_skb && hci->delayed_acl_skb != skb) {
			BTDEBUG("Stale delayed ACL packet.\n");
			kfree_skb(hci->delayed_acl_skb);
		}
		hci->delayed_acl_skb = NULL;
	}
#endif /* CONFIG_AFFIX_DTL1_FIX */
	hpf_recv_raw(hci, con, skb);
	hcc_put(con);
	DBFEXIT;
	return 0;
}

/*
    Receive sco packet..
*/
int hci_receive_sco(hci_struct *hci, struct sk_buff *skb)
{
	hci_con			*con;
	HCI_SCO_Packet_Header	*hdr = (void*)skb->data;

	DBFENTER;
	DBPRT("HCI SCO packet received\n");
	DBPARSEHCI(skb->data, skb->len, HCI_SCO, FROM_HOSTCTRL);	
	DBDUMP(skb->data, skb->len);

	if (skb->len < hdr->Length + HCI_SCO_HDR_LEN) {
		kfree_skb(skb);
		return -EINVAL;
	}
	con = hcc_lookup_chandle(hci, HCI_HANDLE(hdr->Connection_Handle));
	if (!con) {
		DBPRT("Unknown connection\n");
		kfree_skb(skb);
		return -EINVAL;
	}
	hpf_recv_raw(hci, con, skb);
	hcc_put(con);
	DBFEXIT;
	return 0;
}


void hci_receive_data(hci_struct *hci, struct sk_buff *skb)
{
	DBFENTER;
	hci->stats.rx_bytes += skb->len;
	switch (skb->pkt_type) {
		case HCI_ACL:
			hci->stats.rx_acl++;
			hci_receive_acl(hci, skb);
			break;
		case HCI_SCO:
			hci->stats.rx_sco++;
			hci_receive_sco(hci, skb);
			break;
		case HCI_EVENT:
			hci->stats.rx_event++;
			hci_receive_event(hci, skb);
#ifdef CONFIG_AFFIX_DTL1_FIX
			if (hci->fixit) {
				if (hci->delayed_acl_skb) {
					BTDEBUG("Trying to process delayed ACL packet...\n");
					hci_receive_acl(hci, hci->delayed_acl_skb);
				}
			}
#endif
			break;
		default:
			DBPRT("Wrong HCI pkt_type: %x\n", skb->pkt_type);
			kfree_skb(skb);
	}
	DBFEXIT;
}

/*
 * **************       HCI COMMAND SUBSYSTEM            *****************
 */

#ifdef CONFIG_AFFIX_HCI_BROADCAST
int lp_connect_broadcast(hci_struct *hci, BD_ADDR *bda, hci_con **ret,hci_struct **selhci)
{
	hci_con		*con;
	int		err = 0;
	hci_struct 	*tmphci = NULL;
		
	DBFENTER;

	tmphci = hci_select();
	if (!tmphci)
		return -ENOMEM;

	con = hcc_create(tmphci, bda);
	if (!con) {
		hci_put(tmphci);
		return -ENOMEM;
	}

	con->chandle = HCI_BROADCAST_CHANDLE;	// my broadcast handle
	
	ENTERSTATE(con, CON_OPEN);

	*ret = con;
	*selhci = tmphci;

	/* select device */
	hci_put(tmphci);
	
	DBFEXIT;
	return err;
}
#endif

int lp_connect_req(hci_struct *hci, BD_ADDR *bda, hci_con **ret)
{
	hci_con		*con;
	int		err = 0;

	DBFENTER;
	con = hcc_create(hci, bda);
	if (!con)
		return -ENOMEM;
	if (bda_zero(bda)) {
		/* loop */
		if (!con->hci) {
			con->hci = loop;
			hci_hold(loop);
		}
		ENTERSTATE(con, CON_OPEN);
		*ret = con;
		return 0;
	}
	DBPRT("Connection State: %d\n", con->state);
	if (STATE(con) == CON_W4_LDISCREQ)
		ENTERSTATE(con, CON_OPEN);
	else if (STATE(con) == CON_CLOSED) {
		ENTERSTATE(con, CON_W4_LCONREQ);
		err = hci_connect_req(con);
		/* start timer here to prevent stale connections */
		hcc_start_timer(con, HCC_CONN_TIMEOUT);
	}
	if (*ret)
		hcc_put(*ret);
	*ret = con;
	DBFEXIT;
	return err;
}

int lp_add_sco(hci_struct *hci, BD_ADDR *bda, hci_con **ret)
{
	hci_con		*con;
	int		err = 0;

	DBFENTER;
	con = hcc_create_sco(hci, bda);
	if (!con) {
		return -ENOMEM;
	}
	if (STATE(con) == CON_CLOSED) {
		ENTERSTATE(con, CON_W4_LCONREQ);
		err = hci_connect_req(con);
		/* start timer here to prevent stale connections */
		hcc_start_timer(con, HCC_CONN_TIMEOUT);
	}
	*ret = con;
	DBFEXIT;
	return err;
}


int lp_disconnect_req(hci_con *con, int reason)
{
	int	err = 0;

	DBFENTER;
	
	if (bda_zero(&con->bda)) {
#if 0 //def CONFIG_AFFIX_HCI_BROADCAST
		if (atomic_dec_and_test(&con->refcnt)) {
			btl_write_lock(&hcicons);
			if (atomic_read(&con->refcnt) == 0)
				__hcc_destroy(con);
			btl_write_unlock(&hcicons);
		}
#endif
		ENTERSTATE(con, CON_W4_LDISCREQ);
		return 0;
	}
	
	if (__hcc_linkup(con)) {
		ENTERSTATE(con, CON_W4_LDISCREQ);
		err = hci_disconnect_req(con, reason);
		/* start timer here to prevent stale connections */
		hcc_start_timer(con, HCC_DISC_TIMEOUT);
	}
	DBFEXIT;
	return err;
}

int lp_auth_req(hci_con *con)
{
	int	err = 0;

	DBFENTER;
	con->btdev->state = BTDEV_AUTHPENDING;
	hci_auth_req(con);
	hcc_start_timer(con, HCC_AUTH_TIMEOUT);
	DBFEXIT;
	return err;
}

int lp_send_data(hci_con *con, struct sk_buff *skb)
{
	int	err;
	
	DBFENTER;
	if (bda_zero(&con->bda)) {
		struct sk_buff	*new_skb;

		new_skb = skb_clone(skb, GFP_ATOMIC);
		kfree_skb(skb);
		if (!new_skb)
			return 0;
		lp_receive_data(con, 1, 0, new_skb);
		return 0;
	}
	if (__is_acl(con)) {
		skb->pkt_type = HCI_ACL;
		hci_skb(skb)->pb = HCI_PB_FIRST;
	} else {
		skb->pkt_type = HCI_SCO;
	}
	err = hci_queue_xmit(con->hci, con, skb);
	DBFEXIT;
	return err;
}

#ifdef CONFIG_AFFIX_HCI_BROADCAST
int lp_broadcast_data(hci_con *con, struct sk_buff *skb)
{
	int	err = 0;
	
	DBFENTER;
	if (__is_acl(con)) {	/* We need to check this? */
		skb->pkt_type = HCI_ACL;
		hci_skb(skb)->pb = HCI_PB_FIRST;
		err = hci_queue_xmit(con->hci, con, skb);
	}
	DBFEXIT;
	return err;
}
#endif

/************************ COMMAND LOCK MANAGEMENT ******************************/

/*
  create new HCI lock for specified command
*/
hci_lock_t *hci_lock_create(hci_struct *hci, __u16 opcode)
{
	hci_lock_t	*lock;

	btl_write_lock(&hci->hci_locks);	/* lock */
	btl_for_each (lock, hci->hci_locks)
		if (lock->opcode == opcode)
			break;
	if (!lock)  {
		lock = kmalloc(sizeof(*lock), GFP_ATOMIC);
		if (lock) {
			memset(lock, 0, sizeof(*lock));
			atomic_set(&lock->refcnt, 0);
			init_MUTEX(&lock->sema);
			lock->hci = hci;
			lock->opcode = opcode;
			__btl_add_tail(&hci->hci_locks, lock);
		}
	}
	if (lock)
		atomic_inc(&lock->refcnt);	/* use it */
	btl_write_unlock(&hci->hci_locks);	/* unlock */
	return lock;
}

void hci_lock_destroy(hci_lock_t *lock)
{
	hci_struct	*hci = lock->hci;

	btl_write_lock(&hci->hci_locks);	/* lock */
	if (atomic_read(&lock->refcnt) == 0) {
		__btl_unlink(&hci->hci_locks, lock);
		kfree(lock);
	}
	btl_write_unlock(&hci->hci_locks);	/* unlock */
}


/*
    Bluetooth network device library
*/

void hci_rx_task(unsigned long arg)
{
	hci_struct	*hci = (hci_struct *)arg;
	struct sk_buff	*skb;

	DBFENTER;
	while ((skb = skb_dequeue(&hci->rx_queue)))
		hci_receive_data(hci, skb);
	DBFEXIT;
}

void hci_close_con(hci_struct *hci)
{
	hci_con		*con, *next;
	int		state;

	DBFENTER;
	btl_write_lock(&hcicons);
	btl_for_each_safe (con, hcicons, next) {
		if (__is_dead(con) || con->hci != hci)
			continue;
		hcc_hold(con);
		state = hcc_release(con);
		switch (state) {
			case CON_OPEN:
				hpf_disconnect_ind(con);
				break;
			default:
				hpf_connect_cfm(con, HCI_ERR_HARDWARE_FAILURE);
		}
		__hcc_put(con);
	}
	btl_write_unlock(&hcicons);
	DBFEXIT;
}


void hcidev_state_change(hci_struct *hci, int event)
{
	if (hci->type == HCI_LOOP)
		return;
	/* pass to kernel */
	notifier_call_chain(&affix_chain, event, hci);
	/* local chain handler */
	switch (event) {
		case HCIDEV_REGISTER:
			hci_run_hotplug(hci, "register");
			break;
		case HCIDEV_UNREGISTER:
			hci_run_hotplug(hci, "unregister");
			break;
		case HCIDEV_ATTACH:
			hci_run_hotplug(hci, "attach");
			break;
		case HCIDEV_DETACH:
			hcidev_close(hci);
			hci_run_hotplug(hci, "detach");
			break;
		case HCIDEV_UP:
			hci_run_hotplug(hci, "up");
			break;
		case HCIDEV_DOWN:
			hci_run_hotplug(hci, "down");
			break;
		default:
			break;
	}
	/* pass to processes */
	hci_state_change(hci, event);
}

int __hcidev_ioctl(hci_struct *hci, int cmd, void *arg)
{
	if (hci->ioctl)
		return hci->ioctl(hci, cmd, arg);
	return -ENOIOCTLCMD;
}

int hcidev_ioctl(hci_struct *hci, int cmd, void *arg)
{
	int	err;
	hcidev_lock();
	err = __hcidev_ioctl(hci, cmd, arg);
	hcidev_unlock();
	return err;
}
	
int __hcidev_open(hci_struct *hci)
{
	int	err = 0;

	DBFENTER;
	if (test_and_set_bit(HCIDEV_STATE_RUNNING, &hci->state))
		return 0;
	if (!hcidev_present(hci)) {
		clear_bit(HCIDEV_STATE_RUNNING, &hci->state);
		return -ENODEV;
	}
	if (try_module_get(hci->owner)) {
		if (hci->open) {
			err = hci->open(hci);
			if (err != 0) {
                                module_put(hci->owner);
			}
		}
	} else
		err = -ENODEV;
	if (err) {
		clear_bit(HCIDEV_STATE_RUNNING, &hci->state);
		return err;
	}
	atomic_set(&hci->cmd_count, 1);
	hci->flags = 0;
	//DBPRT("Bluetooth device up\n");
	BTDEBUG("Bluetooth device opened: %s\n", hci->name);
	DBFEXIT;
	return 0;
}

int hcidev_open(hci_struct *hci)
{
	int	err;
	hcidev_lock();
	err = __hcidev_open(hci);
	hcidev_unlock();
	return err;
}

void __hcidev_close(hci_struct *hci)
{
	DBFENTER;
	if (!test_and_clear_bit(HCIDEV_STATE_RUNNING, &hci->state))
		return;
	set_bit(HCIDEV_STATE_XOFF, &hci->state);
	/* cleanup */
	hci->flags = 0;
	tasklet_kill(&hci->rx_task);		/* SCHED_YIELD */
	tasklet_kill(&hci->tx_task);
	spin_unlock_wait(&hci->xmit_lock);	/* unlock xmit_lock */
	if (hci->close)
		hci->close(hci);
	hci_close_con(hci);
	skb_queue_purge(&hci->cmd_queue);
	skb_queue_purge(&hci->acl_queue);
#if defined(CONFIG_AFFIX_SCO)
	skb_queue_purge(&hci->sco_queue);
#endif
	skb_queue_purge(&hci->rx_queue);
	hcidev_state_change(hci, HCIDEV_DOWN);
	//DBPRT("Bluetooth device down\n");
	BTDEBUG("Bluetooth device closed: %s\n", hci->name);
        module_put(hci->owner);
	DBFEXIT;
}

void hcidev_close(hci_struct *hci)
{
	hcidev_lock();
	__hcidev_close(hci);
	hcidev_unlock();
}

int hcidev_alloc_name(hci_struct *hci)
{
	int		i;
	char		name[IFNAMSIZ];
	char		*p;
	hci_struct	*h;

	DBFENTER;
	p = strchr(hci->name, '%');
	if (!p)
		return 0;

	if ((p[1] != 'd' || strchr(p+2, '%')))
		return -EINVAL;
	/*
	 * If you need over 100 please also fix the algorithm...
	 */
	btl_read_lock(&hcidevs);
	for (i = 0; i < 100; i++) {
		snprintf(name, sizeof(name), hci->name, i);
		btl_for_each (h, hcidevs) {
			if (!h->deadbeaf && strncmp(h->name, name, IFNAMSIZ) == 0)
				break;
		}
		if (!h) {
			strcpy(hci->name, name);
			btl_read_unlock(&hcidevs);
			return i;
		}
	}
	btl_read_unlock(&hcidevs);
	return -ENFILE;	/* Over 100 of the things .. bail out! */
}

/* 
   called from drivers like btuart, btusb
*/
hci_struct *hcidev_alloc(void)
{
	hci_struct	*hci;
	int 		alloc_size;

	/* ensure 32-byte alignment of the private area */
	alloc_size = sizeof(*hci) + 31;
	hci = (hci_struct*) kmalloc(alloc_size, in_softirq() ? GFP_ATOMIC : GFP_KERNEL);
	if (hci == NULL) {
		BTERROR("kmalloc() failed\n");
		return NULL;
	}
	memset(hci, 0, alloc_size);
	strcpy(hci->name, "bt%d");
	return hci;
}

int hcidev_register(hci_struct *hci, void *param)
{
	int		err = 0;
	static int	hci_devnum = 1;
	
	DBFENTER;
	
	hcidev_lock();
	hci_hold(hci);
	err = hcidev_alloc_name(hci);
	if (err < 0) {
		hci_put(hci);
		hcidev_unlock();
		return err;
	}

	hci->flags = 0;
	hci->deadbeaf = 0;

	init_rwsem(&hci->sema);
	hci->pid = -1;
	spin_lock_init(&hci->queue_lock);
	spin_lock_init(&hci->xmit_lock);
	hci->xmit_lock_owner = -1;

	hci->devnum = hci_devnum++;
	if (!hci->hdrlen)
		hci->hdrlen = 1;

	/* rx init */
	skb_queue_head_init(&hci->rx_queue);
	tasklet_init(&hci->rx_task, hci_rx_task, (unsigned long) hci);

	/* tx init */
	skb_queue_head_init(&hci->cmd_queue);
	skb_queue_head_init(&hci->acl_queue);
#if defined(CONFIG_AFFIX_SCO)
	skb_queue_head_init(&hci->sco_queue);
#endif
	btl_head_init(&hci->hci_locks);
	tasklet_init(&hci->tx_task, hci_tx_task, (unsigned long) hci);

	set_bit(HCIDEV_STATE_PRESENT, &hci->state);

	btl_add_tail(&hcidevs, hci);	/* add to queue */

	hci->devinfo = param;
	hcidev_state_change(hci, HCIDEV_REGISTER);
	BTINFO("Bluetooth device registered, assigned  name: %s, devnum: %d\n", hci->name, hci->devnum);
	hcidev_unlock();
	DBFEXIT;
	return 0;
}

void hcidev_unregister(hci_struct *hci)
{
	DBFENTER;
	hcidev_lock();
	/* If device is running, close it first. */
	__hcidev_close(hci);
	//BUG_TRAP(hci->deadbeaf == 0);
	hci->deadbeaf = 1;
	clear_bit(HCIDEV_STATE_PRESENT, &hci->state);
	hcidev_state_change(hci, HCIDEV_UNREGISTER);
	hci->devinfo = NULL;
	BTINFO("Bluetooth device %s unregistered\n", hci->name);
	hci_put(hci);
	hcidev_unlock();
	DBFEXIT;
}

int hcidev_rx(hci_struct *hci, struct sk_buff *skb)
{
	skb_queue_tail(&hci->rx_queue, skb);
	if (hcidev_running(hci))
		tasklet_hi_schedule(&hci->rx_task);
	return 0;
}


/*
 * UART related
 */
void affix_set_uart(struct affix_uart_operations *ops)
{
	DBFENTER;
	if (ops == NULL)
		affix_uart_ops.owner = NULL;
	else
		affix_uart_ops = *ops;
	DBFEXIT;
}

int affix_open_uart(struct open_uart *line)
{
	affix_uart_t	uart;
	int		err;

	DBFENTER;

#if defined(CONFIG_AFFIX_UART_MODULE)
	if (!affix_uart_ops.owner || !try_module_get(affix_uart_ops.owner)) {
		request_module("affix_uart");
		if (!affix_uart_ops.owner || !try_module_get(affix_uart_ops.owner))
			return -ENODEV;
	}
#elif !defined(CONFIG_AFFIX_UART)
		return -EPROTONOSUPPORT;
#endif
	memset(&uart, 0, sizeof(uart));
	strncpy(uart.name, line->dev, AFFIX_UART_PATHLEN);
	uart.prodid = line->type;
	uart.proto = line->proto;
	uart.speed = line->speed;
	uart.flags = line->flags;

	err = affix_uart_ops.attach(&uart);
	if (!err)
		return 0;	// return if no error
        module_put(affix_uart_ops.owner);
	DBFEXIT;
	return err;
}

int affix_close_uart(struct open_uart *line)
{
	int	err;

	DBFENTER;
#if defined(CONFIG_AFFIX_UART_MODULE)
	if (!affix_uart_ops.owner)
		return -ENODEV;
#elif !defined(CONFIG_AFFIX_UART)
	return -EPROTONOSUPPORT;
#endif	
	err = affix_uart_ops.detach(line->dev);
	if (err)
		return err;
        module_put(affix_uart_ops.owner);
	DBFEXIT;
	return 0;
}

char *hci_devtype(int type)
{
	switch (type) {
		case HCI_USB:
			return "usb";
		case HCI_PCCARD:
			return "pcmcia";
		case HCI_UART:
			return "uart";
		case HCI_UART_CS:
			return "uart_cs";
		case HCI_PCI:
			return "pci";
		default:
			return "unknown";
	}
}

int uart_speed(int s)
{
	switch (s) {
	case B9600:
		return 9600;
	case B19200:
		return 19200;
	case B38400:
		return 38400;
	case B57600:
		return 57600;
	case B115200:
		return 115200;
	case B230400:
		return 230400;
	case B460800:
		return 460800;
	case B921600:
		return 921600;
	case B1000000:
		return 1000000;
	default:
		return 57600;
	}
}

void hci_run_hotplug(hci_struct *hci, char *action)
{
	char 	*argv [6], **envp, *buf, *scratch;
	int	i = 0, value;

	DBFENTER;
	
	if (in_interrupt ()) {
		DBPRT("In_interrupt");
		return;
	}

	if (!current->fs->root) {
		/* statically linked USB is initted rather early */
		DBPRT("%s -- no FS yet", action);
		return;
	}
	
	if (!(envp = (char **) kmalloc (20 * sizeof (char *), in_softirq() ? GFP_ATOMIC : GFP_KERNEL))) {
		DBPRT("enomem");
		return;
	}
	
	if (!(buf = kmalloc (256, in_softirq() ? GFP_ATOMIC : GFP_KERNEL))) {
		kfree (envp);
		DBPRT("enomem2");
		return;
	}

	/* only one standardized param to hotplug command: type */
	argv [0] = "/etc/affix/affix";
	argv [1] = "affix";	/* hotplug compliant */
	argv [2] = action;
	argv [3] = hci->name;
	argv [4] = hci_devtype(hci->type);
	argv [5] = 0;

	/* minimal command environment */
	envp [i++] = "HOME=/";
	envp [i++] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";

#ifdef	DEBUG
	/* hint that policy agent should enter no-stdout debug mode */
	envp [i++] = "DEBUG=kernel";
#endif
	/* extensible set of named bus-specific parameters,
	 * supporting multiple driver selection algorithms.
	 */
	scratch = buf;

#if 0
	/* action:  add, remove */
	envp [i++] = scratch;
	scratch += sprintf (scratch, "ACTION=%s", action) + 1;

	envp [i++] = scratch;
	scratch += sprintf (scratch, "IFACE=%s", hci->name) + 1;
#endif

	if (!hci->devinfo)
		goto skip;

#if defined(CONFIG_USB)  || defined(CONFIG_USB_MODULE)
	if (hci->type == HCI_USB) {
		struct usb_device	*usb_dev = hci->devinfo;

		if (usb_dev->devnum < 0) {
			DBPRT("device already deleted ??");
			kfree (envp);
			kfree (argv);
			return;
		}

#ifdef	CONFIG_USB_DEVICEFS
		/* 
		 * If this is available, userspace programs can directly read
		 * all the device descriptors we don't tell them about.  Or
		 * even act as usermode drivers.
		 *
		 */
		envp [i++] = "DEVFS=/proc/bus/usb";
		envp [i++] = scratch;
		scratch += sprintf (scratch, "DEVICE=/proc/bus/usb/%03d/%03d",
				usb_dev->bus->busnum, usb_dev->devnum) + 1;
#endif
		/* per-device configuration hacks are common */
		envp [i++] = scratch;
		scratch += sprintf (scratch, "PRODUCT=%x/%x/%x",
				usb_dev->descriptor.idVendor,
				usb_dev->descriptor.idProduct,
				usb_dev->descriptor.bcdDevice) + 1;

		/* class-based driver binding models */
		envp [i++] = scratch;
		scratch += sprintf (scratch, "TYPE=%d/%d/%d",
				usb_dev->descriptor.bDeviceClass,
				usb_dev->descriptor.bDeviceSubClass,
				usb_dev->descriptor.bDeviceProtocol) + 1;
	} else 
#endif
	if (hci->type == HCI_UART || hci->type == HCI_UART_CS) {
		affix_uart_t	*uart = hci->devinfo;

		envp [i++] = scratch;
		scratch += sprintf (scratch, "DEVICE=%s", uart->name) + 1;
		envp [i++] = scratch;
		scratch += sprintf (scratch, "PRODUCT=%#x:%#x", uart->manfid, uart->prodid) + 1;
		envp [i++] = scratch;
		scratch += sprintf (scratch, "SPEED=%d", uart_speed(uart->speed)) + 1;
		envp [i++] = scratch;
		scratch += sprintf (scratch, "FLAGS=%#x", uart->flags) + 1;
	}

skip:
	envp [i++] = 0;
	/* assert: (scratch - buf) < sizeof buf */

	/* NOTE: user mode daemons can call the agents too */

	//DBPRT("%s %s %d", argv [0], action, dev->devnum);
	value = call_usermodehelper (argv[0], argv, envp, 0);
	kfree (buf);
	kfree (envp);
	if (value != 0)
		DBPRT("returned 0x%x\n", value);

	DBFEXIT;
}

/* 
   Socket interface subsystem
*/

int affix_sock_wait_for_state_sleep(struct sock *sk, int state, int nonblock, wait_queue_head_t	*sleep)
{
	int	err = 0;
	long	timeo;
	DECLARE_WAITQUEUE(wait, current);

	DBFENTER;
	if (sk->sk_state == state)
		return 0;
	timeo = sock_sndtimeo(sk, nonblock);
	if (!timeo)
		return -EINPROGRESS;
	add_wait_queue(sleep, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	while (sk->sk_state != state) {
		if (!timeo) {
			err = -EAGAIN;
			break;
		}
		if ((err = sock_error(sk))) 
			break;
		if (signal_pending(current)) {
			err = sock_intr_errno(timeo);
			break;
		}
		//release_sock(sk);
		timeo = schedule_timeout(timeo);
		//lock_sock(sk);
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(sleep, &wait);
	DBFEXIT;
	return err;
}

#if 0
static inline long affix_wait_on_queue(struct sock *sk, long timeo)
{
	DECLARE_WAITQUEUE(wait, current);

	DBFENTER;
	add_wait_queue(sk->sk_sleep, &wait);	// accept uses _exclusive
	set_current_state(TASK_INTERRUPTIBLE);
	if (skb_peek(&sk->sk_receive_queue) == NULL) {
		//release_sock(sk);
		timeo = schedule_timeout(timeo);
		//lock_sock(sk);
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(sk->sk_sleep, &wait);
	DBFEXIT;
	return timeo;
}

struct sk_buff *affix_sock_dequeue(struct sock *sk, int noblock, int *err)
{
	struct sk_buff	*skb;
	long		timeo;
	
        *err = sock_error(sk);
        if (*err)
                return NULL;
	timeo = sock_rcvtimeo(sk, noblock);
	while ((skb = skb_dequeue(&sk->sk_receive_queue)) == NULL) {
		if (!timeo) {
			*err = -EAGAIN;
			return NULL;
		}
		/* Socket shut down? */
		if (sk->sk_shutdown & RCV_SHUTDOWN)
			return NULL;	// *err == 0

		if (affix_sock_connection_based(sk) &&
				!(sk->sk_state == CON_ESTABLISHED || sk->sk_state == CON_LISTEN)) {
			*err = -ENOTCONN;
			return NULL;
		}
		if (signal_pending(current)) {
			*err = sock_intr_errno(timeo);
			return NULL;
		}
		timeo = affix_wait_on_queue(sk, timeo);
		*err = sock_error(sk);	// maybe move it up???
		if (*err)
			return NULL;
	}
	*err = 0;
	return skb;
}
#endif

int affix_sock_flush(struct sock *sk)
{
	int		err = 0;
	DECLARE_WAITQUEUE(wait, current);

	DBFENTER;
	add_wait_queue(sk->sk_sleep, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	while (atomic_read(&sk->sk_wmem_alloc) != 0) {
		if ((err = sock_error(sk))) 
			break;
		if (signal_pending(current)) {
			err = -ERESTARTSYS;
			break;
		}
		schedule();
		set_current_state(TASK_INTERRUPTIBLE);
	}
	set_current_state(TASK_RUNNING);
	remove_wait_queue(sk->sk_sleep, &wait);
	DBFEXIT;
	return err;
}

int affix_sock_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
{
	struct sock		*sk = sock->sk;
	int			err = -ENOTCONN;
	struct sk_buff		*skb;

	DBFENTER;
	if (sock->state != SS_CONNECTED) 
		return -ENOTCONN;
	// sk->sk_state can be not CON_ESTABLISHED but we need to get rest of the data
	skb = skb_recv_datagram(sk, (flags & ~MSG_DONTWAIT) | MSG_PEEK, flags & MSG_DONTWAIT, &err);
	if (!skb)
		goto exit;
	DBPRT("We received %d bytes\n", skb->len);
	DBDUMP(skb->data, skb->len);
	skb->h.raw = skb->data;
	if (skb->len <= size)
		size = skb->len;
	else if (sock->type == SOCK_SEQPACKET) {
		msg->msg_flags |= MSG_TRUNC;
		if (!(flags & MSG_PEEK))
			skb_trim(skb, size);
	}
	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, size);
	if (!(flags & MSG_PEEK))
		skb_pull(skb, size);
	if (!skb->len) {
		skb_unlink(skb);
	//	kfree_skb(skb);
	}
	kfree_skb(skb);		// remove MSG_PEEK link
	err = size;
 exit:
	DBFEXIT;
	return err;
}

/*
   register protocol and allow clients to establish connection
*/
int affix_sock_listen(struct socket *sock, int backlog)
{
	struct sock	*sk = sock->sk;

	DBFENTER;
	if (sock->state != SS_UNCONNECTED || !affix_sock_connection_based(sk))
		return -EINVAL;
	if (((affix_sock_t*)sk->sk_prot)->sport == 0)
		return -EOPNOTSUPP;	
	if (sk->sk_state != CON_LISTEN) {
		sk->sk_max_ack_backlog = backlog;
		sk->sk_state = CON_LISTEN;
	}
	DBFEXIT;
	return 0;
}

int affix_sock_accept(struct socket *sock, struct socket *newsock, int flags)
{
	struct sock	*sk = sock->sk, *newsk;
	struct sk_buff	*skb;
	int		err = -EINVAL;

	DBFENTER;
	if (sock->state != SS_UNCONNECTED || sk->sk_state != CON_LISTEN)
		goto exit;
	
	skb = skb_recv_datagram(sk, 0, flags & O_NONBLOCK, &err);
	if (!skb)
		goto exit;
		
	if (newsock->sk) {
		sk_free(newsock->sk);
		newsock->sk = NULL;
	}
	newsk = skb->sk;
	sock_graft(newsk, newsock);
	sk->sk_ack_backlog--;
	kfree_skb(skb);
	if (newsk->sk_state == CON_ESTABLISHED)
		newsock->state = SS_CONNECTED;
	DBFEXIT;
	return 0;
 exit:			
	return err ? : -ECONNABORTED;
}

/*
    Connect to remote device
    Wait until connection established
*/
int affix_sock_connect(struct socket *sock, struct sockaddr *addr, int alen, int flags, affix_do_connect_t connect)
{
	struct sock		*sk = sock->sk;
	struct sockaddr_affix	*saddr = (void*)addr;
	int			err = 0;

	DBFENTER;
#ifdef CONFIG_AFFIX_L2CAP_GROUPS	
	if (sock->type == SOCK_DGRAM && !bda_zero(&saddr->bda)) {
		sock->state = SS_UNCONNECTED;
		return -ECONNREFUSED;
	}
#endif
	/* deal with restarts */
	if (sock->state == SS_CONNECTING) {
		switch (sk->sk_state) {
			case CON_ESTABLISHED:
				sock->state = SS_CONNECTED;
				return 0;
			case CON_CLOSED:
				return sock_error(sk) ? : -ECONNABORTED;
			case CON_CONNECTING:
				goto wait;
		}
	} else if (sock->state == SS_CONNECTED) {
		return -EISCONN;      /* No reconnect on a seqpacket socket */
	}

	// SS_UNCONNECTED and should be CON_CLOSED

	/* Move to connecting socket, start sending Connect Requests */
	sock->state = SS_CONNECTING;
	sk->sk_state = CON_CONNECTING;
	sk->sk_shutdown = 0;

	err = connect(sk, saddr);
	if (err < 0)
		goto exit;
wait:
	err = affix_sock_wait_for_state(sk, CON_ESTABLISHED, flags & O_NONBLOCK);
	if (err)
		goto exit;
	sock->state = SS_CONNECTED;
 exit:
	DBFEXIT;
	return err;
}

void affix_sock_reset(struct sock *sk, int reason, affix_do_destroy_t destroy)
{
	unsigned char	old_state = sk->sk_state;

	DBFENTER;
	DBPRT("reason: %x\n", reason);
	sock_hold(sk);
	sk->sk_state = CON_CLOSED;
	if (reason > 0) {
		switch (reason) {
			case AFFIX_ERR_TIMEOUT:
				sk->sk_err = ETIMEDOUT;
				break;
			default:
				sk->sk_err = ECONNREFUSED;
				
		}
	} else {
		if (old_state != CON_ESTABLISHED) //  connecting...
			sk->sk_err = ECONNRESET;
	}
	sk->sk_shutdown = SHUTDOWN_MASK;
	if (!sock_flag(sk, SOCK_DEAD)) {
		sk->sk_state_change(sk);
	}
	if (sk->sk_pair && destroy) // not owned by the socket yet
		destroy(sk);
	sock_put(sk);
	DBFEXIT;
}

int affix_sock_ready(struct sock *sk)
{
	struct sk_buff	*skb;
	struct sock	*pair = sk->sk_pair;

	if (sock_flag(pair, SOCK_DEAD))
		return -1;
	skb = alloc_skb(0, GFP_ATOMIC); 
	if (!skb)
		return -1;
	skb_set_owner_r(skb, sk);
	sk->sk_state = CON_ESTABLISHED;
	skb_queue_tail(&pair->sk_receive_queue, skb);
	pair->sk_data_ready(pair, skb->len);
	sock_put(pair);			/* put pair, not needed */
	sk->sk_pair = NULL;
	return 0;
}

int affix_sock_rx(struct sock *sk, struct sk_buff *skb, affix_sock_destruct_t destruct)
{
	skb->dev = NULL;
	skb_set_owner_r(skb, sk);
	if (destruct)
		skb->destructor = destruct;
	skb_queue_tail(&sk->sk_receive_queue, skb);
	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_data_ready(sk, skb->len);
	return 0;
}


struct net_proto_family		*btprotos[BTPROTO_MAX];

int affix_sock_register(struct net_proto_family *pf, int protocol)
{
	if (protocol >= BTPROTO_MAX)
		return -EINVAL;
	if (btprotos[protocol])
		return -EEXIST;
	btprotos[protocol] = pf;
	return 0;
}


int affix_register_notifier(struct notifier_block *nb)
{
	return notifier_chain_register(&affix_chain, nb);
}

int affix_unregister_notifier(struct notifier_block *nb)
{
	return notifier_chain_unregister(&affix_chain, nb);
}

int affix_sock_unregister(int protocol)
{
	if (protocol >= BTPROTO_MAX)
		return -EINVAL;
	btprotos[protocol] = NULL;
	return 0;
}

int affix_sock_create(struct socket *sock, int protocol)
{
	int	i;

	DBFENTER;
	if (protocol >= BTPROTO_MAX)
		return -EPROTONOSUPPORT;
	if (btprotos[protocol] == NULL) {
		if (protocol == BTPROTO_RFCOMM)
			request_module("affix_rfcomm");
		if (btprotos[protocol] == NULL)
			return -EPROTONOSUPPORT;
	}
	if (!try_module_get(btprotos[protocol]->owner))
		return -EPROTONOSUPPORT;
	i = btprotos[protocol]->create(sock, protocol);
	module_put(btprotos[protocol]->owner);
	return i;
}


struct net_proto_family bluetooth_family_ops = {
	owner:	THIS_MODULE,
	family:	PF_AFFIX,
	create:	affix_sock_create
};


#ifdef CONFIG_PROC_FS

/*
    PROC file system
*/
int hci_proc_read(char *buf, char **start, off_t offset, int len)
{
	int 		count = 0, i = 0;
	hci_struct	*hci;
	hci_con		*con;

	DBFENTER;
	count += sprintf(buf+count, "Devices (%d)\n", hcidevs.len);
	btl_read_lock(&hcidevs);
	btl_for_each (hci, hcidevs) {
		count += sprintf(buf+count, "%s (%d):\t%s, flags: %#x, refcnt: %d, dead: %d\n"
				"\t\tcmd_count: %d, acl_count: %d, sco_count: %d\n", 
				hci->name, hci->devnum, bda2str(&hci->bda), hci->flags,
				atomic_read(&hci->refcnt), hci->deadbeaf,
				atomic_read(&hci->cmd_count), 
				atomic_read(&hci->acl_count), 
				atomic_read(&hci->sco_count));
	}
	btl_read_unlock(&hcidevs);
	count += sprintf(buf+count, "Connections (%d)\n", hcicons.len);
	btl_read_lock(&hcicons);
	btl_for_each (con, hcicons) {
		i++;
		count += sprintf(buf+count, "%d: devnum: %d, handle: %#x, bda: %s, type: %s\n"
				"   pkt_type: %#x, qlen: %d, refcnt: %d, pending: %d, state: %d\n",
				i, con->hci->devnum, con->chandle, bda2str(&con->bda), 
				(__is_acl(con))?"ACL":"SCO", 
				con->pkt_type, skb_queue_len(&con->tx_queue),
				atomic_read(&con->refcnt), 
				atomic_read(&con->pending),
				STATE(con));
	}
	btl_read_unlock(&hcicons);
	DBFEXIT;
	return count;
}

struct hci_proc_entry {
	char		*name;
	get_info_t	*fn;
};

struct hci_proc_entry dir[] = {
	{"hci", hci_proc_read},
	{0, 0}
};

int __init hci_proc_init(void)
{
	struct proc_dir_entry	*ent;
	struct hci_proc_entry	*p;

	ent = create_proc_entry("affix", S_IFDIR, proc_net);
	if (!ent)
		return -1;
	proc_affix = ent;
	for (p = dir; p->name; p++)
		ent = create_proc_info_entry(p->name, 0, proc_affix, p->fn);
	return 0;
}

void __exit hci_proc_exit(void)
{
	struct hci_proc_entry	*p;
	
	if (proc_affix == NULL)
		return;

	for (p = dir; p->name; p++)
		remove_proc_entry(p->name, proc_affix);

	proc_affix = NULL;
	remove_proc_entry("affix", proc_net);
}

#endif

int init_hpf(void);
void exit_hpf(void);

/* 
   main hci initialization 
*/
int __init init_hci(void)
{
	int		err = -ENOMEM;

	DBFENTER;

	btl_head_init(&hcidevs);
	btl_head_init(&hcicons);
	affix_set_uart(NULL);

	/* register our socket family */
	err = sock_register(&bluetooth_family_ops);
	if (err)
		goto err1;
#ifdef CONFIG_PROC_FS
	err = hci_proc_init();
	if (err < 0) {
		BTERROR("Unable to initialize procfs\n");
		goto err2;
	}
#endif
	err = init_hpf();
	if (err < 0) {
		BTERROR("Unable to intialize HCI socket interface\n");
		goto err3;
	}
	/* run it at last when all stuff initialized */
	err = hci_start_manager();
	if (err < 0) {
		BTERROR("Unable to start hci thread\n");
		goto err4;
	}
	/* register loop device */
	loop = hcidev_alloc();
	loop->type = HCI_LOOP;
	strcpy(loop->name, "loop");
	hcidev_register(loop, 0);
	hcidev_open(loop);
	btl_unlink(&hcidevs, loop);
	DBFEXIT;
	return 0;
err4:
	exit_hpf();
err3:
#ifdef CONFIG_PROC_FS
	hci_proc_exit();	
err2:
#endif
	sock_unregister(PF_AFFIX);
err1:
	return err;
}

void __exit exit_hci(void)
{
	DBFENTER;
	hcidev_unregister(loop);
	hci_stop_manager();
	exit_hpf();
#ifdef CONFIG_PROC_FS
	hci_proc_exit();
#endif
	sock_unregister(PF_AFFIX);
	DBFEXIT;
}


int affix_sysctl_register(void);
void affix_sysctl_unregister(void);

int init_debug(void);

int init_l2cap(void);
void exit_l2cap(void);

int init_rfcomm(void);
void exit_rfcomm(void);

int init_pan(void);
void exit_pan(void);

void init_btuart(void);
void init_btuart_cs(void);
void init_btusb(void);
void init_bluecard_cs(void);
void init_bt3c_cs(void);
void init_bt950uart_cs(void);

#if 0 //def MODULE
static int can_unload(void)
{
	DBFENTER;
	return 0;
	if (GET_USE_COUNT(THIS_MODULE) > 0) {
		return -EBUSY;
	}
	DBFEXIT;
	return 0;
}
#endif

/*
  affix_init
  This function register new line discipline for BT
  returns: zero on success, non-zero on failure.
*/
int __init affix_init(void)
{
	int	err;

	printk("Affix Bluetooth Protocol Stack loaded\n");
	printk("Copyright (C) 2001, 2002 Nokia Corporation\n");
	printk("Written by Dmitry Kasatkin <dmitry.kasatkin@nokia.com>\n");

	DBFENTER;

#if 0 //def MODULE
	if (!mod_member_present(&__this_module, can_unload)) {
		return -EBUSY;
	}
	__this_module.can_unload = can_unload;
#endif

#if defined(CONFIG_SYSCTL)
	err = affix_sysctl_register();
	if (err)
		goto err;
#endif
#if defined(CONFIG_AFFIX_DEBUG)
	err = init_debug();
	if (err)
		goto err1;
#endif
	err = init_hci();
	if (err)
		goto err1;
#if defined(CONFIG_AFFIX_L2CAP)
	err = init_l2cap();
	if (err)
		goto err2;
#endif  /*L2CAP*/
#if defined(CONFIG_AFFIX_RFCOMM)
	init_rfcomm();
#endif
#if defined(CONFIG_AFFIX_PAN)
	init_pan();
#endif
	/* now initialize drivers */
#ifdef CONFIG_AFFIX_UART
	init_btuart();
#endif
#ifdef CONFIG_AFFIX_UART_CS
	init_btuart_cs();
#endif
#ifdef CONFIG_AFFIX_USB
	init_btusb();
#endif
#ifdef CONFIG_AFFIX_BLUECARD_CS
	init_bluecard_cs();
#endif
#ifdef CONFIG_AFFIX_BT3C_CS
	init_bt3c_cs();
#endif
#ifdef CONFIG_AFFIX_BT950UART_CS
	init_bt950uart_cs();
#endif
	DBFEXIT;
	return err;
#if defined(CONFIG_AFFIX_L2CAP)
err2:
	exit_hci();
#endif /*L2CAP*/
err1:
#if defined(CONFIG_SYSCTL)
	affix_sysctl_unregister();
err:
#endif
	return err;
}

/*
  affix_exit

  returns: nothing
*/
void __exit affix_exit(void)
{
	DBFENTER;
#if defined(CONFIG_AFFIX_L2CAP)
	exit_l2cap();
#endif
	exit_hci();
#if defined(CONFIG_SYSCTL)
	affix_sysctl_unregister();
#endif
	DBFEXIT;
}

EXPORT_SYMBOL(affix_dbmask);
EXPORT_SYMBOL(proc_affix);
EXPORT_SYMBOL(hci_destroy);
EXPORT_SYMBOL(hcidev_alloc);
EXPORT_SYMBOL(hcidev_register);
EXPORT_SYMBOL(hcidev_unregister);
EXPORT_SYMBOL(hcidev_rx);
EXPORT_SYMBOL(hcidev_state_change);
EXPORT_SYMBOL(affix_set_uart);
EXPORT_SYMBOL(affix_set_pan);
EXPORT_SYMBOL(affix_set_hidp);
EXPORT_SYMBOL(affix_register_notifier);
EXPORT_SYMBOL(affix_unregister_notifier);
EXPORT_SYMBOL(affix_sock_register);
EXPORT_SYMBOL(affix_sock_unregister);
EXPORT_SYMBOL(affix_sock_wait_for_state_sleep);
//EXPORT_SYMBOL(affix_sock_dequeue);
EXPORT_SYMBOL(affix_sock_flush);
EXPORT_SYMBOL(affix_sock_recvmsg);
EXPORT_SYMBOL(affix_sock_listen);
EXPORT_SYMBOL(affix_sock_accept);
EXPORT_SYMBOL(affix_sock_connect);
EXPORT_SYMBOL(affix_sock_reset);
EXPORT_SYMBOL(affix_sock_ready);
EXPORT_SYMBOL(affix_sock_rx);
EXPORT_SYMBOL(hci_lookup_bda);
EXPORT_SYMBOL(hci_lookup_name);
EXPORT_SYMBOL(hci_lookup_devnum);
EXPORT_SYMBOL(hci_select);
EXPORT_SYMBOL(lp_auth_req);
EXPORT_SYMBOL(hci_deliver_msg);


/*  If we are resident in kernel we call affix_init manually after root FS is mounted.  */
module_init(affix_init);
module_exit(affix_exit);

MODULE_AUTHOR("Dmitry Kasatkin <dmitry.kasatkin@nokia.com>");
MODULE_DESCRIPTION("affix (hci, l2cap, sockets) Bluetooth Driver Core for Linux");
MODULE_PARM(affix_dbmask, "i"); /* For insmod */
MODULE_LICENSE("GPL");



/*
  NOTES:

  4. maybe HUP signal restart mgr_thread()

  - btdev_execute_command & btdev_recv_event should check hci->state
  - hci->hwtimeout. HCI manager set it to try command execution  - 20sec now
  - check btdev->evl->hci. It's not used for some reason
*/

