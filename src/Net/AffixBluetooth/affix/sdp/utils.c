/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001,2002 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   Original Author: Guruprasad Krishnamurthy <kgprasad@hotmail.com>

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
   $Id: utils.c,v 1.60 2004/03/01 15:46:28 kassatki Exp $

   SDP client utilities to perform GIAC, connect to an SDP server,
   and to initialize the SDP machinery

   Fixes:
	    Dmitry Kasatkin		: SDPServerConnection changed to complex type
*/

#include <affix/config.h>

#include <stdio.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>
#include "utils.h"
#include "cstate.h"
#include "attr.h"


static int	sdp_up = 0;
static int	sdp_flags = 0;
sdp_mode_t	sdp_mode;


char *sdp_errlist(int err)
{
	switch (err) {
		case SDP_ERR_SDP_VERSION:
			return "Invalid/Unsupported SDP version";
		case SDP_ERR_SERVICE_RECORD_HANDLE:
			return "Invalid Service Record Handle";
		case SDP_ERR_REQUEST_SYNTAX:
			return "Invalid request syntax";
		case SDP_ERR_PDU_SIZE:
			return "Invalid PDU Size";
		case SDP_ERR_CONT_STATE:
			return "Invalid Continuation State";
		case SDP_ERR_RESOURCES:
			return "Insufficient Resources to satisfy Request";
		case SDP_ERR_INVALID_ARG:
			return "Invalid Arguments";
		case SDP_ERR_NOT_EXIST:
			return "Attribute/Service does not exist";
		case SDP_ERR_SYNTAX:
			return "Invalid Data Synax";
		case SDP_ERR_INTERNAL:
			return "Internal Error";
		case SDP_ERR_SERVER:
			return "Unspecified Server Error";
		default:
			return "Unknown error";
	}
}

void _sdp_error(char *buf, int err)
{
	if (err < 0)
		sprintf(buf, "System error: %s (%d)", strerror(errno), errno);
	else if (err > 0)
		sprintf(buf, "SDP error: %s (%d)", sdp_errlist(err), err);
	else
		sprintf(buf, "No error (0)\n");
}

char *sdp_error(int err)
{
	static unsigned char 	buf[80][2];
	static int 		num = 0; 

	num = 1 - num; /* switch buf */
	_sdp_error(buf[num], err);
	return buf[num];
}

int sdp_init(int flags)
{
	int	err = 0;
	
	if (!sdp_up) {
		if (flags & SDP_SVC_SERVER)
			sdp_mode = SDP_SERVER;
		else
			sdp_mode = SDP_CLIENT;
		sdp_flags = flags;
		sdp_up = 1;
	}
	return err;
}

void sdp_cleanup(void)
{
	if (sdp_up) {
		sdp_up = 0;
		sdp_flags = 0;
	}
}
	
int sdp_connect(struct sockaddr_affix *saddr)
{
	int		status = 0;
	int		clientSockFd;
	uint16_t	port;

	if (saddr->family == PF_INET) {
		/*
		 ** Code for a local SDP server connection
		 */
		struct sockaddr_in	*inAddr = (struct sockaddr_in*)saddr;
		DBPRT("Attempt to open a TCP socket, local client\n");
		clientSockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (clientSockFd < 0)
			return clientSockFd;
		port = inAddr->sin_port;
		inAddr->sin_port = SDP_TCP_PORT;
		DBPRT("TCP fd : %d\n", clientSockFd);
		DBPRT("Local addr : %x\n", inAddr->sin_addr.s_addr);
		status = connect(clientSockFd, (struct sockaddr *)inAddr, sizeof(*inAddr));
		if (status < 0) {
			close(clientSockFd);
			BTERROR("TCP connect error:%s", strerror(errno));
		} else {
			DBPRT("Connected\n");
		}
		inAddr->sin_port = port;
	} else if (saddr->family == PF_AFFIX) {                                             
		/*
		 ** Create a BT socket for ourselves
		 */
		DBPRT("Attempt to open BT connection\n");
		DBPRT("BT addr str : %s\n", bda2str(&saddr->bda));
		clientSockFd = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (clientSockFd < 0)
			return clientSockFd;
		port = saddr->port;
		saddr->port = SDP_PSM;
		DBPRT("Connecting\n");
		status = connect(clientSockFd, (struct sockaddr*)saddr, sizeof(*saddr));          
		if (status < 0) {
			close(clientSockFd);
			DBPRT("connect error:%s", strerror(errno));
		} else {
			DBPRT("Connect success, mtu: %d\n", l2cap_getmtu(clientSockFd));
		}
		//printf("Connect success, mtu: %d\n", l2cap_getmtu(clientSockFd));
		saddr->port = port;
	} else
		return SDP_ERR_INVALID_ARG;

	return status ? status : clientSockFd;
}                                 


int sdp_connect_local(void)
{
	struct sockaddr_in	sa;
	int			err;

	if (sdp_flags & SDP_SVC_PROVIDER) {
		/* start server */
		err = affix_system("btsdpd -d", 0);
		if (err)
			return err;
		err = affix_wait_for_service_up(SDP_TCP_PORT, 2);
		if (err < 0)
			return err;
	}
	/* now try to connect */
	sa.sin_family = PF_INET;
	inet_aton("127.0.0.1", &sa.sin_addr);
	return sdp_connect((struct sockaddr_affix*)&sa);
}


