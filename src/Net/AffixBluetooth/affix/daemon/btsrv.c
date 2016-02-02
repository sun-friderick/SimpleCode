/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 - 2004 Nokia Corporation
   Authors: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
	    Deak <ext-imre.deak@nokia.com>

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
   $Id: btsrv.c,v 1.26 2004/05/26 10:24:21 kassatki Exp $

   User space daemon for making local services accessable to remote devices   

   Fixes:
   		Imre Deak <ext-imre.deak@nokia.com>
		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <affix/config.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <sys/mman.h>

/* bluetooth stuff */
#include <affix/bluetooth.h>
#include <affix/btcore.h>

#include "btsrv.h"

#define DEF_CONFIG_FILE		"/etc/affix/btsrv.conf"

struct btservice	services[MAX_SERVICE_NUM];
struct btdevice		devices[MAX_DEVICE_NUM];

int		updevnums = 0;

char		*config_file = NULL;
char		*config_expr = NULL;
int		initdev = 0;
int		startsvc = 0;
int		managepin = 0;
int		managekey = 0;

/* local */
int 		svcnums;
int		devnums;
int		efd, mfd;
char		confdir[80];
int		gobackground = 0;
int		restart = 0;

struct btprofile profiles[] = {
        {
		"SerialPort", 
		SDP_UUID_SERIAL_PORT, 
		0x0000,
		SDP_UUID_SERIAL_PORT,
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"DialupNetworking", 
		SDP_UUID_DUN, 
		SDP_UUID_GENERIC_NETWORKING,
		SDP_UUID_DUN, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
	{
		"LANAccess", 
		SDP_UUID_LAN, 
		SDP_UUID_GENERIC_NETWORKING,
		SDP_UUID_LAN, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"OBEXFiletransfer", 
		SDP_UUID_OBEX_FTP, 
		0x0000, 
		SDP_UUID_OBEX_FTP, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
	{
		"OBEXObjectPush", 
		SDP_UUID_OBEX_PUSH, 
		0x0000, 
		SDP_UUID_OBEX_PUSH, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
	{
		"PANPanu", 
		SDP_UUID_PANU, 
		0x0000, 
		SDP_UUID_PANU, 
		BTPROTO_L2CAP, 
		sdpreg_pan
	},
	{
		"PANGn", 
		SDP_UUID_GN, 
		0x0000, 
		SDP_UUID_GN, 
		BTPROTO_L2CAP, 
		sdpreg_pan
	},
	{
		"PANNap", 
		SDP_UUID_NAP, 
		0x0000, 
		SDP_UUID_NAP, 
		BTPROTO_L2CAP, 
		sdpreg_pan
	},
        {
		"FAX", 
		SDP_UUID_FAX, 
		SDP_UUID_GENERIC_TELEPHONY, 
		SDP_UUID_FAX, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"HandsFree", 
		SDP_UUID_HANDSFREE, 
		SDP_UUID_GENERIC_AUDIO, 
		SDP_UUID_HANDSFREE, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"HandsFreeAG", 
		SDP_UUID_HANDSFREE_AG, 
		SDP_UUID_GENERIC_AUDIO, 
		SDP_UUID_HANDSFREE, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"Headset", 
		SDP_UUID_HEADSET, 
		SDP_UUID_GENERIC_AUDIO, 
		SDP_UUID_HEADSET, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
        {
		"HeadsetAG", 
		SDP_UUID_HEADSET_AG, 
		SDP_UUID_GENERIC_AUDIO, 
		SDP_UUID_HEADSET, 
		BTPROTO_RFCOMM, 
		sdpreg_rfcomm
	},
	{NULL,}
};


int btsrv_devconf(int fd);

void printusage(void)
{
	printf("usage: btsrv "
			"[-d] "
			"[-v] "
			"[--config config_file | -C config_file] "
			"[--expression config_expr | -e config_expr] "
			"[--initdev | -i] "
			"[--noinitdev] "
			"[--startsvc | -s] "
			"[--nostartsvc] "
			"[--managepin | -p] "
			"[--nomanagepin] "
			"[--managekey | -k]"
			"[--nomanagekey] "
			"\n"
	      );
}

int start_service(struct btservice *svc)
{
	struct sockaddr_affix	saddr;
	int		err, fd;
	socklen_t	len = sizeof(saddr);

	if (!svc->active || svc->running)
		return 0;

	svc->srv_fd = -1;

	if (!(svc->flags & SRV_FLAG_SOCKET))
		goto skip_socket;

	if (svc->profile->proto == BTPROTO_RFCOMM) {
		fd = socket(PF_AFFIX, SOCK_STREAM, BTPROTO_RFCOMM);
		if (fd < 0) {
			BTERROR("Unable to create RFCOMM socket");
			return -1;
		}
		if (svc->flags & SRV_FLAG_RFCOMM_TTY) {
			err = rfcomm_set_type(fd, RFCOMM_TYPE_BTY);
			if (err < 0) {
				BTERROR("Unable to set RFCOMM interface type to tty");
				return -1;
			}
		}
	} else {
		fd = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (fd < 0) {
			BTERROR("Unable to create L2CAP socket");
			return -1;
		}
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
		BTERROR("Unable to set non blocking mode for RFCOMM socket");
		return -1;
	}
	saddr.family = AF_AFFIX;
	saddr.devnum = HCIDEV_ANY;
	saddr.port = svc->port;	/* if 0 - dynamically allocated */
	saddr.bda = BDADDR_ANY;
	err = bind(fd, (struct sockaddr*)&saddr, sizeof(saddr));
	if (err < 0) {
		BTERROR("Unable to bind address");
		return -1;
	}

	setsockopt(fd, SOL_AFFIX, BTSO_SECURITY, &svc->security, sizeof(int));

	err = getsockname(fd, (struct sockaddr*)&saddr, &len);
	if (err < 0) {
		BTERROR("Unable to get address info");
		return -1;
	}

	BTINFO("Bound service %s to port %d", svc->name, saddr.port);

	err = listen(fd, 5);
	if (err < 0) {
		BTERROR("Unable to listen for connection requests");
		return -1;
	}

	svc->srv_fd = fd;
	svc->port = saddr.port;

skip_socket:
#if defined(CONFIG_AFFIX_SDP)
	if (sdpreg_register(svc) < 0 )
		return -1;
#endif	
	svc->running = 1;
	return 0;
}

void stop_service(struct btservice *svc)
{
	if (!svc->running)
		return;
	svc->running = 0;
#if defined(CONFIG_AFFIX_SDP)
	sdpreg_unregister(svc);
#endif
	if (svc->srv_fd != -1)
		close(svc->srv_fd);
}

void stop_services(void)
{
	int	i;

	for (i = 0; i < svcnums; i++)
		stop_service(&services[i]);
#if defined(CONFIG_AFFIX_SDP)
	sdpreg_cleanup();
#endif
}

int start_services(void)
{
	int	i;

	if (!startsvc || updevnums < 1)
		return 0;
#if defined(CONFIG_AFFIX_SDP)
	if (sdpreg_init() != 0) {
		BTERROR("SDP initialization failed");
		return -1;
	}
#endif
	for (i = 0; i < svcnums; i++)
		if (start_service(&services[i]) < 0)
			goto fail;
	return 0;
fail:
	stop_services();
	return -1;
}

char *btsrv_format_cmd(const char *cmd, int conid, int line, BD_ADDR *bda, int port)
{
        char    *formatted_cmd;
	int	i, j, res;
 
        formatted_cmd = (char *)malloc(MAX_CMD_LEN + 1);
        if (!formatted_cmd)
                return NULL;
        i = j = 0;
        while (cmd[i]) {
                switch (cmd[i]) {
                case '%':
                        switch(cmd[i + 1])
                        {
                        case 'a':
                                res = snprintf(&formatted_cmd[j], MAX_CMD_LEN - j, "%s", bda2str(bda));
                                break;
                        case 'l':
                                res = snprintf(&formatted_cmd[j],
					      MAX_CMD_LEN - j, "%d", line);
                                break;
			case 'i':
                                res = snprintf(&formatted_cmd[j],
					      MAX_CMD_LEN - j, "%d", conid);
                                break;

                        case 'c':
                                res = snprintf(&formatted_cmd[j],
					       MAX_CMD_LEN - j, "%d", port);
                                break;
                        case '%':
                                formatted_cmd[j] = '%';
				res = j < MAX_CMD_LEN ? 1 : -1;
                                break;
			default:
				res = -1;
                        }
                        i++;	/* skip % sign */
                        break;
                default:
                        formatted_cmd[j] = cmd[i];
			res = j < MAX_CMD_LEN ? 1 : -1;
                }
		if (res == -1) {
			free(formatted_cmd);
			return NULL;
		}
		j += res;
                i++;
        }
        formatted_cmd[j] = '\0';
        return formatted_cmd;
}

int execute_cmd(char *cmd, int sock_fd, int port, BD_ADDR *bd_addr, int flags)
{
	char *formatted_cmd;
	int  line;
	int  fd = -1;
	int  conid;

	conid = sock_fd;
	if (flags & SRV_FLAG_RFCOMM_TTY) {
		char dev[32];

		line = rfcomm_open_tty(sock_fd, RFCOMM_BTY_ANY);
		close(sock_fd);
		if (line < 0) {
			BTERROR("Unable to bind a socket to virtual port device");
			return -1;
		}
		sprintf(dev, "/dev/bty%d", line);
		BTINFO("Socket bound to virtual port device %s", dev);
		if (flags & SRV_FLAG_STD) {
			fd = open(dev, O_RDWR);
			if (fd < 0) {
				BTERROR("Unable to open port device %s", dev);
				return -1;
			}
		}
	} else {
		fd = sock_fd;
		line = -1;
	}
	if (flags & SRV_FLAG_STD) {
		BTINFO("Socket multiplexed to stdin/stdout");
		if (affix_dup2std(fd) < 0) {
			BTERROR("Unable to duplicate socket descriptor to stdin/stdout\n");
			return -1;
		}
	}
	formatted_cmd = btsrv_format_cmd(cmd, conid, line, bd_addr, port);
	if (formatted_cmd) {
		BTINFO("Execute %s", formatted_cmd);
		/*  the following does not return normally  */
		execl("/bin/sh", "sh", "-c", formatted_cmd, NULL);
		BTERROR("Exec error");
		return -1;
	}
	BTERROR("Parse error in the command line");
	return -1;
}
/*
 * Event processing block
 */

int event_pin_code_request(struct PIN_Code_Request_Event *evt, int devnum)
{
	int		fd, err, flags;
	FILE		*fp;
	char		pin[32], cmdline[100];
	char		name[248];

	if (!managepin)
		return 0;
	err = hci_get_flags_id(devnum, &flags);
	if (err)
		return 0;
	fd = hci_open_id(devnum);
	if (fd < 0)
		return fd;
#if 1
	{
		/* get device name first */
		INQUIRY_ITEM	dev;
		dev.bda = evt->bda;
		dev.PS_Repetition_Mode = 0x00;
		dev.PS_Mode = 0x00;
		dev.Clock_Offset = 0x00;
		err = HCI_RemoteNameRequest(fd, &dev, name);
		if (err) {
			BTDEBUG("Name request failed: %s", hci_error(err));
			name[0] = '\0';
			//strcpy(name, bda2str(&evt->bda));
			//close(fd);
			//return err;
		}
	}
#else
	name[0] = '\0';
	//strcpy(name, bda2str(&evt->bda));
#endif

	if (!(flags & HCI_SECURITY_PAIRABLE))
		goto err;

	sprintf(cmdline, "/etc/affix/btsrv-gui pin \"%s\" %s", name, bda2str(&evt->bda));
	DBPRT("cmdline: [%s]", cmdline);
	fp = popen(cmdline, "r");
	if (!fp) {
		BTERROR("popen() failed");
		goto err;
	}
	err = fscanf(fp, "%s", pin);
	if (err == EOF) {
		BTERROR("fscanf() failed");
		pclose(fp);
		goto err;
	}
	DBPRT("Got PIN code from pipe: %s, len: %d", pin, strlen(pin));
	pclose(fp);
	err = HCI_PINCodeRequestReply(fd, &evt->bda, strlen(pin), pin);
	if (err) {
		BTERROR("unable to set pin code: %d", err);
	}
	close(fd);
	return 0;
err:
	err = HCI_PINCodeRequestNegativeReply(fd, &evt->bda);
	close(fd);
	return 0;
}

int event_link_key_request(struct Link_Key_Request_Event *evt, int devnum)
{
	int		fd, err;
	btdev_struct	*btdev;

	fd = hci_open_id(devnum);
	if (fd < 0)
		return fd;
	btdev_cache_reload();
	btdev = btdev_cache_lookup(&evt->bda);
	if (btdev && (btdev->flags & BTDEV_KEY)) {
		err = HCI_LinkKeyRequestReply(fd, &evt->bda, btdev->link_key);
	} else {
		err = HCI_LinkKeyRequestNegativeReply(fd, &evt->bda);
	}
	if (err) {
		BTERROR("unable to set link key: %d", err);
	}
	btdev_cache_unlock();
	hci_close(fd);
	return err;
}

int event_link_key_notification(struct Link_Key_Notification_Event *evt, int devnum)
{
	btdev_struct	*btdev;

	if (!managekey)
		return 0;
	btdev_cache_reload();
	btdev = btdev_cache_add(&evt->bda);
	if (btdev == NULL)
		return -1;
	btdev->key_type = evt->Key_Type;
	memcpy(btdev->link_key, evt->Link_Key, 16);
	btdev->flags |= BTDEV_KEY;
	btdev_cache_save();
	return 0;
}


int event_handler(int fd)
{
	uint8_t			buf[HCI_MAX_EVENT_SIZE];
	int			err, devnum;
	HCI_Event_Packet_Header	*hdr = (void*)buf;

	err = hci_recv_event_any(fd, &devnum, buf, sizeof(buf));
	if (err < 0)
		return err;
	DBPRT("got event: %#x, from: %d", hdr->EventCode, devnum);
	switch (hdr->EventCode) {
		case HCI_E_LINK_KEY_REQUEST:
			event_link_key_request((void*)hdr, devnum);
			break;
		case HCI_E_LINK_KEY_NOTIFICATION:
			event_link_key_notification((void*)hdr, devnum);
			break;
		case HCI_E_PIN_CODE_REQUEST:
			event_pin_code_request((void*)hdr, devnum);
			break;
		default:
			break;
	}

	return 0;
}

int message_handler(int fd)
{
	uint8_t			buf[HCI_MAX_MSG_SIZE];
	struct hci_msg_hdr	*msg = (void*)(buf+1);
	int			err;

	err = recv(fd, buf, sizeof(buf), 0);
	if (err < 0)
		return err;
	DBPRT("got message: %x", msg->opcode);
	if (msg->opcode == HCICTL_STATE_CHANGE) {
		struct hci_state_change	*cmd = (void*)msg;

		if (cmd->event == HCIDEV_UP) {
			updevnums++;
			BTINFO("device %d is up, count: %d\n", cmd->devnum, updevnums);
			err = btsrv_devconf(cmd->devnum);
			if (err) {
				BTERROR("Unable to configure device: %d", cmd->devnum);
				return err;
			}
			start_services();
		} else if (cmd->event == HCIDEV_DOWN) {
			updevnums--;
			BTINFO("device %d is down, count: %d\n", cmd->devnum, updevnums);
			if (updevnums < 1) {
				// no active devices.. stop services
				stop_services();
			}
		}
	}
	return 0;
}

int handle_connections(void)
{
	int	i, err = 0;
	int	fd_cnt;
	fd_set	fds_read;
	int	max_fd;

	for(;;) {
		FD_ZERO(&fds_read);
		max_fd = -1;
		/* set service fd */
		for (i = 0; i < svcnums; i++) {
			if (services[i].running && services[i].srv_fd != -1) {
				FD_SET(services[i].srv_fd, &fds_read);
				if (max_fd < services[i].srv_fd)
					max_fd = services[i].srv_fd;
			}
		}
		/* set event fd */
		FD_SET(efd, &fds_read);
		if (max_fd < efd)
			max_fd = efd;
		FD_SET(mfd, &fds_read);
		if (max_fd < mfd)
			max_fd = mfd;
		/* poll data */
		fd_cnt = select(max_fd + 1, &fds_read, NULL, NULL, NULL);
		if (fd_cnt < 0) {
			if (errno != EINTR) {
				BTERROR("Select error: %s", strerror(errno));
				err = -1;
			}
			goto exit;
		} else if (fd_cnt == 0) {
			//should not happen
		}
		if (FD_ISSET(efd, &fds_read)) {
			event_handler(efd);
		}
		if (FD_ISSET(mfd, &fds_read)) {
			message_handler(mfd);
		}
		for (i = 0; fd_cnt && (i < svcnums); i++) {
			if (services[i].running && services[i].srv_fd != -1 && 
					FD_ISSET(services[i].srv_fd, &fds_read)) {
				int 			newsock;
				struct sockaddr_affix	caddr;
				socklen_t		calen = sizeof(caddr);
				int			pid = 0;

				newsock = accept(services[i].srv_fd, (struct sockaddr*)&caddr, &calen);
				if (newsock == -1) {
					BTERROR("Accept error");
					continue;
				}
				BTINFO("Connection from %s\nchannel %d (%s Profile)",
						bda2str(&caddr.bda), caddr.port, services[i].name);

				if (services[i].cmd[0] != '\0') {
					pid = fork();
					if (pid == 0) {
						// child
						int res, j;

						for (j = 0; j < svcnums; j++)
							close(services[j].srv_fd);

						res = execute_cmd(services[i].cmd, newsock,
								caddr.port, &caddr.bda, 
								services[i].flags);
						/*  In case of error.. */
						exit(res);
					}
				}
				if (pid < 0) {
					BTERROR("Fork error");
				}
				close(newsock);
				fd_cnt--;
			}
		}
	}
exit:
	return err;
}

/*
 * read HCI events and kernel messages
 */

int start_event_handler(void)
{
	uint64_t	mask = 0;

	efd = hci_open_event();
	if (efd < 0) {
		BTERROR("Unable to open hci(NULL)");
		return efd;		
	}
	mfd = hci_open_mgr();
	if (mfd < 0) {
		BTERROR("Unable to open mgr");
		close(efd);
		return mfd;
	}
	if (managepin) {
		hci_set_mode(mfd, AFFIX_MODE_PIN);
		mask |= PIN_CODE_REQUEST_MASK;
	}
	if (managekey) {
		hci_set_mode(mfd, AFFIX_MODE_KEY);
		mask |= LINK_KEY_NOTIFICATION_MASK | LINK_KEY_REQUEST_MASK;
	}
	if (mask)
		hci_event_mask(efd, mask);

	return 0;
}

void stop_event_handler(void)
{
	close(mfd);
	close(efd);
}

int btsrv_setdev(int fd, struct btdevice *dev)
{
	int	err;

	err = HCI_WriteSecurityMode(fd, dev->security);
	if (err < 0) {
		BTERROR("hci_set_secmode() failed\n");
		return err;
	}
	err = hci_set_role(fd, dev->role);
	if (err < 0) {
		BTERROR("hci_set_role() failed\n");
		return err;
	}
	err = hci_set_pkttype(fd, dev->pkt_type);
	if (err < 0) {
		BTERROR("hci_set_pkttype() failed\n");
		return err;
	}
	err = HCI_WriteClassOfDevice(fd, dev->cod);
	if (err) {
		BTERROR("WriteScanEnable error\n");
		return err;
	}
	if (dev->btname) {
		err = HCI_ChangeLocalName(fd, dev->btname);
		if (err) {
			BTERROR("WriteScanEnable error\n");
			return err;
		}
	}
	err = HCI_WriteScanEnable(fd, dev->scan);
	if (err) {
		BTERROR("WriteScanEnable error\n");
		return err;
	}
	return 0;
}

int btsrv_devconf(int devnum)
{
	int			err = 0, i, fd;
	struct btdevice		*dev;
	struct hci_dev_attr	da;

	if (!initdev)
		return 0;

	err = hci_get_attr_id(devnum, &da);
	if (err)
		return err;
	fd = hci_open(da.name);
	if (fd < 0) {
		BTERROR("hci_open_id() failed: %s\n", da.name);
		return fd;
	}
	for (i = 0; i < devnums; i++) {
		dev = &devices[i];
		if ((i == 0 && dev->name[0] == '*') ||
				(strcasecmp(dev->name, da.name) == 0)) {
			err = btsrv_setdev(fd, dev);
			if (err)
				break;
		}
	}
	close(fd);
	return err;
}

int btsrv_devinit(void)
{
	int			i, err, num;
	struct hci_dev_attr	da;
	int			devs[HCI_MAX_DEVS];

	num = hci_get_devs(devs);
	if (num < 0) {
		BTERROR("Unable to get device info\n");				
		return num;
	}
	for (i = 0; i < num; i++) {
		err = hci_get_attr_id(devs[i], &da);
		if (err < 0) {
			BTERROR("hci_get_attr() failed: %d\n", devs[i]);
			continue;
		}
		if (!(da.flags & HCI_FLAGS_UP))
			continue;
		updevnums++;
		err = btsrv_devconf(da.devnum);
		if (err)
			return err;
	}
	return 0;
}

int btsrv_startup(void)
{
	if (start_event_handler() < 0) {
		BTERROR("Cannot start event handler");
		return -1;
	}
	/* initialize devices */
	if (btsrv_devinit()) {
		BTERROR("Unable to initialize devices\n");
		return -1;
	}
	if (start_services() < 0) {
		BTERROR("Cannot start services");
		return -1;
	}
	return 0;
}

int btsrv_cleanup(void)
{
	int		i;
	struct btdevice	*dev;

	BTINFO("Terminating ..");
	stop_event_handler();
	stop_services();
	free(config_file); config_file = NULL;
	free(config_expr); config_expr = NULL;
	for (i = 0; i < svcnums; i++) {
		if (services[i].name)
			free(services[i].name);
		if (services[i].prov)
			free(services[i].prov);
		if (services[i].desc)
			free(services[i].prov);
	}
	for (i = 0; i < devnums; i++) {
		dev = &devices[i];
		if (dev->btname)
			free(dev->btname);
	}
	memset(services, 0, sizeof(services));
	memset(services, 0, sizeof(devices));
	return 0;
}

void get_cmd_opts(int argc, char **argv)
{
	int	c, lind = 0;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"daemon", 0, 0, 'd'},
		{"config", 1, 0, 'C'},
		{"debug", 0, 0, 'v'},
		{"initdev", 0, 0, 'i'},
		{"noinitdev", 0, 0, 'I'},
		{"startsvc", 0, 0, 's'},
		{"nostartsvc", 0, 0, 'S'},
		{"managepin", 0, 0, 'p'},
		{"nomanagepin", 0, 0, 'P'},
		{"managekey", 0, 0, 'k'},
		{"nomanagekey", 0, 0, 'K'},
		{"expression", 0, 0, 'e'},
		{0, 0, 0, 0}
	};

	optind = 0;

	for (;;) {
		c = getopt_long(argc, argv, "hdpskvC:e:", opts, &lind);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				printusage();
				exit(0);
				break;
			case 'd':	
				gobackground = 1;
				break;
			case 'C':
				config_file = strdup(optarg);
				break;
			case 'v':
				verboseflag = 1;
				break;
			case 'p':
				managepin = 1;
				break;
			case 'P':
				managepin = 0;
				break;
			case 'k':
				managekey = 1;
				break;
			case 'K':
				managekey = 0;
				break;
			case 'i':
				initdev = 1;
				break;
			case 'I':
				initdev = 0;
				break;
			case 's':
				startsvc = 1;
				break;
			case 'S':
				startsvc = 0;
				break;
			case 'e':
				config_expr = strdup(optarg);
				break;
			case '?':
				fprintf(stderr, "Unknown option: %c\n", optopt);
				exit(1);
				break;
		}
	}
}

void signal_handler(int sig)
{
	BTINFO("Sig handler : %d", sig);
	if (sig != SIGHUP)
		exit(0);
	restart = 1;
}

void do_exit(void)
{
	btsrv_cleanup();
}

int main(int argc, char **argv)
{
	int	err;

	if (affix_init(argc, argv, LOG_DAEMON)) {
		BTERROR("Affix initialization failed\n");
		return 1;
	}

	BTINFO("%s started [%s].", argv[0], affix_version);
	
restart:
	// read first to get config_file
	get_cmd_opts(argc, argv);

	if (config_expr) {
		// use command line configuration
		if (btsrv_read_config_buf(config_expr, &svcnums, &devnums)) {
			BTERROR("Invalid config expr: %s", config_expr);
			return -1;
		}
		startsvc = 1;	// default action for this
	} else {
		if (!config_file)
			config_file = strdup(DEF_CONFIG_FILE);
		// read config_file
		if (btsrv_read_config(config_file, &svcnums, &devnums)) {
			BTERROR("Invalid config file: %s", config_file);
			return -1;
		}
	}
	// read again to overwride config file setting by command line
	get_cmd_opts(argc, argv);
	
	/* test X access */
	if (managepin) {
		err = system("/etc/affix/btsrv-gui try 2>/dev/null");
		if (err) {
			BTERROR("Unable to launch to /etc/affix/btsrv-gui\n"
				"-> maybe unable to connect to X-server or python-gtk problems\n"
				"-> Disable options requiring btsrv-gui (e.g. give --nomanagepin)\n");
			return 1;
		}
	}

	if (startsvc && svcnums < 1)
		BTINFO("No services found in %s", config_file);

	if (btsrv_startup() < 0) {
		return -1;
	}

	if (!restart) {
		if (gobackground)
			affix_background();
		
		atexit(do_exit);	// exit handler
		
		signal(SIGHUP, signal_handler);
		signal(SIGINT, signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGABRT, signal_handler);
		signal(SIGQUIT, signal_handler);
	}

	signal(SIGCHLD, SIG_IGN);
	restart = 0;
	if (handle_connections() < 0) {
		BTERROR("Unable to listen to incoming connections");
	}
	signal(SIGCHLD, SIG_DFL);

	if (restart) {
		BTINFO("restarting...");
		btsrv_cleanup();	// cleanup first
		goto restart;
	}
	return 0;
}

