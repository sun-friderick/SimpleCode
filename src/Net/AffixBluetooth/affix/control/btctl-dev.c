/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001,2002 Nokia Corporation
   Adopted for Affix by Dmitry Kasatkin

   Written 2000,2001 by Maxim Krasnyansky <maxk@qualcomm.com>
   BlueZ - Bluetooth protocol stack for Linux
   Copyright (C) 2000-2001 Qualcomm Incorporated

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
   $Id: btctl-dev.c,v 1.83 2004/03/02 18:31:24 kassatki Exp $

   Utility functions to read and parse the BT server config file. 

   Fixes:
*/

#include <affix/config.h>

#include <sys/fcntl.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <affix/btcore.h>

#include "btctl.h"

static int	hci = -1;

struct affix_tupla uart_flags[] = {
	{CRTSCTS, "ctl"},
	{AFFIX_UART_RI | CRTSCTS, "ring"},
	{AFFIX_UART_LOW, "low"},
	{PARENB, "pareven"},
	{PARENB | PARODD, "parodd"},
	{CSTOPB, "stopb"},
	{0, NULL}
};

/*
 * UART stuff
 */
struct uart_t {
	char	*name;
	int 	type;
	int	proto;
	int	speed;
	int	init_speed;
	int	flags;
	int	(*init) (int fd, struct uart_t *u, struct termios *ti);
};

static int uart_speed(int s)
{
	switch (s) {
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		case 57600:
			return B57600;
		case 115200:
			return B115200;
		case 230400:
			return B230400;
		case 460800:
			return B460800;
		case 921600:
			return B921600;
		case 1000000:
			return B1000000;
		default:
			return B57600;
	}
}

/* 
 * Ericsson specific initialization 
 */

struct Ericsson_Set_UART_Speed {
	HCI_Command_Packet_Header	hci;
	__u8				speed;
}__PACK__;

#define HCI_C_ERICSSON_SET_UART_SPEED	0xfc09

static int ericsson(int fd, struct uart_t *u, struct termios *ti)
{
	struct timespec 			tm = {0, 50000};
	int					err;
	struct Ericsson_Set_UART_Speed		cmd;
	uint8_t					buf[HCI_MAX_EVENT_SIZE];
	struct Command_Complete_Status		*ccs = (void*)buf;

	switch (u->speed) {
		case 57600:
			cmd.speed = 0x03;
			break;
		case 115200:
			cmd.speed = 0x02;
			break;
		case 230400:
			cmd.speed = 0x01;
			break;
		case 460800:
			cmd.speed = 0x00;
			break;
		case 921600:
			cmd.speed = 0x20;
			break;
		default:
			cmd.speed = 0x03;
			u->speed = 57600;
			break;
	}
#if 0
	err = hci_exec_cmd(fd, HCI_C_ERICSSON_SET_UART_SPEED, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, ccs);
#else
	ccs->Status = 0;
	err = hci_exec_cmd1(fd, HCI_C_ERICSSON_SET_UART_SPEED, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, HCI_SKIP_STATUS);
	nanosleep(&tm, NULL);
#endif
	BTINFO("err: %d, status: %#x\n", err, ccs->Status);
	if (err)
		return err;
	return ccs->Status;
}

/* 
 * Digianswer specific initialization 
 */
struct Digi_Set_UART_Speed {
	HCI_Command_Packet_Header	hci;
	__u8				speed;
}__PACK__;

#define HCI_C_DIGI_SET_UART_SPEED	0xfc07

static int digi(int fd, struct uart_t *u, struct termios *ti)
{
	struct timespec 			tm = {0, 50000};
	int					err;
	struct Digi_Set_UART_Speed		cmd;

	switch (u->speed) {
		case 57600:
			cmd.speed = 0x08;
			break;
		case 115200:
			cmd.speed = 0x09;
			break;
		default:
			cmd.speed = 0x09;
			u->speed = 115200;
			break;
	}

	err = hci_exec_cmd1(fd, HCI_C_DIGI_SET_UART_SPEED, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, HCI_SKIP_STATUS);
	if (err)
		return err;
	nanosleep(&tm, NULL);
	return 0;
}


