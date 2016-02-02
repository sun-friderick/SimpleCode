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
   $Id: hci_mgr.c,v 1.104 2004/03/17 12:54:45 kassatki Exp $

   HCI Manager

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/		

/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>
#include <linux/version.h>

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
#include <linux/notifier.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/utsname.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <net/pkt_sched.h>


/* Local Includes */
#define FILEBIT	DBHCIMGR

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>


#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

DECLARE_MUTEX_LOCKED(exit_sema);
static volatile int	pid = -1, mgr_exit = 0;
btlist_head_t		btdevs;	/* Neighbor Devices list */
struct PIN_Code		affix_pin_code = { {{0, 0, 0, 0, 0, 0}}, 0, ""};
int			affix_ctl_mode	= 0;


/*
  Driver Internal Commands
*/

int message_handler(struct hci_msg_hdr *hdr)
{
	int	err = 0, fd;

	DBFENTER;

	DBPRT("Manager Command: %d\n", hdr->opcode);
	switch (hdr->opcode) {
		case HCICTL_CONNECT_REQ: 
			{
				struct hci_connect_req	*cmd = (void*)hdr;
				INQUIRY_ITEM	dev;
				hci_con		*con;

				con = hcc_lookup_id(cmd->id);
				if (!con)
					break;

				if (con->hci == NULL) {
					hci_struct	*hci;

					/* select device */
					hci = hci_select();
					if (hci == NULL)
						goto fail;
					hcc_bind(con, hci);
					hci_put(hci);
				}
				if (STATE(con) != CON_W4_LCONREQ)
					goto fail;
				ENTERSTATE(con, CON_W4_CONRSP);

				DBPRT("attempt... %d\n", con->attempt);

				if (__is_acl(con)) {
					struct btdev	*btdev;
					
					fd = hci_open_id(con->hci->devnum);
					if (fd < 0)
						goto fail;
					/* get device from the cache */
					btdev = btdev_lookup_bda(&con->bda);
						
					if (btdev && (jiffies - btdev->stamp) < (HZ << 12)) {// 1.13 hour
						dev.PS_Repetition_Mode = btdev->PS_Repetition_Mode;
						dev.PS_Mode = btdev->PS_Mode;
						dev.Clock_Offset = btdev->Clock_Offset | 0x8000;
					} else {
						dev.PS_Repetition_Mode = 0x00;
						dev.PS_Mode = 0x00;
						dev.Clock_Offset = 0x00;
					}
					dev.bda = con->bda;
					err = __HCI_CreateConnection(fd, &dev, con->hci->pkt_type & HCI_PT_ACL, 
							!(con->hci->flags & HCI_ROLE_DENY_SWITCH));
					if (btdev)
						btdev_put(btdev);
					hci_close(fd);
				} else {
					hci_con		*acl;

					acl = hcc_lookup_acl(con->hci, &con->bda);
					if (!acl) {
						/* not connected FIXME:*/
						goto fail;
					}
					fd = hci_open_id(acl->hci->devnum);
					if (fd < 0) {
						hcc_put(acl);
						goto fail;
					}

					err = __HCI_AddSCOConnection(fd, acl->chandle, acl->hci->pkt_type & HCI_PT_SCO);
					hci_close(fd);
					hcc_put(acl);
				}
				if (err)
					goto fail;
				hcc_put(con);
				break;
fail:
				hcc_stop_timer(con);
				ENTERSTATE(con, DEAD);
				hpf_connect_cfm(con, (err < 0) ? HCI_ERR_HARDWARE_FAILURE : err);
				hcc_put(con);
			}
			break;

		case HCICTL_DISCONNECT_REQ: 
			{
				struct hci_disconnect_req	*cmd = (void*)hdr;
				hci_con		*con;

				con = hcc_lookup_id(cmd->id);
				if (!con)
					break;

				if (STATE(con) != CON_W4_LDISCREQ) {
					hcc_put(con);
					break;
				}
				hcc_stop_timer(con);
				ENTERSTATE(con, CON_W4_DISCRSP);
				fd = hci_open_id(con->hci->devnum);
				if (fd < 0) {
					hcc_put(con);
					break;

				}
#if 0
				err = __HCI_Disconnect(fd, con->chandle, cmd->reason);
#else
				/* do it exclusively */
				hci_lock(fd, 1);
				err = HCI_Disconnect(fd, con->chandle, cmd->reason);
				hci_lock(fd, 0);
#endif
				hci_close(fd);
				hcc_put(con);
			}
			break;

		case HCICTL_AUTH_REQ: 
			{
				struct hci_auth_req	*cmd = (void*)hdr;
				hci_con		*con;

				con = hcc_lookup_id(cmd->id);
				if (!con)
					break;

				fd = hci_open_id(con->hci->devnum);
				if (fd < 0) {
					hcc_put(con);
					break;

				}
				err = __HCI_AuthenticationRequested(fd, con->chandle);
				hci_close(fd);
				hcc_put(con);
			}
			break;

#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
		case HCICTL_UPDATECLOCKOFFSET_REQ:
			{
				struct hci_updateclockoffset_req *cmd = (void*)hdr;
				hci_con		*con;

				con = hcc_lookup_id(cmd->id);
				if (!con)
					break;

				fd = hci_open_id(con->hci->devnum);
				if (fd < 0) {
					hcc_put(con);
					break;

				}
				
				err = __HCI_ReadClockOffset(fd, con->chandle);
				if (err) {
					DBPRT("\nError Reading Clock Offset\n");
				}
				hci_close(fd);
				hcc_put(con);
			}
			break;
#endif

		default:
			break;
	}
	return err;
}



