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
   $Id: obex_server.c,v 1.33 2003/02/13 14:19:36 kds Exp $

   OBEX server application for Affix

*/

#include <affix/config.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <stdint.h>

#include <openobex/obex.h>

#include <affix/btcore.h>
#include <affix/obex.h>

//
// Read data from stream.
//
static int readstream(obex_t *handle, obex_object_t *object)
{
	int 		actual;
	obexsrv_t	*srv = OBEX_GetUserData(handle);
	const uint8_t	*buf;
	int		len;

	if (srv->sfd < 0) {
		/* create temporary storage for an object */
		srv->name = strdup("/tmp/obex_tmp_XXXXXX");
		if (!srv->name)
			return -1;
		srv->sfd = mkstemp(srv->name);
		if (srv->sfd < 0) {
			DBPRT("unable to create tmp file: %s\n", srv->name);
			free(srv->name);
			srv->name = NULL;
			return srv->sfd;
		}
		DBPRT("created tmp file: %s\n", srv->name);
		srv->flags = 0x01;
	}
	srv->streamming = TRUE;
	actual = OBEX_ObjectReadStream(handle, object, &buf);
	DBPRT("got stream: %d\n", actual);
	if (actual > 0) {
		len = write(srv->sfd, buf, actual);
	}
	return actual;
}


void obexsrv_put(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t hv;
	uint8_t		hi;
	int		hlen;
	const uint8_t	*body = NULL;
	int 		body_len = 0;
	char		*name = NULL;
	int		endofbody = 0;
	int		len, con_id = -1;
	char		*obj_type = NULL;
	obexsrv_t	*srv = OBEX_GetUserData(handle);
	int		flags = 0, err;
	

	DBPRT("");

	/* body received. close it */
	if (srv->sfd >= 0) {
		close(srv->sfd);
		srv->sfd = -1;
	}

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
		case OBEX_HDR_CONNECTION:
			con_id = hv.bq4;
			DBPRT("Found connection id: %08x", con_id);
			break;
		case OBEX_HDR_LENGTH:
			len = hv.bq4;
			DBPRT("Found Length: %d, hlen: %d", len, hlen);
			break;
		case OBEX_HDR_NAME:
			if ((name = malloc(hlen/2))) {
				OBEX_UnicodeToChar(name, hv.bs, hlen);
			}
			DBPRT("Found name: %s", name);
			break;
		case OBEX_HDR_TYPE:
			obj_type = (char *)hv.bs;
			DBPRT("Found type:%*s, hlen: %d", hlen, obj_type, hlen);
			break;
		case OBEX_HDR_BODY:
			DBPRT("Found body");
			body = hv.bs;
			body_len = hlen;
			break;
		case OBEX_HDR_BODY_END:
			DBPRT("Found end of body");
			endofbody = 1;
			break;
		default:
			DBPRT("Skipped header %02x", hi);
			break;
		}
	}
	if (!body) {
		if (endofbody) {
			DBPRT("Got a PUT with only end of body header->create empty object.");
			flags |= 0x01;	// empty object
		} else {
			DBPRT("Got a PUT without a body?????");
			if (!srv->streamming) {
				DBPRT("Got a PUT without a body->delete");
				flags |= 0x02;	// delete
			}
		}
	}
	if (!name) {
		if ((flags & 0x03)) {// empty or delete
			BTERROR("name is missing.");
			OBEX_ObjectSetRsp(object, OBEX_RSP_BAD_REQUEST, OBEX_RSP_BAD_REQUEST);
			goto error;
		}
	}
	
	// call handler
	err = srv->put(srv, srv->name, name, obj_type, flags);
	if (err < 0) {
		if (errno == ENOENT)
			OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_FOUND, OBEX_RSP_NOT_FOUND);
		else 
			OBEX_ObjectSetRsp(object, OBEX_RSP_INTERNAL_SERVER_ERROR, OBEX_RSP_INTERNAL_SERVER_ERROR);
		if (srv->name)
			unlink(srv->name);
		goto error;
	}
	OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
error:
	if (srv->name) {
		free(srv->name);
		srv->name = NULL;
	}
	if (name)
		free(name);
}


