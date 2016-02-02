/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
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
   $Id: obex_client.c,v 1.35 2004/03/24 18:55:52 kassatki Exp $

   OBEX client lib 

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

*/

#include <affix/config.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>

// OBEX stuff
#include <openobex/obex.h>
#include <affix/obex.h>

obex_target_t	file_target = {16, "\xF9\xEC\x7B\xC4\x95\x3C\x11\xD2\x98\x4E\x52\x54\x00\xDC\x9E\x09"};

void _obex_error(char *buf, int err)
{
	if (err < 0)
		sprintf(buf, "System error: %s (%d)", strerror(errno), errno);
	else if (err > 0)
		sprintf(buf, "OBEX error: %s (%#x)", OBEX_ResponseToString(err), err);
	else
		sprintf(buf, "No error (0)");
}

char *obex_error(int err)
{
	static unsigned char 	buf[80][2];
	static int 		num = 0; 

	num = 1 - num; /* switch buf */
	_obex_error(buf[num], err);
	return buf[num];
}


/* obex client library */
obex_file_t *obex_create_file(char *name)
{
	obex_file_t	*file;

	file = malloc(sizeof(obex_file_t));
	if (!file)
		return NULL;
	if (!name) {
		file->name = strdup("/tmp/obex_tmp_XXXXXX");
		if (!file->name) {
			free(file);
			return NULL;
		}		
		file->fd = mkstemp(file->name);
	} else {
		file->name = strdup(name);
		if (!file->name) {
			free(file);
			return NULL;
		}
		file->fd = open(file->name, O_RDONLY);
	}
	if (file->fd < 0) {
		free(file->name);
		free(file);
		return NULL;
	}
	file->map_size = 0;
	return file;
}

void obex_close_file(obex_file_t *file)
{
	if (file->map_size)
		munmap(file->map, file->map_size);
	if (file->fd >= 0) {
		close(file->fd);
		file->fd = -1;
	}
}

void obex_destroy_file(obex_file_t *file, int del)
{
	obex_close_file(file);
	if (del)
		unlink(file->name);
	free(file->name);
	free(file);
}

int obex_open_file(obex_file_t *file)
{
	if (file->fd >= 0)
		return lseek(file->fd, 0, SEEK_SET);
	else
		return (file->fd = open(file->name, O_RDONLY));
}

char *obex_map_file(obex_file_t *file)
{
	if (obex_open_file(file) < 0)
		return NULL;
	file->map_size = get_fdsize(file->fd);
	file->map = mmap(0, file->map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, file->fd, 0);
	if (file->map == MAP_FAILED)
		return NULL;
	return file->map;
}

		
void obexclt_connect(obex_t *handle, obex_object_t *object, int obex_rsp)
{
	obex_headerdata_t	hv;
	uint8_t			hi;
	int			hlen;
	uint8_t			*nonhdrdata;
	const uint8_t		*who = NULL;
	int			who_len = 0;
	obexclt_t	*clt = OBEX_GetUserData(handle);

	DBPRT("");

	clt->rsp = obex_rsp;
	if (obex_rsp != OBEX_RSP_SUCCESS)
		return;

	if(OBEX_ObjectGetNonHdrData(object, &nonhdrdata) == 4) {
		DBPRT("Version: 0x%02x. Flags: 0x%02x  OBEX packet length:%d",
			nonhdrdata[0], nonhdrdata[1], *(uint16_t*)&(nonhdrdata[2]));
	} else {
		BTERROR("Invalid packet content.");
	}
	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
			case OBEX_HDR_WHO:
				who = hv.bs;
				who_len = hlen;
				DBPRT("got WHO: %*s, len: %d", who_len, who, who_len);
				break;
			case OBEX_HDR_CONNECTION:
				clt->conid = hv.bq4;
				DBPRT("got Conection ID: %#x\n", hv.bq4);
				break;
			default:	
				DBPRT(" Skipped header %02x", hi);
				break;
		}
	}
}

