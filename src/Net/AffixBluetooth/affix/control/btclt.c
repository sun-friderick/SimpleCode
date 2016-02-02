/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001, 2002 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
 
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
   $Id: btclt.c,v 1.2 2004/03/03 08:13:17 kassatki Exp $

   Fixes:
		Dmitry Kasatkin		: search added, fixes
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
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>

#include <stdint.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>
#include <affix/utils.h>

extern struct command cmds[];
int cmd_search(struct command *cmd);
int cmd_browse(struct command *cmd);

/* common */
/* tools helpers */
int do_inquiry(int length)
{
	int		fd, i;
	int		err;
	INQUIRY_ITEM	devs[20];
	__u8		num;

	if (!length)
		length = 8;

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
	printf("done.\n");
	if (num == 0) {
		printf("No devices found.\n");
	} else {
		btdev_cache_reload();
		btdev_cache_retire();
		for (i = 0; i < num; i++)
			__btdev_cache_add(devs[i].bda, devs[i].Class_of_Device, NULL);
		btdev_cache_print(DEVSTATE_RANGE);
		btdev_cache_save();
	}
	return 0;
}

int do_discovery(int length)
{
	int		fd, i;
	int		err;
	INQUIRY_ITEM	devs[20];
	char		*devnames[20];
	char		namebuf[248];
	__u8		num;

	if (!length)
		length = 8;

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
		printf("Searching done. Resolving names ...\n");
		for (i = 0; i < num; i++) {
			devs[i].Clock_Offset |= 0x8000;
			err = HCI_RemoteNameRequest(fd, &devs[i], namebuf);
			if (!err)
				devnames[i] = strdup(namebuf);
			else 
				devnames[i] = NULL;
		}
		printf("done.\n");
		btdev_cache_reload();
		btdev_cache_retire();
		for (i = 0; i < num; i++) {
			__btdev_cache_add(devs[i].bda, devs[i].Class_of_Device, devnames[i]);
			if (devnames[i])
				free(devnames[i]);
		}
		btdev_cache_print(DEVSTATE_RANGE);
		btdev_cache_save();
	}
	return 0;
}

int cmd_list(struct command *cmd)
{
	btdev_cache_load(0);
	btdev_cache_print(DEVSTATE_ALL);
	return 0;
}

int cmd_purge(struct command *cmd)
{
	btdev_cache_purge();
	return 0;
}

int cmd_discovery(struct command *cmd)
{
	__u32		length;
	int		err;

	if (__argv[optind]) {
		sscanf(__argv[optind], "%x", &length);
	} else 
		length = 8;

	if (cmd->cmd == 0)
		err = do_inquiry(length);
	else
		err = do_discovery(length);
	return err;
}

struct btservent {
	char	name[32];
	char	longname[32];
	int	svcclass;
	int	proto;
	int	port;
};

#if 0
int affix_getservbyname(const char *name, struct btservent *ent)
{
	int		i, err;
	char		linebuf[80];
	char		proto[32];
	char		*ch;
	FILE		*fp;
	int		line;
	//struct btservent	ent;

	fp = fopen("/etc/affix/services", "r");
	if (!fp) {
		perror("failed to open /etc/affix/services\n");
		return -1;
	}
	for (line = 1; fgets(linebuf, sizeof(linebuf), fp); line++) {
		ch = strchr(linebuf, '#');
		if (ch)
			*ch = '\0';
		ent->name[0] = '\0';
		ent->longname[0] = '\0';
		proto[0] = '\0';
		i = sscanf(linebuf, "%s %s %x %s %d", ent->name, ent->longname, &ent->svcclass, proto, &ent->port);
		if (i < 4)
			continue;
		if (!str2val(affix_protos, proto, &ent->proto)) {
			fprintf(stderr, "proto unknown in line %d\n", line);
			continue;
		}
		printf("%s %s %x %s(%d) %d\n", ent->name, ent->longname, ent->svcclass, proto, ent->proto, ent->port);
		if (i != 5)
			ent->port = 0;
		if (strcasecmp(ent->name, name) == 0) {
			// found
			return 1;
		}
	}
	fclose(fp);
	exit(0);
	return 0;
}

#endif

/*
 * RFCOMM stuff
 */
