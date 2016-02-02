/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Authors:
   		Muller Ulrich <ulrich.muller@nokia.com> 
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

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
   $Id: btpan.c,v 1.13 2004/06/15 10:06:05 hranki Exp $

   panctl.c - pan filter control

   The PAN kernel module supports configuration of protocol/multicast filters via ioctl.
   Therefore datatypes with a constant size are needed.

   Filter settings are always send to all remote devices. They are stored internally to
   configure a remote device if the connection is established after setting the filter.
   By default, no protocol filters are in use. At a PAN User, the
   multicast configuration of the network device is used as multicast address
   filter configuration; at a Group Ad-hoc Network/Network Access Point, no multi-
   cast address filters are in use by default. Setting a filter at a Group Ad-hoc
   Network/Network Access Point also affects packets exchanged between PAN users
   connected to the same Group Ad-hoc Network/Network Access Point. The Ethernet
   Broadcast address FF:FF:FF:FF:FF:FF is never filtered out. If a filter setting
   is rejected by the remote device, the remote device keeps its previous setting
   according to the specification. To avoid side effects, the current implemen-
   tation always resets the affected filter in this case.

   This program set the filters of the local pan network device and get the stored values.

*/

/************************************************************************************************/
/* includes */

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
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>

#include <stdint.h>

#include <features.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>
#include <affix/utils.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>


extern struct command cmds[];

int	sdpmode = 1;

enum {mode_unknown, mode_m_stop, mode_m_next, mode_p_stop, mode_p_next};

void pan_usage (void) {
	printf( "See *btctl help pan*\n");
	exit(0);
}

/* convert given ethernet address from str into ea, return 0 on success */
int get_addr(char *str, ETH_ADDR *ea)
{
	unsigned int n[6];

	if(sscanf(str, "%x:%x:%x:%x:%x:%x", &n[0], &n[1], &n[2], &n[3], &n[4], &n[5]) != 6)
		return -EFAULT;

	(*ea)[0] = n[0];
	(*ea)[1] = n[1];
	(*ea)[2] = n[2];
	(*ea)[3] = n[3];
	(*ea)[4] = n[4];
	(*ea)[5] = n[5];

	return 0;
}

/* convert given string into __u16, convert to network byte order, return 0 on success */
int get_short(char *str, __u16 *n)
{
	long int result = strtol(str, (char **)NULL, 0); /* autodetects base */
	*n = htons( result & 0xFFFF );
	return 0;
}

int str2role(char *arg)
{
	if (strcasecmp(arg, "panu") == 0)
		return AFFIX_PAN_PANU;
	if (strcasecmp(arg, "nap") == 0)
		return AFFIX_PAN_NAP;
	if (strcasecmp(arg, "gan") == 0)
		return AFFIX_PAN_GN;
	if (strcasecmp(arg, "gn") == 0)
	        return AFFIX_PAN_GN;
	        
	return 0;
}

char *role2str(int role)
{
	if (role == AFFIX_PAN_PANU)
		return "PANU";
	if (role == AFFIX_PAN_NAP)
		return "NAP";
	if (role == AFFIX_PAN_GN)
		return "GN";
	return "(?)";
}