/* 
 * CSR specific initialization 
 * Inspired strongly by code in OpenBT and experimentations with Brainboxes
 * Pcmcia card.
 * Jean Tourrilhes <jt@hpl.hp.com> - 14.11.01
 *
 * Adopted to be HCI style - Dmitry Kasatkin
 *
 */
/* 15 bytes in UART: 1 + 3 + 11 */
struct CSR_BCC_Header {
	/* MSG header */
	__u8	channel;	/* 0xC2 for BCC */
	/* BCC header */
	__u16	type;		/* getreq - 0x00, getresp - 0x01, setreq - 0x02 */
	__u16	len;		/* in 16 bits */
	__u16	seq_num;
	__u16	var_id;
	__u16	status;
	/* payload data */
	__u16	data[0];
}__PACK__;

#define HCI_C_CSR_COMMAND	0xfc00

struct CSR_Get_Build_ID {
	HCI_Command_Packet_Header	hci;
	struct CSR_BCC_Header		bcc;
	__u16				data[6];
}__PACK__;

struct CSR_Get_Build_ID_Event {
	HCI_Event_Packet_Header		hci;	// 3 bytes
	struct CSR_BCC_Header		bcc;	// 11 bytes
	__u8				data[19];
}__PACK__;

struct CSR_Set_UART_Speed {
	HCI_Command_Packet_Header	hci;
	struct CSR_BCC_Header		bcc;
	__u16				speed;
	__u16				extra[3];
}__PACK__;

static int csr(int fd, struct uart_t *u, struct termios *ti)
{
	struct timespec tm = {0, 10000000};	/* 10ms - be generous */
	static int csr_seq = 0;	/* Sequence number of command */
	int  divisor;
	int					err;
	struct CSR_Get_Build_ID			cmd1;
	struct CSR_Get_Build_ID_Event		cce;
	struct CSR_Set_UART_Speed		cmd2;

	/* Try to read the build ID of the CSR chip */
	cmd1.bcc.channel = 0xc2;			/* first+last+channel=BCC */
	cmd1.bcc.type = __htob16(0x0000);		/* type = GET-REQ */
	cmd1.bcc.len = __htob16(5 + 4);			/* ??? */
	cmd1.bcc.seq_num = __htob16(csr_seq);		/* seq num */
	csr_seq++;
	cmd1.bcc.var_id = __htob16(0x2819);		/* var_id = CSR_CMD_BUILD_ID */
	cmd1.bcc.status = __htob16(0x0000);		/* status = STATUS_OK */
	/* CSR BCC payload */
	memset(cmd1.data, 0, sizeof(cmd1.data));

	/* Send command */
	err = hci_exec_cmd1(fd, HCI_C_CSR_COMMAND, &cmd1, sizeof(cmd1), 
			ALL_EVENTS_MASK, HCI_SKIP_STATUS);
	if (err)
		return err;

	do {
#if 0	// it's here in original code
		/* Send command */
		err = hci_exec_cmd(fd, &cmd1, ALL_EVENTS_MASK, HCI_SKIP_STATUS, NULL);
		if (err)
			return err;

#endif
		err = hci_recv_event(fd, &cce, sizeof(cce), 20);
		if (err < 0) {
			return err;
		}
	} while (cce.hci.EventCode != 0xFF);

	/* Display that to user */
	BTINFO("CSR build ID 0x%02X-0x%02X\n", cce.data[12], cce.data[11]);
	
	/* Now, create the command that will set the UART speed */
	cmd2.bcc.channel = 0xc2;
	cmd2.bcc.type = __htob16(0x0002);	// SET-REQ
	cmd2.bcc.len = __htob16(5 + 4);
	cmd2.bcc.seq_num = __htob16(csr_seq);
	csr_seq++;
	cmd2.bcc.var_id = __htob16(0x6802);	// CONFIG_UART
	cmd2.bcc.status = __htob16(0x0000);	// STATUS_OK

	switch (u->speed) {
		case 9600:
			divisor = 0x0027;
			break;
			/* Various speeds ommited */ 
		case 57600:
			divisor = 0x00EC;
			break;
		case 115200:
			divisor = 0x01D8;
			break;
			/* For Brainbox Pcmcia cards */
		case 460800:
			divisor = 0x075F;
			break;
		case 921600:
			divisor = 0x0EBF;
			break;
		default:
			/* Safe default */
			divisor = 0x01D8;
			u->speed = 115200;
			break;
	}
	/* No parity, one stop bit -> divisor |= 0x0000; */
	cmd2.speed = __htob16(divisor);
	memset(cmd2.extra, 0, sizeof(cmd2.extra));

	err = hci_exec_cmd1(fd, HCI_C_CSR_COMMAND, &cmd2, sizeof(cmd2),
			ALL_EVENTS_MASK, HCI_SKIP_STATUS);
	if (err)
		return err;

#if 0	// no wait for response in original code
	do {
		err = hci_recv_event(fd, &cce, sizeof(cce), 20);
		if (err < 0) {
			return err;
		}

	} while (cce.hci.EventCode != 0xFF);
#endif
	nanosleep(&tm, NULL);
	return 0;
}

