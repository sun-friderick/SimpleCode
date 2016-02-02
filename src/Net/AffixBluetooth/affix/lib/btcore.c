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
   $Id: btcore.c,v 1.72 2004/03/24 12:28:09 kassatki Exp $

   HCI Command Library

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <sys/uio.h>

#include <sys/time.h>
#include <sys/file.h>

#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <arpa/inet.h>

/* affix specific */
#include <affix/config.h>
#include <affix/bluetooth.h>
#include <affix/btcore.h>

/* Library variables */
char		*affix_version = PACKAGE_STRING;
unsigned long	affix_logmask;		

char		btdev[IFNAMSIZ] = "bt0";
int		linkmode = PF_AFFIX;
int		promptmode = 0;
int		verboseflag = 0;
char		*progname = "(none)";
int		__argc;
char		**__argv;
char		affix_userpref[80];
char		affix_cachefile[80];

btdev_list	devcache;

struct affix_tupla debug_flags_map[] = {
	/* core */
	{DBHCI, "hcicore"},
	{DBAFHCI, "afhci"},
	{DBHCIMGR, "hcimgr"},
	{DBHCISCHED, "hcisched"},
	{DBHCI|DBAFHCI|DBHCIMGR|DBHCISCHED, "hci"},
	/* l2cap */
	{DBL2CAP, "pl2cap"},
	{DBAFL2CAP, "afl2cap"},
	{DBL2CAP|DBAFL2CAP, "l2cap"},
	/* rfcomm */
	{DBRFCOMM, "prfcomm"},
	{DBAFRFCOMM, "afrfcomm"},
	{DBBTY, "bty"},
	{DBRFCOMM|DBAFRFCOMM|DBBTY, "rfcomm"},
	/* pan */
	{DBPAN, "pan"},
	/* drivers */
	{DBDRV, "drv"},
	{DBALLMOD, "allmod"},
	/* details */
	{DBCTRL, "ctrl"},
	{DBPARSE, "parse"},
	{DBCHARDUMP, "chardump"},
	{DBHEXDUMP, "dump"},
	{DBFNAME, "fname"},
	{DBFUNC, "func"},
	{DBALLDETAIL, "alldetail"},

	{0xFFFFFFFF, "all"},
	{0, 0}
};

/* ERROR messages */
char *hci_errlist[] = {
	"NO ERROR",
	"Unknown HCI command",
	"No connection",
	"Hardware failure",
	"Page timeout",
	"Authentication failure",
	"Key missing",
	"Memory full",
	"Connection timeout",
	"Max number of connections",
	"Max number of SCO connections to a device",
	"ACL connection already exists",
	"Command disallowed",
	"Host rejected due to limited resources",
	"Host rejected due to security reason",
	"Host rejected due to remote device is only a personal device",
	"Host timeout",
	"Unsupported feature or parameter value",
	"Invalid HCI command parameters",
	"Other end terminated connection: user ended connection",
	"Other end terminated connection: low resources",
	"Other end terminated connection: about to power off",
	"Connection terminated by local host",
	"Repeated attempts",
	"Pairing not allowed",
	"Unknow LMP PDU",
	"Unsupported remote feature",
	"SCO offset rejected",
	"SCO interval rejected",
	"SCO air mode reejected",
	"Invalid LMP parameters",
	"Unspecified error",
	"unsupported LMP parameters value",
	"Role change not allowed",
	"LMP response timeout",
	"LMP error transaction collision",
	"LMP PDU not allowed",
	NULL
};

char *lmp_compid[] = {
	"Ericsson Mobile Communications",
	"Nokia Mobile Phones",
	"Intel Corp",
	"IBM Corp",
	"Toshiba Corp",
	"3Com",
	"Microsoft",
	"Lucent",
	"Motorola",
	"Infineon Technologies AG",
	"Cambridge Silicon Radio",
	"Silicon Wave",
	"Digianswer",
	"Texas Instruments Inc.",
	"Parthus Technologies Inc.",
	"Broadcom Corporation",
	"Mitel Semiconductor",
	"Widcomm Inc.",
	"Telencomm Inc.",
	"Atmel Corporation",
	"Mitsubishi Electric Corporation",
	"RTX Telecom A/S",
	"KC Technology Inc.",
	"Newlogic",
	"Transilica Inc.",
	"Rohde & Schwartz GmbH &Co. KG",
	"TTPCom Limited",
	"Signia Technologies Inc.",
	"Conexant Systems Inc.",
	"Qualcomm",
	"Inventel",
	"AVM Berlin",
	"BrandSpeed Inc.",
	"Mansella Ltd",
	"NEC Corporation",
	"WavePlus Technology Co., Ltd.",
	"Alcatel",
	NULL
};

char *lmp_features[] = {
	NULL
};

struct affix_tupla affix_protos[] = {
	{BTPROTO_L2CAP, "L2CAP", "L2CAP"},
	{BTPROTO_RFCOMM, "RFCOMM", "RFCOMM"},
	{0, NULL, NULL}
};


/* devices classes */
struct affix_tupla codsdp_service_map[] =  {
	{HCI_COD_NETWORKING, "netw", "Networking"},
	{HCI_COD_RENDERING, "rend", "Rendering"},
	{HCI_COD_CAPTURING, "capt", "Capturing"},
	{HCI_COD_TRANSFER, "tran", "Object Transfer"},
	{HCI_COD_AUDIO, "audi", "Audio"},
	{HCI_COD_TELEPHONY, "tele", "Telephony"},
	{HCI_COD_INFORMATION, "info", "Information"},
	{0, NULL, NULL}
};

struct affix_tupla codMajorClassMnemonic[] =  {
	{HCI_COD_MISC, "misc", "Miscellaneous"},
	{HCI_COD_COMPUTER, "comp", "Computer"},
	{HCI_COD_PHONE, "phon", "Phone"},
	{HCI_COD_LAP, "lap", "LAN Access Point"},
	{HCI_COD_MAUDIO, "audi", "Audio"},
	{HCI_COD_PERIPHERAL, "peri", "Peripheral"},
	{0, NULL, NULL}
};

struct affix_tupla codMinorComputerMnemonic[] =  {
	{HCI_COD_DESKTOP, "desk", "Desktop"},
	{HCI_COD_COMPUTER, "serv", "Server"},
	{HCI_COD_LAPTOP, "lapt", "Laptop"},
	{HCI_COD_HANDPC, "hand", "Handheld PC/PDA"},
	{HCI_COD_PALMPC, "palm", "Palm PC/PDA"},
	{0, NULL, NULL}
};

struct affix_tupla codMinorPhoneMnemonic[] =  {
	{HCI_COD_CELLULAR, "cell", "Cellular"},
	{HCI_COD_CORDLESS, "cord", "Cordless"},
	{HCI_COD_SMART, "smart", "Smart phone"},
	{HCI_COD_MODEM, "modem", "Wired Modem/VoiceGW"},
	{0, NULL, NULL}
};

