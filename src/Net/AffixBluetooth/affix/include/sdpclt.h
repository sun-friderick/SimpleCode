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
   $Id: sdpclt.h,v 1.42 2004/02/25 16:27:19 kassatki Exp $

   SDP service discovery client API definitions

   Fixes:
		Dmitry Kasatkin		: bug fixes, cleanup, re-arrangement, 
					  continuation state, mtu
*/
	 
#ifndef SDP_CLT_H
#define SDP_CLT_H

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>

#include <affix/sdp.h>

__BEGIN_DECLS

#define SDP_SVC_PROVIDER		0x01
#define SDP_SVC_SERVER			0x02

#define MAX_LEN_UUID_STR		37	// The maximum string length

/* SDP */
extern struct affix_tupla sdp_proto_map[];
extern struct affix_tupla sdp_service_map[];

int sdp_init(int flags);
void sdp_cleanup(void);

int sdp_connect(struct sockaddr_affix *saddr);
void sdp_close(int srvHandle);
int sdp_connect_local(void);

slist_t *sdp_get_seq(sdpdata_t *data);

sdpdata_t *sdp_put_u8(uint8_t val);
sdpdata_t *sdp_append_u8(sdpdata_t *seq, uint8_t val);
uint8_t sdp_get_u8(sdpdata_t *data);
sdpdata_t *sdp_put_i8(int8_t val);
sdpdata_t *sdp_append_i8(sdpdata_t *seq, int8_t val);
int8_t sdp_get_i8(sdpdata_t *data);
sdpdata_t *sdp_put_bool(int8_t val);
sdpdata_t *sdp_append_bool(sdpdata_t *seq, int8_t val);
int8_t sdp_get_bool(sdpdata_t *data);
sdpdata_t *sdp_put_u16(uint16_t val);
sdpdata_t *sdp_append_u16(sdpdata_t *seq, uint16_t val);
uint16_t sdp_get_u16(sdpdata_t *data);
sdpdata_t *sdp_put_i16(int16_t val);
sdpdata_t *sdp_append_i16(sdpdata_t *seq, int16_t val);
int16_t sdp_get_i16(sdpdata_t *data);
sdpdata_t *sdp_put_u32(uint32_t val);
sdpdata_t *sdp_append_u32(sdpdata_t *seq, uint32_t val);
uint32_t sdp_get_u32(sdpdata_t *data);
sdpdata_t *sdp_put_i32(int32_t val);
sdpdata_t *sdp_append_i32(sdpdata_t *seq, int32_t val);
int32_t sdp_get_i32(sdpdata_t *data);
sdpdata_t *sdp_put_u64(uint64_t val);
sdpdata_t *sdp_append_u64(sdpdata_t *seq, uint64_t val);
uint64_t sdp_get_u64(sdpdata_t *data);
sdpdata_t *sdp_put_i64(int64_t val);
sdpdata_t *sdp_append_i64(sdpdata_t *seq, int64_t val);
int64_t sdp_get_i64(sdpdata_t *data);
sdpdata_t *sdp_put_u128(uint128_t *val);
sdpdata_t *sdp_append_u128(sdpdata_t *seq, uint128_t *val);
uint128_t *sdp_get_u128(sdpdata_t *data);
sdpdata_t *sdp_put_i128(int128_t *val);
sdpdata_t *sdp_append_i128(sdpdata_t *seq, int128_t *val);
int128_t *sdp_get_i128(sdpdata_t *data);
sdpdata_t *sdp_put_uuid(uuid_t *val);
sdpdata_t *sdp_append_uuid(sdpdata_t *seq, uuid_t *val);
uuid_t *sdp_get_uuid(sdpdata_t *data);
sdpdata_t *sdp_put_uuid16(uint16_t val);
sdpdata_t *sdp_append_uuid16(sdpdata_t *seq, uint16_t val);
sdpdata_t *sdp_put_uuid32(uint32_t val);
sdpdata_t *sdp_append_uuid32(sdpdata_t *seq, uint32_t val);
sdpdata_t *sdp_put_uuid128(uint128_t *val);
sdpdata_t *sdp_append_uuid128(sdpdata_t *seq, uint128_t *val);
sdpdata_t *sdp_put_str(char *str);
sdpdata_t *sdp_append_str(sdpdata_t *seq, char *str);
char *sdp_get_str(sdpdata_t *data);
sdpdata_t *sdp_put_url(char *url);
sdpdata_t *sdp_append_url(sdpdata_t *seq, char *str);
char *sdp_get_url(sdpdata_t *data);
sdpdata_t *sdp_create_seq(void);
sdpdata_t *sdp_append_seq(sdpdata_t *seq);
slist_t *sdp_get_seq(sdpdata_t *data);
sdpdata_t *sdp_create_alt(void);
sdpdata_t *sdp_append_alt(sdpdata_t *seq);


int __sdp_search_req(
		struct sockaddr_affix *sa,
		slist_t *svcSearchList, 
		uint16_t maxSvcRecordCount,
		slist_t **svcResponseList,
		uint16_t *handleCount
		);



typedef enum attributeRequestType {
	IndividualAttributes=1,
	RangeOfAttributes
} sdp_attrreq_t;


