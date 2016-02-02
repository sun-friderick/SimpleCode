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
   $Id: btcore.h,v 1.71 2004/03/24 12:28:09 kassatki Exp $

   HCI access library

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#ifndef	_BTCORE_H
#define	_BTCORE_H

#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <syslog.h>

/* pidof */
#include <sys/wait.h>
#include <dirent.h>

#include <stdint.h>

#include <affix/hci_types.h>
#include <affix/bluetooth.h>

__BEGIN_DECLS

extern char		*affix_version;
extern unsigned long	affix_logmask;

extern char		btdev[IFNAMSIZ];
extern int		linkmode;
extern int		promptmode;

extern int		verboseflag;
extern char		*progname;

extern int		__argc;
extern char		**__argv;

extern char		affix_userpref[80];
extern char		affix_cachefile[80];

extern char 		*hci_errlist[];
extern char 		*lmp_compid[];

#define NUM_LMPCOMPID	37

struct affix_tupla {
	unsigned int	value;
	char		*match;
	char		*str;
};

extern struct affix_tupla debug_flags_map[];

extern struct affix_tupla affix_protos[];

extern struct affix_tupla sdp_proto_map[];
extern struct affix_tupla codsdp_service_map[];
extern struct affix_tupla codMajorClassMnemonic[];
extern struct affix_tupla codMinorComputerMnemonic[];
extern struct affix_tupla codMinorPhoneMnemonic[];
extern struct affix_tupla codMinorAudioMnemonic[];

extern struct affix_tupla pkt_type_map[];
extern struct affix_tupla sec_level_map[];
extern struct affix_tupla sec_mode_map[];
extern struct affix_tupla role_map[];
extern struct affix_tupla scan_map[];
extern struct affix_tupla audio_features_map[];
extern struct affix_tupla policy_features_map[];
extern struct affix_tupla radio_features_map[];
extern struct affix_tupla timing_features_map[];
extern struct affix_tupla packets_features_map[];

#define BTDEBUG_MODULE_CONTROL	0x01
#define BTDEBUG_MODULE_SDP	0x02
#define BTDEBUG_MODULE_OBEX	0x04
#define BTDEBUG_MODULE_HFP	0x08
#define BTDEBUG_MODULE_TOOLS	0x10

#ifndef BTDEBUG_MODULE 
#define BTDEBUG_MODULE		0
#endif


void btlog_hexdump(const char *fname, const unsigned char *data, int len);

#ifdef CONFIG_AFFIX_DEBUG
#define DBPRT(fmt, args...)	do { \
					if (affix_logmask & BTDEBUG_MODULE)\
						syslog(LOG_DEBUG, "%s: " fmt, __FUNCTION__ , ##args);\
				} while (0)

#define BTDUMP(data, len)	do { \
					if (affix_logmask & BTDEBUG_MODULE)\
						btlog_hexdump(__FUNCTION__, data, len);\
				} while (0)
#else
#define DBPRT(args...)
#define BTDUMP(data, len)
#endif

#define BTDEBUG(fmt, args...)		syslog(LOG_DEBUG, "%s: " fmt, __FUNCTION__ , ##args)
#define BTERROR(fmt, args...)		syslog(LOG_ERR, "%s: " fmt, __FUNCTION__ , ##args)
#define BTWARN(fmt, args...)		syslog(LOG_WARNING, "%s: " fmt, __FUNCTION__ , ##args)
#define BTNOTICE(fmt, args...)		syslog(LOG_NOTICE, "%s: " fmt, __FUNCTION__ , ##args)
#define BTINFO(fmt, args...)		syslog(LOG_INFO, "%s: " fmt, __FUNCTION__ , ##args)


// General purpose functions
char *hci_error(int err);
int str2bda(BD_ADDR *p, char *str);
void _bda2str(char *str, BD_ADDR *p);
char *bda2str(BD_ADDR *bda);

int affix_init(int argc, char *argv[], int facility);

/* general hci */