/* 
 * Silicon Wave specific initialization 
 * Thomas Moser <Thomas.Moser@tmoser.ch>
 */
struct Swave_Set_UART_Speed {
	HCI_Command_Packet_Header	hci;
	__u8	subcmd;
	__u8	tag;
	__u8	length;
	/* parameters */
	__u8	flow;
	__u8	type;
	__u8	speed;
}__PACK__;

#define HCI_C_SWAVE_COMMAND	0xfc0B

struct Swave_Event {
	HCI_Event_Packet_Header		hci;
	__u8	subcmd;
	__u8	setevent;
	__u8	tag;
	__u8	length;
	/* parameters */
	__u8	flow;
	__u8	type;
	__u8	speed;
}__PACK__;

struct Swave_Soft_Reset {
	HCI_Command_Packet_Header	hci;
	__u8	subcmd;
}__PACK__;

static int swave(int fd, struct uart_t *u, struct termios *ti)
{
	struct timespec 			tm = {0, 500000};
	int					err;
	struct Swave_Set_UART_Speed		cmd;
	struct Swave_Event			cce;
	struct Swave_Soft_Reset			cmd1;

	// Silicon Wave set baud rate command
	// see HCI Vendor Specific Interface from Silicon Wave
	// first send a "param access set" command to set the
	// appropriate data fields in RAM. Then send a "HCI Reset
	// Subcommand", e.g. "soft reset" to make the changes effective.

	cmd.subcmd = 0x01;		// param sub command
	cmd.tag = 0x11;			// tag 17 = 0x11 = HCI Transport Params
	cmd.length = 0x03;		// length of the parameter following
	cmd.flow = 0x01;		// HCI Transport flow control enable
	cmd.type = 0x01;		// HCI Transport Type = UART

	switch (u->speed) {
		case 19200:
			cmd.speed = 0x03;
			break;
		case 38400:
			cmd.speed = 0x02;
			break;
		case 57600:
			cmd.speed = 0x01;
			break;
		case 115200:
			cmd.speed = 0x00;
			break;
		default:
			u->speed = 115200;
			cmd.speed = 0x00;
			break;
	}
	/* Send command */
	err = hci_exec_cmd1(fd, HCI_C_SWAVE_COMMAND, &cmd, sizeof(cmd),
			ALL_EVENTS_MASK, HCI_SKIP_STATUS);
	if (err)
		return err;

	nanosleep(&tm, NULL);

	do {
		err = hci_recv_event(fd, &cce, sizeof(cce), 20);
		if (err < 0) {
			return err;
		}
	} while (cce.hci.EventCode != 0xFF);

	cmd1.subcmd = 0x03;		// param sub command
	err = hci_exec_cmd1(fd, HCI_C_SWAVE_COMMAND, &cmd1, sizeof(cmd1), 
			ALL_EVENTS_MASK, HCI_SKIP_STATUS);
	if (err)
		return err;

	nanosleep(&tm, NULL);

	// now the uart baud rate on the silicon wave module is set and effective.
	// change our own baud rate as well. Then there is a reset event comming in
 	// on the *new* baud rate. This is *undocumented*! The packet looks like this:
	// 04 FF 01 0B (which would make that a confirmation of 0x0B = "Param 
	// subcommand class". So: change to new baud rate, read with timeout, parse
	// data, error handling. BTW: all param access in Silicon Wave is done this way.
	// Maybe this code would belong in a seperate file, or at least code reuse...

	return 0;
}