int cmd_pan_init(struct command *cmd)
{
	int	err, mode = 0, i;

	__argv = &__argv[optind];

	if (cmd->cmd == 0)
		mode = AFFIX_PAN_PANU;

	for (i = 0; *__argv; __argv++, i++) {
		if (i == 0) {
			mode = str2role(*__argv);
			if (!mode) {
				fprintf(stderr, "invalid role: %s\n", *__argv);
				return 1;
			}
		} else {
			/* flags */
			if (strcasecmp(*__argv, "auto") == 0)
				mode |= AFFIX_PAN_AUTO;
		}
	}
	err = affix_pan_init(btdev, mode);
	if (err) {
		BTERROR("PAN init/stop failed\n");
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	return err;	
}

int cmd_pan_discovery(struct command *cmd)
{
	int		role = AFFIX_PAN_NAP;
	int		fd, i, found = 0;
	__u32		length = 8;
	int		err;
	INQUIRY_ITEM	devs[20];
	char		*devnames[20];
	char		name[248];
	__u8		num;
	uint16_t	ServiceID;
	uint16_t	count;
	slist_t		*searchList = NULL;
	slist_t		*attrList = NULL;
	slist_t		*svcList = NULL;
	sdpsvc_t	*svcRec;
	struct sockaddr_affix	saddr;

	__argv = &__argv[optind];

	if (*__argv) {
		role = str2role(*__argv);
		if (!role) {
			fprintf(stderr, "invalid role: %s\n", *__argv);
			return 1;
		}
		if (*(++__argv))
			sscanf(*__argv, "%x", &length);
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	printf("Searching %d sec ...\n", length);
	err = HCI_Inquiry(fd, length, 20, devs, &num);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	if (num == 0) {
		printf("done.\nNo devices found.\n");
	} else {
		printf("Searching done. Checking for service %s ...\n", role2str(role));
		btdev_cache_reload();
		btdev_cache_retire();
		for (i = 0; i < num; i++) {
			if (!(devs[i].Class_of_Device & HCI_COD_NETWORKING))
				continue;
			saddr.family = PF_AFFIX;
			saddr.bda = devs[i].bda;
			saddr.devnum = HCIDEV_ANY;
			printf("% 2d: %s ", ++found, bda2str(&saddr.bda));
			devs[i].Clock_Offset |= 0x8000;
			err = HCI_RemoteNameRequest(fd, &devs[i], name);
			if (!err)
				devnames[i] = strdup(name);
			else 
				devnames[i] = NULL;
			printf("(%s)... ", name);
#if defined(CONFIG_AFFIX_SDP)
			if (role == AFFIX_PAN_NAP)
				ServiceID = SDP_UUID_NAP;
			else if (role == AFFIX_PAN_GN)
				ServiceID = SDP_UUID_GN;
			else
				ServiceID = SDP_UUID_PANU;

			/* search for service ServiceID */
			s_list_append_uuid16(&searchList, ServiceID);
			/* set attributes to find */
			s_list_append_uint(&attrList, SDP_ATTR_SERVICE_RECORD_HANDLE);
			s_list_append_uint(&attrList, SDP_ATTR_PROTO_DESC_LIST);
			err = __sdp_search_attr_req(&saddr, searchList, IndividualAttributes, attrList, 0xffff, &svcList, &count);
			s_list_destroy(&searchList);
			s_list_free(&attrList);
			hci_disconnect(&saddr);
			if (err) {
				//fprintf(stderr, "%s\n", sdp_error(err));
				printf("no\n");
				continue;
			}
			if (count == 0) {
				printf("no\n");
				continue;
			}
			printf("yes\n");
			svcRec = s_list_dequeue(&svcList);
			sdp_free_svc(svcRec);
			sdp_free_svclist(&svcList);
			//hci_get_conn();
#else
			fprintf(stderr, "Affix SDP support disabled at compile time!\n");
			break;
#endif
			__btdev_cache_add(devs[i].bda, devs[i].Class_of_Device, devnames[i]);
			if (devnames[i])
				free(devnames[i]);
		}
		btdev_cache_save();
	}
	close(fd);
	return 0;
}

int cmd_pan_connect(struct command *cmd)
{
	int		err, role = AFFIX_PAN_NAP;
	BD_ADDR		bda;
	struct sockaddr_affix	saddr;

	__argv = &__argv[optind];

	if (!*__argv) {
		printf("Address missing\n");
		print_usage(cmd);
		return 1;
	}
	err = btdev_get_bda(&bda, *__argv);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}
	if (*(++__argv)) {
		/* role */
		role = str2role(*__argv);
		if (!role) {
			fprintf(stderr, "invalid role: %s\n", *__argv);
			return 1;
		}
	}

	saddr.family = PF_AFFIX;
	saddr.bda = bda;
	saddr.devnum = HCIDEV_ANY;

#if defined(CONFIG_AFFIX_SDP)
	if (sdpmode) {
		uint16_t	ServiceID;
		uint16_t	count;
		slist_t		*searchList = NULL;
		slist_t		*attrList = NULL;
		slist_t		*svcList = NULL;
		sdpsvc_t	*svcRec;

		if (role == AFFIX_PAN_NAP)
			ServiceID = SDP_UUID_NAP;
		else if (role == AFFIX_PAN_GN)
			ServiceID = SDP_UUID_GN;
		else
			ServiceID = SDP_UUID_PANU;

		printf("Connecting to host %s ...\n", bda2str(&bda));

		/* search for service ServiceID */
		s_list_append_uuid16(&searchList, ServiceID);
		/* set attributes to find */
		s_list_append_uint(&attrList, SDP_ATTR_SERVICE_RECORD_HANDLE);
		s_list_append_uint(&attrList, SDP_ATTR_PROTO_DESC_LIST);
		err = __sdp_search_attr_req(&saddr, searchList, IndividualAttributes, attrList, 0xffff, &svcList, &count);
		s_list_destroy(&searchList);
		s_list_free(&attrList);
		if (err) {
			fprintf(stderr, "%s\n", sdp_error(err));
			return -1;
		}
		if (count == 0) {
			printf("services [%s] not found\n", val2str(sdp_service_map, ServiceID));
			return -1;
		}
		printf("Service found\n");
		svcRec = s_list_dequeue(&svcList);
		sdp_free_svc(svcRec);
		sdp_free_svclist(&svcList);
	}
#endif
	saddr.devnum = hci_devnum(btdev);
	err = affix_pan_connect(&saddr, role);
	if (err) {
		if (errno == EINVAL)
			fprintf(stderr, "Connection not allowed in this role.\n");
		else
			fprintf(stderr, "failed.\n");
	} else 
		fprintf(stderr, "connected.\n");
	return 0;
}

int cmd_pan_disconnect(struct command *cmd)
{
	int		err;
	struct sockaddr_affix	saddr;

	saddr.family = PF_AFFIX;
	saddr.bda = BDADDR_ANY;		// means disconnect
	saddr.devnum = HCIDEV_ANY;

	saddr.devnum = hci_devnum(btdev);
	err = affix_pan_connect(&saddr, 0);
	if (err)
		printf("failed\n");
	else
		printf("done.\n");
	return 0;
}

int cmd_panctl(struct command *cmd)
{
	struct ifreq		ifr;
	protocol_filter		pf;
	multicast_filter 	mf;
	int			fd, result, i;
	int			mode = mode_unknown; /* state when parsing options */
	int			p_set = 0; /* protocol filter set? */
	int			m_set = 0; /* multicast filter set? */

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		perror("error opening socket");
		return -1;
	}

	if(__argc - optind < 1)
		pan_usage();

	memset(&pf, 0, sizeof(pf));
	memset(&mf, 0, sizeof(mf));

	strncpy(ifr.ifr_name, __argv[optind], IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';

	/* parse options */
	pf.count = 0;
	mf.count = 0;
	for (i = optind+1; i < __argc; i++) {
		switch(mode) {
			case mode_unknown: /* we dont know what comes next */
				if (__argv[i][0] == 'm') {
					mode = mode_m_next;
					m_set = 1;
				} else if(__argv[i][0] == 'p') {
					mode = mode_p_next;
					p_set = 1;
				} else pan_usage();
				break;
			case mode_m_next: /* we expect either next multicast address start or next command */
				if(__argv[i][0] == 'm') { /* nothing to do */
				} else if(__argv[i][0] == 'p') {
					mode = mode_p_next;
					p_set = 1;
				} else {
					if(mf.count == MULTICAST_FILTER_MAX) {
						printf("too many multicast ranges\n");
						exit(0);
					}
					if(get_addr(__argv[i], &mf.multicast[mf.count][F_START]))
						pan_usage();
					else
						mode = mode_m_stop;
				}
				break;
			case mode_m_stop: /* we expect multicast address stop */
				if(get_addr(__argv[i], &mf.multicast[mf.count][F_STOP]))
					pan_usage();
				else
					mode = mode_m_next;
				mf.count++;
				break;
			case mode_p_next: /* we expect either next protocol range start or next command */
				if(__argv[i][0] == 'm') {
					mode = mode_m_next;
					m_set = 1;
				} else if(__argv[i][0] == 'p') { /* nothing to do */
				} else {
					if(pf.count == PROTOCOL_FILTER_MAX) {
						printf("too many protocol ranges\n");
						exit(0);
					}
					if(get_short(__argv[i], &pf.protocol[pf.count][F_START]))
						pan_usage();
					else
						mode = mode_p_stop;
				}
				break;
			case mode_p_stop: /* we expect protocol range stop */
				if(get_short(__argv[i], &pf.protocol[pf.count][F_STOP]))
					pan_usage();
				else
					mode = mode_p_next;
				pf.count++;
				break;
		}
	}

	if (mode == mode_m_stop || mode == mode_p_stop) /* range stop missing */
		pan_usage();

	/* set protocol filter */
	if(p_set) {
		printf("setting protocol filter...\n");

		ifr.ifr_ifru.ifru_data = (char*) &pf;
		if ((result = ioctl(fd, SIOCSFILTERPROTOCOL, &ifr))) {
			perror("ioctl error");
		}
	}

	/* set multicast filter */
	if(m_set) {
		printf("setting multicast filter...\n");

		ifr.ifr_ifru.ifru_data = (char*) &mf;
		if ((result = ioctl(fd, SIOCSFILTERMULTICAST, &ifr))) {
			perror("ioctl error");
		}
	}

	/* read protocol filter from device and show */
	ifr.ifr_ifru.ifru_data = (char*) &pf;
	if ((result = ioctl(fd, SIOCGFILTERPROTOCOL, &ifr)))
		perror("ioctl error");
	else {
		printf("protocol filter settings: %d entries\n", pf.count);
		for (i = 0; i < pf.count && i < PROTOCOL_FILTER_MAX; i++)
			printf("   0x%04x - 0x%04x\n", ntohs(pf.protocol[i][F_START]), ntohs(pf.protocol[i][F_STOP]));
	}

	/* read multicast filter from device and show */
	ifr.ifr_ifru.ifru_data = (char*) &mf;
	if ((result = ioctl(fd, SIOCGFILTERMULTICAST, &ifr)))
		perror("ioctl error");
	else {
		printf("multicast filter settings: %d entries\n", mf.count);
		for (i = 0; i < mf.count && i < MULTICAST_FILTER_MAX; i++)
			printf("   %02x:%02x:%02x:%02x:%02x:%02x - %02x:%02x:%02x:%02x:%02x:%02x\n",
					mf.multicast[i][F_START][0], mf.multicast[i][F_START][1], mf.multicast[i][F_START][2],
					mf.multicast[i][F_START][3], mf.multicast[i][F_START][4], mf.multicast[i][F_START][5],
					mf.multicast[i][F_STOP][0], mf.multicast[i][F_STOP][1], mf.multicast[i][F_STOP][2],
					mf.multicast[i][F_STOP][3], mf.multicast[i][F_STOP][4], mf.multicast[i][F_STOP][5]);
	}

	return 0;
}

int cmd_help(struct command *cmd)
{
	if (!__argv[optind]) {
		print_usage(cmd);
		exit(0);
	}

	return print_command_usage(cmds, __argv[optind]);
}

void usage(void)
{
	printf("Usage: btpan [-i <name>] [-a] [--nosdp | -s] <command> [parameters..]>\n");

	print_all_usage(cmds);
#if 0
	printf("\n"
		"Notes:\n"
		"\t1. *address* argument can be replaced by cache entry number (btctl list)\n"
		"\n"
		);
#endif
}

struct command cmds[] = {
	{0, 0, 0, 0, "---->>>> General commands <<<<----\n"},
	{"help", cmd_help, 0, "<command name>"},
	/* PAN */
#if defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MOD)
	{0, 0, 0, 0, "---->>>> PAN commands <<<<----\n"},
	{"init", cmd_pan_init, 0, "[role] [flags]",
		"\tinitialize PAN\n"
		"\trole: panu, nap, gn\n"
	},
	{"stop", cmd_pan_init, 1, "",
		"\tstop PAN\n"
	},
	{"discovery", cmd_pan_discovery, 0, "[service]",
		"discover PAN device.\n"
		"service: panu, nap, gn\n"
	},
	{"connect", cmd_pan_connect, 0, "<address> [service]",
		"connects to the NAP service.\n"
		"service: nap, gn\n"
	},
	{"disconnect", cmd_pan_disconnect, 0, "",
		"close connection to connected service.\n"
	},
	{"ctl", cmd_panctl, 2, "<interface> [m (start_addr stop_addr)* ] [p (range_start range_stop)* ]",
		"Examples:\n"
		"\t1. lists all filters of PAN device pan0:\n"
		"\t   panctl pan0\n"
		"\t2. set multicast filter to 03:00:00:20:00:00 only:\n"
		"\t   panctl pan0 m 03:00:00:20:00:00 03:00:00:20:00:00\n"
		"\t3. set protocol filter to range 0x800 - 0x801 and 0x806\n"
		"\t   panctl pan0 p 0x800 0x801 0x806 0x806\n"
		"\t4. reset protocol filter\n"
		"\t   panctl pan0 p\n"
	},
#endif
	{0, 0, 0, NULL}
};

void do_exit(void)
{
}

int main(int argc, char **argv)
{
	int	c, lind, i, err = 0;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"tcp", 0, 0, 't'},
		{"nosdp", 0, 0, 's'},
		{"version", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	
	if (affix_init(argc, argv, LOG_USER)) {
		fprintf(stderr, "Affix initialization failed\n");
		return 1;
	}

	atexit(do_exit);

	for (;;) {
		c = getopt_long(argc, __argv, "+htvsVi:", opts, &lind);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage();
				return 0;
				break;
			case 'v':
				verboseflag = 1;
				break;
			case 'i':
				strncpy(btdev, optarg, IFNAMSIZ);
				break;
			case 's':
				sdpmode = 0;
				break;
			case 'V':
				printf("Affix version: %s\n", affix_version);
				return 0;
				break;
			case ':':
				printf("Missing parameters for option: %c\n", optopt);
				return 1;
				break;
			case '?':
				printf("Unknown option: %c\n", optopt);
				return 1;
				break;
		}
	}
	if (__argv[optind] && sscanf(__argv[optind], "bt%d", &i) > 0) {
		/* interface name */
		sprintf(btdev, "bt%d", i);
		optind++;
	}
	if (__argv[optind] == NULL) {
		usage();
		//err = cmd_prompt(NULL);
		goto exit;
	}
	err = call_command(cmds, __argv[optind++]);
exit:
	return err;
}