/* Control */
int hci_ioctl(int cmd, void *arg);
int hci_add_pin(BD_ADDR *bda, int Length, __u8 *Code);
int hci_add_key(BD_ADDR *bda, __u8 key_type, __u8 *key);
int hci_remove_key(BD_ADDR *bda);

int hci_open_uart(char *name, int type, int proto, int speed, int flags);
int hci_close_uart(char *name);
int hci_setup_uart(char *name, int proto, int speed, int flags);

int hci_get_conn(int fd, BD_ADDR *bda);


static inline int hci_set_mode(int fd, int mode)
{
	return btsys_ioctl(fd, BTIOC_SET_CTL, &mode);
}

static inline int hci_get_attr(char *name, struct hci_dev_attr *attr)
{
	strncpy(attr->name, name, IFNAMSIZ);
	attr->devnum = 0;
	return hci_ioctl(BTIOC_GET_ATTR, attr);
}

static inline int hci_get_attr_id(int devnum, struct hci_dev_attr *attr)
{
	attr->devnum = devnum;
	return hci_ioctl(BTIOC_GET_ATTR, attr);
}

static inline int hci_get_attr_fd(int fd, struct hci_dev_attr *attr)
{
	return btsys_ioctl(fd, BTIOC_GET_ATTR, attr);
}

static inline int hci_set_attr(int fd, struct hci_dev_attr *attr)
{
	return btsys_ioctl(fd, BTIOC_SET_ATTR, attr);
}

static inline int hci_devnum(char *name)
{
	int			err;
	struct hci_dev_attr	attr;
	
	err = hci_get_attr(name , &attr);
	if (err)
		return err;
	return attr.devnum;
}

static inline int hci_get_bda_id(int devnum, BD_ADDR *bda)
{
	int			err;
	struct hci_dev_attr	attr;
	
	err = hci_get_attr_id(devnum , &attr);
	if (err)
		return err;
	*bda = attr.bda;
	return 0;
}

static inline int hci_get_flags_id(int devnum, int *flags)
{
	int			err;
	struct hci_dev_attr	attr;
	
	err = hci_get_attr_id(devnum , &attr);
	if (err)
		return err;
	*flags = attr.flags;
	return 0;
}

static inline int hci_start_dev(int fd)
{
	int	op = 1;
	return btsys_ioctl(fd, BTIOC_START_DEV, &op);
}

static inline int hci_stop_dev(int fd)
{
	int	op = 0;
	return btsys_ioctl(fd, BTIOC_START_DEV, &op);
}

static inline int hci_get_pkttype(int fd, int *pkt_type)
{
	return btsys_ioctl(fd, BTIOC_GET_PKTTYPE, pkt_type);
}

static inline int hci_set_pkttype(int fd, int pkt_type)
{
	return btsys_ioctl(fd, BTIOC_SET_PKTTYPE, &pkt_type);
}

static inline int hci_set_role(int fd, int role)
{
	return btsys_ioctl(fd, BTIOC_SET_ROLE, &role);
}

static inline int hci_remove_pin(BD_ADDR *bda)
{
	return hci_ioctl(BTIOC_REMOVEPINCODE, bda);
}

static inline int hci_set_dbmask(__u32 dbmask)
{
	return hci_ioctl(BTIOC_DBMSET, &dbmask);
}

static inline int hci_get_dbmask(__u32 *dbmask)
{
	return hci_ioctl(BTIOC_DBMGET, dbmask);
}

static inline int hci_get_devs(int *devs)
{
	return hci_ioctl(BTIOC_GETDEVS, devs);
}

static inline int hci_get_version(int fd, struct affix_version *ver)
{
	return btsys_ioctl(fd, BTIOC_GET_VERSION, ver);
}

static inline int hci_disconnect(struct sockaddr_affix *sa)
{
	return hci_ioctl(BTIOC_HCI_DISC, sa);
}


/* socket stuff */

static inline int hci_getmtu(int fd)
{
	int		err, mtu;
	socklen_t	len;

	err = getsockopt(fd, SOL_AFFIX, BTSO_MTU, &mtu, &len);
	if (err < 0)
		return err;

	return mtu;
}