struct affix_tupla codMinorAudioMnemonic[] =  {
	{HCI_COD_HEADSET, "head", "Headset"},
	{0, NULL, NULL}
};

struct affix_tupla pkt_type_map[] = {
	{HCI_PT_DM1, "DM1"},
	{HCI_PT_DH1, "DH1"},
	{HCI_PT_DM3, "DM3"},
	{HCI_PT_DH3, "DH3"},
	{HCI_PT_DM5, "DM5"},
	{HCI_PT_DH5, "DH5"},
	{HCI_PT_HV1, "HV1"},
	{HCI_PT_HV2, "HV2"},
	{HCI_PT_HV3, "HV3"},
	{0, 0}
};

struct affix_tupla sec_level_map[] = {
	{HCI_SECURITY_OPEN, "open"},
	{HCI_SECURITY_AUTHOR, "author"},
	{HCI_SECURITY_AUTH, "auth"},
	{HCI_SECURITY_ENCRYPT, "encrypt"},
	{0, 0}
};

struct affix_tupla sec_mode_map[] = {
	/* modes */
	{HCI_SECURITY_OPEN, "open"},
	{HCI_SECURITY_LINK, "link"},
	{HCI_SECURITY_SERVICE, "service"},
	{HCI_SECURITY_PAIRABLE, "pair"},
	/* levels */
	{HCI_SECURITY_AUTH, "auth"},
	{HCI_SECURITY_AUTHOR, "author"},
	{HCI_SECURITY_ENCRYPT, "encrypt"},
	{0, 0}
};

struct affix_tupla role_map[] = {
	{HCI_ROLE_DENY_SWITCH, "deny"},
	{HCI_ROLE_ALLOW_SWITCH, "allow"},
	{HCI_ROLE_BECOME_MASTER, "master"},
	{HCI_ROLE_REMAIN_SLAVE, "slave"},
	{0, 0}
};

struct affix_tupla scan_map[] = {
	{HCI_SCAN_INQUIRY, "disc"},
	{HCI_SCAN_PAGE, "conn"},
	{0, 0}
};

struct affix_tupla audio_features_map[] = {
	{HCI_LF_SCO, "SCO"},
	{HCI_LF_HV2, "HV2"},
	{HCI_LF_HV3, "HV3"},
	{HCI_LF_ULAWLOG, "u-Law log"},
	{HCI_LF_ALAWLOG, "a-Law log"},
	{HCI_LF_CVSD, "CVSD"},
	{HCI_LF_TRANSPARENT_SCO, "transparent SCO"},
	{0, 0}
};

struct affix_tupla policy_features_map[] = {
	{HCI_LF_SWITCH, "switch"},
	{HCI_LF_HOLD_MODE, "hold mode"},
	{HCI_LF_SNIFF_MODE, "sniff mode"},
	{HCI_LF_PARK_MODE, "park mode"},
	{0, 0}
};

struct affix_tupla timing_features_map[] = {
	{HCI_LF_SLOT_OFFSET, "slot offset"},
	{HCI_LF_TIMING_ACCURACY, "timing accuracy"},
	{0, 0}
};

struct affix_tupla radio_features_map[] = {
	{HCI_LF_RSSI, "RSSI"},
	{HCI_LF_CQD_DATARATE, "CQD data rate"},
	{HCI_LF_PAGING_SCHEME, "paging scheme"},
	{0, 0}
};

struct affix_tupla packets_features_map[] = {
	{HCI_LF_3SLOTS, "3-slots"},
	{HCI_LF_5SLOTS, "5-slots"},
	{0, 0}
};


/* ------------------------------------------ */

void _hci_error(char *buf, int err)
{
	if (err < 0)
		sprintf(buf, "System error: %s (%d)", strerror(errno), errno);
	else if (err > 0)
		sprintf(buf, "HCI error: %s (%d)", hci_errlist[err], err);
	else
		sprintf(buf, "No error (0)");
}

char *hci_error(int err)
{
	static unsigned char 	buf[80][2];
	static int 		num = 0; 

	num = 1 - num; /* switch buf */
	_hci_error(buf[num], err);
	return buf[num];
}


/* general purpose */
int str2bda(BD_ADDR *p, char *str)
{
	int	i, val, err;
	char	*res=(char*)p;

	for (i = 5; i >= 0; i--) {
		err = sscanf(str, "%2x", &val);
		if (err == 0)
			return 0;
		res[i] = val;
		if (i == 0)
			break;
		str = strchr(str,':');
		if (str == 0)
			return 0;
		str++;
	}
	return 1;
}

void _bda2str(char *str, BD_ADDR *p)
{
	__u8	*ma=(__u8*)p;

	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x", 
		ma[5], ma[4], ma[3], ma[2], ma[1], ma[0]);

}

char *bda2str(BD_ADDR *bda)
{
	static unsigned char 	buf[2][18];
	static int 		num = 0; 

	num = 1 - num; /* switch buf */
	_bda2str(buf[num], bda);
	return buf[num];
}

void btlog_hexdump(const char *fname, const unsigned char *data, int len)
{
	int 	i, j;
	char	buf[81];

	for (i = 0; i < len; i++) {
		if (((i % 16) == 0) && i > 0)
			syslog(LOG_DEBUG, "%s: %s\n", fname, buf);
		j = (i % 16) * 3;
		sprintf(&(buf[j]), "%02x ", data[i]);
	}
	if (len)
		syslog(LOG_DEBUG, "%s: %s\n", fname, buf);
}

static void do_exit(void)
{
	btdev_cache_free();
}

int affix_init(int argc, char *argv[], int facility)
{
	int		fd, err;
	static int	affix_running = 0;
	
	if (affix_running)
		return 0;

	affix_logmask = 0xffff;

	fd = socket(PF_AFFIX, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0 && errno == EAFNOSUPPORT) {
		// try to load module
		system("/sbin/modprobe affix");
		fd = socket(PF_AFFIX, SOCK_RAW, BTPROTO_HCI);
		if (fd < 0)
			return fd;
	}
	close(fd);
	affix_running = 1;

	/* set environment */
	__argc = argc;
	__argv = argv;
	progname = argv[0];

	openlog(argv[0], LOG_PERROR, facility);
	
	if (getuid() == 0) {
		/* root -> put the stuff to /var/spool/affix */
		sprintf(affix_userpref, "/var/spool/affix");
	} else
		sprintf(affix_userpref, "%s/.bluetooth", getenv("HOME"));
	err = rmkdir(affix_userpref, 0700);
	if (err < 0) {
		perror("Unable to create dir\n");
		return err;
	}
	sprintf(affix_cachefile, "%s/devcache", affix_userpref);
	btdev_cache_init();
	atexit(do_exit);
	return 0;
}

#include <affix/hci_cmds.h>	//XXX:


/* **********   Control Commands  ************** */

