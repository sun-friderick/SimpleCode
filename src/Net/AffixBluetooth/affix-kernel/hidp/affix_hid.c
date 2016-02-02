/* -*-   Mode:C; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001, 2002, 2004 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
   Original Author: Andre Hebben <hebben@cs.uni-bonn.de>

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
   $Id: affix_hid.c,v 1.14 2004/07/15 16:08:22 hebben Exp $

   affix_hid.c - HID kernel module
   
*/


#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/ioctl.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/list.h>
#include <linux/workqueue.h>

#define FILEBIT      0x800    /* TODO: assign correct value */
#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>

#include "affix_hid.h"
#include "hid.h"

#define VERSION "0.03"

/* global data */

static LIST_HEAD(hidp_connections);
static DECLARE_RWSEM(hidp_connections_sem);
static DECLARE_WORK(hidp_reg_task, NULL, NULL);
static DECLARE_WORK(hidp_unreg_task, NULL, NULL);

/* helper function */
struct hidp_connection *hidp_is_bda_known(BD_ADDR *bda)
{
	struct hidp_connection *dev;
	struct list_head *p;
	int known = 0;

	list_for_each(p, &hidp_connections) {
		dev = list_entry(p, struct hidp_connection, list);
		if (!memcmp(&dev->bda, bda, 6)) {
			known = -1;
			break;
		}
	}
	if (known)
		return dev;
	else
		return NULL;
}


int hidp_send_report(struct hid_device *device, unsigned char *data, int size)
{
	struct hidp_connection *conn = device->driver_data;
	struct sk_buff *skb;
	int err;

	DBFENTER;
	skb = alloc_skb(size + 17, GFP_ATOMIC);
	if (!skb) {
		BTERROR("No memory for skb left.");
		return -ENOMEM;
	}
	skb_reserve(skb, 10);
	*skb_put(skb, 1) = 0xa2;
	memcpy(skb_put(skb, size), data, size);

	err = l2ca_send_data(conn->intr_ch, skb);
	if (err)
		BTERROR("error\n");
	DBFEXIT;
	return err;
}

void hidp_unreg_device(void *data) 
{
	struct hidp_connection *conn = data;

	hid_unregister_device(conn->hid);
	hid_free_device(conn->hid);
	conn->hid = NULL;
	return;
}

int hidp_kill_connection(struct hidp_connection *conn)
{
	struct hid_device *hid;
	l2cap_ch *ch;
	DBFENTER;
	if ((hid = conn->hid)) {
		PREPARE_WORK(&hidp_unreg_task, hidp_unreg_device, 
			     (void *)conn);
		schedule_work(&hidp_unreg_task);

	}
	if ((ch = conn->intr_ch)) {
		l2ca_orphan(ch);
		l2ca_put(ch);
		conn->intr_ch = NULL;
	}
	if ((ch = conn->ctrl_ch)) {
		l2ca_orphan(ch);
		l2ca_put(ch);
		conn->ctrl_ch = NULL;
	}
	conn->state = HIDP_CONN_CLOSED;
	DBFEXIT;
	return 0;
}

/* L2CAP protocol
   control channel */
int hidp_connect_req(struct hidp_connection *conn)
{
	l2cap_ch *ch;
	int err = 0;

	DBFENTER;
	if (!conn)
		return -ENODEV;
	if (conn->state != HIDP_CONN_CLOSED) {
		return -ENODEV; /* check correct value */
	}
	ch = l2ca_create(&hidp_ctrl_ops);
	if (!ch) 
		return -ENOMEM;
	/* select any available hci, todo: make this
	   configurable from user space */
	ch->hci = hci_select(); 
	conn->state = HIDP_CONN_CONFIG;
	conn->ctrl_ch = ch;
	ch->priv = conn;
	l2ca_set_mtu(ch, HIDP_DEF_MTU);
	l2ca_set_flushto(ch, 0xffff);
	err = l2ca_connect_req(ch, &conn->bda, HIDP_CTRL_PSM);
	if (err) {
		BTERROR("cant't establish connection.\n");
		hidp_kill_connection(conn);
		return err;
	}
	DBFEXIT;
	return 0;
}