/* L2CAP */
static inline int l2cap_getmtu(int fd)
{
	return hci_getmtu(fd);
}

static inline int l2cap_setmtu(int fd, int mtu)
{
	return setsockopt(fd, SOL_AFFIX, BTSO_MTU, &mtu, sizeof(mtu));
}

static inline int l2cap_flush(int fd)
{
	return ioctl(fd, SIOCL2CAP_FLUSH, 0);
}

int l2cap_sendping(int fd, char *data, int size, int type);
int l2cap_ping(int fd, char *data, int size);

static inline int l2cap_singleping(int fd, char *data, int size)
{
	return l2cap_sendping(fd, data, size, L2CAP_SINGLEPING);
}


/* RFCOMM stuff */
int rfcomm_get_ports(struct rfcomm_port *pi, int size);

static inline int rfcomm_open_tty(int fd, int line)
{
	int	err;

	err = ioctl(fd, SIOCRFCOMM_OPEN_BTY, &line);
	if (err < 0)
		return err;
	return line;
}

static inline int rfcomm_close_tty(int line)
{
	int	fd;

	fd = socket(PF_AFFIX, SOCK_STREAM, BTPROTO_RFCOMM);
	if (fd < 0)
		return fd;
	return ioctl(fd, SIOCRFCOMM_CLOSE_BTY, &line);
}

static inline int rfcomm_set_type(int fd, int type)
{
	return ioctl(fd, SIOCRFCOMM_SETTYPE, &type);
}

static inline int rfcomm_bind_tty(int fd, struct sockaddr_affix *sa, int line)
{
	int	err;
	struct rfcomm_port port;
	
	port.line = line;
	port.addr = *sa;
	err = ioctl(fd, SIOCRFCOMM_BIND_BTY, &port);
	if (err < 0)
		return err;
	return port.line;
}



int affix_pan_init(char *name, int mode);
int affix_pan_connect(struct sockaddr_affix *sa, int role);


/* core utils */

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline void *u8_to_ptr(uint8_t data)
{
	unsigned int	tmp = data;
	return ((void*)tmp);
}
static inline void *u16_to_ptr(uint16_t data)
{
	unsigned int	tmp = data;
	return ((void*)tmp);
}
static inline void *u32_to_ptr(uint32_t data)
{
	unsigned int	tmp = data;
	return ((void*)tmp);
}
#elif __BYTE_ORDER == __BIG_ENDIAN
#warning "BIGENDIAN"
static inline void *u8_to_ptr(uint8_t data)
{
	unsigned int	tmp = data;
	tmp <<= 24;
	return ((void*)tmp);
}
static inline void *u16_to_ptr(uint16_t data)
{
	unsigned int	tmp = data;
	tmp <<= 16;
	return ((void*)tmp);
}
static inline void *u32_to_ptr(uint32_t data)
{
	unsigned int	tmp = data;
	return ((void*)tmp);
}
#else
#error "__BYTE_ORDER is not defined"
#endif

typedef struct _slist_t{
	void		*data;
	struct _slist_t	*next;
} slist_t;

typedef void (slist_func)(void *data, void *param);
typedef int (slist_sort_func)(const void *data1, const void *data2);

static inline slist_t *s_list_next(slist_t *list)
{
	return list->next;
}

static inline void *s_list_data(slist_t *list)
{
	return list->data;
}

static inline unsigned int s_list_uint(slist_t *list)
{
	return (unsigned int)list->data;
}


slist_t *s_list_append(slist_t **list, void *data);
slist_t *s_list_insert(slist_t **list, void *data, int i);
slist_t *s_list_insert_sorted(slist_t **list, void *data, slist_sort_func *func);
int s_list_length(slist_t *list);
void s_list_free(slist_t **list);
void *s_list_nth_data(slist_t *list, int i);
void s_list_foreach(slist_t *list, slist_func *func, void *param);
slist_t *s_list_find_custom(slist_t *list, void *data, slist_sort_func *func);
void s_list_remove(slist_t **list, void *data);
void s_list_remove_custom(slist_t **list, void *data, slist_sort_func *func);
void *s_list_dequeue(slist_t **list);
void s_list_destroy(slist_t **list);

