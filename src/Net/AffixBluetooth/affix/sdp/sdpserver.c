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
   $Id: sdpserver.c,v 1.63 2004/03/19 15:55:05 kassatki Exp $

   Contains the implementation of the SDP server itself including
   the main() of the server and it's initialization

   Fixes:
   		Imre Deak <ext-imre.deak@nokia.com>
		Dmitry Kasatkin
*/

#include <affix/config.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <affix/bluetooth.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>
#include "utils.h"
#include "servicedb.h"
#include "attr.h"
#include "cstate.h"


fd_set		activeFdSet;
int		serverBTSockFd;
int		serverTCPSockFd;
int		maxFd;

/*
 ** Server and client socket addresses
 */
struct sockaddr_affix	serverBTAddr;
struct sockaddr_in	serverINAddr;

extern void startWorkers(int);

uuid_t sdpSrvUUID;
slist_t *versionNumberSeq = NULL;

sdpsvc_t *sdpSvcRec;
sdpsvc_t *publicBrowseGroupRoot;

/*
 ** List of version numbers supported
 ** by the SDP server. Add to this list
 ** when newer versions are supported
 */
const int sdpServerVnumEntries = 1;
uint16_t sdpVnumArray[1] = { 0x0100 };

char *pSvcName = "SDP Server";
char *pProvName = "Nokia Research Center";
char *pSvcDesc = "Affix Service Discovery server";

char *pBrowseGroupName = "Public Browse Group Root";
char *pGroupDesc = "Root of public group hierarchy";

int _sdp_process_req(sdp_request_t *request);
int _sdp_disc_req(int fd);


/*
 ** The service DB state is an attribute of the service record
 ** of the SDP server itself. This attribute is guaranteed to
 ** change if any of the contents of the service repository
 ** changes. This function updates the timestamp of value of
 ** the svcDBState attribute
 ** Set the SDP server DB. Simply a timestamp which is the marker
 ** when the DB was modified.
 */
void sdp_update_time(void)
{
	uint32_t	dbts = sdp_get_time();
	
	sdp_append_attr(sdpSvcRec, SDP_ATTR_SERVICE_DB_STATE, sdp_put_u32(dbts));
	sdp_gen_svc_pdu(sdpSvcRec);
}


int registerPublicBrowseGroupRoot(void)
{
	int		status = 0;
	sdpdata_t	*attr;

	publicBrowseGroupRoot = sdp_create_svc();
	publicBrowseGroupRoot->serviceRecordHandle = (uint32_t)publicBrowseGroupRoot;
	sdp_add_svc(publicBrowseGroupRoot);
	sdp_append_attr(publicBrowseGroupRoot, SDP_ATTR_SERVICE_RECORD_HANDLE, 
					sdp_put_u32(publicBrowseGroupRoot->serviceRecordHandle));

	sdp_set_info_attr(publicBrowseGroupRoot, pBrowseGroupName, pProvName, pGroupDesc);

	attr = sdp_set_class_attr(publicBrowseGroupRoot);
	sdp_add_class(attr, SDP_UUID_BROWSE_GROUP_DESC);
	sdp_add_uuid16_to_pattern(publicBrowseGroupRoot, SDP_UUID_BROWSE_GROUP_DESC);
	
	sdp_set_group_attr(publicBrowseGroupRoot, SDP_UUID_PUBLIC_BROWSE_GROUP);
	sdp_add_uuid16_to_pattern(publicBrowseGroupRoot, SDP_UUID_PUBLIC_BROWSE_GROUP);

	sdp_gen_svc_pdu(publicBrowseGroupRoot);
	return status;
}

/*
 ** The SDP server must present it's own service record to
 ** the service repository. This can be accessed by service
 ** discovery clients. This methods constructs a service record
 ** and stores it in the reposssitory
 */