int hidp_c_connect_ind(l2cap_ch *ch)
{
	struct hidp_connection *conn;

	DBFENTER;
	DBPRT("HID: co_ch connection request from %s\n", bda2str(&ch->bda));
	conn = hidp_is_bda_known(&ch->bda);
	if (!conn) {
		/* device is unknown, deny connection */
		l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
		/* check: CONRSP_SECURITY ?? */
		return 0;
	}
	/* device may connect... */
	if (conn->state != HIDP_CONN_CLOSED) {
		l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
		return 0;
	}
	conn->state = HIDP_CONN_CONFIG;
	conn->ctrl_ch = ch;
	ch->priv = conn;
	l2ca_hold(ch);
	l2ca_set_mtu(ch, HIDP_DEF_MTU);
	l2ca_set_flushto(ch, 0xffff);
	l2ca_connect_rsp(ch, L2CAP_CONRSP_SUCCESS, 0);

	DBFEXIT;
	return 0;
}

int hidp_c_connect_cfm(l2cap_ch *ch, int result, int status)
{
	struct hidp_connection *conn = ch->priv;
	l2cap_ch *intr_ch;
	l2cap_qos_t qos;

	DBPRT("c_conn_cfm. res: %d, stat: %d.\n", result, status);
	DBFENTER;
	switch (result) {
	case L2CAP_CONRSP_SUCCESS:
		if (l2ca_server(ch))
			return 0;
		/* host initiated connection, so start 
		   interrupt channel next */
		intr_ch = l2ca_create(&hidp_intr_ops);
		if (!intr_ch) {
			hidp_kill_connection(conn);
			break;
		}
		intr_ch->hci = ch->hci;
		conn->intr_ch = intr_ch;
		intr_ch->priv = conn;
		l2ca_set_mtu(intr_ch, HIDP_DEF_MTU);
		l2ca_set_flushto(ch, 0xffff);
		qos.service_type = L2CAP_QOS_GUARANTEED;
		qos.token_rate = 0xffffffff;
		qos.token_size = 0xffffffff;
		qos.bandwidth = 0xffffffff;
		l2ca_set_qos(intr_ch, &qos);

		if (l2ca_connect_req(intr_ch, &conn->bda, HIDP_INTR_PSM)) {
			hidp_kill_connection(conn);
		}
		break;
	case L2CAP_CONRSP_PENDING:
		return 0;
	default:
		BTERROR("HIDP: connection to %s failed.\n", bda2str(&ch->bda));
		hidp_kill_connection(conn);
		break;
	}
	DBFEXIT;
	return 0;
}

int hidp_c_config_ind(l2cap_ch *ch)
{
	DBFENTER;
	DBPRT("c_conf_ind\n");
	if (l2ca_get_mtu(ch) < HIDP_MIN_MTU) { /* wrong MTU */
		DBPRT("wrong MTU (%d instead %d), sending negative ConfigRsp\n", l2ca_get_mtu(ch), HIDP_MIN_MTU);
		l2ca_set_mtu(ch, HIDP_MIN_MTU);
		l2ca_config_rsp(ch, L2CAP_CFGRSP_PARAMETERS);
		return 0;
	}
	l2ca_config_rsp(ch, L2CAP_CFGRSP_SUCCESS);

	DBFEXIT;
	return 0;
}

int hidp_c_config_cfm(l2cap_ch *ch, int result)
{
	return 0;
}