/*
 * HCI events processing
 */
int connection_request(struct Connection_Request_Event *evt, int devnum)
{
	int		err = 0, fd;
	hci_struct	*hci;
	
	DBFENTER;

	hci = hci_lookup_devnum(devnum);
	if (!hci) {
		return -ENODEV;
	}
	fd = hci_open_id(devnum);
	if (fd < 0) {
		hci_put(hci);
		return fd;
	}
	err = __HCI_AcceptConnectionRequest(fd, &evt->bda, !(hci->flags & HCI_ROLE_BECOME_MASTER));
	hci_close(fd);
	hci_put(hci);

	DBFEXIT;
	return err;
}

int pin_code_request(struct PIN_Code_Request_Event *evt, int devnum)
{
	int		fd, err;
	struct btdev	*btdev = NULL;
	hci_struct	*hci;
	__u8		length = 0;
	__u8		*pin = NULL;

	DBFENTER;
	if (affix_ctl_mode & AFFIX_FLAGS_PIN)
		return 0;	// somebody takes care about pin
	hci = hci_lookup_devnum(devnum);
	if (!hci)
		return 0;
	fd = hci_open_id(devnum);
	if (fd < 0) {
		hci_put(hci);
		return fd;
	}
	if (hci->flags & HCI_SECURITY_PAIRABLE) {
		btdev = btdev_lookup_bda(&evt->bda);
		if (btdev && (btdev->flags & NBT_PIN)) {
			pin = btdev->PIN_Code;
			length = btdev->PIN_Code_Length;
		} else {
			if (affix_pin_code.Length) {
				pin = affix_pin_code.Code;
				length = affix_pin_code.Length;
			}
		}
	}
	if (pin)
		err = HCI_PINCodeRequestReply(fd, &btdev->bda, length, pin);
	else
		err = HCI_PINCodeRequestNegativeReply(fd, &evt->bda);
	if (btdev)
		btdev_put(btdev);
	hci_close(fd);
	hci_put(hci);
	DBFEXIT;
	return err;
}

int link_key_request(struct Link_Key_Request_Event *evt, int devnum)
{
	int		fd, err, ok = 0;
	struct btdev	*btdev;

	DBFENTER;

	if (affix_ctl_mode & AFFIX_FLAGS_KEY)
		return 0;	// somebody takes care about pin

	fd = hci_open_id(devnum);
	if (fd < 0)
		return fd;

	btdev = btdev_lookup_bda(&evt->bda);
	if (btdev && (btdev->flags & NBT_KEY))
		ok = 1;

	if (ok) {
		err = HCI_LinkKeyRequestReply(fd, &evt->bda, btdev->Link_Key);
	} else {
		err = HCI_LinkKeyRequestNegativeReply(fd, &evt->bda);
	}
	if (btdev)
		btdev_put(btdev);
	hci_close(fd);

	DBFEXIT;
	return err;
}