void obexclt_get(obex_t *handle, obex_object_t *object, int obex_rsp)
{
	obex_headerdata_t	hv;
	uint8_t			hi;
	int			hlen;
	char			*body = NULL;
	int			body_len = 0;
	int			len;
	obexclt_t	*clt = OBEX_GetUserData(handle);

	DBPRT("");

	clt->rsp = obex_rsp;
	if (obex_rsp != OBEX_RSP_SUCCESS)
		return;

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
		case OBEX_HDR_LENGTH:
			len = hv.bq4;
			DBPRT("got LENGTH");
			break;
		case OBEX_HDR_BODY:
			//DBPRT("%*s\n", hlen, hv.bs);
			body = (char*)hv.bs;
			body_len = hlen;
			DBPRT("got BODY: %d", body_len);
			break;
		case OBEX_HDR_BODY_END:
			DBPRT("got BODYEND");
			break;
		case OBEX_HDR_CONNECTION:
			DBPRT("got CONNECTION");
			break;
		case OBEX_HDR_NAME:
			DBPRT("got NAME");
			break;
		default:	
			DBPRT(" Skipped header %02x", hi);
			break;
		}
	}
}

void obexclt_put(obex_t *handle, obex_object_t *object, int obex_rsp)
{
	obex_headerdata_t	hv;
	uint8_t			hi;
	int			hlen;
	obexclt_t	*clt = OBEX_GetUserData(handle);

	int	len;

	DBPRT("");

	clt->rsp = obex_rsp;
	if (obex_rsp != OBEX_RSP_SUCCESS)
		return;

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
		case OBEX_HDR_LENGTH:
			len = hv.bq4;
			DBPRT("got LENGTH");
			break;
		case OBEX_HDR_CONNECTION:
			DBPRT("got CONNECTION");
			break;
		case OBEX_HDR_NAME:
			DBPRT("got NAME");
			break;

		default:
			DBPRT(" Skipped header %02x", hi);
		}
	}
}

void obexclt_setpath(obex_t *handle, obex_object_t *object, int obex_rsp)
{
	obex_headerdata_t	hv;
	uint8_t			hi;
	int			hlen;
	obexclt_t	*clt = OBEX_GetUserData(handle);

	DBPRT("");

	clt->rsp = obex_rsp;
	if (obex_rsp != OBEX_RSP_SUCCESS)
		return;

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
		case OBEX_HDR_CONNECTION:
			DBPRT("got CONNECTION");
			break;
		case OBEX_HDR_NAME:
			DBPRT("got NAME");
			break;
		default:
			DBPRT(" Skipped header %02x", hi);
			break;
		}
	}
}


void request_done(obex_t *handle, obex_object_t *object, int obex_cmd, int obex_rsp)
{
	obexclt_t *clt = OBEX_GetUserData(handle);

	DBPRT("Command (%02x) has now finished, rsp: %02x", obex_cmd, obex_rsp);

	switch (obex_cmd) {
	case OBEX_CMD_DISCONNECT:
		DBPRT("Disconnect done!");
		OBEX_TransportDisconnect(handle);
		clt->clientdone = TRUE;
		break;
	case OBEX_CMD_CONNECT:
		DBPRT("Connected!\n");
		obexclt_connect(handle, object, obex_rsp);
		clt->clientdone = TRUE;
		break;
	case OBEX_CMD_GET:
		obexclt_get(handle, object, obex_rsp);
		clt->clientdone = TRUE;
		break;
	case OBEX_CMD_PUT:
		obexclt_put(handle, object, obex_rsp);
		clt->clientdone = TRUE;
		break;
	case OBEX_CMD_SETPATH:
		obexclt_setpath(handle, object, obex_rsp);
		clt->clientdone = TRUE;
		break;

	default:
		DBPRT("Command (%02x) has now finished", obex_cmd);
		break;
	}
}


int obex_request(obexclt_t *clt, obex_object_t *object)
{
	int	err = 0;

	err = OBEX_Request(clt->handle, object);
	if (err)
		return err;

	clt->clientdone = FALSE;
	while (!clt->clientdone) {
		if ((err = OBEX_HandleInput(clt->handle, 1)) < 0) {
			BTERROR("Error while doing OBEX_HandleInput()");
			break;
		}
		err = (clt->rsp == OBEX_RSP_SUCCESS)?0:clt->rsp;
	}
	return err;
}

