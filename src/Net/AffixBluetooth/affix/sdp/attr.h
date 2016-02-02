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
   $Id: attr.h,v 1.23 2003/03/14 08:48:18 kds Exp $

   Fixes:
   		Dmitry Kasatkin		: cleanup, fixes
*/
	 
#ifndef SDP_DYN_ATTR_H
#define SDP_DYN_ATTR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <affix/sdp.h>

#define SDP_PDU_CHUNK_SIZE	1024

void sdp_set_seq_length_pdu(sdppdu_t *pdu);
int sdp_append_pdu(sdppdu_t *dst, sdppdu_t *src);

int sdp_gen_pdu(sdppdu_t *pdu, sdpdata_t *pData);
void sdp_set_seq_length(char *ptr, int length);
void sdp_gen_svc_pdu(sdpsvc_t *svcRec);
void sdp_gen_attr_pdu(void * value, void * data);
void sdp_add_uuid_to_pattern(sdpsvc_t *svcRec, uuid_t *uuid);

static inline void sdp_add_uuid16_to_pattern(sdpsvc_t *svcRec, uint16_t uuid16)
{
	uuid_t	uuid;
	sdp_val2uuid16(&uuid, uuid16);
	sdp_add_uuid_to_pattern(svcRec, &uuid);
}

int __sdp_extr_uuid(char *buffer, uuid_t *uuid, int *bytesScanned);

uuid_t *sdp_uuidcpy128(uuid_t *uuid);
sdpsvc_t *sdp_clt_extr_pdu(char *pdata, uint32_t handleExpected, int *bytesScanned);
int sdp_extr_attrs(sdpsvc_t *svcRec, char *localSeqPtr, int sequenceLength);
int sdp_attrcmp(const void * key1, const void * key2);
void sdp_print_attr(void * value, void * userData); 

void sdp_print_svc(slist_t *pTable);
void sdp_print_data(sdpdata_t *data);
void sdp_print_seq(slist_t *dataSeq);
void sdp_print_pdu(sdppdu_t *pdu);

#endif // SDP_DYN_ATTR_H