#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
int read_clock_offset_complete(struct Read_Clock_Offset_Event *evt, int devnum)
{
	hci_con		*con;
	struct btdev	*btdev;

	DBFENTER;

	if (evt->Status) // something wrong, forget...
		return 0;
	
	con = hcc_lookup_chandle_devnum(devnum, evt->Connection_Handle);
	if (!con)
		return 0;

	/* get device from the cache */
	btdev = btdev_lookup_bda(&con->bda);
	if (btdev) {
		// update the clock offset in device cache
		// this makes the connection establishment faster
		// refer to bluetooth specs.
		btdev->Clock_Offset = __btoh16(evt->Clock_Offset);
		// make the cache think that the timestamp is latest
		// for the reconnection logic
		btdev->stamp = jiffies;
		btdev_put(btdev);
	}
	hcc_put(con);
	DBFEXIT;
	return 0;
}
#endif

int event_handler(HCI_Event_Packet_Header *event, int devnum)
{
	int	err = 0;

	DBFENTER;
	DBPRT("Manager has an event: 0x%02x\n", event->EventCode);
	switch (event->EventCode) {
		case HCI_E_CONNECTION_REQUEST:
			err = connection_request((void*)event, devnum);
			break;
		case HCI_E_PIN_CODE_REQUEST:
			pin_code_request((void*)event, devnum);
			break;
		case HCI_E_LINK_KEY_REQUEST:
			link_key_request((void*)event, devnum);
			break;
#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
		case HCI_E_READ_CLOCK_OFFSET_COMPLETE:
			read_clock_offset_complete((void*)event, devnum);
			break;
#endif
		default:
			break;
	}
	DBFEXIT;
	return err;
}

/*  This is like the standard daemonize call, but without destroying
 *  fs and files object of current.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
static void daemonize_keep_fs(void)
{
	int fd;
	
	exit_mm(current);
	set_special_pids(1, 1);
	current->tty = NULL;
	for (fd = 0; fd < 255; fd++)
		close(fd);
}
#endif
int affixd_thread(void *startup)
{
	int			ret = 0;
	struct pollfd		fds[2];
	__u8			msg[HCI_MAX_MSG_SIZE];
	__u8			event[HCI_MAX_EVENT_SIZE];

	DBFENTER;
	daemonize("affixd");
	allow_signal(SIGTERM);
	allow_signal(SIGUSR1);

	/*  Open HCI manager */
	fds[0].fd = hci_open_mgr();
	if (fds[0].fd < 0) {
		ret = fds[0].fd;
		fds[1].fd = -1;
		goto exit;
	}
	fds[0].events = POLLIN;
	
	fds[1].fd = hci_open_event();
	if (fds[1].fd < 0) {
		ret = fds[1].fd;
		goto exit;
	}
	fds[1].events = POLLIN;

	up(&exit_sema);
	
	hci_event_mask(fds[1].fd, ALL_EVENTS_MASK &
			~(COMMAND_COMPLETE_MASK | COMMAND_STATUS_MASK |
			  NUMBER_OF_COMPLETE_PACKETS_MASK | FLUSH_OCCURRED_MASK));
	module_put(THIS_MODULE);
	module_put(THIS_MODULE);
	module_put(THIS_MODULE);
	module_put(THIS_MODULE);
	for (;;) {
		ret = hci_poll(fds, 2, -1);
		if (ret > 0) {/* we have something */
			if ((fds[0].revents | fds[1].revents) & (POLLERR | POLLHUP | POLLNVAL))
				break;
			if (fds[1].revents) {
				if (fds[1].revents & POLLIN) { /* HCI event */
					int	devnum;
					hci_recv_event_any(fds[1].fd, &devnum, event, sizeof(event));
					event_handler((void*)event, devnum);
				}
			}
			if (fds[0].revents) {
				if (fds[0].revents & POLLIN) {/* MSG */
					btsys_recv(fds[0].fd, msg, sizeof(msg), 0);
					message_handler((void*)(msg+1));
				}
			}
		}
		if (signal_pending(current)) {
			DBPRT("Got a signal!!!\n");
//			if (sigismember(&current->pending.signal, SIGUSR1)) {
//				sigdelset(&current->pending.signal, SIGUSR1);
//			}
			flush_signals(current);	// flush pending signals
			spin_lock_irq(&current->sighand->siglock);
			recalc_sigpending();
			spin_unlock_irq(&current->sighand->siglock);
			if (mgr_exit)
				break;
		}
	}
	//try_module_get(THIS_MODULE);
	//try_module_get(THIS_MODULE);
	//try_module_get(THIS_MODULE);
	//try_module_get(THIS_MODULE);