/*
 ** Close an existing connection to the SDP server on a said
 ** socket descriptor. 
 */
void sdp_close(int fd)
{
	if (fd >= 0)
		close(fd);
}                         

/*
 ** A simple function which returns the time of day in
 ** seconds. Used for updating the service db state
 ** attribute of the service record of the SDP server
 */
long sdp_get_time(void)
{
	static struct timeval tm;
	/*
	 ** To handle failure in gettimeofday, so an old
	 ** value is returned and service does not fail
	 */
	gettimeofday(&tm, NULL);
	return tm.tv_sec;
}


/*
 ** Generate unique transaction identifiers
 */
uint16_t sdp_gen_trans(void)
{
	uint16_t j;

	j=1+(int) (100.0*rand()/(RAND_MAX+1.0));

	return j;
}


/*
 ** A generic send request, wait for response method. Uses
 ** select system call to block for response or timeout
 */
int sdp_send_req_w4_rsp(
		int srvHandle,
		char *requestBuffer,
		char *responseBuffer,
		int requestSize,
		int *responseSize
		)
{
	int status = 0;
	int bytesSent = 0;
	int bytesRecvd = 0;
	sdp_hdr_t *pduRequestHeader;
	sdp_hdr_t *pduResponseHeader;
	fd_set readFds;
	struct timeval timeout;

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;

	/*
	 ** Send the request, wait for response and if
	 ** not error, set the appropriate values
	 ** and return
	 */
	status = send(srvHandle, requestBuffer, requestSize, 0);
	if (status < 0) {
		BTERROR("Error sending data:%s", strerror(errno));
		return status;
	}
	bytesSent = status;
	if (bytesSent != requestSize) {
		DBPRT("Attempt to send : %d sent : %d\n", requestSize, bytesSent);
		return SDP_ERR_INTERNAL;
	}
	DBPRT("Bytes sent to handle : %d is : %d\n", srvHandle, bytesSent);        
	BTDUMP(requestBuffer, bytesSent);

	DBPRT("Waiting for response\n");
	timeout.tv_sec = SDP_TIMEOUT; 
	timeout.tv_usec = 0;

	FD_ZERO(&readFds);
	FD_SET(srvHandle, &readFds);
	status = select(srvHandle+1, &readFds, NULL, NULL, &timeout);
	if (status < 0)
		return status;
	if (status == 0) {
		BTERROR("Client timed out\n");
		errno = ETIME;
		return -1;
	}

	DBPRT("Trying to read\n");
	status = recv(srvHandle, responseBuffer, SDP_RSP_BUF_SIZE, 0);
	if (status < 0)
		return status;
	if (status == 0) {
		errno = ECONNRESET;
		return -1;
	}

	bytesRecvd = status;
	DBPRT("Read : %d\n", bytesRecvd);
	BTDUMP(responseBuffer, bytesRecvd);
	/*
	 ** Process the response
	 */
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;
	if (pduRequestHeader->transactionId != pduResponseHeader->transactionId)
		return SDP_ERR_INTERNAL;
	*responseSize = bytesRecvd;
	return 0;
}

union sockaddr_sdp {
	struct sockaddr_affix	bt;
	struct sockaddr_in	in;
};

int sdp_getmtu(int fd)
{
	union sockaddr_sdp	caddr;
	socklen_t		len = sizeof(caddr);
	int			mtu;

	getsockname(fd, (struct sockaddr*)&caddr, &len);
	if (caddr.bt.family == PF_AFFIX)
		mtu = l2cap_getmtu(fd);
	else {
		len = sizeof(int);
		getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &mtu, &len);
		DBPRT("TCP MTU: %d\n", mtu);
	}
	return mtu;
}

/* ------------------------------------------------------- */

/*
 ** This version is for use by SDP server when creating new
 ** service records
 */
sdpsvc_t *sdp_create_svc(void)
{
	sdpsvc_t *svcRec;

	svcRec = (sdpsvc_t*)malloc(sizeof(sdpsvc_t));
	if (!svcRec)
		return NULL;
	memset(svcRec, 0, sizeof(sdpsvc_t));
	svcRec->serviceRecordHandle = 0xffffffff;
	svcRec->fd = -1;
	return svcRec;
}

/*
 ** Free the contents of a service record
 */
void sdp_free_svc(sdpsvc_t *svcRec)
{
	if (!svcRec)
		return;
	if (svcRec->pdu.data)
		free(svcRec->pdu.data);
	s_list_destroy(&svcRec->targetPattern);
	sdp_free_seq(&svcRec->attributeList);
	free(svcRec);
}

void sdp_free_svclist(slist_t **svcList)
{
	slist_t		*list;
	sdpsvc_t	*svcRec;
	
	for (list = *svcList; list; list = s_list_next(list)) {
		svcRec = s_list_data(list);
		if (!svcRec)
			continue;
		sdp_free_svc(svcRec);
	}
	s_list_free(svcList);
}


