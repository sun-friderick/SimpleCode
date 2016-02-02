/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Imre Deak <ext-imre.deak@nokia.com>

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
   $Id: btdebug.c,v 1.17 2004/02/24 15:11:03 kassatki Exp $

   BTDEBUG - Debug primitives

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
                Imre Deak <ext-imre.deak@nokia.com>
*/

/* The following prevents "kernel_version" from being set in this file. */
#define __NO_VERSION__

#include <linux/config.h>

/* Module related headers, non-module drivers should not include */
#include <linux/module.h>
#include <linux/init.h>

/* Standard driver includes */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>

#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/types.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>

/* Ethernet and network includes */
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>

/* for snprint */

/* Local Includes */
#define FILEBIT	DBHCI
#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>


/* little function to perform hexdumps, 16 bytes per line */
void printk_hexdump(const __u8 *p, __u32 len)
{
	int 	i,j;
	char	buf[81];

	for (i = 0; i < len; i++) {
		if ( ((i % 16) == 0) && i > 0)
			printk(KERN_DEBUG "%s\n", buf);
		j = (i % 16) * 3;
		sprintf( &(buf[j]), "%02x ", p[i]);
	}
	if (len)
		printk(KERN_DEBUG "%s\n", buf);
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define BOUND(val, min_lim, max_lim) (MIN(max_lim, MAX(min_lim, val)))

spinlock_t pkt_parse_lock = SPIN_LOCK_UNLOCKED;

static char parse_buf[1024];

void printk_chardump(const __u8 *data, int len)
{
	int	i;
	int	dump_len;
	unsigned char	*buf;
	unsigned char	msg[25];
	long	flags;

#define CHARS_PER_LINE 32
	
	buf = parse_buf;
	dump_len = MIN(sizeof(parse_buf) - 1, len);
	if (dump_len != len)
		sprintf(msg, "dump %d bytes", dump_len);
	else
		msg[0] = '\0';
	printk(KERN_DEBUG "CHAR DUMP: %d bytes %s\n", len, msg); 
	spin_lock_irqsave(&pkt_parse_lock, flags);
	for (i = 0; i < dump_len; i++)
	{
		buf[i % CHARS_PER_LINE] = (*data >= 33 && *data <= 126) ?
					  *data : '.';
		data++;	
		if ((i % CHARS_PER_LINE == CHARS_PER_LINE - 1) ||
		    i + 1 == dump_len)
		{
			buf[(i % CHARS_PER_LINE) + 1] = '\0';
			printk(KERN_DEBUG "%s\n", buf);
		}
	}
	spin_unlock_irqrestore(&pkt_parse_lock, (unsigned long)flags);
}


/*  Packet parsing functions. They are to be used with macros in btdebug.h  */

#define NOF_ARRAY_ITEMS(x) (sizeof(x) / sizeof(x[0]))

#define CU16(x) (*((__u16 *)&(x)))
#define CU24(x) (*((__u16 *)&(x)) + (((__u8 *)&x)[2] << 16))
#define CU32(x) (*((__u32 *)&(x)))

#define PARSE_FUNC(fname) static int fname(char *buf, int buf_size, const __u8 *p, int data_size)

typedef int (*parse_fn_t)(char *buf, int buf_size, const __u8 *p, int data_size);

#define PARSE_PROLOG(min_size, max_size, msg_hdr) \
	int pos = 0, msg_hdr_size; \
	int max_data_size = max_size; \
	\
	if (msg_hdr) \
		pos = snprintf(buf, buf_size, KERN_DEBUG "%s:", msg_hdr); \
	if (data_size < (unsigned int)min_size) \
	{ \
		pos += snprintf(&buf[pos], buf_size - pos, "???data too short\n"); \
		return pos; \
	} \
	buf += pos; \
	msg_hdr_size = pos; \
	pos = 0


#define PARSE_EPILOG \
	if (data_size > (unsigned int)max_data_size) \
		pos += snprintf(&buf[pos], buf_size - pos, "???data too long\n"); \
	return pos + msg_hdr_size	


PARSE_FUNC(nocmdpar);
PARSE_FUNC(noevpar);

PARSE_FUNC(cmdpar_inquiry);
PARSE_FUNC(cmdpar_chg_loc_name);

PARSE_FUNC(evpar_con_complete);
PARSE_FUNC(evpar_cmd_complete);
PARSE_FUNC(evpar_cmd_status);
PARSE_FUNC(evpar_inquiry_result);

PARSE_FUNC(cmdcmp_status);
PARSE_FUNC(cmdcmp_read_buf_size);

/*  Error string descriptor table */
static const char *hci_err_str[] =
{
	"Success",
	"Unknown HCI Command",
	"No Connection",
	"Hardware Failure",
	"Page Timeout",
	"Authentication Failure",
	"Key Missing",
	"Memory Full",
	"Connection Timeout",
	"Max Number Of Connections",  
	"Max Number Of SCO Connections To A Device",
	"ACL connection already exists",
	"Command Disallowed",
	"Host Rejected due to limited resources",
	"Host Rejected due to security reasons",
	"Host Rejected due to remote device is only a personal device",
	"Host Timeout",
	"Unsupported Feature or Parameter Value",
	"Invalid HCI Command Parameters",
	"Other End Terminated Connection: User Ended Connection",
	"Other End Terminated Connection: Low Resources",
	"Other End Terminated Connection: About to Power Off",
	"Connection Terminated by Local Host",
	"Repeated Attempts",
	"Pairing Not Allowed",
	"Unknown LMP PDU",
	"Unsupported Remote Feature",
	"SCO Offset Rejected",
	"SCO Interval Rejected",
	"SCO Air Mode Rejected",
	"Invalid LMP Parameters",
	"Unspecified Error",
	"Unsupported LMP Parameter Value",
	"Role Change Not Allowed",
	"LMP Response Timeout",
	"LMP Error Transaction Collision",
	"LMP PDU Not Allowed",
	"Encryption Mode Not Acceptable",
	"Unit Key Used",
	"QoS is Not Supported",
	"Instant Passed",
	"Pairing with Unit Key Not Supported",
	"Reserved for Future Use"
};

#define NOF_HCI_ERR NOF_ARRAY_ITEMS(hci_err_str)


/*  Description table for HCI command groups.  */
static const char *hci_cmd_grp_str[] =
{
	"Link control",
	"Link policy",
	"Host ctrl&baseb",
	"Inform params",
	"Status params",
	"Testing",
	"Invalid"
};

#define NOF_HCI_CMD_GRP NOF_ARRAY_ITEMS(hci_cmd_grp_str)


/*  Unfortunately we have to define this in advance. */
#define NOF_CMD_IN_GRP 63

/*  Descriptor table for HCI commands. */
static const struct
{
	const char	*str;
	parse_fn_t	param_fn;
	parse_fn_t	result_fn;
} hci_cmd_desc_tbl[NOF_HCI_CMD_GRP][NOF_CMD_IN_GRP] =
{
	{ // OGF=1 Link control
		{ "Inquiry", 			cmdpar_inquiry, noevpar },	// 1
		{ "Inquiry cancel", 		nocmdpar, noevpar },
		{ "Periodic inquiry mode",	nocmdpar, noevpar },
		{ "Exit periodic inquiry mode", nocmdpar, noevpar },
		{ "Create connection",		nocmdpar, noevpar },	// 5
		{ "Disconnect",			nocmdpar, noevpar },
		{ "Add SCO connection",		nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },
		{ "Accept connection req",	nocmdpar, noevpar },
		{ "Reject connect req",		nocmdpar, noevpar },	// 10
		{ "Link key req reply",		nocmdpar, noevpar },
		{ "Link key req negative reply", nocmdpar, noevpar },
		{ "PIN code req reply",		nocmdpar, noevpar },
		{ "PIN code req negative reply", nocmdpar, noevpar },
		{ "Change connection pkt type", nocmdpar, noevpar },	// 15
		{ "Reserved",			nocmdpar, noevpar },
		{ "Authentication requested",	nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },
		{ "Set connection encryption",	nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },	// 20
		{ "Change connection link key",	nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },
		{ "Master link key",		nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },
		{ "Remote name request",	nocmdpar, noevpar },	// 25
		{ "Reserved",			nocmdpar, noevpar },
		{ "Read remote supported features", nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },
		{ "Read remote version info",	nocmdpar, noevpar },
		{ "Reserved",			nocmdpar, noevpar },	// 30
		{ "Read clock offset",		nocmdpar, noevpar }	// 31
	},
	{
		//  OGF=2  Link policy
		{ "Hold mode",		nocmdpar, noevpar },		// 1
		{ "Reserved",		nocmdpar, noevpar },
		{ "Sniff mode",		nocmdpar, noevpar },
		{ "Exit sniff mode",	nocmdpar, noevpar },
		{ "Park mode",		nocmdpar, noevpar },		// 5
		{ "Exit park mode",	nocmdpar, noevpar },
		{ "QoS setup",		nocmdpar, noevpar },
		{ "Reserved",		nocmdpar, noevpar },
		{ "Role discovery",	nocmdpar, noevpar },
		{ "Reserved",		nocmdpar, noevpar },		// 10
		{ "Switch role",	nocmdpar, noevpar },	
		{ "Read link policy setting", nocmdpar, noevpar },
		{ "Write link policy setting", nocmdpar, noevpar }	// 13
	},
	{	//  OGF=3  Host ctrl&baseband
		{ "Set event mask",			nocmdpar, noevpar }, // 1
		{ "Reserved",				nocmdpar, noevpar },
		{ "Reset",				nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Set event filter",			nocmdpar, noevpar }, // 5
		{ "Reserved",				nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Flush",				nocmdpar, noevpar },
		{ "Read PIN type",			nocmdpar, noevpar },
		{ "Write PIN type",			nocmdpar, noevpar }, // 10
		{ "Create new unit key",		nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read stored link key",		nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar }, // 15
		{ "Reserved",				nocmdpar, noevpar },
		{ "Write stored link key",		nocmdpar, noevpar },
		{ "Delete stored link key",		nocmdpar, noevpar },
		{ "Change local name",			cmdpar_chg_loc_name,
							cmdcmp_status },
		{ "Read local name",			nocmdpar, noevpar }, // 20
		{ "Read connection accept timeout",	nocmdpar, noevpar },
		{ "Write connection accept timeout",	nocmdpar, noevpar },
		{ "Read page timeout",			nocmdpar, noevpar },
		{ "Write page timeout",			nocmdpar, noevpar },
		{ "Read scan enable",			nocmdpar, noevpar }, // 25
		{ "Write scan enable",			nocmdpar, noevpar },
		{ "Read page scan activity",		nocmdpar, noevpar },
		{ "Write page scan activity",		nocmdpar, noevpar },
		{ "Read inquiry scan activity",		nocmdpar, noevpar },
		{ "Write inquiry scan activity",	nocmdpar, noevpar }, // 30
		{ "Read authentication enable",		nocmdpar, noevpar },
		{ "Write authentication enable",	nocmdpar, noevpar },
		{ "Read encryption mode",		nocmdpar, noevpar },
		{ "Write encryption mode",		nocmdpar, noevpar },
		{ "Read class of device",		nocmdpar, noevpar }, // 35
		{ "Write class of device",		nocmdpar, noevpar },
		{ "Read voice setting",			nocmdpar, noevpar },
		{ "Write voice setting",		nocmdpar, noevpar },
		{ "Read auto flush timeout",		nocmdpar, noevpar },
		{ "Write auto flush timeout",		nocmdpar, noevpar }, // 40
		{ "Read num of broadcast retransmissions", nocmdpar, noevpar },
		{ "Write num of broadcast retransmissions", nocmdpar, noevpar },
		{ "Read hold mode activity",		nocmdpar, noevpar },
		{ "Write hold mode activity",		nocmdpar, noevpar },
		{ "Read transmit power level",		nocmdpar, noevpar }, // 45
		{ "Read SCO flow control enable",	nocmdpar, noevpar },
		{ "Write SCO flow control enable",	nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Set host controller to host flow ctrl", nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar }, // 50
		{ "Host buffer size",			nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Host number of completed packets",	nocmdpar, noevpar },
		{ "Read link supervision timeout",	nocmdpar, noevpar },
		{ "Write link supervision timeout",	nocmdpar, noevpar }, // 55
		{ "Read number of supported IAC",	nocmdpar, noevpar },
		{ "Read current IAC LAP",		nocmdpar, noevpar },
		{ "Write current IAC LAP",		nocmdpar, noevpar },
		{ "Read page scan period mode",		nocmdpar, noevpar },
		{ "Write page scan period mode",	nocmdpar, noevpar }, // 60
		{ "Read page scan mode",		nocmdpar, noevpar },
		{ "Write page scan mode",		nocmdpar, noevpar },
		{ "Invalid",				nocmdpar, noevpar }  // 63
	},
	{ // OGF=4   Informational parameters
		{ "Read local version information",	nocmdpar, noevpar }, // 1
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read local supported features",	nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read buffer size",			nocmdpar,
							cmdcmp_read_buf_size },	// 5
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read country code",			nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read BD address",			nocmdpar, noevpar }  // 9
	},
	{ // OGF=5   Status parameters
		{ "Read failed contact counter",	nocmdpar, noevpar }, // 1
		{ "Reset failed contact counter",	nocmdpar, noevpar },
		{ "Get link quality",			nocmdpar, noevpar },
		{ "Reserved",				nocmdpar, noevpar },
		{ "Read RSSI",				nocmdpar, noevpar }  // 5
	},
	{ // OGF=6   Testing commands
		{ "Read loopback mode",			nocmdpar, noevpar }, // 1
		{ "Write loopback mode",		nocmdpar, noevpar },
		{ "Enable device under test mode",	nocmdpar, noevpar }  // 3
	}
};


static const struct
{
	const char	*str;
	parse_fn_t	param_fn;
} hci_ev_tbl[] =
{
	{ "Inquiry Complete",		noevpar },
	{ "Inquiry Result",		evpar_inquiry_result },
	{ "Connection Complete",	evpar_con_complete },
	{ "Connection Request",		noevpar },
	{ "Disconnection Complete",	noevpar },
	{ "Authentication Complete",	noevpar },
	{ "Remote Name Request Complete", noevpar },
	{ "Encryption Change",		noevpar },
	{ "Change Connection Link Key Complete",	noevpar },
	{ "Master Link Key Complete",			noevpar },
	{ "Read Remote Supported Features Complete",	noevpar },
	{ "Read Remote Version Information Complete",	noevpar },
	{ "QoS Setup Complete",				noevpar },
	{ "Command Complete",		evpar_cmd_complete },
	{ "Command Status",		evpar_cmd_status },
	{ "Hardware Error",		noevpar },
	{ "Flush Occurred",		noevpar },
	{ "Role Change",		noevpar },
	{ "Number Of Completed Packets", noevpar },
	{ "Mode Change",		noevpar },
	{ "Return Link Keys",		noevpar },
	{ "PIN Code Request",		noevpar },
	{ "Link Key Request",		noevpar },
	{ "Link Key Notification",	noevpar },
	{ "Loopback Command",		noevpar },
	{ "Data Buffer Overflow",	noevpar },
	{ "Max Slots Change",		noevpar },
	{ "Read Clock Offset Complete", noevpar },
	{ "Connection Packet Type Changed", noevpar },
	{ "QoS Violation",		noevpar },
	{ "Page Scan Mode Change",	noevpar },
	{ "Page Scan Repetition Mode Change", noevpar },
	{ "Invalid",			noevpar }
};

#define NOF_HCI_EVENTS NOF_ARRAY_ITEMS(hci_ev_tbl)


static const char *hci_acl_pktbnd_str[] =
{
	"Reserved",
	"Continuing pkt frag",
	"First pkt frag",
	"Reserved"
};

static const char *hci_acl_brdcst_from_host_str[] =
{
	"PointToPoint",
	"ActiveBrdcst",
	"PiconetBrdcst",
	"Reserved"
};

static const char *hci_acl_brdcst_to_host_str[] =
{
	"PointToPoint",
	"SlaveNotInPark",
	"SlaveInPark",
	"Reserved"
};

static const char *ev_inq_scan_rep_str[] =
{
	"R0",
	"R1",
	"R2",
	"Reserved"
};

static const char *ev_inq_scan_per_str[] =
{
	"P0",
	"P1",
	"P2",
	"Reserved"
};

static const char *ev_inq_scan_mode_str[] =
{
	"Mand Page Scan Mode",
	"Opt Page Scan Mode I",
	"Opt Page Scan Mode II",
	"Opt Page Scan Mode III",
	"Reserved"
};

PARSE_FUNC(nocmdpar)
{
	PARSE_PROLOG(0, -1, "HCI CMD");

	pos = snprintf(buf, buf_size, "--\n");

	PARSE_EPILOG;
}

PARSE_FUNC(noevpar)
{
	PARSE_PROLOG(0, -1, "HCI EVENT");

	pos = snprintf(buf, buf_size, "--\n");

	PARSE_EPILOG;
}

PARSE_FUNC(cmdpar_chg_loc_name)
{
	PARSE_PROLOG(1, 248, "HCI CMD");
	
	pos = snprintf(buf, buf_size, "Name=%-248s\n", p);

	PARSE_EPILOG;
}

PARSE_FUNC(cmdpar_inquiry)
{
	PARSE_PROLOG(5, 5, "HCI CMD");

	pos = snprintf(buf, buf_size, "LAP=0x%06X InqLen=0x%02X(%ds) MaxResp=%d%s\n",
			CU24(p[0]), p[2], p[2] * 1280, p[4], p[4] ? "" : "(unlimited)");

	PARSE_EPILOG;
}

PARSE_FUNC(cmdcmp_status_no_cr)
{
	int err_num;
	int err_idx;

	PARSE_PROLOG(1, 1, "NULL");
	
	err_num = p[0];
	err_idx = BOUND(err_num, 0, NOF_HCI_ERR);
	pos = snprintf(buf, buf_size, "Stat=0x%02X(%s)", 
			err_num, hci_err_str[err_idx]);

	PARSE_EPILOG;
}

PARSE_FUNC(cmdcmp_status)
{
	PARSE_PROLOG(1, 1, "HCI EVENT");

	pos += cmdcmp_status_no_cr(buf, buf_size, p, data_size);
	pos += snprintf(&buf[pos], buf_size - pos, "\n");

	PARSE_EPILOG;
}

PARSE_FUNC(evpar_inquiry_res_item)
{
	const __u8 *bda;
	__u8	pg_scan_rep, pg_scan_per, pg_scan_mode;
	__u8	pg_scan_rep_idx, pg_scan_per_idx, pg_scan_mode_idx;
	__u32	cod;
	__u16	clk_ofs;

	PARSE_PROLOG(14, 14, "NULL");

	bda = p;
	pg_scan_rep = p[6];
	pg_scan_rep_idx = MIN(pg_scan_rep, NOF_ARRAY_ITEMS(ev_inq_scan_rep_str) - 1);
	pg_scan_per = p[7];
	pg_scan_per_idx = MIN(pg_scan_per, NOF_ARRAY_ITEMS(ev_inq_scan_per_str) - 1);
	pg_scan_mode = p[8];
	pg_scan_mode_idx = MIN(pg_scan_mode, NOF_ARRAY_ITEMS(ev_inq_scan_mode_str) - 1);
	cod = CU24(p[9]);
	clk_ofs = CU16(p[12]);
	
	snprintf(buf, buf_size, "BDA=%02X:%02X:%02X:%02X:%02X:%02X PageScanRepMode=%d(%s) PageScanPerMode=%d(%s) PageScanMode=%d(%s) CoD=0x%06X ClkOffs=0x%04X\n",
		bda[5], bda[4], bda[3], bda[2], bda[1], bda[0],
		pg_scan_rep, ev_inq_scan_rep_str[pg_scan_rep_idx],
		pg_scan_per, ev_inq_scan_per_str[pg_scan_per_idx],
		pg_scan_mode, ev_inq_scan_mode_str[pg_scan_mode_idx],
		cod, clk_ofs);

	PARSE_EPILOG;
}

PARSE_FUNC(evpar_inquiry_result)
{
	__u8 res_cnt, i;
	
	PARSE_PROLOG(1, -1, "HCI EVENT");

	res_cnt = p[0];
	snprintf(buf, buf_size, "ResCnt=%d\n", res_cnt);
	for (i = 0; i < res_cnt; i++)
	{
		evpar_inquiry_res_item(&buf[pos], buf_size - pos, p + 1 + i * 14, MIN(14, data_size - (1 + i * 14)));
	}

	PARSE_EPILOG;
}

const char *con_comp_status[] =
{
	"Success",
	"Unknown HCI Command",
	"No Connection",
	"Hardware Failure",
	"Page Timeout",
	"Authentication Failure",
	"Key Missing",
	"Memory Full",
	"Connection Timeout",
	"Max Number Of Connections",
	"Max Number Of SCO Connections To A Device",
	"ACL connection already exists",
	"Command Disallowed",
	"Host Rejected due to limited resources",
	"Host Rejected due to security reasons",
	"Host Rejected due to remote device is only a personal device",
	"Host Timeout",
	"Unsupported Feature or Parameter Value",
	"Invalid HCI Command Parameters",
	"Other End Terminated Connection: User Ended Connection",
	"Other End Terminated Connection: Low Resources",
	"Other End Terminated Connection: About to Power Off",
	"Connection Terminated by Local Host",
	"Repeated Attempts",
	"Pairing Not Allowed",
	"Unknown LMP PDU",
	"Unsupported Remote Feature",
	"SCO Offset Rejected",
	"SCO Interval Rejected",
	"SCO Air Mode Rejected",
	"Invalid LMP Parameters",
	"Unspecified Error",
	"Unsupported LMP Parameter Value",
	"Role Change Not Allowed",
	"LMP Response Timeout",
	"LMP Error Transaction Collision",
	"LMP PDU Not Allowed",
	"Encryption Mode Not Acceptable",
	"Unit Key Used",
	"QoS is Not Supported",
	"Instant Passed",
	"Pairing with Unit Key Not Supported",
	"Invalid"
};

#define NOF_CON_COMP_STATUS NOF_ARRAY_ITEMS(con_comp_status)

const char *con_comp_link[] =
{
	"SCO",
	"ACL",
	"Invalid"
};

#define NOF_CON_COMP_LINK NOF_ARRAY_ITEMS(con_comp_link)

const char *con_comp_encrpt_mode[] =
{
	"Disabled",
	"Only for P2P packets",
	"Both P2P and broadcast packets"
	"Invalid"
};

#define NOF_CON_COMP_ENCRPT_MODE NOF_ARRAY_ITEMS(con_comp_encrpt_mode)
	
PARSE_FUNC(evpar_con_complete)
{
	PARSE_PROLOG(11, 11, "HCI EVENT");
	
	pos = snprintf(buf, buf_size, "Status=%s  ConHnd=%03x %02x:%02x:%02x:%02x:%02x:%02x  Link=%s  EncrptMode=%s\n",
				con_comp_status[MIN(p[0], NOF_CON_COMP_STATUS - 1)], CU16(p[1]),
				p[8], p[7], p[6], p[5], p[4], p[3], con_comp_link[MIN(p[9], NOF_CON_COMP_LINK - 1)],
				con_comp_encrpt_mode[MIN(p[10], NOF_CON_COMP_ENCRPT_MODE - 1)]);
	PARSE_EPILOG;
}

PARSE_FUNC(evpar_cmd_complete)
{
	int		cmd_grp_idx, cmd_idx;
	__u8		cmd_count;
	__u16		op_code;
	parse_fn_t	result_fn;
	const char	*cmd_str;

	PARSE_PROLOG(3, -1, "HCI EVENT");
	
	cmd_idx = p[1] + ((p[2] & 3) << 8);
	cmd_idx = BOUND(cmd_idx, 1, NOF_CMD_IN_GRP) - 1;
	cmd_grp_idx = (p[2] & 0xfc) >> 2;
	cmd_grp_idx = BOUND(cmd_grp_idx, 1, NOF_HCI_CMD_GRP) - 1;
	result_fn = hci_cmd_desc_tbl[cmd_grp_idx][cmd_idx].result_fn;
	cmd_str = hci_cmd_desc_tbl[cmd_grp_idx][cmd_idx].str;
	cmd_count = p[0];
	op_code = CU16(p[1]);
	
	pos = snprintf(buf, buf_size,
			  "CmdNum=%d OpCode=0x%04X(%s)\n",
			  cmd_count, op_code, cmd_str);
	if (result_fn)
		pos += result_fn(&buf[pos], buf_size - pos, &p[3], data_size - 3);
	
	PARSE_EPILOG;
}

PARSE_FUNC(evpar_cmd_status)
{
	int		cmd_grp_idx, cmd_idx, err_idx, err_num;
	__u8		cmd_count;
	__u16		op_code;
	const char	*cmd_str;

	PARSE_PROLOG(4, -1, "HCI EVENT");
	
	err_num = p[0];
	err_idx = BOUND(err_num, 0, NOF_HCI_ERR);
	cmd_count = p[1];
	cmd_idx = p[2] + ((p[3] & 3) << 8);
	cmd_idx = BOUND(cmd_idx, 1, NOF_CMD_IN_GRP) - 1;
	cmd_grp_idx = (p[3] & 0xfc) >> 2;
	cmd_grp_idx = BOUND(cmd_grp_idx, 1, NOF_HCI_CMD_GRP) - 1;
	cmd_str = hci_cmd_desc_tbl[cmd_grp_idx][cmd_idx].str;
	op_code = CU16(p[2]);
	
	pos = snprintf(buf, buf_size,
			  "CmdNum=%d OpCode=0x%04X(%s), Stat=0x%02X(%s)\n",
			  cmd_count, op_code, cmd_str, err_num, hci_err_str[err_idx]);
	PARSE_EPILOG;
}


PARSE_FUNC(cmdcmp_read_buf_size)
{
	PARSE_PROLOG(8, 8, "HCI EVENT");

	pos = cmdcmp_status_no_cr(buf, buf_size, p, 1);
	pos += snprintf(&buf[pos], buf_size - pos,
			   "ACLPktLen=%d SCOPktLen=%d ACLPktNum=%d SCOPktNum=%d\n",
			   CU16(p[1]), p[3], CU16(p[4]), CU16(p[6]));

	PARSE_EPILOG;
}

PARSE_FUNC(parse_hci_cmd)
{
	int		cmd, grp;
	int		cmd_idx, grp_idx;
	parse_fn_t	param_fn;
	const char	*cmd_str, *grp_str;
	__u8		par_len;

	PARSE_PROLOG(3, -1, "HCI CMD");
	
	cmd = p[0] + ((p[1] & 0x03) << 8);
	grp = (p[1] & 0xfc) >> 2;
	grp_idx = BOUND(grp, 1, NOF_HCI_CMD_GRP) - 1;
	cmd_idx = BOUND(cmd, 1, NOF_CMD_IN_GRP) - 1;
	param_fn = hci_cmd_desc_tbl[grp_idx][cmd_idx].param_fn;
	cmd_str = hci_cmd_desc_tbl[grp_idx][cmd_idx].str;
	grp_str = hci_cmd_grp_str[grp_idx];
	par_len = p[2];

	pos = snprintf(buf, buf_size,
		"OCF=0x%02X(%s) OGF=0x%02X(%s) ParLen=%d\n", 
		cmd, cmd_str, 
		grp, grp_str,
		par_len);
	
	if (param_fn)
		pos += param_fn(&buf[pos], buf_size - pos, &p[3], data_size - 3);

	PARSE_EPILOG;
}

PARSE_FUNC(parse_hci_acl_from_host)
{
	int	pb, bc;
	__u32	total_len;
	__u16	con_hnd;

	PARSE_PROLOG(4, -1, "HCI ACL");
	
	pb = (p[1] >> 4) & 0x03;
	bc = ((p[1] >> 6) & 0x03);
	con_hnd = p[0] + ((p[1] & 0x0f) << 8);
	total_len = CU16(p[2]);
	
	pos = snprintf(buf, buf_size,
			"ConHnd=0x%03X%s PktBound=%d(%s) Brdcst=%d(%s) TotalLen=%d\n", 
			con_hnd, con_hnd >= 0xF00 ? "(Reserved)" : "",
			pb, hci_acl_pktbnd_str[pb],
			bc, hci_acl_brdcst_from_host_str[bc],
			total_len);

	PARSE_EPILOG;
}

PARSE_FUNC(parse_hci_acl_to_host)
{
	int	pb, bc;
	__u32	total_len;
	__u16	con_hnd;

	PARSE_PROLOG(4, -1, "HCI ACL");
	
	pb = (p[1] >> 4) & 0x03;
	bc = (p[1] >> 6) & 0x03;
	con_hnd = p[0] + ((p[1] & 0x0f) << 8);
	total_len = CU16(p[2]);
	
	pos = snprintf(buf, buf_size,
			"ConHnd=0x%03X%s PktBound=%d(%s) Brdcst=%d(%s) TotalLen=%d\n", 
			con_hnd, con_hnd >= 0xF00 ? "(Reserved)" : "",
			pb, hci_acl_pktbnd_str[pb],
			bc, hci_acl_brdcst_to_host_str[bc],
			total_len);

	PARSE_EPILOG;
}

PARSE_FUNC(parse_hci_event)
{
	int		ev_num, ev_idx, par_len;
	const char	*cod_str;
	const char	*ev_str;
	parse_fn_t	param_fn;

	PARSE_PROLOG(2, -1, "HCI EVENT");
	
	ev_num = p[0];
	ev_idx = BOUND(ev_num, 1, NOF_HCI_EVENTS) - 1;
	par_len = p[1];
	cod_str = hci_ev_tbl[ev_idx].str;
	param_fn = hci_ev_tbl[ev_idx].param_fn;
	ev_str = hci_ev_tbl[ev_idx].str;
	
	pos = snprintf(buf, buf_size,
			"Code=0x%02X(%s) ParLen=%d\n",
			ev_num, ev_str, par_len);
	
	if (param_fn)
		pos += param_fn(&buf[pos], buf_size - pos, &p[2], data_size - 2);

	PARSE_EPILOG;
}

PARSE_FUNC(parse_hci_sco)
{
	__u16	con_hnd;
	
	PARSE_PROLOG(3, -1, "HCI SCO");

	con_hnd = p[0] + ((p[1] & 0x0f) << 8);
	
	pos = snprintf(buf, buf_size,
			"ConHnd=0x%03X%s, TotalLen=%d\n", 
			con_hnd, con_hnd >= 0xF00 ? "(Reserved)" : "", p[2]);

	PARSE_EPILOG;
}
			
//	Parse and print a HCI packet.
//	type :  HCI_COMMAND
//		HCI_ACL
//		HCI_SCO
//		HCI_EVENT
//	dir:	0 - Host to host controller
//		1 - Host controller to host
void printk_parse_hci(const __u8 *p, int data_size, int type, pkt_dir dir)
{
	long flags;

	/*  We will use a static buffer to format the message, so we need a spinlock. */
	spin_lock_irqsave(&pkt_parse_lock, flags);

	switch (type)
	{
		case HCI_COMMAND:
			parse_hci_cmd(parse_buf, sizeof(parse_buf), p, data_size);
			break;

		case HCI_ACL:
			if (dir == 0)
				parse_hci_acl_from_host(parse_buf, sizeof(parse_buf), p, data_size);
			else
				parse_hci_acl_to_host(parse_buf, sizeof(parse_buf), p, data_size);
			break;

		case HCI_SCO:
			parse_hci_sco(parse_buf, sizeof(parse_buf), p, data_size);
			break;

		case HCI_EVENT:
			parse_hci_event(parse_buf, sizeof(parse_buf), p, data_size);	
			break;

		default:
			strcpy(parse_buf, KERN_DEBUG "Unknown HCI pkt type\n");
			break;
	}
	printk(parse_buf);

	spin_unlock_irqrestore(&pkt_parse_lock, (unsigned long)flags);
}

char *rfcomm_frtype[] =
{
	"UIH", "UI", "I", "INV"
};

#define NOF_RFCOMM_FRTYPE (NOF_ARRAY_ITEMS(rfcomm_frtype))

PARSE_FUNC(rfcomm_pn)
{
	PARSE_PROLOG(8, 8, "RFCOMM");

	pos = snprintf(buf, buf_size, "PN  |DLCI=%d|FRTYPE=%s|ConvL=%d|PRIO=%d|T1,ACKTIM=%d|N1,FRSIZE=%d|N2,RETR=%d|CREDIT=%d\n",
			p[0], rfcomm_frtype[btmin(p[1] & 0x0F, NOF_RFCOMM_FRTYPE - 1)],
			(p[1] >> 4), p[2], p[3], p[4] + (p[5] << 8), p[6], p[7]);
	PARSE_EPILOG;
}

PARSE_FUNC(rfcomm_msc)
{
	PARSE_PROLOG(2, 3, "RFCOMM");

	pos = snprintf(buf, buf_size, "MSC |ADDR EA=%d DLCI=%d|V24 EA=%d FC=%d RTC=%d RTR=%d IC=%d DV=%d",
			p[0] & 1, p[0] >> 2, p[1] & 1, (p[1] & 2) >> 1, (p[1] & 4) >> 2,
			(p[1] & 8) >> 3, (p[1] & 64) >> 6, (p[1] & 128) >> 7);
	if (data_size == 3)
		pos += snprintf(&buf[pos], buf_size - pos, "|BRK EA=%d PRESENT=%d LENGTH=%d\n",
				p[2] & 1, (p[2] & 0x0E) >> 1, (p[2] & 0xF0) >> 4);
	else
		pos += snprintf(&buf[pos], buf_size - pos, "\n");

	PARSE_EPILOG;
}

char *rfcomm_rpn_baud[] =
{
	"2400", "4800", "9600", "19200", "38400", "57600", "115200", "230400", "INV"
};

#define NOF_RFCOMM_RPN_BAUD NOF_ARRAY_ITEMS(rfcomm_rpn_baud)

char *rfcomm_rpn_parity[] =
{
	"ODD", "EVEN", "MARK", "SPACE"
};

#define NOF_RFCOMM_RPN_PARITY NOF_ARRAY_ITEMS(rfcomm_rpn_parity)

char *rfcomm_rpn_flow[] =
{
	"XOXFINP", "XOXFOUT", "RTRINP", "RTROUT", "RTCINP", "RTCOUT", "INV"
};

#define NOF_RFCOMM_RPN_FLOW NOF_ARRAY_ITEMS(rfcomm_rpn_flow)

char *rfcomm_rpn_mask[] =
{
	"BAUD", "DBITS", "SBITS", "PAR", "PTYPE", "XONCHR", "XOFFCHR", "INV",
	"XOXOFINP", "XOXFOUT", "RTRINP", "RTROUT", "RTCINP", "RTCOUT", "INV"
};

#define NOF_RFCOMM_RPN_MASK NOF_ARRAY_ITEMS(rfcomm_rpn_mask)
		
PARSE_FUNC(rfcomm_rpn)
{
	char *baud;
	static char flow[300];
	static char mask[300];
	int i;
	
	PARSE_PROLOG(1, 8, "RFCOMM");

	if (data_size != 1 && data_size != 8)
	{
		pos = snprintf(buf, buf_size, "RPN|invalid parameter length %d. Should be 1 or 8\n",
				data_size);
		goto exit;
	}
	pos = snprintf(buf, buf_size, "RPN |ADDR EA=%d DLCI=%d", p[0] & 1, p[0] >> 2);
	if (data_size == 8)
	{
		baud = rfcomm_rpn_baud[btmin(p[1], NOF_RFCOMM_RPN_BAUD - 1)];
		flow[0] = '\0';
		for (i = 0; i < NOF_RFCOMM_RPN_FLOW; i++)
		{
			if ((1 << i) & p[3])
			{
				if (flow[0] != '\0')
					strcat(flow, ",");
				strcat(flow, rfcomm_rpn_flow[i]);
			}
		}
		mask[0] = '\0';
		for (i = 0; i < NOF_RFCOMM_RPN_MASK; i++)
		{
			if ((1 << i) & *(__u16 *)&p[6])
			{
				if (mask[0] != '\0')
					strcat(mask, ",");
				strcat(mask, rfcomm_rpn_mask[i]);
			}
		}
		pos += snprintf(&buf[pos], buf_size - pos, "|BAUD=%s|DBITS=%d|SBITS=%s|PAR=%d|PTYPE=%s|FLOW=%s|XON=%#02X|XOFF=%#02X|MASK=%s\n",
				baud, (p[2] & 3) + 5, (p[2] & 4) ? "1" : "1.5", (p[2] & 8) >> 3, rfcomm_rpn_parity[btmin((p[2] & 0x30) >> 4, NOF_RFCOMM_RPN_PARITY - 1)],
				flow, p[4], p[5], mask);
	}
	else
		pos += snprintf(&buf[pos], buf_size - pos, "\n");
exit:
	PARSE_EPILOG;
}

char *rfcomm_rls_state[] =
{
	"Overrun Error", "Parity Error", "Framing Error", "Invalid status code"
};

#define NOF_RFCOMM_RLS_STATE NOF_ARRAY_ITEMS(rfcomm_rls_state)

PARSE_FUNC(rfcomm_rls)
{
	PARSE_PROLOG(2, 2, "RFCOMM");

	pos = snprintf(buf, buf_size, "RLS|ADDR EA=%d DLCI=%d|%s\n",
				p[0] & 1, p[0] >> 2, rfcomm_rls_state[btmin(p[1], NOF_RFCOMM_RLS_STATE - 1)]);
	
	PARSE_EPILOG;
}

struct
{
	int  id;
	char *str;
	parse_fn_t parse_fn;
} rfcomm_mcc[] =
{
	{ 0x80, "DLC PN", rfcomm_pn },
	{ 0x40, "PSC", NULL },
	{ 0xC0, "CLD", NULL },
	{ 0x20, "Test", NULL },
	{ 0xA0, "FCon", NULL },
	{ 0x60, "FCoff", NULL },
	{ 0xE0, "MSC", rfcomm_msc },
	{ 0x10, "NSC", NULL },
	{ 0x90, "RPN", rfcomm_rpn },
	{ 0x50, "RLS", rfcomm_rls },
};

#define NOF_RFCOMM_MCC NOF_ARRAY_ITEMS(rfcomm_mcc)
	
PARSE_FUNC(rfcomm_mcc_uih)
{
	int  hdr_len;
	int  par_len;
	char *str;
	int  i;

	PARSE_PROLOG(0, -1, "RFCOMM");

	if (data_size < 2 || data_size < 3 - (p[1] & 1))
	{
		pos = snprintf(buf, buf_size, "invalid size %d\n", data_size);
		goto exit;
	}
	hdr_len = 3 - (p[1] & 1);
	par_len = (p[1] >> 1) + (p[1] & 1 ? 0 : ((p[2] & ~1) << 6));
	if (data_size != hdr_len + par_len)
	{
		pos = snprintf(buf, buf_size, "invalid size %d. Should be %d\n",
				data_size, hdr_len + par_len);
		goto exit;
	}
	str = "INVCMD";
	for (i = 0; i < NOF_RFCOMM_MCC; i++)
	{
		if (rfcomm_mcc[i].id == (p[0] & ~3))
		{
			str = rfcomm_mcc[i].str;
			break;
		}
	}
	pos = snprintf(buf, buf_size, "MCC |TYPE EA=%d CR=%d %s|LENGTH=%d\n",
			p[0] & 1, (p[0] & 2) >> 1, str, par_len);
	if (i < NOF_RFCOMM_MCC && rfcomm_mcc[i].parse_fn)
		pos += rfcomm_mcc[i].parse_fn(buf + pos, buf_size - pos, &p[hdr_len], par_len);
exit:
	PARSE_EPILOG;
}

struct
{
	int id;
	char *str;
} rfcomm_ctrl[] =
{
	{ 0x2F, "SABM" },
	{ 0x63, "UA"  },
	{ 0x0F, "DM" },
	{ 0x43, "DISC" },
	{ 0xEF, "UIH" },
	{ 0x03, "UI" }
};

#define NOF_RFCOMM_CTRL NOF_ARRAY_ITEMS(rfcomm_ctrl)

void printk_parse_rfcomm(const __u8 *data, int len, pkt_dir dir)
{
	int	i;
	int	payload_len;
	int	hdr_len;
	int	uih_credit_present;
	char	*buf;
	char	*ctrl_str;
	long	flags;
	int	pos;

#define CHARS_PER_LINE 32
	
	spin_lock_irqsave(&pkt_parse_lock, flags);
	buf = parse_buf;

	if (len < 3 || (!(data[2] & 1) && len < 4))
	{
		sprintf(buf, KERN_DEBUG "RFCOMM:%s|invalid length %d\n", dir ? "RECV" : "XMIT", len);
		goto exit;
	}
	hdr_len = 4 - (data[2] & 1);
	uih_credit_present = data[1] == 0xff;
	payload_len = (data[2] >> 1) + ((data[2] & 1) ? 0 : data[3] << 7);
	if (hdr_len + uih_credit_present + payload_len + 1 != len)
	{
		sprintf(buf, KERN_DEBUG "RFCOMM:%s|invalid length %d."
					"Should be %d\n", dir ? "RECV" : "XMIT",
			len, payload_len + hdr_len);
		goto exit;
	}
	ctrl_str = "INVCMD";
	for (i = 0; i < NOF_RFCOMM_CTRL; i++)
	{
		if (rfcomm_ctrl[i].id == (data[1] & ~(1 << 4)))
		{
			ctrl_str = rfcomm_ctrl[i].str;
			break;
		}
	}
	if (uih_credit_present)
	{
		pos = snprintf(buf, sizeof(parse_buf),
				KERN_DEBUG "RFCOMM:%s|ADDR EA=%d CR=%d DLCI=%d|"
					   "CTRL %s  PF=%d|LENGTH=%d|CREDIT=%d|"
					   "FCS=%02x\n",
				dir ? "RECV" : "XMIT", data[0] & 1,
				(data[0] & 2) >> 1, data[0] >> 2,
				ctrl_str, (data[1] & (1 << 4)) >> 4,
				payload_len, data[hdr_len], data[len - 1]);
	}
	else
	{
		pos = snprintf(buf, sizeof(parse_buf),
				KERN_DEBUG "RFCOMM:%s|ADDR EA=%d CR=%d DLCI=%d|"
					   "CTRL %s PF=%d|LENGTH=%d|FCS=%02x\n",
				dir ? "RECV" : "XMIT", data[0] & 1,
				(data[0] & 2) >> 1, data[0] >> 2,
				ctrl_str, (data[1] & (1 << 4)) >> 4,
				payload_len, data[len - 1]);
	}
	if (i == 4 && (data[0] >> 2) == 0)  /*  UIH MCC */
		pos += rfcomm_mcc_uih(buf + pos, sizeof(parse_buf) - pos,
				      &data[hdr_len], payload_len);
	snprintf(&buf[pos], sizeof(parse_buf) - pos,
			KERN_DEBUG "RFCOMM:%s -----\n", dir ? "RECV" : "XMIT");

	
exit:
	printk(parse_buf);
	spin_unlock_irqrestore(&pkt_parse_lock, (unsigned long)flags);
}

int __init init_debug(void)
{

#if 0
	{
		struct Command_Complete_Event	cce;
		__u32	lap;
		BD_ADDR	bda1;
		BDADDR	bda2;
		DBPRT("LAP SIZE: %d, cce size: %d, BD_ADDR: %d, pBD_ADDR: %d\n",
			sizeof(lap), sizeof(cce), sizeof(bda1), sizeof(*bda2));
	}
#endif

#if 0
	{
		typedef struct type1_S {
			__u8	value;
		}type1;

		typedef struct type2_S {
			__u8	value;
		} __attribute__ ((packed)) type2;

		typedef struct type3_S {
			__u8	value;
		}type3 __attribute__ ((packed));


		struct type4_S {
			__u8	value;
		};

		struct type5_S {
			__u8	value;
		} __attribute__ ((packed));

		type1		a1;
		type2		a2;
		type3		a3;
		struct type4_S	a4;
		struct type5_S	a5;
		rfcomm_hdr_t	fr;

		DBPRT("Checking data sizes:\ntype1: %d, type2: %d, type3: %d, type4: %d, type5: %d\n",
				sizeof(a1), sizeof(a2), sizeof(a3), sizeof(a4), sizeof(a5));
		DBPRT("rfcomm_msc: %d, rfcomm_cmd_hdr_t: %d, rfcomm_modem: %d, address_field: %d\n",
				sizeof(rfcomm_msc), sizeof(rfcomm_cmd_hdr_t), sizeof(rfcomm_modem), sizeof(address_field));
		DBPRT("SHORT_FRAME SIZE: %d\n", sizeof(fr));
	}
#endif

	return 0;
}

EXPORT_SYMBOL(printk_hexdump);
EXPORT_SYMBOL(printk_chardump);

EXPORT_SYMBOL(printk_parse_hci);
EXPORT_SYMBOL(printk_parse_rfcomm);