int hci_add_pin(BD_ADDR *bda, int Length, __u8 *Code)
{
	struct PIN_Code	pin;
	int		err;

	pin.bda = *bda;
	pin.Length = Length;
	memcpy(pin.Code, Code, 16);
	err = hci_ioctl(BTIOC_ADDPINCODE, &pin);
	if (err)
		return err;
	return hci_remove_key(bda);
}

int hci_add_key(BD_ADDR *bda, __u8 key_type, __u8 *key)
{
	struct link_key	k;

	k.bda = *bda;
	k.key_type = key_type;
	memcpy(k.key, key, 16);
	return hci_ioctl(BTIOC_ADDLINKKEY, &k);
}

int hci_remove_key(BD_ADDR *bda)
{
	int		err = 0;
	int		fd;
	__u16		deleted;
	int		i, num, flags;
	int		devs[HCI_MAX_DEVS];

	num = hci_get_devs(devs);
	for (i = 0; i < num; i++) {
		hci_get_flags_id(devs[i], &flags);
		if (!(flags & HCI_FLAGS_UP))
			continue;
		fd = hci_open_id(devs[i]);
		if (fd < 0) {
			err = fd;
			break;
		}
		err = HCI_DeleteStoredLinkKey(fd, bda, (!bda) ? 1 : 0, &deleted);
		hci_close(fd);
		if (err < 0)
			return err;
	}
	return hci_ioctl(BTIOC_REMOVELINKKEY, bda);
}


int hci_open_uart(char *dev, int type, int proto, int speed, int flags)
{
	struct open_uart	line;

	realpath(dev, line.dev);
	line.type = type;
	line.proto = proto;
	line.speed = speed;
	line.flags = flags;
	return hci_ioctl(BTIOC_OPEN_UART, &line);
}

int hci_setup_uart(char *name, int proto, int speed, int flags)
{
	struct open_uart	line;

	strncpy(line.dev, name, IFNAMSIZ);
	line.proto = proto;
	line.speed = speed;
	line.flags = flags;
	return hci_ioctl(BTIOC_SETUP_UART, &line);
}

int hci_close_uart(char *dev)
{
	struct open_uart	line;

	realpath(dev, line.dev);
	line.flags = 0;
	return hci_ioctl(BTIOC_CLOSE_UART, &line);
}

/* L2CAP - user mode stuff */

/*
 * returns L2CAP channel (socket) MTU
 */
int l2cap_sendping(int fd, char *data, int size, int type)
{
	int		err = 0;
	struct msghdr	msg;
	struct iovec	iov;
	struct cmsghdr	*cmsg;
	char		buf[CMSG_SPACE(0)];
	int		len = size;

	while (len) {
		iov.iov_base = data;
		iov.iov_len = len;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = buf;
		msg.msg_controllen = sizeof buf;
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_AFFIX;
		cmsg->cmsg_type = type;
		cmsg->cmsg_len = CMSG_LEN(0);
		msg.msg_controllen = cmsg->cmsg_len;	

		err = sendmsg(fd, &msg, 0);
		if (err < 0)
			return err;
		if (err == 0)
			return (size - len);
		len -= err;
	}
	return size;
}

int l2cap_ping(int fd, char *data, int size)
{
	int	err = 0;
	int	len = 0;

	err = l2cap_sendping(fd, data, size, L2CAP_PING);
	if (err < 0)
		return err;
	while (len < size) {
		err = recv(fd, data+len, size - len, 0);
		if (err < 0)
			return err;
		if (err == 0)
			break;
		len += err;
	}
	return len;
}



/* RFCOMM - user space stuff */

int rfcomm_get_ports(struct rfcomm_port *pi, int size)
{
	int	fd, err;
	struct rfcomm_ports	cp;
	
	fd = socket(PF_AFFIX, SOCK_STREAM, BTPROTO_RFCOMM);
	if (fd < 0)
		return fd;

	cp.ports = pi;
	cp.size = size;
	err = ioctl(fd, SIOCRFCOMM_GET_PORTS, &cp);
	if (err < 0)
		return err;

	return cp.count;
}


/* **************************  Control Commands ******************* */

int hci_ioctl(int cmd, void *arg)
{
	int	fd, err;
	fd = btsys_socket(PF_AFFIX, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0)
		return fd;
	err = btsys_ioctl(fd, cmd, arg);
	btsys_close(fd);
	return err;
}

int hci_get_conn(int fd, BD_ADDR *bda)
{
	struct affix_conn_info	ci;
	int			err;

	ci.bda = *bda;
	err = btsys_ioctl(fd, BTIOC_GET_CONN, &ci);
	if (err)
		return err;
	return ci.dport;
}

/*
 * PAN
 */

int affix_pan_init(char *name, int mode)
{
	struct pan_init		pan;

	if (name) {
		strncpy(pan.name, name, IFNAMSIZ);
		pan.name[IFNAMSIZ-1] = '\0';
	} else
		pan.name[0] = '\0';
	pan.mode = mode;
	return hci_ioctl(BTIOC_PAN_INIT, &pan);
}

int affix_pan_connect(struct sockaddr_affix *sa, int role)
{
	struct pan_connect      pan;
	memcpy(&pan.saddr, sa, sizeof(struct sockaddr_affix));
	pan.peer_role = role;
	return hci_ioctl(BTIOC_PAN_CONNECT, &pan);
}


/* CORE utils */
/* Info about a process. */
typedef struct _proc_ {
	char *fullname;	/* Name as found out from argv[0] */
	char *basename;	/* Only the part after the last / */
	char *statname;	/* the statname without braces    */
	ino_t ino;		/* Inode number			  */
	dev_t dev;		/* Device it is on		  */
	pid_t pid;		/* Process ID.			  */
	int sid;		/* Session ID.			  */
	struct _proc_ *next;	/* Pointer to next struct. 	  */
} PROC;

/* pid queue */
typedef struct _pidq_ {
	struct _pidq_	*front;
	struct _pidq_	*next;
	struct _pidq_	*rear;
	PROC		*proc;
} PIDQ;

/* List of processes. */
static PROC *plist = NULL;
/* Did we stop a number of processes? */
static int scripts_too = 0;

#if 1
//
// Get some file-info. (size and lastmod)
//
int get_fileinfo(const char *name, char *lastmod)
{
	struct stat stats;
	struct tm *tm;
	
	stat(name, &stats);
	tm = gmtime(&stats.st_mtime);
	snprintf(lastmod, 21, "%04d-%02d-%02dT%02d:%02d:%02dZ",
			tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
			tm->tm_hour, tm->tm_min, tm->tm_sec);
	return (int) stats.st_size;
}