int hidp_c_disconnect_ind(l2cap_ch *ch)
{
	struct hidp_connection *conn = ch->priv;

	DBFENTER;
	DBPRT("c_disc_ind\n");

	conn->ctrl_ch = NULL;
	l2ca_orphan(ch);
	l2ca_put(ch);
	if (conn->state == HIDP_CONN_CLOSING) {
		conn->state = HIDP_CONN_CLOSED;
		if (conn->hid) {
			PREPARE_WORK(&hidp_unreg_task, hidp_unreg_device, 
				     (void *)conn);
			schedule_work(&hidp_unreg_task);
		}
       	} else
		conn->state = HIDP_CONN_CLOSING;

	DBFEXIT;
	return 0;

}

int hidp_c_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	DBPRT("data on co channel.\n");
	kfree_skb(skb);
	return 0;
}

/* L2CAP protocol
   interrupt channel */
int hidp_i_connect_ind(l2cap_ch *ch)
{
	struct hidp_connection *conn;

	DBFENTER;
	DBPRT("HID: in_ch connection request from %s\n", bda2str(&ch->bda));
	conn = hidp_is_bda_known(&ch->bda);
	if (!conn) {
		DBPRT("Device %s is not allowed to connect!\n", bda2str(&ch->bda));
       		goto fail;
	}
	if (conn->state != HIDP_CONN_CONFIG) {
		DBPRT("Not in config state !\n");
		goto fail;
	}
	conn->intr_ch = ch;
	ch->priv = conn;
	l2ca_hold(ch);
	l2ca_set_mtu(ch, HIDP_DEF_MTU);
	l2ca_set_flushto(ch, 0xffff);
	l2ca_connect_rsp(ch, L2CAP_CONRSP_SUCCESS, 0);

	DBFEXIT;
	return 0;

 fail:
	l2ca_connect_rsp(ch, L2CAP_CONRSP_RESOURCE, 0);
	if (conn->state != HIDP_CONN_CLOSED)
		hidp_kill_connection(conn);
	return 0;

}

void hidp_reg_device(void *data) 
{
	struct hidp_connection *conn = data;
	struct hidp_conn_info *info = &conn->info;
	struct hid_device *hid;

	if (conn->hid)
		return;
	conn->hid = hid_alloc_device(info->rd_data, info->rd_size);
	if (!conn->hid) {
		BTERROR("hid_alloc_device failed\n");
		hidp_kill_connection(conn);
		return;
	}
	conn->flags = info->flags;
	conn->idle_to = info->idle_to;
	
	hid = conn->hid;
	hid->driver_data = conn;
	hid->parser_version = info->parser;
	hid->country_code = info->country;
	hid->bus = BUS_BLUETOOTH;
	hid->vendor = info->vendor;
	hid->product = info->product;
	hid->version = info->version;
	strncpy(hid->name, info->name, 128);
	strncpy(hid->uniq, bda2str(&conn->bda), 64);
	strncpy(hid->phys, bda2str(&conn->bda), 64);  /* check this */
	hid->send = hidp_send_report;
	
	hid_register_device(hid);
	printk("Affix HIDP: Device '%s', with Adress '%s' registered.\n", hid->name, bda2str(&conn->bda));
	conn->state = HIDP_CONN_OPEN;
	return;
}

int hidp_i_connect_cfm(l2cap_ch *ch, int result, int status)
{
	struct hidp_connection *conn = ch->priv;

	DBPRT("i_conn_cfm. res: %d, stat: %d.\n", result, status);
	DBFENTER;

	switch (result) {
	case L2CAP_CONRSP_SUCCESS:
		/* register HID, do so in process context */
		PREPARE_WORK(&hidp_reg_task, hidp_reg_device, (void *)conn);
		schedule_work(&hidp_reg_task);
		break;
	case L2CAP_CONRSP_PENDING:
		return 0;
	default:
		BTERROR("HIDP: connection to %s failed.\n", bda2str(&ch->bda));
		hidp_kill_connection(conn);
		break;
	}
	DBFEXIT;
	return 0;
}