#if 0
static int texas(int fd, struct uart_t *u, struct termios *ti)
{
	struct timespec tm = {0, 50000};
	char cmd[10];
	unsigned char resp[100];		/* Response */
	int n;

	memset(resp,'\0', 100);

	/* Switch to default Texas baudrate*/
	if (set_speed(fd, ti, 115200) < 0) {
		perror("Can't set default baud rate");
		return -1;
	}

	/* It is possible to get software version with manufacturer specific 
	   HCI command HCI_VS_TI_Version_Number. But the only thing you get more
	   is if this is point-to-point or point-to-multipoint module */

	/* Get Manufacturer and LMP version */
	cmd[0] = HCI_COMMAND_PKT;
	cmd[1] = 0x01;
	cmd[2] = 0x10;
	cmd[3] = 0x00;	

	do {
		n = write(fd, cmd, 4);
		if (n < 0) {
			perror("Failed to write init command (READ_LOCAL_VERSION_INFORMATION)");
			return -1;
		}
		if (n < 4) {
			fprintf(stderr, "Wanted to write 4 bytes, could only write %d. Stop\n", n);
			return -1;
		}

		/* Read reply. */
		if (read_hci_event(fd, resp, 100) < 0) {
			perror("Failed to read init response (READ_LOCAL_VERSION_INFORMATION)");
			return -1;
		}

		/* Wait for command complete event for our Opcode */
	} while (resp[4] != cmd[1] && resp[5] != cmd[2]);

	/* Verify manufacturer */
	if ((resp[11] & 0xFF) != 0x0d)
		fprintf(stderr,"WARNING : module's manufacturer is not Texas Instrument\n");

	/* Print LMP version */
	fprintf(stderr, "Texas module LMP version : 0x%02x\n", resp[10] & 0xFF);

	/* Print LMP subversion */
	fprintf(stderr, "Texas module LMP sub-version : 0x%02x%02x\n", resp[14] & 0xFF, resp[13] & 0xFF);
	
	nanosleep(&tm, NULL);
	return 0;
}
#endif

struct uart_t uart[] = {
	/* can be any card */
	{ "h4", 1, HCI_UART_H4, 115200, 115200, CRTSCTS, NULL },

	/* nokia */
	{ "tlp", 2, HCI_UART_TLP, 115200, 115200, AFFIX_UART_RI | AFFIX_UART_LOW | CRTSCTS, NULL },

	/* csr bcsp */
	{ "bcsp", 3, HCI_UART_BCSP, 115200, 115200, PARENB, NULL },
	
	/* CSR */
	{ "csr", 4, HCI_UART_H4, 115200, 115200, CRTSCTS, csr },

	/* ericsson */
	{ "ericsson", 5, HCI_UART_H4, 115200, 57600, CRTSCTS, ericsson },

	/* digi */
	{ "digi", 6, HCI_UART_H4, 115200, 9600, CRTSCTS, digi },

	/* Silicon Wave */
	{ "swave", 7, HCI_UART_H4, 115200, 115200, CRTSCTS, swave },

        { NULL, 0 }
};