//
// Add more data to stream.
//
static int writestream(obex_t *handle, obex_object_t *object)
{
	int 			actual;
	obexsrv_t		*srv = OBEX_GetUserData(handle);
	obex_headerdata_t	hv;
		
	actual = read(srv->sfd, srv->buf, OBEX_STREAM_CHUNK);
	DBPRT("sent %d bytes\n", actual);
	if(actual > 0) {
		/* Read was ok! */
		hv.bs = srv->buf;
		OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY,
				hv, actual, OBEX_FL_STREAM_DATA);
	} else if(actual == 0) {
		/* EOF */
		hv.bs = srv->buf;
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

void obexsrv_get(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t	hv;
	uint8_t 		hi;
	int 			hlen;
	char 			*obj_type = NULL;
	char			*name = NULL;
	int			con_id = -1;
	obexsrv_t		*srv = OBEX_GetUserData(handle);
	int			err;

	DBPRT("");

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch(hi) {
			case OBEX_HDR_NAME:
				if ((name = malloc(hlen/2))) {
					OBEX_UnicodeToChar(name, hv.bs, hlen);
				}
				DBPRT(" Found name:  %s", name);
				break;
			case OBEX_HDR_TYPE:
				obj_type = (char *)hv.bs;
				DBPRT(" Found type:%-30s", obj_type);
				break;
			case OBEX_HDR_CONNECTION:
				con_id = hv.bq4;
				DBPRT(" Found connection id: %08x", con_id);
				break;
			default:
				DBPRT(" Skipped header %02x", hi);
				break;
		}
	}

	/* call handler */
	err = srv->get(srv, name, obj_type);
	if (err < 0 || !srv->name) {
		DBPRT("File not found: %s\n", name);
		OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_FOUND, OBEX_RSP_NOT_FOUND);
		return;
	}
	srv->sfd = open(srv->name, O_RDONLY);
	if (srv->sfd < 0) {
		OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_FOUND, OBEX_RSP_NOT_FOUND);
		return;	
	}
	srv->buf = malloc(OBEX_STREAM_CHUNK);
	if (!srv->buf) {
		OBEX_ObjectSetRsp(object, OBEX_RSP_INTERNAL_SERVER_ERROR, OBEX_RSP_INTERNAL_SERVER_ERROR);
		return;	
	}
	OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
	hv.bq4 = get_fdsize(srv->sfd);
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_LENGTH, hv, sizeof(uint32_t), 0);
	hv.bs = NULL;
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY, hv, 0, OBEX_FL_STREAM_START);

	if (name)
		free(name);
	return;
}


void obexsrv_setpath(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t	hv;
	uint8_t 		hi;
	int 			hlen;
	char			*name = NULL;
	int			con_id = -1;
	uint8_t			*nonhdrdata;
	int			flags;
	int			len;
	obexsrv_t		*srv = OBEX_GetUserData(handle);
	int			err;

	DBPRT("");
	
	if ((len = OBEX_ObjectGetNonHdrData(object, &nonhdrdata)) == 2) {
		obex_setpath_hdr_t	*hdr = (obex_setpath_hdr_t*)nonhdrdata;
		flags = hdr->flags;
		DBPRT("Flags= 0x%02x  Constants=%d", flags, hdr->constants);
		
	} else {
		BTERROR("Invalid packet content. len=%d", len);
		flags = 0;
	}

	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch(hi) {
		case OBEX_HDR_NAME:
			if (hlen) {
				if ((name = malloc(hlen/2)))
					OBEX_UnicodeToChar(name, hv.bs, hlen);
			} else
				flags |= 0x80;	// name exits but zero -> set root
			DBPRT("Found name:  %s", name);
			break;

		case OBEX_HDR_CONNECTION:
			con_id = hv.bq4;
			DBPRT("Found connection id: %08x", con_id);
			break;
		
		default:
			DBPRT("Skipped header %02x", hi);
			break;
		}
	}
	// call handler
	err = srv->setpath(srv, name, flags);
	if (err)
		goto error;
		
	OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
	if (name)
		free(name);
	return;