//
// Add more data to stream.
//
static int writestream(obex_t *handle, obex_object_t *object)
{
	int 			actual;
	obexclt_t		*clt = OBEX_GetUserData(handle);
	obex_headerdata_t	hv;
		
	actual = read(clt->sfd, clt->buf, OBEX_STREAM_CHUNK);
	//DBPRT("send %d bytes\n", actual);
	if(actual > 0) {
		/* Read was ok! */
		hv.bs = clt->buf;
		OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY,
				hv, actual, OBEX_FL_STREAM_DATA);
	} else if(actual == 0) {
		/* EOF */
		hv.bs = clt->buf;
		OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY,
				hv, 0, OBEX_FL_STREAM_DATAEND);
	} else {
		/* Error */
		hv.bs = NULL;
		OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY,
				hv, 0, OBEX_FL_STREAM_DATA);
	}
	return actual;
}

//
// Read data from stream.
//
static int readstream(obex_t *handle, obex_object_t *object)
{
	int 		actual;
	obexclt_t	*clt = OBEX_GetUserData(handle);
	const uint8_t	*buf;
	int		len;
		
	actual = OBEX_ObjectReadStream(handle, object, &buf);
	//DBPRT("Got %d bytes\n", actual);
	if (actual > 0) {
		len = write(clt->sfd, buf, actual);
	}
	return actual;
}


//
// Called by the obex-layer when some event occurs.
//
void obex_event(obex_t *handle, obex_object_t *object, int mode, int event, int obex_cmd, int obex_rsp)
{
	switch (event)	{
		case OBEX_EV_PROGRESS:
			//DBPRT("Made some progress...");
			break;
		case OBEX_EV_ABORT:
			BTERROR("Request aborted!");
			break;
		case OBEX_EV_REQDONE:
			request_done(handle, object, obex_cmd, obex_rsp);
			break;
		case OBEX_EV_REQHINT:
			/* Accept any command. Not rellay good, but this is a test-program :) */
			//OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
			break;
		case OBEX_EV_REQ:
			DBPRT("Command request (%02x)", obex_cmd);
			//server_request(handle, object, event, obex_cmd);
			break;
		case OBEX_EV_LINKERR:
			OBEX_TransportDisconnect(handle);
			BTERROR("Link broken!");
			break;
		case OBEX_EV_STREAMEMPTY:
			writestream(handle, object);
			break;
		case OBEX_EV_STREAMAVAIL:
			readstream(handle, object);
			break;
		case OBEX_EV_PARSEERR:
			//OBEX_TransportDisconnect(handle);
			BTERROR("Parse Error!");
			break;
		default:
			BTERROR("Unknown event %02x!", event);
			break;
	}
}


/* -----------------  OBEX client --------------------------- */

obexclt_t *obex_connect(struct sockaddr_affix *sa, obex_target_t *target, int *err)
{
	obex_object_t		*oo;	// OBEX Object
	obex_headerdata_t	hv;
	obexclt_t		*clt;

	clt = malloc(sizeof(obexclt_t));
	if (clt == NULL)
		return NULL;
	memset(clt, 0, sizeof(obexclt_t));

	/* set members */
	clt->family = sa->family;
	clt->conid = -1;

	if (sa->family == PF_AFFIX) {
		clt->handle = OBEX_Init(OBEX_TRANS_FD, obex_event, 0);
		if (!clt->handle) {
			BTERROR( "OBEX_Init failed:%s", strerror(errno));
			free(clt);
			return NULL;
		}
		// connect to the server
		DBPRT("Connecting to host %s ...\n", bda2str(&sa->bda));
		clt->fd = socket(PF_AFFIX, SOCK_STREAM, BTPROTO_RFCOMM);
		if (clt->fd < 0) {
			BTERROR("socket(PF_AFFIX, ..) failed\n");
			*err = clt->fd;	
			free(clt);
			return NULL;
		}
		*err = connect(clt->fd, (struct sockaddr*)sa, sizeof(*sa));
		if (*err < 0) {
			DBPRT("Unable to connect to remote side\n");
			close(clt->fd);
			free(clt);
			return NULL;
		}
		FdOBEX_TransportSetup(clt->handle, clt->fd, clt->fd, 0);
	} else if (sa->family == PF_INET)  { // tcpmode
		struct sockaddr_in	*in_addr = (struct sockaddr_in*)sa;

		DBPRT("Using TCP transport: host = %s", inet_ntoa(in_addr->sin_addr));
		if (!(clt->handle = OBEX_Init(OBEX_TRANS_INET, obex_event, 0))) {
			BTERROR( "OBEX_Init failed:%s", strerror(errno));
			*err = -1;
			free(clt);
			return NULL;
		}
		// connect to the server
		*err = OBEX_TransportConnect(clt->handle, (struct sockaddr*)in_addr, sizeof(*in_addr));
		if (*err < 0) {
			DBPRT("Unable to connect to the server\n");
			free(clt);
			return NULL;
		}
	} else {
		*err = -1;
		free(clt);
		return NULL;
	}
	//printf("target: %p\n", target);
	OBEX_SetUserData(clt->handle, clt);

	// create new object
	oo = OBEX_ObjectNew(clt->handle, OBEX_CMD_CONNECT);
	if (target) {
		hv.bs = target->data;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_TARGET, hv, target->len, 0);
	}
	*err = obex_request(clt, oo);
	DBPRT("Connection return code: %d, id: %d\n", *err, clt->conid);
	if (*err)
		goto exit;
	if (target && clt->conid == -1) {
		*err = OBEX_RSP_CONFLICT;
		goto exit;
	}
	DBPRT("Connection established\n");
	return clt;