struct uart_t * get_by_type(char *name)
{
	int		i, speed = 0;
	int		m_id, p_id, mid, pid;
	char		linebuf[80];
	char		iftype[32], type[32], flags[32] = {'\0'};
	char		*ch;
	FILE		*fp;
	struct uart_t	*u = NULL;

	i = sscanf(name, "%x:%x", &m_id, &p_id);
	if (i > 1) {
		/* we have 0x:0x type here
		 * read info from config_file
		 */
		fp = fopen(uart_map, "r");
		if (!fp) {
			BTERROR("unable to open map file\n");
			return NULL;
		}
		type[0] = '\0';
		for (; fgets(linebuf, sizeof(linebuf), fp);) {
			ch = strchr(linebuf, '#');
			if (ch)
				*ch = '\0';
			type[0] = '\0';
			speed = 0;
			flags[0] = '\0';
			i = sscanf(linebuf, "%s %x:%x %s %d %s", iftype, &mid, &pid, type, &speed, flags);
			if (i < 3)
				continue;
			//printf("%s %x:%x %s %d %s\n", iftype, mid, pid, type, speed, flags);
			if (strcasecmp(iftype, "uart") != 0)
				continue;
			if (m_id == mid && p_id == pid) {
				/* found it"
				 */
				name = type;
				break;
			}
		}
		fclose(fp);
		if (type[0] == '\0') {
			/* try H4 */
			name = "h4";
		}
	}
	for (i = 0; uart[i].name; i++) {
		if (!strcmp(uart[i].name, name)) {
			u = &uart[i];
			break;
		}
	}
	if (!u)
		return NULL;
	if (speed)
		u->speed = speed;
	if (flags[0] != '\0') {
		u->flags = 0;
		if (!str2mask(uart_flags, flags, &u->flags)) {
			BTERROR("flags error\n");
			return NULL;
		}
	}
	//printf("type: %s, speed: %d, flags: %x\n", u->name, u->speed, u->flags);
	return u;
}

static void sig_alarm(int sig)
{
	BTERROR("Initialization timed out.\n");
	if (hci >= 0) {
		hci_stop_dev(hci);
		close(hci);
	}
	exit(1);
}


/* Initialize UART device */
int init_uart(char *dev, struct uart_t *u)
{
	struct termios	ti;
	int		fd, err;
	int 		to = 20, speed;

	/* now device ready for initialization */
	signal(SIGALRM, &sig_alarm);
	alarm(to);

	BTINFO("Initializing UART");

	if (uart_rate)
		speed = uart_speed(uart_rate > 0 ? uart_rate : u->init_speed);
	else
		speed = 0;
	err = hci_setup_uart(dev, u->proto, speed, u->flags);
	if (err)
		return err;

	/* Vendor specific initialization */
	if (u->init) {
		fd = _hci_open(dev);
		if (fd < 0) {
			BTERROR("unable to open: %s\n", dev);
			return fd;
		}
		err =  u->init(fd, u, &ti);
		close(fd);
	}

	/* Set actual baudrate */
	if (!err) { 
		err = hci_setup_uart(dev, u->proto, uart_speed(u->speed), u->flags);
		if (err)
			BTERROR("Can't setup uart");
	}
	alarm(0);
	return err;
}