int registerSDPServerService(void)
{
	int		status = 0, i;
	sdpdata_t	*data;
	sdpdata_t	*attr, *param;

	sdpSvcRec = sdp_create_svc();
	/*
	 ** Force the svcRecHandle to be SDP_ATTR_SERVICE_RECORD_HANDLE - 0
	 */
	sdpSvcRec->serviceRecordHandle = SDP_ATTR_SERVICE_RECORD_HANDLE;
	sdp_add_svc(sdpSvcRec);
	sdp_append_attr(sdpSvcRec, SDP_ATTR_SERVICE_RECORD_HANDLE, sdp_put_u32(sdpSvcRec->serviceRecordHandle));

	/*
	 ** Add all attributes to service record. No need to
	 ** commit since we are the server and this record is
	 ** already in the serviceDB
	 */
	status = sdp_set_info_attr(sdpSvcRec, pSvcName, pProvName, pSvcDesc);

	attr = sdp_set_class_attr(sdpSvcRec);
	sdp_add_class(attr, SDP_UUID_SDP_SERVER);
	sdp_add_uuid16_to_pattern(sdpSvcRec, SDP_UUID_SDP_SERVER);

	/*
	 ** Set the version numbers supported, these are passed as arguments
	 ** to the server on command line. Now defaults to 1.0
	 ** Build the version number sequence first
	 */
	data = sdp_create_seq();
	for (i = 0; i < sdpServerVnumEntries; i++)
		sdp_append_u16(data, sdpVnumArray[i]);
	sdp_append_attr(sdpSvcRec, SDP_ATTR_VERSION_NUMBER_LIST, data);

	sdp_set_service_attr(sdpSvcRec, SDP_UUID_SDP);
	sdp_add_uuid16_to_pattern(sdpSvcRec, SDP_UUID_SDP);

	attr = sdp_set_proto_attr(sdpSvcRec);
	param = sdp_add_proto(attr, SDP_UUID_L2CAP, SDP_PSM, 2, 1);

	attr = sdp_set_subgroup_attr(sdpSvcRec);
	sdp_add_subgroup(attr, SDP_UUID_PUBLIC_BROWSE_GROUP);
	sdp_add_uuid16_to_pattern(sdpSvcRec, SDP_UUID_PUBLIC_BROWSE_GROUP);

	sdp_update_time();
	return 0;
}


/*
 ** SDP server initialization on startup includes creating
 ** the BT PSM and TCP sockets over which service discovery clients
 ** and service registration clients respectively access the
 ** SDP server
 */
int sdp_init_server(void)
{
	int status = 0;

	status = sdp_init(SDP_SVC_SERVER);
	if (status != 0) {
		BTERROR("Could not find local BT device address, terminating");
		return -1;
	}
	sdp_init_svcdb();
	sdp_init_cscache();
	/*
	 ** Register the public browse group Root
	 */
	registerPublicBrowseGroupRoot();
	/*
	 ** Register the SDP server as a service
	 */
	registerSDPServerService();

	/*
	 ** Create our socket on PSM 1 and
	 ** set activeFdSet
	 */
	serverBTSockFd = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (serverBTSockFd < 0) {
		BTERROR("BT Socket creation failure:%s", strerror(errno));
		return serverBTSockFd;
	}

	serverBTAddr.family = AF_AFFIX;
	serverBTAddr.devnum = HCIDEV_ANY;
	serverBTAddr.bda = BDADDR_ANY;
	serverBTAddr.port = SDP_PSM;
	status = bind(serverBTSockFd, (struct sockaddr *)&serverBTAddr, sizeof(serverBTAddr));
	if (status < 0) {
		BTERROR("BT bind failure:%s", strerror(errno));
		return status;
	}

	status = listen(serverBTSockFd, 5);
	if (status < 0)
		return status;

	FD_SET(serverBTSockFd, &activeFdSet);
	maxFd = serverBTSockFd;

	serverTCPSockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverTCPSockFd < 0) {
		BTERROR("TCP socket creation failure:%s", strerror(errno));
		return serverTCPSockFd;
	}
	serverINAddr.sin_family = AF_INET;
	serverINAddr.sin_port = SDP_TCP_PORT;
	status = bind(serverTCPSockFd, (struct sockaddr *)&serverINAddr, sizeof(serverINAddr));
	if (status < 0) {
		BTERROR("TCP bind failure:%s", strerror(errno));
		return status;
	}

	status = listen(serverTCPSockFd, 5);
	FD_SET(serverTCPSockFd, &activeFdSet);
	maxFd = serverTCPSockFd;

	return status;
}

void sdp_exit_server(void)
{
	sdp_reset_svcdb();
	sdp_cleanup_cscache();
	close(serverBTSockFd);
	close(serverTCPSockFd);
}


/*
 ** This is the SDP server main thread
 ** After appropriate initialization, uses the select()
 ** to field inputs from both the BT and TCP sockets.
 **
 ** Calls the _sdp_process_req() if single threaded else
 ** extracts the request and populates the request queue 
 ** for servicing by one or more worker threads that are
 ** active
 */

void sdp_sig_handler(int sig)
{
	exit(1);
}

void do_exit(void)
{
	sdp_exit_server();
}

void usage(void)
{
	fprintf(stderr, "btsdpd %s [-d] [-b]\n"
			"  -d: start as daemon\n  -b: with debug info\n", affix_version);
}

