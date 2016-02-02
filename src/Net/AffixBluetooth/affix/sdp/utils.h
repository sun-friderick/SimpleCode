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
   $Id: utils.h,v 1.20 2003/04/15 09:34:24 kds Exp $

   Miscellaneous implementation functions for use by the
   top level APIs

   Fixes:
   		Dmitry Kasatkin		- fixes
*/
	 
#ifndef SDP_UTILS_INT_H
#define SDP_UTILS_INT_H

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <affix/btcore.h>
#include <affix/sdp.h>


extern sdp_mode_t	sdp_mode;


uint16_t sdp_gen_trans(void);
long sdp_get_time(void);
void sdp_shutdown(void);
int sdp_getmtu(int socketFd);

char *sdp_extr_seq_dtd(char *dst, uint8_t *pdataType, int *seqLength, int *bytesScanned);
int sdp_gen_attr_seq_pdu(char *pdata, slist_t *pSeq, uint8_t dataType);
int sdp_gen_uuid_seq_pdu(char *pdata, slist_t *pSeq);
int sdp_extr_svc_handles(char *pdata, slist_t **pSeq, int handleCount, int *bytesScanned);
int sdp_extr_data_seq(char *pdata, slist_t **pSeq,uint8_t *pDataType);
int sdp_send_req_w4_rsp(int srvHandle, char *requestBuffer, 
					char *responseBuffer, int requestSize, int *responseSize);

#endif //SDP_UTILS_INT_H