//
// Read a file and alloc a buffer for it
//
uint8_t* easy_readfile(const char *filename, int *file_size)
{
	int actual;
	int fd;
	uint8_t *buf;


	fd = open(filename, O_RDONLY, 0);
	if (fd == -1) {
		return NULL;
	}
	*file_size = get_filesize(filename);
	DBPRT("name=%s, size=%d\n", filename, *file_size);
	if(! (buf = malloc(*file_size)) ) {
		return NULL;
	}

	actual = read(fd, buf, *file_size);
	close(fd); 

	*file_size = actual;
	return buf;
}

#endif

/* put yourself in the background */
void affix_background(void)
{
	int 	fd;
	int	nulldev_fd ;		/* file descriptor for null device */

	if (getppid() != 1) {
		/* if parent is not init */
		switch (fork()) {
			case -1:
				exit(1);
				break;
			case 0:
				break;
			default:
				exit(0); //parent exit
		}
	}
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	setsid();
	chdir("/");
	if ((nulldev_fd = open("/dev/null", O_RDWR)) != -1) {
		for (fd = 0; fd < 3; fd++) {
			if (isatty(fd))
				dup2(nulldev_fd, fd) ;
		}
		close(nulldev_fd) ;
	}
}

static void freeproc(void)
{
	PROC	*p, *n;
	/* Free the already existing process list. */
	for (p = plist; p; p = n) {
		n = p->next;
		if (p->fullname)
			free(p->fullname);
		if (p->statname)
			free(p->statname);
		free(p);
	}
	plist = NULL;

}

/*
 *	Read the proc filesystem.
 */
static int readproc(void)
{
	DIR *dir;
	struct dirent *d;
	char path[256];
	char buf[256];
	char *s, *q;
	FILE *fp;
	int pid, f;
	PROC *p;
	struct stat st;
	int c;

	/* Open the /proc directory. */
	if ((dir = opendir("/proc")) == NULL) {
		BTERROR("cannot opendir(/proc)");
		return -1;
	}

	freeproc();

	/* Walk through the directory. */
	while ((d = readdir(dir)) != NULL) {
		/* See if this is a process */
		if ((pid = atoi(d->d_name)) == 0) continue;
		/* Get a PROC struct . */
		p = (PROC *)malloc(sizeof(PROC));
		memset(p, 0, sizeof(PROC));
		/* Open the status file. */
		snprintf(path, sizeof(path), "/proc/%s/stat", d->d_name);

		/* Read SID & statname from it. */
		if ((fp = fopen(path, "r")) != NULL) {
			buf[0] = 0;
			fgets(buf, 256, fp);

			/* See if name starts with '(' */
			s = buf;
			while (*s != ' ') s++;
			s++;
			if (*s == '(') {
				/* Read program name. */
				q = strrchr(buf, ')');
				if (q == NULL) {
					p->sid = 0;
					BTERROR("can't get program name from %s\n", path);
					free(p);
					continue;
				}
				s++;
			} else {
				q = s;
				while (*q != ' ') q++;
			}
			*q++ = 0;
			while (*q == ' ') q++;
			p->statname = strdup(s);

			/* This could be replaced by getsid(pid) */
			if (sscanf(q, "%*c %*d %*d %d", &p->sid) != 1) {
				p->sid = 0;
				BTERROR("can't read sid from %s\n",
						path);
				free(p);
				continue;
			}
			fclose(fp);
		} else {
			/* Process disappeared.. */
			free(p);
			continue;
		}

		/* Now read argv[0] */
		snprintf(path, sizeof(path), "/proc/%s/cmdline", d->d_name);
		if ((fp = fopen(path, "r")) != NULL) {
			f = 0;
			while(f < 127 && (c = fgetc(fp)) != EOF && c)
				buf[f++] = c;
			buf[f++] = 0;
			fclose(fp);

			/* Store the name into malloced memory. */
			p->fullname = strdup(buf);

			/* Get a pointer to the basename. */
			if ((p->basename = strrchr(p->fullname, '/')) != NULL)
				p->basename++;
			else
				p->basename = p->fullname;
		} else {
			/* Process disappeared.. */
			free(p);
			continue;
		}

		/* Try to stat the executable. */
		snprintf(path, sizeof(path), "/proc/%s/exe", d->d_name);
		if (stat(path, &st) == 0) {
			p->dev = st.st_dev;
			p->ino = st.st_ino;
		}

		/* Link it into the list. */
		p->next = plist;
		plist = p;
		p->pid = pid;
	}
	closedir(dir);
	/* Done. */
	return 0;
}

static inline PIDQ *init_pid_q(PIDQ *q)
{
	q->front =  q->next = q->rear = NULL;
	q->proc = NULL;
	return q;
}

static inline int empty_q(PIDQ *q)
{
	return (q->front == NULL);
}

static inline int add_pid_to_q(PIDQ *q, PROC *p)
{
	PIDQ *tmp;

	tmp = (PIDQ *)malloc(sizeof(PIDQ));

	tmp->proc = p;
	tmp->next = NULL;

	if (empty_q(q)) {
		q->front = tmp;
		q->rear  = tmp;
	} else {
		q->rear->next = tmp;
		q->rear = tmp;
	}
	return 0;
}

static inline PROC *get_next_from_pid_q(PIDQ *q)
{
	PROC *p;
	PIDQ *tmp = q->front;

	if (!empty_q(q)) {
		p = q->front->proc;
		q->front = tmp->next;
		free(tmp);
		return p;
	}

	return NULL;
}

/* Try to get the process ID of a given process. */
static PIDQ *pidof(char *prog)
{
	struct stat st;
	int dostat = 0;
	PROC *p;
	PIDQ *q;
	char *s;
	int foundone = 0;
	int ok = 0;

	/* Try to stat the executable. */
	if (prog[0] == '/' && stat(prog, &st) == 0) dostat++;

	/* Get basename of program. */
	if ((s = strrchr(prog, '/')) == NULL)
		s = prog;
	else
		s++;

	q = (PIDQ *)malloc(sizeof(PIDQ));
	q = init_pid_q(q);

	/* First try to find a match based on dev/ino pair. */
	if (dostat) {
		for (p = plist; p; p = p->next) {
			if (p->dev == st.st_dev && p->ino == st.st_ino) {
				add_pid_to_q(q, p);
				foundone++;
			}
		}
	}

	/* If we didn't find a match based on dev/ino, try the name. */
	if (!foundone) {
		for (p = plist; p; p = p->next) {
			ok = 0;

			ok += (strcmp(p->fullname, prog) == 0);
			ok += (strcmp(p->basename, s) == 0);

			if (p->fullname[0] == 0 ||
					strchr(p->fullname, ' ') ||
					scripts_too)
				ok += (strcmp(p->statname, s) == 0);

			if (ok) add_pid_to_q(q, p);
		}
	}

	return q;
}

#define PIDOF_OMITSZ	5

/*
 *	Pidof functionality.
 */