error:
	OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_FOUND, OBEX_RSP_NOT_FOUND);
	if (name)
		free(name);
	return;
}


void obexsrv_connect(obex_t *handle, obex_object_t *object)
{
	obex_headerdata_t 	hv;
	uint8_t			hi;
	int			hlen;
	uint8_t			*nonhdrdata;
	obex_target_t		target = {0, NULL};
	obexsrv_t		*srv = OBEX_GetUserData(handle);
	int			err;

	DBPRT("");

	if(OBEX_ObjectGetNonHdrData(object, &nonhdrdata) == 4) {
#ifdef CONFIG_AFFIX_DEBUG
		obex_connect_hdr_t	*hdr = (obex_connect_hdr_t*)nonhdrdata;
		DBPRT("Version: 0x%02x. Flags: 0x%02x  OBEX packet length:%d",
			hdr->version, hdr->flags, ntohs(hdr->mtu));
#endif
	} else {
		BTERROR("Invalid packet content.");
	}
	while(OBEX_ObjectGetNextHeader(handle, object, &hi, &hv, &hlen)) {
		switch (hi) {
			case OBEX_HDR_TARGET:
				target.data = (void*)hv.bs;
				target.len = hlen;
				if (hlen == 16)
					DBPRT("got TARGET. uuid_t: %08X-%04X-%04X-%04X-%08X%04X",
							*(uint32_t *)&target.data[0], *(uint16_t *)&target.data[4],
							*(uint16_t *)&target.data[6], *(uint16_t *)&target.data[8],
							*(uint32_t *)&target.data[10], *(uint16_t *)&target.data[14]);
				else
					DBPRT("got TARGET. unknown fmt");
				break;
			default:	
				DBPRT(" Skipped header %02x", hi);
				break;
		}
	}

	// call handler
	err = srv->connect(srv, &target);
	if (err < 0) {
		/* error */
		OBEX_ObjectSetRsp(object, OBEX_RSP_INTERNAL_SERVER_ERROR, OBEX_RSP_INTERNAL_SERVER_ERROR);
	} else {
		OBEX_ObjectSetRsp(object, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
		if (target.data) {
			hv.bq4 = err;	/* set connection id */
			OBEX_ObjectAddHeader(handle, object, OBEX_HDR_CONNECTION, hv, 4, 0);
			hv.bs = target.data;
			OBEX_ObjectAddHeader(handle, object, OBEX_HDR_WHO, hv, target.len, 0);
		}
	}
}


void obexsrv_req(obex_t *handle, obex_object_t *object, int cmd)
{
	switch(cmd) {
	case OBEX_CMD_CONNECT:
		obexsrv_connect(handle, object);
		break;
	case OBEX_CMD_DISCONNECT:
		DBPRT("We got a disconnect-request");
		OBEX_ObjectSetRsp(object, OBEX_RSP_SUCCESS, OBEX_RSP_SUCCESS);
		break;
	case OBEX_CMD_GET:
		obexsrv_get(handle, object);
		break;
	case OBEX_CMD_PUT:
		obexsrv_put(handle, object);
		break;
	case OBEX_CMD_SETPATH:
		obexsrv_setpath(handle, object);
		break;
	default:
		BTERROR(" Denied %02x request", cmd);
		OBEX_ObjectSetRsp(object, OBEX_RSP_NOT_IMPLEMENTED, OBEX_RSP_NOT_IMPLEMENTED);
		break;
	}
	return;
}

void obexsrv_reqhint(obex_t *handle, obex_object_t *object, int cmd)
{
	DBPRT("reqhint: %#x\n", cmd);
	switch(cmd) {
		case OBEX_CMD_PUT: 
			OBEX_ObjectReadStream(handle, object, NULL);
			break;
		default:
			break;
	}
	/* accept all */
	OBEX_ObjectSetRsp(object, OBEX_RSP_CONTINUE, OBEX_RSP_SUCCESS);
}

void obexsrv_reqdone(obex_t *handle, obex_object_t *object, int obex_cmd, int obex_rsp)
{
	obexsrv_t	*srv = OBEX_GetUserData(handle);

	DBPRT("Server request finished!");

	switch (obex_cmd) {
		case OBEX_CMD_DISCONNECT:
			DBPRT("Disconnect done!");
			srv->state = SRVSTATE_CLOSED;
			srv->disconnect(srv);
			break;
		default:
			DBPRT(" Command (%02x) has now finished", obex_cmd);
			srv->serverdone = TRUE;
			break;
	}
	/* cleanup resources */
	if (srv->sfd >= 0) {
		close(srv->sfd);
		srv->sfd = -1;
	}
	if (srv->name) {
		if (srv->flags & 0x01)
			unlink(srv->name);
		free(srv->name);
		srv->name = NULL;
	}
	if (srv->buf) {
		free(srv->buf);
		srv->buf = NULL;
	}
	srv->streamming = FALSE;	/* disable streaming */
}

void obexsrv_set_file(obexsrv_t *srv, char *name, int del)
{
	srv->name = strdup(name);
	if (del)
		srv->flags |= 0x01;	/* delete object after transfer */
	else
		srv->flags &= ~0x01;
}

//
// Called by the obex-layer when some event occurs.
//
void obexsrv_event(obex_t *handle, obex_object_t *object, int mode, int event, int obex_cmd, int obex_rsp)
{
	obexsrv_t	*srv = OBEX_GetUserData(handle);

	//DBPRT("event: %d, cmd: %x, rsp: %x\n", event, obex_cmd, obex_rsp);
	switch (event) {
		case OBEX_EV_PROGRESS:
			//DBPRT("Made some progress...");
			break;
		case OBEX_EV_ABORT:
			BTERROR("Request aborted!");
			break;
		case OBEX_EV_REQDONE:
			obexsrv_reqdone(handle, object, obex_cmd, obex_rsp);
			break;
		case OBEX_EV_REQHINT:
			obexsrv_reqhint(handle, object, obex_cmd);
			break;
		case OBEX_EV_REQ:
			obexsrv_req(handle, object, obex_cmd);
			break;
		case OBEX_EV_LINKERR:
			if (srv->state != SRVSTATE_CLOSED) {
				BTERROR("Link broken!");
				srv->state = SRVSTATE_CLOSED;
				srv->disconnect(srv);
			}
			srv->serverdone = TRUE;
			break;
		case OBEX_EV_PARSEERR:
			BTERROR("Parse error!");
			break;
		case OBEX_EV_STREAMEMPTY:
			writestream(handle, object);
			break;
		case OBEX_EV_STREAMAVAIL:
			readstream(handle, object);
			break;
		default:
			BTERROR("Unknown event %02x!", event);
			break;
	}
}

/*
 * obex_start_server()
 *
 * runs obex server fds transport
 */
int obexsrv_run(obexsrv_t *srv, int rfd, int wfd)
{
	int	err = 0, to;

	srv->handle = OBEX_Init(OBEX_TRANS_FD, obexsrv_event, 0);
	if (!srv->handle) {
		BTERROR( "OBEX_Init failed:%s", strerror(errno));
		return -1;
	}

	/* init some members */
	srv->sfd = -1;
	srv->name = NULL;
	srv->flags = 0;
	srv->buf = NULL;

	// set private pointer
	OBEX_SetUserData(srv->handle, srv);

	FdOBEX_TransportSetup(srv->handle, rfd, wfd, 0);
	
	for (;;) {
		/* request processing loop */
		DBPRT("Processing request...\n");
		srv->serverdone = FALSE;
		to = 1000;	/* unlimmited - waiting for request */
		while (!srv->serverdone) {
			if ((err = OBEX_HandleInput(srv->handle, to)) < 0) {
				BTERROR("Error while doing OBEX_HandleInput()");
				break;
			}
			to = 5;	/* processing request */
		}
		if (srv->state == SRVSTATE_CLOSED)
			break;
		if (err < 0)
			break;
	}
	OBEX_Cleanup(srv->handle);
	srv->handle = NULL;
	return 0;
}

/*
 * NAME
 * TYPE
 * LENGTH
 * TARGET
 * WHO
 * CONNECTION ID
 * AUTHENTICATION CHANLLENGE
 * AUTHENTICATION RESPONSE
 *
 */