exit:
	DBPRT("Unable to connect\n");
	obex_disconnect(clt);
	return NULL;	
}


int obex_disconnect(obexclt_t *clt)
{
	int			err;
	obex_object_t		*oo;	// OBEX Object

	// create new object
	oo = OBEX_ObjectNew(clt->handle, OBEX_CMD_DISCONNECT);
	err = obex_request(clt, oo);

	if (clt->family != PF_INET) {
		OBEX_Cleanup(clt->handle);
		close(clt->fd);
	} else
		OBEX_TransportDisconnect(clt->handle);

	free(clt);
	return 0;
}

static int getuname(char **uname, char *name)
{
	int	uname_len;

	if (!name) {
		*uname = NULL;
		return 0;
	}
	uname_len = (strlen(name) + 1) << 1;
	*uname = malloc(uname_len);
	if (!*uname)
		return -1;
	OBEX_CharToUnicode(*uname, name, uname_len);
	return uname_len;
}


int obex_put(obexclt_t *clt, char *local, char *remote, char *type)
{
	int			err;
	obex_object_t		*oo;	// OBEX Object
	obex_headerdata_t	hv;
	char 			*uname;
	int			uname_len;

	if (!type && !remote)
		return -1;
	
	uname_len = getuname(&uname, remote);
	if (uname_len < 0)
		return -1;
	
	if (local) {
		clt->sfd = open(local, O_RDONLY);
		if (clt->sfd < 0) {
			if (uname)
				free(uname);
			return clt->sfd;
		}
		clt->buf = malloc(OBEX_STREAM_CHUNK);
		if (!clt->buf) {
			if (uname)
				free(uname);
			close(clt->sfd); clt->sfd = -1;
			return -1;	
		}
	} else {
		/* in a case of delete */
		clt->sfd = -1;
		clt->buf = NULL;
	}

	clt->opcode = OBEX_PUT;
	
	DBPRT("Sending file: %s, path: %s, size: %d\n", local, remote, get_fdsize(clt->sfd));

	// create new object
	oo = OBEX_ObjectNew(clt->handle, OBEX_CMD_PUT);
	if (clt->conid != -1) { 
		hv.bq4 = clt->conid;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_CONNECTION, hv, 4, 0);
	}
	if (uname) {
		hv.bs = uname;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_NAME, hv, uname_len, 0);
		free(uname);
	}
	if (type) {
		hv.bs = type;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_TYPE, hv, strlen(type) + 1, 0);
	}
	if (clt->sfd >= 0) {
		hv.bq4 = get_fdsize(clt->sfd);
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_LENGTH, hv, sizeof(uint32_t), 0);
		hv.bs = NULL;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_BODY, hv, 0, OBEX_FL_STREAM_START);
	}

	err = obex_request(clt, oo);

	if (clt->buf) {
		free(clt->buf);
		clt->buf = NULL;
	}
	if (clt->sfd >= 0) {
		close(clt->sfd);
		clt->sfd = -1;
	}
	return err;
}