int affix_pidof(char *name, int flags, pid_t pid)
{
	PROC	*p;
	PIDQ	*q;
	int	i,oind;
	pid_t	opid[PIDOF_OMITSZ], spid = 0;
	char	*basec = NULL;

	for (oind = PIDOF_OMITSZ-1; oind > 0; oind--)
		opid[oind] = 0;

	if (flags&PIDOF_SCRIPTS)
		scripts_too++;

	if (flags&PIDOF_OMIT) {
		opid[oind] = pid;
		oind++;
	}
	if (flags&PIDOF_POMIT) {
		opid[oind] = getppid();
		oind++;
	}
	if (flags&PIDOF_BASENAME) {
		char	*ch;

		basec = strdup(name);
		name = basename(basec);
		if ((ch = strchr(name, ' '))) {
			*ch = '\0';
		}
	}
	/* Print out process-ID's one by one. */
	readproc();
	if ((q = pidof(name)) == NULL)
		goto exit;
	while ((p = get_next_from_pid_q(q))) {
		if (flags & PIDOF_OMIT) {
			for (i = 0; i < oind; i++) {
				if (opid[i] == p->pid)
					break;
			}
			/*
			 *	On a match, continue with
			 *	the for loop above.
			 */
			if (i < oind)
				continue;
		}
		if (flags & PIDOF_SINGLE) {
			if (spid)
				continue;
			else
				spid = p->pid;
		}
	}
exit:
	free(basec);
	freeproc();
	return spid;
}



int rmkdir(char *new_dir, int mode)
{
	size_t i = 0;

	DBPRT("new_dir: %s\n", new_dir);	
	if (new_dir == NULL || new_dir[0] == '\0')
		return -1;

	if (access(new_dir, R_OK|X_OK) == 0)
		return 0;
	
	if (new_dir[0] == '/')
		i++;
	
	for (; new_dir[i] != '\0'; i++) {
		if (new_dir[i] == '/') {
			char tmpdir[PATH_MAX + 1];

			strncpy (tmpdir, new_dir, i);
			tmpdir[i] = '\0';

			if ((mkdir(tmpdir, mode) == -1) && (errno != EEXIST))
				return -1;
		}	
	}

	if (mkdir(new_dir, mode) == -1 && errno != EEXIST)
		return -1;

	return 0;
}

/* Mapping */

char *val2str(struct affix_tupla *map, int value)
{
	for (;map; map++) {
		if (map->value == value) {
			if (map->str == NULL)
				return map->match;
			return map->str;
		}
	}
	return "";
}


int str2val(struct affix_tupla *map, char *str, unsigned int *val)
{
	char			*fp, *tmp;
	struct affix_tupla	*m;
	int			found;

	if (!str || !(str = strdup(str)))
		return 0;

	*val = 0;
	found = 1;

	for (fp = strtok_r(str, ", ", &tmp); fp; fp = strtok_r(NULL, ", ", &tmp)) {
		//printf("arg: [%s]\n", fp);
		for (m = map; m->match || m->str || (found = 0); m++) {
			if (m->match && strcasecmp(fp, m->match) == 0) {
				*val = m->value;
				free(str);
				return 1;
			}
		}
		if (!found) {
			free(str);
			return 0;
		}
	}
	free(str);
	return 0;
}


int str2mask(struct affix_tupla *map, char *str, unsigned int *mask)
{
	char			*fp, *tmp;
	struct affix_tupla	*m;
	int			found;

	if (!str || !(str = strdup(str)))
		return 0;

	*mask = 0;
	found = 1;

	for (fp = strtok_r(str, ", ", &tmp); fp; fp = strtok_r(NULL, ", ", &tmp)) {
		//printf("arg: [%s]\n", fp);
		for (m = map; m->match || (found = 0); m++) {
			if (strcasecmp(fp, m->match) == 0) {
				*mask |= m->value;
				break;
			}
		}
		if (!found) {
			free(str);
			return 0;
		}
	}
	free(str);
	return 1;
}

int mask2str(struct affix_tupla *map, char *str, unsigned int mask)
{
	int			i = 0;
	struct affix_tupla	*m;

	str[0] = '\0';
	for (m = map; m->match; m++) {
		if (m->value & mask)
			i += sprintf(str + i, "%s ", m->match);
	}
	if (i)
		str[i-1] = '\0';
	return 0;
}

int mask2str_comma(struct affix_tupla *map, char *str, unsigned int mask)
{
	int			i = 0;
	struct affix_tupla	*m;

	str[0] = '\0';
	for (m = map; m->match; m++) {
		if (m->value & mask)
			i += sprintf(str + i, "%s, ", m->match);
	}
	if (i)
		str[i-2] = '\0';
	return 0;
}

int str2cod(char *str, uint32_t *cod)
{
	struct affix_tupla	*map;
	int			found = 1;
	char			*arg, *tmp;

	if (!str || !(str = strdup(str)))
		return -1;

	/* get Major */
	arg = strtok_r(str, ", ", &tmp);
	if (arg == NULL) {
		free(str);
		return -1;
	}

	*cod = 0;
	for (map = codMajorClassMnemonic; map->match || (found=0); map++) {
		if (strncasecmp(map->match, arg, 3) == 0) {
			*cod |= map->value;
			break;
		}
	}
	if (!found){
		free(str);
		return -1;
	}

	/* get minor */
	arg = strtok_r(NULL, ", ", &tmp);
	if (arg == NULL) {
		free(str);
		return -1;
	}

	switch (*cod & HCI_COD_MAJOR) {
		case HCI_COD_COMPUTER:
			for (map = codMinorComputerMnemonic; map->match || (found=0); map++) {
				if (strncasecmp(map->match, arg, 3) == 0) {
					*cod |= map->value;
					break;
				}
			}
			break;
		case HCI_COD_PHONE:
			for (map = codMinorPhoneMnemonic; map->match || (found=0); map++) {
				if (strncasecmp(map->match, arg, 3) == 0) {
					*cod |= map->value;
					break;
				}
			}
			break;
		case HCI_COD_MAUDIO:
			for (map = codMinorAudioMnemonic; map->match || (found=0); map++) {
				if (strncasecmp(map->match, arg, 3) == 0) {
					*cod |= map->value;
					break;
				}
			}
			break;
		default:
			found = 0;
	}

	if (!found) {
		free(str);
		return -1;
	}

	/* get services */
	while ((arg = strtok_r(NULL, ", ", &tmp))) {
		for (map = codsdp_service_map; map->match || (found=0); map++) {
			if (strncasecmp(map->match, arg, 3) == 0) {
				*cod |= map->value;
				break;
			}
		}
		if (!found) {
			free(str);
			return -1;
		}
	}

	free(str);
	return 0;
}

int str2cod_svc(char *str, uint32_t *cod)
{
	struct affix_tupla	*map;
	int			found = 1;
	char			*arg, *tmp;

	if (!str || !(str = strdup(str)))
		return -1;

	/* get services */
	for (arg = strtok_r(str, ", ", &tmp); arg; arg = strtok_r(NULL, ", ", &tmp)) {
		for (map = codsdp_service_map; map->match || (found=0); map++) {
			if (strncasecmp(map->match, arg, 3) == 0) {
				*cod |= map->value;
				break;
			}
		}
		if (!found) {
			free(str);
			return -1;
		}
	}

	free(str);
	return 0;
}