exit:
	if (fds[0].fd >= 0)
		hci_close(fds[0].fd);
	if (fds[1].fd >= 0)
		hci_close(fds[1].fd);
	mgr_exit = ret;
	up(&exit_sema);
	DBFEXIT;
	return ret;
}

int __init hci_start_manager(void)
{
	DBFENTER;
	btl_head_init(&btdevs);
	/* start thread here */
	pid = kernel_thread(affixd_thread, NULL, 0);
	if (pid < 0) {
		BTERROR("Unable to create Affix thread\n");
		return pid;
	}
	down(&exit_sema);
	if (mgr_exit < 0) {
		BTERROR("Unable to initialize Affix thread\n");
		return mgr_exit;
	}
	DBFEXIT;
	return 0;
}

void __exit hci_stop_manager(void)
{
	DBFENTER;
	if (pid >= 0) {
		mgr_exit = 1;
		kill_proc(pid, SIGTERM, 1);
		down(&exit_sema);
	}
	btdev_flush();
	DBFEXIT;
}


/*
  Messages (commands) for the HCI Manager
*/

int hci_state_change(hci_struct *hci, int event)
{
	struct hci_state_change	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_STATE_CHANGE;
	msg.devnum = hci->devnum;
	msg.event = event;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int hci_connect_req(hci_con *con)
{
	struct hci_connect_req	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_CONNECT_REQ;
	msg.id = con->id;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

int hci_auth_req(hci_con *con)
{
	struct hci_auth_req	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_AUTH_REQ;
	msg.id = con->id;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}

#ifdef CONFIG_AFFIX_UPDATE_CLOCKOFFSET
int hci_updateclockoffset_req(hci_con *con, __u16 chandle)
{
	struct hci_updateclockoffset_req	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_UPDATECLOCKOFFSET_REQ;
	msg.id = con->id;
	msg.chandle = chandle;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}
#endif

int hci_disconnect_req(hci_con *con, __u8 reason)
{
	struct hci_disconnect_req	msg;

	DBFENTER;
	msg.hdr.opcode = HCICTL_DISCONNECT_REQ;
	msg.id = con->id;
	msg.reason = reason;
	hci_deliver_msg(&msg, sizeof(msg));
	DBFEXIT;
	return 0;
}


/* Neighbor Device management */
struct btdev *btdev_alloc(void)
{
	struct btdev	*btdev;

	btdev = kmalloc(sizeof(*btdev), GFP_ATOMIC);
	if (btdev == NULL)
		return NULL;
	memset(btdev, 0, sizeof(*btdev));
	return btdev;
}

void btdev_free(struct btdev *btdev)
{
	kfree(btdev);
}

struct btdev *__btdev_lookup_bda(BD_ADDR *bda)
{
	struct btdev	*btdev;

	btl_for_each (btdev, btdevs) {
		if (memcmp(&btdev->bda, bda, 6) == 0)
			break;
	}
	return btdev;
}

struct btdev *btdev_lookup_bda(BD_ADDR *bda)
{
	struct btdev	*btdev;

	DBFENTER;

	btl_read_lock(&btdevs);
	btdev = __btdev_lookup_bda(bda);
	if (btdev)
		btdev_hold(btdev);
	btl_read_unlock(&btdevs);

	DBFEXIT;
	return btdev;
}

struct btdev *btdev_create(BD_ADDR *bda)
{
	struct btdev	*btdev;

	DBFENTER;

	btl_write_lock(&btdevs);
	btdev = __btdev_lookup_bda(bda);
	if (btdev == NULL) {
		btdev = btdev_alloc();
		if (btdev) {
			memcpy(&btdev->bda, bda, 6);
			atomic_set(&btdev->refcnt, 1);
			__btl_add_tail(&btdevs, btdev);
		}
	}
	if (btdev)
		btdev_hold(btdev);
	btl_write_unlock(&btdevs);

	DBFEXIT;
	return btdev;
}

void __btdev_destroy(struct btdev *btdev)
{
	__btl_unlink(&btdevs, btdev);
	btdev_free(btdev);
}

/*
  flush all entries in the device list
*/
void btdev_flush(void)
{
	struct btdev	*btdev, *next;

	DBFENTER;

	btl_write_lock(&btdevs);
	btl_for_each_safe (btdev, btdevs, next) {
		__btdev_put(btdev);
	}
	btl_write_unlock(&btdevs);

	DBFEXIT;
}


/* 
 * PIN Code management
 */
int affix_add_pin(struct PIN_Code *pin)
{
	struct btdev	*btdev;

	DBFENTER;

	if (memcmp(&pin->bda, &affix_pin_code.bda, 6) == 0) {
		/* default */
		affix_pin_code = *pin;
	} else {
		btdev = btdev_create(&pin->bda);
		if (btdev == NULL)
			return -ENOMEM;

		btdev->PIN_Code_Length = pin->Length;
		memcpy(btdev->PIN_Code, pin->Code, 16);
		btdev->flags |= NBT_PIN;
		btdev_put(btdev);
	}
	DBFEXIT;
	return 0;
}

int affix_remove_pin(BD_ADDR *pbda)
{
	int		err = 0;
	struct btdev	*btdev;
	BD_ADDR		bda;

	DBFENTER;

	btl_read_lock(&btdevs);
	if (pbda == NULL) {
		// remove all
		btl_for_each (btdev, btdevs)
			btdev->flags &= ~NBT_PIN;
	} else {
		err = copy_from_user(&bda, pbda, sizeof(BD_ADDR));
		if (err)
			goto exit;
		if (bda_zero(&bda))
			affix_pin_code.Length = 0;
		else {
			btdev = __btdev_lookup_bda(&bda);
			if (btdev == NULL) {
				err = -ENOENT;
				goto exit;
			}
			btdev->flags &= ~NBT_PIN;
		}
	}
exit:
	btl_read_unlock(&btdevs);
	DBFEXIT;
	return err;
}

int affix_add_key(struct link_key *key)
{
	struct btdev	*btdev;

	DBFENTER;

	btdev = btdev_create(&key->bda);
	if (btdev == NULL)
		return -ENOMEM;

	btdev->Key_Type = key->key_type;
	memcpy(btdev->Link_Key, key->key, 16);
	btdev->flags |= NBT_KEY;
	btdev_put(btdev);
	DBFEXIT;
	return 0;
}

int affix_remove_key(BD_ADDR *pbda)
{
	int		err = 0;
	struct btdev	*btdev;
	BD_ADDR		bda;

	DBFENTER;
	if (pbda) {
		err = copy_from_user(&bda, pbda, sizeof(BD_ADDR));
		if (err)
			return err;
	}
	btl_read_lock(&btdevs);
	if (pbda) {
		btdev = __btdev_lookup_bda(&bda);
		if (btdev)
			btdev->flags &= ~NBT_KEY;
	} else {
		// remove all
		btl_for_each (btdev, btdevs)
			btdev->flags &= ~NBT_KEY;
	}
	btl_read_unlock(&btdevs);
	DBFEXIT;
	return err;
}