int cmd_uart(struct command *cmd)
{
	int			err;
	char			*dev = NULL;
	struct uart_t		*u = NULL;

	closelog(); 
	openlog(__argv[0], LOG_PERROR|LOG_PID, LOG_USER);

	if (__argc - optind < 1 || (cmd->cmd == CMD_OPEN_UART && __argc - optind < 2)) {
		print_usage(cmd);
		exit(1);
	}
	if (cmd->cmd != CMD_INIT_UART)
		dev = __argv[optind++];
		
	if (cmd->cmd != CMD_CLOSE_UART) {
		u = get_by_type(__argv[optind++]);
		if (!u) {
			BTERROR("Device unknown.\n");
			return 1;
		}
		if (!__argv[optind])
			goto next;

		u->speed = atoi(__argv[optind++]);

		if (!__argv[optind])
			goto next;
		
		/* get 0xXXXX flags */
		if (sscanf(__argv[optind], "0x%x", &u->flags) > 0)
			goto next;
		else {
			/* get symbolic */
			u->flags = 0;
			if (!str2mask(uart_flags, argv2str(__argv + optind), &u->flags)) {
				BTERROR("flags error\n");
				return -1;
			}
		}
next:
		if (cmd->cmd == CMD_OPEN_UART) {
			/* atatch device */
			err = hci_open_uart(dev, u->type, u->proto, uart_speed(u->speed), u->flags);
		} else {
			/* init device */
			err = init_uart(btdev, u);
			if (err < 0)
				BTERROR("Can't init device\n"); 
		}
	} else {
		err = hci_close_uart(dev);
	}
	if (err)
		BTERROR("%s", hci_error(err));
	return err;
}

int cmd_initdev(struct command *cmd)
{
	int	err;

	if (cmd->cmd == CMD_INITDEV && !__argv[optind]) {
		BTERROR("device type missed\n");
		return -1;
	}
	/* open HCI device */
	hci = _hci_open(btdev);
	if (hci < 0) {
		BTERROR("unable to open: %s, %s (%d)\n", btdev, strerror(errno), errno);
		return -1;
	}
	if (cmd->cmd == CMD_UPDEV)
		err = hci_start_dev(hci);
	else
		err = hci_stop_dev(hci);
	if (err) {
		BTERROR("Unable to %s device (%s)\n", cmd->cmd == CMD_UPDEV ? "start" : "stop", btdev);
		return err;
	}
	/* do stuff */
	if (cmd->cmd == CMD_INITDEV) {
		if (strcasecmp(__argv[optind], "usb") == 0) {
		} else if (strcasecmp(__argv[optind], "pcmcia") == 0) {
		} else if (strcasecmp(__argv[optind], "uart") == 0) {
		} else if (strcasecmp(__argv[optind], "uart_cs") == 0) {
		}
	}
	/* close HCI device */
	if (cmd->cmd == CMD_INITDEV)
		hci_stop_dev(hci);
	close(hci);
	hci = -1;
	return 0;
}

/* ---------------------  HCI packet dumper --------------------- */

#define	PCAP_MAGIC			0xa1b2c3d4
#define	PCAP_SWAPPED_MAGIC		0xd4c3b2a1
#define	PCAP_MODIFIED_MAGIC		0xa1b2cd34
#define	PCAP_SWAPPED_MODIFIED_MAGIC	0x34cdb2a1

/* "libpcap" file header (minus magic number). */
struct pcap_file_hdr {
	uint32_t	magic;          /* magic */
	uint16_t	version_major;	/* major version number */
	uint16_t	version_minor;	/* minor version number */
	int32_t		thiszone;	/* GMT to local correction */
	uint32_t	sigfigs;	/* accuracy of timestamps */
	uint32_t	snaplen;	/* max length of captured packets, in octets -> max length saved portion of each pkt */
	uint32_t	linktype;	/* data link type */
	uint8_t		data[0];
} __PACK__;

/* "libpcap" record header. */
struct pcap_pkt_hdr {
	struct timeval	ts;		/* time stamp */
	uint32_t	caplen;		/* number of octets of packet saved in file */
	uint32_t	len;		/* actual length of packet */
	uint8_t		data[0];
} __PACK__;

struct pcap_eth_hdr {
	uint8_t		dst[6];
	uint8_t		src[6];
	uint16_t	proto;
	uint8_t		data[0];
} __PACK__;