int cmd_connect(struct command *cmd)
{
	int		err, fd;
	BD_ADDR		bda;
	int		sch;
	int		line = RFCOMM_BTY_ANY;
	struct sockaddr_affix	saddr;

	if (__argc - optind < 2) {
		printf("Parameters missing\n");
		print_usage(cmd);
		return 1;
	}

	err = btdev_get_bda(&bda, __argv[optind++]);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}
	if (cmd->cmd != 2)
		printf("Connecting to host %s ...\n", bda2str(&bda));

	saddr.family = PF_AFFIX;
	saddr.bda = bda;
	saddr.devnum = hci_devnum(btdev);//HCIDEV_ANY;

	if (__argv[optind+1]) {
		// bty line number
		line = atoi(__argv[optind+1]);
	}

	sch = sdp_find_port_by_name(&saddr, __argv[optind]);
	if (sch < 0)
		return sch;

	if (cmd->cmd == 0)	// just sdp request for port
		return 0;

	saddr.port = sch;

	if (cmd->cmd != 2)
		printf("Connecting to channel %d ...\n", sch);

	fd = socket(PF_AFFIX, SOCK_STREAM, BTPROTO_RFCOMM);
	if (fd < 0) {
		printf("Unable to create RFCOMM socket: %d\n", PF_AFFIX);
		return 1;
	}

	if (cmd->cmd == 1) {
		// connect

		err = rfcomm_set_type(fd, RFCOMM_TYPE_BTY);
		if (err < 0) {
			BTERROR("Unable to set RFCOMM interface type to tty");
			close(fd);
			return 1;
		}

		err = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
		if (err < 0) {
			fprintf(stderr, "Unable to connect to remote side: %s\n", strerror(errno));
			close(fd);
			return 1;
		}
		line = rfcomm_open_tty(fd, line);
		if (line < 0) {
			printf("Unable to bind a port to the socket\n");
			close(fd);
			return 1;
		}

		printf("Connected. Bound to line %d [/dev/bty%d].\n", line, line);

	} else if (cmd->cmd == 2) {
		// bind
		line = rfcomm_bind_tty(fd, &saddr, line);
		if (line < 0) {
			fprintf(stderr, "Unable to bind a port to the socket: %s\n", strerror(errno));
			close(fd);
			return 1;
		}

		printf("Bound to bda: %s, channel: %d, line %d [/dev/bty%d].\n", bda2str(&bda), sch, line, line);
	}

	close(fd);
	return 0;
}

int cmd_disconnect(struct command *cmd)
{
	int	line = -1, err;

	if (__argv[optind] == NULL) {
		printf("disconnect all\n");
	} else {
		line = atoi(__argv[optind]);
		printf("Disconnecting line %d [/dev/bty%d]\n", line, line);
	}
	// Disconnect here
	err = rfcomm_close_tty(line);
	if (err < 0) {
		printf("Unable to disconnect line: %d\n", line);
		return 1;
	}
	return 0;
}

int cmd_status(struct command *cmd)
{
	struct rfcomm_port	info[10];
	int		count;

	count = rfcomm_get_ports(info, 10);
	if (count < 0) {
		printf("Unable to get portinfo\n");
		return 1;
	}
	if (count == 0)
		printf("No connected lines\n");
	else {
		int	i;
		struct rfcomm_port	*port = info;

		printf("Connected lines:\n");
		for (i=0; i < count; i++, port++) {
			printf("line: %d [/dev/bty%d], bda: %s, channel: %d, flags: %s %s\n",
					port->line, port->line, bda2str(&port->addr.bda), port->addr.port,
					(port->flags & RFCOMM_SOCK_BOUND) ? "bound" : "\b",
					(port->flags & RFCOMM_SOCK_CONNECTED) ? "connected" : "\b"
					);
		}
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
	printf("Usage: btclt [-i <name>] <command> [parameters..]>\n");

	print_all_usage(cmds);

	printf("\n"
		"Notes:\n"
		"\t1. *address* argument can be replaced by cache entry number (btctl list)\n"
		"\t2. use *search* command if *browse* returns nothing\n"
		"\n"
		);
}

struct command cmds[] = {
	{0, 0, 0, 0, "---->>>> General commands <<<<----\n"},
	{"help", cmd_help, 0, "<command name>"},
	{"list", cmd_list, 0, "", "shows know/found devices\n"
	},
	{"flush", cmd_purge, 1, "", "removes all know devices from the cache\n"
	},
	/* HCI */
	{0, 0, 0, 0, "---->>>> HCI commands <<<<----\n"},
	{"inquiry", cmd_discovery, 0, "[length]",
		"search for the device\n"
	},
	{"discovery", cmd_discovery, 1, "[length]",
		"search for the devices and resolve their names\n"
	},
	/* SDP */
#if defined(CONFIG_AFFIX_SDP)
	{0, 0, 0, 0, "---->>>> SDP commands <<<<----\n"},
	{"browse", cmd_browse, 0, "<address>",
		"browse for services on remote device\n"
	},
	{"search", cmd_search, 0, "<address>",
		"search for services on remote device\n"
		"notes: used if browse does not find anything, because\n"
		"some devices does not suport browseing\n"
	},
#endif
	/* RFCOMM */
#if defined(CONFIG_AFFIX_RFCOMM) || defined(CONFIG_AFFIX_RFCOMM_MOD)
	{0, 0, 0, 0, "---->>>> RFCOMM commands <<<<----\n"},
	{"port", cmd_connect, 0, "<address> [channel | service]", 
		"Make SDP request to get RFCOMM server channel for a service"
	},
	{"connect", cmd_connect, 1, "<address> [channel | service] [line number]",
		"create RFCOMM connection.\n"
		"\tservice_type: SERial | DUN | FAX | LAN | HEAdset.\n"
	},
	{"bind", cmd_connect, 2, "<address> [channel | service] [line number]",
		"bind RFCOMM connection.\n"
	},
	{"disconnect", cmd_disconnect, 1, "[line]",
		"disconnect RFCOMM port\n"
	},
	{"status", cmd_status, 1, "", "shows connected lines\n"
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
			case 't':
				linkmode = PF_INET;
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
		err = cmd_list(NULL);
		//usage();
		goto exit;
	}
	err = call_command(cmds, __argv[optind++]);
exit:
	return err;
}