int hidp_i_config_ind(l2cap_ch *ch)
{
	DBPRT("i_conf_ind\n");
	if (l2ca_get_mtu(ch) < HIDP_MIN_MTU) { /* wrong MTU */
		DBPRT("wrong MTU (%d instead %d), sending negative ConfigRsp\n", l2ca_get_mtu(ch), HIDP_MIN_MTU);
		l2ca_set_mtu(ch, HIDP_MIN_MTU);
		l2ca_config_rsp(ch, L2CAP_CFGRSP_PARAMETERS);
		return 0;
	}
	l2ca_config_rsp(ch, L2CAP_CFGRSP_SUCCESS);

	return 0;
}

int hidp_i_config_cfm(l2cap_ch *ch, int result)
{
	return 0;
}

int hidp_i_disconnect_ind(l2cap_ch *ch)
{
	struct hidp_connection *conn = ch->priv;

	DBFENTER;
	DBPRT("i_disc_ind\n");

	conn->intr_ch = NULL;
	l2ca_orphan(ch);
	l2ca_put(ch);
	if (conn->state == HIDP_CONN_CLOSING) {
		conn->state = HIDP_CONN_CLOSED;
		if (conn->hid) {
			PREPARE_WORK(&hidp_unreg_task, hidp_unreg_device, 
				     (void *)conn);
			schedule_work(&hidp_unreg_task);
		}

	} else
		conn->state = HIDP_CONN_CLOSING;

	hidp_kill_connection(conn);
	DBFEXIT;
	return 0;
}

int hidp_i_data_ind(l2cap_ch *ch, struct sk_buff *skb)
{
	__u8 hdr;
	struct hidp_connection *conn = ch->priv;
	DBFENTER;

	hdr = skb->data[0];
	skb_pull(skb, 1);
	switch (hdr) {
	case 0xa1:
		hid_recv_report(conn->hid, HID_INPUT_REPORT, skb->data, skb->len);
		break;
	default:
		DBPRT("Unknown protocol header 0x%02x", hdr);
		break;
	}
	kfree_skb(skb);

	DBFEXIT;
	return 0;
}

/* callbacks for the l2cap channels */
l2cap_proto_ops hidp_ctrl_ops = {
	.owner = THIS_MODULE,
	.data_ind = hidp_c_data_ind,
	.connect_ind = hidp_c_connect_ind,
	.connect_cfm = hidp_c_connect_cfm,
	.config_ind = hidp_c_config_ind,
	.config_cfm = hidp_c_config_cfm,
	.disconnect_ind = hidp_c_disconnect_ind
};

l2cap_proto_ops hidp_intr_ops = {
	.owner = THIS_MODULE,
	.data_ind = hidp_i_data_ind,
	.connect_ind = hidp_i_connect_ind,
	.connect_cfm = hidp_i_connect_cfm,
	.config_ind = hidp_i_config_ind,
	.config_cfm = hidp_i_config_cfm,
	.disconnect_ind = hidp_i_disconnect_ind
};

int hidp_hci_down(hci_struct *hci)
{
#if 0
	struct hidp_connection *p;

	DBFENTER;
	list_for_each_entry(p, &hidp_connections, list) {
		if (p->state != HIDP_CONN_CLOSED &&
		    p->ctrl_ch && p->ctrl_ch->hci == hci) {
			hidp_kill_connection(p);
		}
	}
	DBFEXIT;
#endif
	return 0;
}

int hidp_event(struct notifier_block *nb, unsigned long event, void *arg)
{
	hci_struct	*hci = arg;

	switch (event) {
		case HCIDEV_DOWN:
			hidp_hci_down(hci);
			break;
		default:
			break;
	}
	return NOTIFY_DONE;
}

struct notifier_block	hidp_notifier_block = {
	.notifier_call = hidp_event,
};