int cmd_capture(struct command *cmd)
{
	int			rfd, wfd, promisc = 1;
	struct sockaddr_affix	sa;
	socklen_t		salen = sizeof(sa);
	char			packet[65535];	// for now
	int			size, pkt_type;
	struct pcap_file_hdr	hdr;
	struct pcap_pkt_hdr	*pcap = (void*)packet;
	struct pcap_eth_hdr	*eth = (void*)pcap->data;
	struct hci_dev_attr	attr;
	unsigned long		rx_acl = 0, rx_sco = 0, rx_event = 0;
	unsigned long		tx_acl = 0, tx_sco = 0, tx_cmd = 0;

	if (!__argv[optind]) {
		fprintf(stderr, "usage: hcidump <filename> | -\n");
		return 1;
	}
	if (__argv[optind][0] == '-') 
		wfd = 1;
	else {
		wfd = open(__argv[optind], O_WRONLY|O_CREAT|O_TRUNC, 0644);
		if (wfd < 0) {
			fprintf(stderr, "%s open failed: %s\n", __argv[optind], strerror(errno));
			return 1;
		}
	}
	rfd = hci_open(btdev);
	if (rfd < 0) {
		fprintf(stderr, "hci_open() faield\n");
		return 1;
	}
	if (setsockopt(rfd, SOL_AFFIX, BTSO_PROMISC, &promisc, sizeof(int))) {
		fprintf(stderr, "setsockopt() failed\n");
		return 1;
	}

	/* init file header */
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic = PCAP_MAGIC;
	hdr.version_major = 2;
	hdr.version_minor = 4;
	hdr.snaplen = 0xffff;	//FIXME
	hdr.linktype = 1;	//Ethernet

	size = write(wfd, &hdr, sizeof(hdr));
	if (size < 0) {
		fprintf(stderr, "write failed: %s\n", strerror(errno));
		return 1;
	}

	/* init SLL header */
	eth->proto = htons(0xb123);
	
	//fprintf(stderr, "Capturing...\n");
	for (;;) {
		fprintf(stderr, "Captured: RX: acl:%lu sco:%lu event:%lu", rx_acl, rx_sco, rx_event);
		fprintf(stderr, ", TX: acl:%lu sco:%lu cmd:%lu\r", tx_acl, tx_sco, tx_cmd);
		fflush(stderr);
		/* reading HCI packet ... */
		size = recvfrom(rfd, eth->data, sizeof(packet), 0, (struct sockaddr*)&sa, &salen);
		if (size < 0) {
			fprintf(stderr, "recvfrom failed: %s\n", strerror(errno));
			return 1;
		}
		hci_get_attr_id(sa.devnum, &attr);
		//fprintf(stderr, "recvfrom(): %d bytes, bda: %s\n", size, bda2str(&sa.bda));
		/* init SLL header */
		pkt_type = eth->data[0] & ~HCI_PKT_OUTGOING;
		if (eth->data[0] & HCI_PKT_OUTGOING) {
			bda2eth(eth->src, &attr.bda);
			if (pkt_type == HCI_COMMAND) {
				bda2eth(eth->dst, &attr.bda);
				tx_cmd++;
				
			} else {
				bda2eth(eth->dst, &sa.bda);
				if (pkt_type == HCI_ACL)
					tx_acl++;
				else
					tx_sco++;
			}
		} else {
			bda2eth(eth->dst, &attr.bda);
			if (pkt_type == HCI_EVENT) {
				bda2eth(eth->src, &attr.bda);
				rx_event++;
			} else {
				bda2eth(eth->src, &sa.bda);
				if (pkt_type == HCI_ACL)
					rx_acl++;
				else
					rx_sco++;
			}
		}
		size += sizeof(*eth);
		/* init pcap header */
		gettimeofday(&pcap->ts, NULL);
		pcap->caplen = size;
		pcap->len = size;
		size += sizeof(*pcap);
		//fprintf(stderr, "write: %d, sizeof(*pcap): %d, sizeof(*sll): %d\n", 
		//			size, sizeof(*pcap), sizeof(*sll));
		size = write(wfd, packet, size);
		if (size < 0) {
			fprintf(stderr, "write failed: %s\n", strerror(errno));
			return 1;
		}
	}
	fprintf(stderr, "\n");
	return 0;

}