int str2pkt_type(char *str, unsigned int *pkt_type)
{
	return str2mask(pkt_type_map, str, pkt_type);
}

int str2sec_level(char *str, unsigned int *sec_level)
{
	return str2mask(sec_level_map, str, sec_level);
}


char *argv2str(char *argv[])
{
	int	i = 0;
	char	*arg;
	static char	str[128];
	
	while ((arg = *argv++)) {
		i += sprintf(str + i, "%s ", arg);
	}
	str[i - 1] = '\0'; // remove last space
	return str;
}

/* slist_t */

slist_t *s_list_append(slist_t **list, void *data)
{
	slist_t	*entry, *prev;

	entry = (slist_t*)malloc(sizeof(slist_t));
	if (!entry)
		return NULL;
	entry->data = data;
	entry->next = NULL;
	
	if (!(*list)) {
		*list = entry;
		return entry;
	}
	
	for (prev = *list; prev->next; prev = prev->next) ;
	
	prev->next = entry;
	return entry;
}

slist_t *s_list_insert(slist_t **list, void *data, int i)
{
	slist_t	*entry, *prev;
	int	count;

	for (count = 0, prev = NULL, entry = *list; entry; 
			prev = entry, entry = entry->next, count++)
		if (count == i)
			break;

	entry = (slist_t*)malloc(sizeof(slist_t));
	if (!entry)
		return NULL;
	entry->data = data;
	if (!prev) {
		entry->next = *list;
		*list = entry;
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	return entry;
}

slist_t *s_list_insert_sorted(slist_t **list, void *data, slist_sort_func *func)
{
	slist_t	*entry, *prev;

	if (!func)
		return NULL;
	for (prev = NULL, entry = *list; entry; prev = entry, entry = entry->next) {
		if (func(data, entry->data) < 0)
			break;
	}
	entry = (slist_t*)malloc(sizeof(slist_t));
	if (!entry)
		return NULL;
	entry->data = data;
	if (!prev) {
		entry->next = *list;
		*list = entry;
	} else {
		entry->next = prev->next;
		prev->next = entry;
	}
	return entry;
}

void *s_list_dequeue(slist_t **list)
{
	void	*data;
	slist_t	*entry = *list;
	
	if (entry == NULL)
		return NULL;
	*list = entry->next;
	data = entry->data;
	free(entry);
	return data;
}

void s_list_remove(slist_t **list, void *data)
{
	slist_t	*entry, *prev;

	for (prev = NULL, entry = *list; entry; prev = entry, entry = entry->next) {
		if (entry->data == data) {
			if (prev)
				prev->next = entry->next;
			else
				*list = entry->next;
			free(entry);
			break;
		}
	}
}

void s_list_remove_custom(slist_t **list, void *data, slist_sort_func *func)
{
	slist_t	*entry = *list, *prev = NULL, *next = *list;

	if (!func)
		return;

	while (next) {
		entry = next;
		next = entry->next;				
		if (func(entry->data, data) == 0) {
			if (prev)
				prev->next = entry->next;
			else
				*list = entry->next;
			free(entry);
		} else
			prev = entry;
	}
}

void s_list_destroy(slist_t **list)
{
	slist_t	*entry;
	
	while (*list) {
		entry = *list;
		*list = entry->next;
		if (entry->data)
			free(entry->data);
		free(entry);
	}
}


int s_list_length(slist_t *list)
{
	slist_t	*entry;
	int	count;

	for (count = 0, entry = list; entry; entry = entry->next, count++) ;
	return count;
}

void s_list_free(slist_t **list)
{
	slist_t	*entry;

	while (*list) {
		entry = *list;
		*list = entry->next;
		free(entry);
	}
}

void *s_list_nth_data(slist_t *list, int i)
{
	slist_t	*entry;
	int	count;

	for (count = 0, entry = list; entry; entry = entry->next, count++)
		if (count == i)
			return entry->data;
	return NULL;
}

void s_list_foreach(slist_t *list, slist_func *func, void *param)
{
	slist_t	*entry;

	if (!func)
		return;
	for (entry = list; entry; entry = entry->next)
		func(entry->data, param);
}

slist_t *s_list_find_custom(slist_t *list, void *data, slist_sort_func *func)
{
	slist_t	*entry;

	if (!func)
		return NULL;
	for (entry = list; entry; entry = entry->next)
		if (func(entry->data, data) == 0)
			return entry;
	return NULL;
}

/* stuff */
int affix_wait_for_service_up(int port, int timeout)
{
	struct sockaddr_in	in;
	int			fd, err;
	time_t			stm, tm;
	
	in.sin_family = AF_INET;
	in.sin_port = port;
	in.sin_addr.s_addr = inet_addr("127.0.0.1");

	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0)
		return -1;
	stm = time(NULL);
	for (;;) {
		err = connect(fd, (struct sockaddr *)&in, sizeof(in));
		if (!err)
			break;
		tm = time(NULL);
		if (difftime(tm, stm) > timeout) {
			close(fd);
			return -1;
		}	
	}
	close(fd);
	return 0;
}

int affix_system(char *prog, int single)
{
	pid_t	pid;
	int	err;
	sighandler_t	sh;

	if (single) {
		pid = affix_pidof(prog, PIDOF_SINGLE | PIDOF_BASENAME, 0);
		if (pid)
			return 0;
	}
	/* start server */
	sh = signal(SIGCHLD, SIG_DFL);
	err = system(prog);
	signal(SIGCHLD, sh);
	return err ? -1 : 0;
}

/* device inquiry/known cache */

char *xml_element(char **buf, char **attr)
{
	char	*start = *buf, *next;

	// find first <
	start = strchr(start, '<');
	if (start == NULL)
		return NULL;
	start++;

	// find last >
	next = strchr(start, '>');
	if (next == NULL) {
		// broken
		return NULL;
	}
	*next = '\0';
	next++;
	*buf = next;

	// get first later of the element
	while (isblank(*start) && *start != '\0')
		start++;

	next = start+1;
	while (!isblank(*next) && *next != '\0')
		next++;
	
	*next = '\0';
	*attr = next+1;

	// check for "/"
	next = *buf-1;
	while (*next == '\0' || isblank(*next))
		next--;
	if (*next == '/')
		*next = '\0';

	return start;
}