int obex_get(obexclt_t *clt, char *local, char *remote, char *type)
{
	int			err;
	obex_object_t		*oo;	// OBEX Object
	obex_headerdata_t	hv;
	char 			*uname;
	int			uname_len;

	if (!local || (!type && !remote))
		return -1;

	uname_len = getuname(&uname, remote);
	if (uname_len < 0)
		return -1;

	clt->sfd = open(local, O_CREAT | O_WRONLY, 0600);
	if (clt->sfd < 0) {
		if (uname)
			free(uname);
		return clt->sfd;
	}
	clt->opcode = OBEX_GET;

	// create new object
	oo = OBEX_ObjectNew(clt->handle, OBEX_CMD_GET);
	if (clt->conid != -1) { 
		hv.bq4 = clt->conid;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_CONNECTION, hv, 4, 0);
	}
	if (uname) {
		hv.bs = uname;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_NAME, hv, uname_len, 0);
		free(uname);
	}
	if (type) {
		hv.bs = type;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_TYPE, hv, strlen(type)+1, 0);
	}

	OBEX_ObjectReadStream(clt->handle, oo, NULL);

	err = obex_request(clt, oo);
	
	close(clt->sfd); clt->sfd = -1;
	return err;
}

		
int __obex_setpath(obexclt_t *clt, char *path, int flags)
{
	int			err;
	obex_object_t		*oo;	// OBEX Object
	obex_headerdata_t	hv;
	char 			*uname = NULL;
	int			uname_len = 0, is_name;
	obex_setpath_hdr_t	nonhdrdata;

	clt->opcode = OBEX_SETPATH;
	nonhdrdata.flags = 0x02;
	nonhdrdata.constants = 0;

	if (path == NULL || *path == '\0') {
		/* empty name. set to root */
		uname = "";
		uname_len = 0;
		is_name = 1;
	} else if (strcmp(path, "..") != 0) {
		uname_len = (strlen(path)+1)<<1;
		uname = malloc(uname_len);
		if (!uname){
			return -1;
		}
		OBEX_CharToUnicode(uname, path, uname_len);
		is_name = 2;
	} else {
		// ".."
		nonhdrdata.flags = 0x03;
		is_name = 0;
	}

	if (flags & SETPATH_CREATE) {
		if (is_name < 2)
			return -EINVAL;
		nonhdrdata.flags &= ~0x02;
	}

	// create new object
	oo = OBEX_ObjectNew(clt->handle, OBEX_CMD_SETPATH);
	OBEX_ObjectSetNonHdrData(oo, (uint8_t*)&nonhdrdata, 2);
	hv.bq4 = clt->conid;
	OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_CONNECTION, hv, 4, 0);

	if (is_name) {
		DBPRT("Setting name: %d\n", uname_len);
		hv.bs = uname;
		OBEX_ObjectAddHeader(clt->handle, oo, OBEX_HDR_NAME, hv, uname_len, 0);
	}
	
	err = obex_request(clt, oo);

	if (is_name == 2)
		free(uname);

	return err;
}

/* -------------------    file transfer API  ---------------------  */

int obex_get_file(obexclt_t *clt, char *local, char *remote)
{
	int	err;
	char	*pathc = NULL;

	if (!remote)
		return -1;

	if (!local) {
		pathc = strdup(remote);
		if (!pathc)
			return -1;
		local = basename(pathc);
	}
	err = obex_get(clt, local, remote, NULL);
	if (pathc)
		free(pathc);
	return err;
}

int obex_put_file(obexclt_t *clt, char *local, char *remote)
{
	int	err;
	char	*pathc = NULL;

	if (!local)
		return -1;

	if (!remote) {
		pathc = strdup(local);
		if (!pathc)
			return -1;
		remote = basename(pathc);
	}
	err = obex_put(clt, local, remote, NULL);
	if (pathc)
		free(pathc);
	return err;
}

int obex_remove(obexclt_t *clt, char *name)
{
	int			err;

	err = obex_put(clt, NULL, name, NULL);

	return err;
}


