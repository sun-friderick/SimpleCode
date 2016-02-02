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
   $Id: obex.h,v 1.23 2004/02/26 13:56:47 kassatki Exp $

   OBEX client lib 

   Fixes:
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

#ifndef OBEX_CLIENT_H
#define OBEX_CLIENT_H

#include <termios.h>
#include <stdint.h>
#include <openobex/obex.h>

#include <affix/btcore.h>

__BEGIN_DECLS

#if defined(CONFIG_AFFIX_SDP)
#include "sdp.h"
#include "sdpclt.h"
#endif

#ifndef OPENOBEX_VERSION
#define OPENOBEX_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#define OBEX_LS		1
#define OBEX_PUT	2
#define OBEX_GET	3
#define OBEX_SETPATH	4
#define OBEX_PUSH	5
#define OBEX_REMOVE	6
#define OBEX_PULL	7

/* Number of bytes passed at one time to OBEX */
#define OBEX_STREAM_CHUNK	4096

typedef struct {
	uint8_t		len;
	uint8_t		*data;
} obex_target_t;

typedef struct {
	obex_t		*handle;
	int		fd;		/* rfcomm socket */
       	int		clientdone;

	uint32_t	conid;		/* connection id */
	int		rsp;		/* error code */
	int		opcode;		/* internal operation code */
	char		*buf;		/* response storage place */
	int		sfd;		/* stream fd */
	
	uint16_t	family;
} obexclt_t;

/* connect/disconnect */
obexclt_t *obex_connect(struct sockaddr_affix *addr, obex_target_t *target, int *err);
int obex_disconnect(obexclt_t *clt);

int obex_get(obexclt_t *clt, char *local, char *remote, char *type);
int obex_put(obexclt_t *clt, char *local, char *remote, char *type);
/* -- file -- */
int obex_remove(obexclt_t *clt, char *name);
int obex_get_file(obexclt_t *clt, char *local, char *remote);
int obex_put_file(obexclt_t *clt, char *local, char *remote);

#define SETPATH_CREATE	0x0001
int __obex_setpath(obexclt_t *clt, char *path, int flags);

extern obex_target_t	file_target;

static inline obexclt_t *obex_connect_file(struct sockaddr_affix *addr, int *err)
{
	return obex_connect(addr, &file_target, err);
}

static inline int obex_browse(obexclt_t *clt, char *local, char *name)
{
	return obex_get(clt, local, name, "x-obex/folder-listing");
}

static inline int obex_setpath(obexclt_t *clt, char *path)
{
	return __obex_setpath(clt, path, 0);
}

static inline int obex_mkdir(obexclt_t *clt, char *path)
{
	int	err;
	err = __obex_setpath(clt, path, SETPATH_CREATE);
	if (!err)
		err = __obex_setpath(clt, "..", 0);
	return err;
}


/* -------------------------------------------------------------- */

#define CHDIR_CHECK_ONLY		0x0001
#define CHDIR_CREATE_IF_NOT_EXIST	0x0002
#define CHDIR_CREATE_RECURSIVE		0x0004

typedef struct obex_common_hdr {
	uint8_t  opcode;
	uint16_t len;
} __PACK__ obex_common_hdr_t;

typedef struct obex_connect_hdr {
	uint8_t  version;
	uint8_t  flags;
	uint16_t mtu;
} __PACK__ obex_connect_hdr_t;


typedef struct obex_setpath_hdr {
	uint8_t  flags;
	uint8_t	constants;
} __PACK__ obex_setpath_hdr_t;


enum obexsrv_state {
	SRVSTATE_IDLE = 0,
	SRVSTATE_CLOSED
};

typedef struct _obexsrv_t	obexsrv_t;

struct _obexsrv_t {
	obex_t		*handle;
	int		state;
	int		flags;
        int		serverdone;
	int		streamming;
        char		*name;
	int		sfd;
	char		*buf;

	/* callbacks */
	int 		(*connect)(obexsrv_t *srv, obex_target_t *target);
	int 		(*put)(obexsrv_t *srv, char *file, char *name, char *type, int flags);
	int 		(*get)(obexsrv_t *srv, char *name, char *type);
	int 		(*setpath)(obexsrv_t *srv, char *path, int flags);
	void 		(*disconnect)(obexsrv_t *srv);
};

int obexsrv_run(obexsrv_t *srv, int rfd, int wfd);
void obexsrv_set_file(obexsrv_t *srv, char *name, int del);


/* ----------------------------------------------------- */

typedef struct {
	char	*name;
	int	fd;
	void	*map;
	int	map_size;
} obex_file_t;

obex_file_t *obex_create_file(char *name);
void obex_close_file(obex_file_t *file);
int obex_open_file(obex_file_t *file);
void obex_destroy_file(obex_file_t *file, int del);
char *obex_map_file(obex_file_t *file);

char *obex_error(int err);

__END_DECLS

#endif