char *xml_attribute(char **buf, char **value)
{
	char *start = *buf, *next;
	int flag = 0;

	//find attr name
	while (isblank(*start) && *start != '\0')
		start++;
	
	if (*start == '\0')
		return NULL;

	next = start+1;
	
	//find end
	while (!isblank(*next) && *next != '=' && *next != '\0')
		next++;

	if (*next == '=')
		flag = 1;
	
	*next = '\0';
	next++;
	
	if (flag == 0) {
		next = strchr(next, '=');
		if (next == NULL)
			return NULL;
		next++;
	}
	
	next = strchr(next, '"');
	if (next == NULL)
		return NULL;
	*value = next+1;
	next = strchr(next+1, '"');
	if (next == NULL)
		return NULL;
	*next = '\0';
	*buf = next+1;
	
	return start;
}

btdev_struct *btdev_cache_lookup(BD_ADDR *bda)
{
	btdev_struct	*entry;
	int		i;
	
	for (i = 0; (entry = s_list_nth_data(devcache.head, i)); i++) {
		if (bda_equal(bda, &entry->bda))
			return entry;
	}
	return NULL;
}

int btdev_cache_del(btdev_struct *entry)
{
	s_list_remove(&devcache.head, entry);
	free(entry);
	return 0;
}

btdev_struct *btdev_cache_add(BD_ADDR *bda)
{
	btdev_struct	*entry, *mount;
	int		i, num = -1;

	for (i = 0; (entry = s_list_nth_data(devcache.head, i)); i++) {
		if (bda_equal(bda, &entry->bda)) {
			s_list_remove(&devcache.head, entry);
			num = i;
			break;
		}
	}
	if (!entry) {
		/* create new */
		entry = malloc(sizeof(btdev_struct));
		if (!entry) {
			perror("btdev_cache allocation failed\n");
			return NULL;
		}
		memset(entry, 0, sizeof(btdev_struct));
		entry->bda = *bda;
		entry->state = DEVSTATE_UNKNOWN;
	}
	/* find linking position */
	for (i = 0; (mount = s_list_nth_data(devcache.head, i)); i++) {
		if (mount->state == DEVSTATE_RANGE)
			continue;
		if (mount->state == DEVSTATE_GONE || i == num)
			break;
	}
	s_list_insert(&devcache.head, entry, i);
	return entry;
}

int btdev_cache_init(void)
{
	devcache.file = strdup(affix_cachefile);
	if (!devcache.file)
		return -1;

	devcache.head = NULL;
	devcache.count = 0;
	devcache.lock = -1;
	devcache.stamp = 0;
	return 0;
}

int btdev_cache_load(int locked)
{
	char		buf[256];
	FILE		*cfd;
	size_t		read;
	char		*next = NULL, *elem, *attrs, *attr, *value;
	BD_ADDR		bda;
	int		found = 0, eof = 0;
	struct stat	st;

	if (!devcache.file && btdev_cache_init() < 0)
		return -1;

	if (stat(devcache.file, &st) < 0) {
		return -1;
	}
	
	if (st.st_mtime == devcache.stamp) {
		if (locked && btdev_cache_lock() < 0) {
			return -1;
		}
		return 0;
	}
	
	devcache.stamp = st.st_mtime;
	
	if (btdev_cache_lock() < 0) {
		return -1;
	}

	cfd = fopen(devcache.file, "r");
	if (!cfd){
		fprintf(stderr, "Unable to open cache: %s\n", devcache.file);
		btdev_cache_unlock();
		return -1;
	}

	if (devcache.head) {
		s_list_destroy(&devcache.head);
		devcache.head = NULL;
		devcache.count = 0;
	}
	
	for (;;) {
		int	free;
		
		if (next) {
			/* we have some info in the buffer */
			free =  next - buf;
			memmove(buf, next, sizeof(buf) - free);
		} else
			free = sizeof(buf);
			
		if (!eof) {
			//printf("reading %d butes\n", free);
			read = fread(buf + sizeof(buf) - free, 1, free, cfd);
			if (!read)
				eof = 1;
		}

		next = (void*)buf;
		elem = xml_element(&next, &attrs);
		if (!elem)
			break;

		if (!found)
			if (strcmp(elem, "device-listing") == 0) {
				found = 1;
				continue;
			}

		if (strcmp(elem, "/device-listing") == 0)
			break;
		//printf("element: %s\n", elem);
		//printf("attr left: %s\n", attrs);
		// get attributes
		if (strcmp(elem, "device") == 0) {
			btdev_struct	*entry;
			entry = NULL;
			while ((attr = xml_attribute(&attrs, &value))) {
				//printf("%s = %s\n", attr, value);
				if (!entry) {
					if (strcmp(attr, "bda") == 0) {
						str2bda(&bda, value);
						entry = btdev_cache_add(&bda);
					}
				} else if (strcmp(attr, "class") == 0) {
					sscanf(value, "%x", &entry->cod);
				} else if (strcmp(attr, "name") == 0) {
					strcpy(entry->name, value);
				} else if (strcmp(attr, "key") == 0) {
					unsigned int	val;
					int		i;
					/* convert key to binary format */
					for (i = 0; sscanf(value, "%2x", &val) > 0 && i < 16; i++, value += 2) {
						entry->link_key[i] = val;
					}
					if (i)
						entry->flags |= BTDEV_KEY;
				}
			}
		}
	}
	fclose(cfd);
	if (!locked)
		btdev_cache_unlock();
	return 0;
}

int btdev_cache_reload(void)
{
	return btdev_cache_load(1);
}

	
int btdev_cache_save(void)
{
	btdev_struct	*entry;
	FILE		*cfd;
	int		i, k;

	if (devcache.lock == -1 && btdev_cache_lock() < 0)
		return -1;

	cfd = fopen(devcache.file, "w");
	if (!cfd) {
		fprintf(stderr, "Unable to fopen cache file: %s\n", devcache.file);
		btdev_cache_unlock();
		return -1;
	}
	fprintf(cfd, "<device-listing>\n");
	for (i = 0; (entry = s_list_nth_data(devcache.head, i)); i++) {
		fprintf(cfd, "<device bda=\"%s\"", bda2str(&entry->bda));
		if (entry->cod)
			fprintf(cfd, " class=\"%x\"", entry->cod);
		if (entry->name[0] != '\0')
			fprintf(cfd, " name=\"%s\"", entry->name);
		if (entry->flags & BTDEV_KEY) {
			fprintf(cfd, " key=\"");
			for (k = 0; k < 16; k++)
				fprintf(cfd, "%02x", entry->link_key[k]);
			fprintf(cfd, "\"");
		}

		fprintf(cfd, "/>\n");
	}
	fprintf(cfd, "</device-listing>\n");
	fclose(cfd);
	btdev_cache_unlock();
	return 0;
}

void btdev_cache_free(void)
{
	if (devcache.head) {
		s_list_destroy(&devcache.head);
		devcache.head = NULL;
		devcache.count = 0;
	}
	if (devcache.file) {
		free(devcache.file);
		devcache.file = NULL;
	}
}

void btdev_cache_purge(void)
{
	if (devcache.head) {
		s_list_destroy(&devcache.head);
		devcache.head = NULL;
		devcache.count = 0;
	}
	if (!devcache.file)
		btdev_cache_init();
	btdev_cache_save();
}