/* IOCTLs */
int hidp_modify_list(struct hidp_ioc *arg)
{
	struct hidp_connection *conn;
	__u8 *rd_data;

	DBFENTER;
	printk("HIDP: modify ioctl called.\n");
	/* check if device is already known */
	conn = hidp_is_bda_known(&arg->saddr.bda);
	if (conn) {
		/* device is known, change status */
		if (conn->state == HIDP_CONN_CLOSED &&
		    (arg->status & HIDP_STATUS_ACTIVE_ADD))
			hidp_connect_req(conn);
	} else { 
		/* device is unknown, add it to list and connect if 
		 * requested */
		if (!arg->conn_info.rd_data)
			return -EINVAL;
		conn = kmalloc(sizeof(struct hidp_connection), GFP_KERNEL);
		if (!conn)
			return -ENOMEM;
		memset(conn, 0, sizeof(struct hidp_connection));
		if (copy_from_user(&conn->bda, &arg->saddr.bda, 6)) {
			kfree(conn);
			return -EFAULT;
		}
		rd_data = kmalloc(arg->conn_info.rd_size, GFP_KERNEL);
		if (!rd_data) {
			kfree(conn);
			return -ENOMEM;
		}
		if (copy_from_user(&conn->info, &arg->conn_info, 
				   sizeof(struct hidp_conn_info))
		    || copy_from_user(rd_data, arg->conn_info.rd_data, 
				      arg->conn_info.rd_size)) {
			kfree(rd_data);
			kfree(conn);
			return -EFAULT;
		}
		conn->info.rd_data = rd_data;
		down_write(&hidp_connections_sem);
		list_add(&conn->list, &hidp_connections);
		up_write(&hidp_connections_sem);
		if (arg->status & HIDP_STATUS_ACTIVE_ADD) {
			DBPRT("Active add, starting connection\n");
			hidp_connect_req(conn);
		}
	}
	DBFEXIT;
	return 0;
}

int hidp_delete_conn(struct hidp_ioc *arg)
{
	struct hidp_connection *conn;

	DBFENTER;
	DBPRT("HIDP: delete ioctl called.\n");
	/* check if device is in known_devices list */
	down_write(&hidp_connections_sem);
	conn = hidp_is_bda_known(&arg->saddr.bda);
	if (!conn) {
		up_write(&hidp_connections_sem);
		return -ENODEV;
	}
	if (conn->state != HIDP_CONN_CLOSED) {
		hidp_kill_connection(conn);
	}
	/* remove list entry */
	kfree(conn->info.rd_data);
	list_del(&conn->list);
	kfree(conn);
	up_write(&hidp_connections_sem);
	DBFEXIT;
	return 0;
}

int hidp_get_list(struct hidp_ioc_getlist *arg)
{
	struct hidp_ioc_getlist *mylist;
	struct hidp_ioc *ent;
	struct hidp_connection *p;
	int size = arg->count;
	int count = 0;
	int left = 0;

	DBFENTER;
	DBPRT("HIDP: get_list ioctl called.\n");
	mylist = kmalloc(sizeof(struct hidp_ioc_getlist) + 
			 size * sizeof(struct hidp_ioc), GFP_KERNEL);
	if (!mylist)
		return -ENOMEM;   /* check this */
	ent = mylist->list;
	down_read(&hidp_connections_sem);
	list_for_each_entry(p, &hidp_connections, list) {
		__u16 status = 0;
		if (count > size) {
			left = -1;
			break;
		}
		memcpy(&ent->saddr.bda, &p->bda, 6);
		if (p->state == HIDP_CONN_OPEN) {
			status |= HIDP_STATUS_CONNECTED;
		}
		ent->status = status;
		ent++;
		count++;
	}
	up_read(&hidp_connections_sem);
	mylist->count = count;
	mylist->left = left;
	copy_to_user(arg, mylist, sizeof(struct hidp_ioc_getlist) +
		     count * sizeof(struct hidp_ioc));
	kfree(mylist);
	DBFEXIT;
	return 0;
}