int main(int argc, char **argv)
{
	/*
	 ** Create our socket for connection
	 ** requests. Start worker threads
	 */
	int	status = 0, lind, c;
	int	gobackground = 0;
	int	clientCount = 0;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"daemon", 0, 0, 'd'},
		{0, 0, 0, 0}
	};

	if (affix_init(argc, argv, LOG_DAEMON)) {
		BTERROR("Affix initialization failed\n");
		return 1;
	}

	if (affix_pidof(argv[0], PIDOF_SINGLE | PIDOF_OMIT | PIDOF_BASENAME, getpid())) {
		BTINFO("btsdpd is already running\n");
		return 0;
	}

	BTINFO("%s %s started.", argv[0], affix_version);

	for (;;) {
		c = getopt_long(argc, argv, "+hdv", opts, &lind);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage();
				return -1;
				break;
			case 'd':
				gobackground = 1;
				break;
			case 'v':
				verboseflag = 1;
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

	status = sdp_init_server();
	if (status) {
		BTERROR("Terminating, init error");
		return -1;
	}

	if (gobackground)
		affix_background();

	atexit(do_exit);	//exit handler

	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, sdp_sig_handler);
	signal(SIGTERM, sdp_sig_handler);
	signal(SIGABRT, sdp_sig_handler);
	signal(SIGQUIT, sdp_sig_handler);

	for (;;) {
		int		numFds;
		int		newFd;
		fd_set		checkFds;
		struct timeval	tv;
		int 		fd;

		FD_ZERO(&checkFds);
		checkFds = activeFdSet;
		if (!clientCount)
			tv.tv_sec = 3;
		else
			tv.tv_sec = 1000;
		tv.tv_usec = 0;
		
		/*
		 ** Check on active fds
		 */
		DBPRT("selecting new FD to read, clientCount: %d\n", clientCount);
		numFds = select(maxFd+1, &checkFds, NULL, NULL, &tv);
		DBPRT("Selected: %d, %s (%d)", numFds, strerror(errno), errno);
		if (numFds < 0) {
			DBPRT("Select error:%s", strerror(errno));
			goto exit;
		} else if (numFds == 0) {
			/* timeout happend */
			if (clientCount == 0) {
				DBPRT("No connected clients, quit...\n");
				goto exit;
			}
		}

		if (FD_ISSET(serverBTSockFd, &checkFds)) {
			/*
			 ** A new Bluetooth connection ..
			 */
			struct sockaddr_affix	clientBTAddr;
			socklen_t		sockLength = sizeof(clientBTAddr);

			numFds--;
			newFd = accept(serverBTSockFd, (struct sockaddr *)&clientBTAddr, &sockLength);
			DBPRT("new bt fd : %d", newFd); 
			if (newFd != -1) {
				if (newFd > maxFd)
					maxFd = newFd;
				FD_SET(newFd, &activeFdSet);
				clientCount++;
			} else
				BTERROR("Error accepting Bluetooth client connection:%s", strerror(errno));
				
		} else if (FD_ISSET(serverTCPSockFd, &checkFds)) {
			/*
			 ** A new TCP connection
			 */
			struct sockaddr_in	clientINAddr;
			socklen_t		sockLength = sizeof(clientINAddr);

			numFds--;
			newFd = accept(serverTCPSockFd, (struct sockaddr *)&clientINAddr, &sockLength);
			DBPRT("new tcp fd : %d", newFd);
			if (newFd != -1) {
				if (newFd > maxFd)
					maxFd = newFd;
				FD_SET(newFd, &activeFdSet);
				clientCount++;
			} else
				BTERROR("Error accepting TCP client connection:%s", strerror(errno));
		} else {
			for (fd = 0; fd <= maxFd && numFds; fd++) {
				if (fd == serverBTSockFd || fd == serverTCPSockFd)
					continue;
				if (FD_ISSET(fd, &checkFds)) {
					int 		rx_count;
					sdp_hdr_t	sdpHeader;
					sdp_request_t	sdpReq;
					int		pkt_len;

					numFds--;
					DBPRT("Reading from : %d", fd);
					rx_count = recv(fd, &sdpHeader, sizeof(sdp_hdr_t), MSG_PEEK);
					DBPRT("Bytes read : %d", rx_count);
					if (rx_count >= sizeof(sdp_hdr_t)) {
						pkt_len = sizeof(sdp_hdr_t) + ntohs(sdpHeader.paramLength);
						DBPRT("PDU size : %d", pkt_len);
						sdpReq.data = (char*)malloc(pkt_len);
						if (!sdpReq.data)
							goto disc;

						rx_count = recv(fd, sdpReq.data, pkt_len, 0);
						DBPRT("Bytes read : %d", rx_count);
						if (rx_count >= sizeof(sdp_hdr_t)) {
							sdpReq.fd = fd;
							sdpReq.mtu = sdp_getmtu(fd);
							sdpReq.len = rx_count;
							status = _sdp_process_req(&sdpReq);
							free(sdpReq.data);
							if (status < 0)
								goto disc;
						} else {
							free(sdpReq.data);
							goto disc;
						}
					} else {
				disc:
						if (rx_count < 0) {
							BTERROR("Read error:%s", strerror(errno));
						}
						_sdp_disc_req(fd);
						sdp_close(fd);
						DBPRT("Conn closed: %d, read: %d", fd, rx_count);
						clientCount--;
						FD_CLR(fd, &activeFdSet);
						if (fd == maxFd)
							maxFd--;
					} 
				}
			}
		}
	}
exit:
	BTINFO("%s terminated.", argv[0]);
	return status;
}