int btdev_cache_lock(void)
{
	devcache.lock = open(devcache.file, O_CREAT, 0644);
	if (devcache.lock < 0) {
		fprintf(stderr, "Unable to open cache for locking: %s\n", devcache.file);
		return devcache.lock;
	}

	if (flock(devcache.lock, LOCK_EX) < 0) {
		fprintf(stderr, "Unable to lock cache\n");
		close(devcache.lock);
		devcache.lock = -1;
		return -1;
	}
	return 0;
}

void btdev_cache_unlock(void)
{
	if (devcache.lock < 0)
		return;

	close(devcache.lock);
	devcache.lock = -1;
}

/*
 * Inquiry Cache Stuff
 */
void btdev_cache_retire(void)
{
	btdev_struct	*entry;
	int		i;
	
	for (i = 0; (entry = s_list_nth_data(devcache.head, i)); i++)
		entry->state = DEVSTATE_GONE;
}

void btdev_cache_print(int state)
{
	btdev_struct	*entry;
	char		buf[256], *name;
	int		i;
	char		ch;

	for (i = 0; (entry = s_list_nth_data(devcache.head, i)); i++) {
		if (!(entry->state & state))
			continue;
		parse_cod(buf, entry->cod);
		if (entry->name[0] != '\0')
			name = entry->name;
		else
			name = "(none)";
		switch (entry->state) {
			case DEVSTATE_RANGE:
				ch = '+';
				break;
			case DEVSTATE_GONE:
				ch = '-';
				break;
			case DEVSTATE_UNKNOWN:
			default:
				ch = ' ';
		}
		printf("%c%d: Address: %s, Class: 0x%06X, Key: \"%s\"", 
				ch, i+1, bda2str(&entry->bda), entry->cod, (entry->flags & BTDEV_KEY)?"yes":"no");
		printf(", Name: \"%s\"\n", name);
		printf("    %s\n", buf);
	}
}

int btdev_cache_resolve(BD_ADDR *bda, int id)
{
	btdev_struct 	*entry;
	
	if (id < 0)
		return -1;
	entry = s_list_nth_data(devcache.head, id - 1);
	if (!entry)
		return -1;
	*bda = entry->bda;
	return 0;
}

btdev_struct *__btdev_cache_add(BD_ADDR bda, uint32_t cod, char *name)
{
	btdev_struct *entry;

	entry = btdev_cache_add(&bda);
	if (!entry)
		return NULL;

	entry->state = DEVSTATE_RANGE;
	entry->cod = cod;
	if (name)
		strcpy(entry->name, name);

	return entry;
}


int btdev_get_bda(BD_ADDR *bda, char *peer)
{
	int	err;
	
	err = str2bda(bda, peer);
	if (!err) {
		/* try resolve */
		int	id;
		id = atoi(peer);
		//printf("id: %d\n", id);
		if (!id) 
			return -1;
		if (btdev_cache_load(0) < 0)
			return -1;
		err = btdev_cache_resolve(bda, id);
		if (err)
			return -1;
	}
	return 0;
}

/*
 * CLASS Of Device stuff
 */
int parse_cod(char *buf, uint32_t  cod)
{
	int			count = 0, found = 1;
	struct affix_tupla	*map;

	switch (cod & HCI_COD_MAJOR) {
		case HCI_COD_COMPUTER:
			for (map = codMinorComputerMnemonic; map->str || (found=0); map++) {
				if (map->value == (cod & HCI_COD_MINOR)) {
					count += sprintf(buf+count, "Computer (%s)", map->str);
					break;
				}
			}
			if (!found)
				count += sprintf(buf+count, "Computer (Unclassified)");
			break;
		case HCI_COD_PHONE:
			for (map = codMinorPhoneMnemonic; map->str || (found=0); map++) {
				if (map->value == (cod & HCI_COD_MINOR)) {
					count += sprintf(buf+count, "Phone (%s)", map->str);
					break;
				}
			}
			if (!found)
				count += sprintf(buf+count, "Phone (Unclassified)");
			break;
		case HCI_COD_MAUDIO:
			for (map = codMinorAudioMnemonic; map->str || (found=0); map++) {
				if (map->value == (cod & HCI_COD_MINOR)) {
					count += sprintf(buf+count, "Audio (%s)", map->str);
					break;
				}
			}
			if (!found)
				count += sprintf(buf+count, "Audio (Unclassified)");
			break;
		default:
			for (map = codMajorClassMnemonic; map->str || (found=0); map++) {
				if (map->value == (cod & HCI_COD_MAJOR)) {
					count += sprintf(buf+count, "%s (Unclassified)", map->str);
					break;
				}
			}
			if (!found)
				count += sprintf(buf+count, "Unclassified (Unclassified)");
	}
	count += sprintf(buf+count, " [");
	for (map = codsdp_service_map; map->str; map++) {
		if (map->value & cod) {
			count += sprintf(buf+count, "%s,", map->str);
		}
	}
	count--;	// remove ,
	count += sprintf(buf+count, "]");

	return 0;
}

/* commands */
void print_usage(struct command *cmd)
{
	printf("usage: %s %s\n", cmd->name, cmd->arg ? cmd->arg : "");
}

void print_full_usage(struct command *cmd)
{
	if (cmd->name)
		printf("usage: %s %s\n", cmd->name, cmd->arg ? cmd->arg : "");
	if (cmd->msg)
		printf("description:\n%s", cmd->msg);

}

int print_command_usage(struct command *cmds, char *command)
{
	struct command	*cmd;
	struct command	nullcmd = {NULL, NULL, 0, NULL, NULL};

	for (cmd = cmds; memcmp(cmd, &nullcmd, sizeof(nullcmd)) != 0; cmd++) {
		if (cmd->name)
			if (strcmp(cmd->name, command) == 0) {
				print_full_usage(cmd);
				return 0;
			}
	}
	printf("invalid command: %s\n", command);
	return 	1;
}

void print_all_usage(struct command *cmds)
{
	struct command	*cmd;
	struct command	nullcmd = {NULL, NULL, 0, NULL, NULL};
	
	for (cmd = cmds; memcmp(cmd, &nullcmd, sizeof(nullcmd)) != 0; cmd++) {
		if (cmd->name && cmd->arg)
			printf("%s %s\n", cmd->name, cmd->arg);
		else if (cmd->msg)
			printf("%s", cmd->msg);
	}
}

int call_command(struct command *cmds, char *command)
{
	struct command	*cmd;
	struct command	nullcmd = {NULL, NULL, 0, NULL, NULL};
	
	for (cmd = cmds; memcmp(cmd, &nullcmd, sizeof(nullcmd)) != 0; cmd++) {
		if (cmd->name && strcasecmp(cmd->name, command) == 0) {
			return cmd->func(cmd);
		}
	}
	printf("Invalid command: %s\n", command);
	return -1;
}