int hidp_ioctl(unsigned int cmd,  unsigned long arg)
{
	int	err = -1;

	switch (cmd) {
	case BTIOC_HIDP_MODIFY:
		err = hidp_modify_list((struct hidp_ioc*)arg);
		break;
	case BTIOC_HIDP_DELETE:
		err = hidp_delete_conn((struct hidp_ioc*)arg);
		break;
	case BTIOC_HIDP_GET_LIST:
		err = hidp_get_list((struct hidp_ioc_getlist*)arg);
		break;
	default:
		return -ENOIOCTLCMD;
	}
	return err;
}


struct affix_pan_operations	hidp_ops = {
	owner:	THIS_MODULE,
	ioctl:	hidp_ioctl,
};


#ifdef CONFIG_PROC_FS
static int hidp_proc_read(char *buf, char **start, off_t offset, int len)
{
	struct hidp_connection *p;
	int count = 0;
	count += sprintf(buf + count, "Known devices\t\tState\n");

	down_read(&hidp_connections_sem);
	list_for_each_entry(p, &hidp_connections, list) {
		count += sprintf(buf + count, "%s\t", bda2str(&p->bda));
		if (p->state == HIDP_CONN_OPEN) {
			count += sprintf(buf + count, "Connected\n");
		} else {
			count += sprintf(buf + count, "Not connected\n");
		}
	}
	up_read(&hidp_connections_sem);

	return count;
}

struct proc_dir_entry    *hidp_proc;
#endif /* CONFIG_PROC_FS */


static int __init hidp_init(void)
{
	int err;

	DBFENTER;

	printk("Affix HID Profile ver %s loaded\n", VERSION);

	/* register protocols for control and interrupt channel */
	err = l2ca_register_protocol(HIDP_CTRL_PSM, &hidp_ctrl_ops);
	if (err < 0) {
		BTERROR("HIDP: error registering L2CAP protocol (ctrl)\n");
		goto err1;
	}
	err = l2ca_register_protocol(HIDP_INTR_PSM, &hidp_intr_ops);
	if (err < 0) {
		BTERROR("HIDP: error registering L2CAP protocol (intr)\n");
		goto err2;
	}
#ifdef CONFIG_PROC_FS
	hidp_proc = create_proc_info_entry("hidp", 0, proc_affix, hidp_proc_read);
	if (!hidp_proc)
		goto err3;
#endif
	affix_register_notifier(&hidp_notifier_block);
	affix_set_hidp(&hidp_ops);

	DBFEXIT;
	return 0;

 err3:
	l2ca_unregister_protocol(HIDP_INTR_PSM);
 err2:
	l2ca_unregister_protocol(HIDP_CTRL_PSM);
 err1:
	BTERROR("module initialization failed\n");
	return err;
}

static void __exit hidp_exit(void)
{
	struct hidp_connection *p;
	struct hidp_connection *n;

	DBFENTER;
	down_write(&hidp_connections_sem);
	list_for_each_entry_safe(p, n, &hidp_connections, list) {
		if (p->state != HIDP_CONN_CLOSED) {
			hidp_kill_connection(p);
		}
		/* remove list entry */
		kfree(p->info.rd_data);
		list_del(&p->list);
		kfree(p);
	}
	up_write(&hidp_connections_sem);
	affix_set_hidp(NULL);
	affix_unregister_notifier(&hidp_notifier_block);
#ifdef CONFIG_PROC_FS
	remove_proc_entry("hidp", proc_affix);
#endif
	l2ca_unregister_protocol(HIDP_INTR_PSM);
	l2ca_unregister_protocol(HIDP_CTRL_PSM);
	DBFEXIT;

}

module_init(hidp_init);
module_exit(hidp_exit);

MODULE_AUTHOR("Andre Hebben <hebben@cs.uni-bonn.de>");
MODULE_DESCRIPTION("Affix HID Profile " VERSION);
MODULE_VERSION(VERSION);
MODULE_LICENSE("GPL");