static inline slist_t *s_list_append_uint(slist_t **list, unsigned int value)
{
	return s_list_append(list, (void*)value);
}

static inline unsigned int s_list_nth_uint(slist_t *list, int i)
{
	return (unsigned int)s_list_nth_data(list, i);
}


char *val2str(struct affix_tupla *map, int value);
int str2val(struct affix_tupla *map, char *str, unsigned int *val);
int str2mask(struct affix_tupla *map, char *str, unsigned int *mask);
int mask2str(struct affix_tupla *map, char *str, unsigned int mask);
int mask2str_comma(struct affix_tupla *map, char *str, unsigned int mask);
int str2cod(char *str, uint32_t *cod);
int str2cod_svc(char *str, uint32_t *cod);
int str2pkt_type(char *str, unsigned int *pkt_type);
int str2sec_level(char *str, unsigned int *sec_level);

/* file stuff */
int rmkdir(char *new_dir, int mode);
int get_fileinfo(const char *name, char *lastmod);

#define PIDOF_SINGLE	0x01
#define PIDOF_OMIT	0x02
#define PIDOF_POMIT	0x04
#define PIDOF_SCRIPTS	0x08
#define PIDOF_BASENAME	0x10

int affix_pidof(char *name, int flags, pid_t pid);


//
// Get the filesize
//
static inline int get_filesize(const char *filename)
{
	struct stat stats;
	/*  Need to know the file length */
	stat(filename, &stats);
	return (int) stats.st_size;
}

static inline int get_fdsize(int fd)
{
	struct stat stats;
	/*  Need to know the file length */
	fstat(fd, &stats);
	return (int) stats.st_size;
}

void affix_background(void);
int affix_system(char *prog, int single);
int affix_wait_for_service_up(int port, int timeout);

/* device inquiry/known cache */
char *xml_element(char **buf, char **attr);
char *xml_attribute(char **buf, char **value);

#define BTDEV_KEY		0x0001

/* Inquiry Cache support */
#define DEVSTATE_RANGE		0x01
#define DEVSTATE_GONE		0x02
#define DEVSTATE_UNKNOWN	0x04
#define DEVSTATE_ALL		0xFF

typedef struct {
	int		flags;
	int		state;	/* to keep inquiry info */
	
	BD_ADDR		bda;
	uint32_t	cod;
	char		name[248];

	__u8		key_type;
	__u8		link_key[16];
	__u8		pin_length;
	__u8		pin[16];
} btdev_struct;

typedef struct {
	slist_t	*head;
	int	count;
	char	*file;
	int	lock;
	time_t	stamp;
} btdev_list;

extern btdev_list	devcache;

int btdev_cache_lock(void);
void btdev_cache_unlock(void);

btdev_struct *btdev_cache_lookup(BD_ADDR *bda);
int btdev_cache_del(btdev_struct *entry);
btdev_struct *btdev_cache_add(BD_ADDR *bda);
int btdev_cache_reload(void);
int btdev_cache_load(int locked);
void btdev_cache_purge(void);
int btdev_cache_save(void);
void btdev_cache_free(void);
int btdev_cache_init(void);

int parse_cod(char *buf, uint32_t  cod);
char *argv2str(char *argv[]);

/* dev cache */
void btdev_cache_retire(void);
void btdev_cache_print(int state);
int btdev_cache_resolve(BD_ADDR *bda, int id);
btdev_struct *__btdev_cache_add(BD_ADDR bda, uint32_t cod, char *name);

int btdev_get_bda(BD_ADDR *bda, char *arg);

/* commands */
struct command {
	char	*name;
	int	(*func)(struct command *cmd);
	int	cmd;
	char	*arg;
	char	*msg;	/* extra info message */
};

void print_usage(struct command *cmd);
void print_all_usage(struct command *cmds);
void print_full_usage(struct command *cmd);
int print_command_usage(struct command *cmds, char *command);
int call_command(struct command *cmds, char *command);

__END_DECLS

#endif