int __sdp_attr_req(
		struct sockaddr_affix *sa,
		uint32_t svcHandle,
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrIDByteCount,
		sdpsvc_t **_svcRec,
		uint16_t *maxAttrResponseByteCount
		);


int __sdp_search_attr_req(
		struct sockaddr_affix *sa,
		slist_t *svcSearchList, 
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrByteCount,
		slist_t **svcResponseList,
		uint16_t *maxAttrResponseByteCount
		);


int sdp_search_req(
		int srvHandle,
		slist_t *svcSearchList, 
		uint16_t maxSvcRecordCount,
		slist_t **svcResponseList,
		uint16_t *handleCountInResponse);
int sdp_attr_req(
		int srvHandle,
		uint32_t svcHandle,
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrIDByteCount,
		sdpsvc_t	**_svcRec,
		uint16_t *maxAttrResponseByteCount);
int sdp_search_attr_req(
		int srvHandle,
		slist_t *svcSearchList, 
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrByteCount,
		slist_t **svcResponseList,
		uint16_t *maxAttrResponseByteCount);


static inline char *sdp_get_string_attr(sdpsvc_t *svcRec, uint16_t attrID)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, attrID);
	if (!data)
		return NULL;
	return sdp_get_str(data);
}

int sdp_is_proto_alt(sdpsvc_t *svcRec);
int sdp_get_proto_alt_attr(sdpsvc_t *svcRec, void **seq, void **state);
int sdp_get_proto_attr(sdpsvc_t *svcRec, void *alt, uuid_t **uuid, void **param, void **state);

int sdp_get_uuid_attr(sdpsvc_t *svcRec, uint16_t attrID, uuid_t **uuid, void **state);

static inline int sdp_get_class_attr(sdpsvc_t *svcRec, uuid_t **uuid, void **state)
{
	return sdp_get_uuid_attr(svcRec, SDP_ATTR_SERVICE_CLASSID_LIST, uuid, state); 
}

static inline int sdp_get_subgroup_attr(sdpsvc_t *svcRec, uuid_t **uuid, void **state)
{
	return sdp_get_uuid_attr(svcRec, SDP_ATTR_BROWSE_GROUP_LIST, uuid, state); 
}

int sdp_get_ttl_attr(sdpsvc_t *svcRec, uint32_t *svcTTLInfo);

int sdp_get_state_attr(sdpsvc_t *svcRec, uint32_t *svcRecState);

int sdp_get_availability_attr(sdpsvc_t *svcRec, uint8_t *svcAvail);

uuid_t *sdp_get_service_attr(sdpsvc_t *svcRec);

uuid_t *sdp_get_group_attr(sdpsvc_t *svcRec);
int sdp_is_group(sdpsvc_t *svcRec);

int sdp_get_lang_attr(sdpsvc_t *svcRec, slist_t **langSeq);

int sdp_get_profile_attr(sdpsvc_t *svcRec, uuid_t **uuid, uint16_t *ver, void **state);

int sdp_get_dbstate_attr(sdpsvc_t *svcRec, uint32_t *svcDBState);

int sdp_get_version_attr(sdpsvc_t *svcRec, uint16_t *ver, void **state);

int sdp_get_info_attr(sdpsvc_t *svcRec, char **name, char **prov, char **desc);

/* -------------------------------------------- */

void sdp_val2uuid16(uuid_t *uuid, uint16_t value16Bit);
void sdp_val2uuid32(uuid_t *uuid, uint32_t value32Bit);
void sdp_val2uuid128(uuid_t *uuid, uint128_t *value128Bit);

int sdp_match_uuid(slist_t *searchPattern, slist_t *targetPattern);
int sdp_uuidcmp(uuid_t *u1, uuid_t *u2);
int sdp_uuidcmp32(uuid_t *u1, uint32_t u2);

void sdp_print_uuid(uuid_t *uuid);  

int sdp_uuid2val(uuid_t *uuid);
int _sdp_uuid2str(uuid_t *uuid, char *str, size_t n);
char *sdp_uuid2str(uuid_t *uuid);

char *sdp_proto2str(uuid_t *uuid);
char *sdp_class2str(uuid_t *uuid);
char *sdp_profile2str(uuid_t *uuid);

/* new */
int sdp_get_rfcomm_port(sdpsvc_t *svcRec);
int sdp_find_uuid(sdpsvc_t *svcRec,  uint32_t uuid32);
int sdp_find_port_by_name(struct sockaddr_affix *saddr, char *svc);
int sdp_find_port(struct sockaddr_affix *saddr, uint16_t ServiceID);

uuid_t *s_list_append_uuid16(slist_t **list, uint16_t uuid16);
uuid_t *s_list_append_uuid32(slist_t **list, uint32_t uuid32);

int sdp_uuidcmp(uuid_t *u1, uuid_t *u2);

static inline sdpdata_t *sdp_append_data(sdpdata_t *seq, sdpdata_t *data)
{
	if (!data)
		return NULL;
	s_list_append(&seq->value.dataSeq, data);
	seq->unitSize += data->unitSize;
	return data;
}

static inline void sdp_remove_data(sdpdata_t *seq, sdpdata_t *data)
{
	if (!data)
		return;
	s_list_remove(&seq->value.dataSeq, data);
	seq->unitSize -= data->unitSize;
	sdp_free_data(data);
}


__END_DECLS

#endif //SDP_CLT_H
