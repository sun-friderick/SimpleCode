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
   $Id: sdp.h,v 1.52 2004/02/26 13:56:47 kassatki Exp $

   Top level SDP definitions for attribute types. 
   This file provides a "one-stop" change for any
   modifications to the attributes

   Fixes:
		Dmitry Kasatkin		: bug fixes, cleanup, re-arrangement, 
					  continuation state, mtu
*/

#ifndef SDP_H
#define SDP_H

#include <stdint.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>

__BEGIN_DECLS


#define SDP_TIMEOUT	20

/*
 ** Protocol UUIDs
 */
#define SDP_UUID_SDP		0x0001
#define SDP_UUID_UDP		0x0002
#define SDP_UUID_RFCOMM		0x0003
#define SDP_UUID_TCP		0x0004
#define SDP_UUID_TCS_BIN	0x0005
#define SDP_UUID_TCS_AT		0x0006
#define SDP_UUID_OBEX		0x0008
#define SDP_UUID_IP		0x0009
#define SDP_UUID_FTP		0x000A
#define SDP_UUID_HTTP		0x000C
#define SDP_UUID_WSP		0x000E
#define SDP_UUID_BNEP		0x000F
#define SDP_UUID_L2CAP		0x0100

/*
 ** Service class UUIDs
 */
#define SDP_UUID_SDP_SERVER      		0x1000
#define SDP_UUID_BROWSE_GROUP_DESC		0x1001
#define SDP_UUID_PUBLIC_BROWSE_GROUP		0x1002
#define SDP_UUID_SERIAL_PORT     		0x1101
#define SDP_UUID_LAN     			0x1102
#define SDP_UUID_DUN     			0x1103
#define SDP_UUID_IRMC_SYNC     			0x1104
#define SDP_UUID_OBEX_PUSH     			0x1105
#define SDP_UUID_OBEX_FTP     			0x1106
#define SDP_UUID_IRMC_SYNC_CMD     		0x1107
#define SDP_UUID_HEADSET     			0x1108
#define SDP_UUID_CORDLESS_TELEPHONY  		0x1109
#define SDP_UUID_INTERCOM  			0x1110
#define SDP_UUID_FAX  				0x1111
#define SDP_UUID_HEADSET_AG  			0x1112
#define SDP_UUID_PANU				0x1115
#define SDP_UUID_NAP				0x1116
#define SDP_UUID_GN				0x1117
#define SDP_UUID_HANDSFREE			0x111E
#define SDP_UUID_HANDSFREE_AG			0x111F
#define SDP_UUID_PNP_INFO  			0x1200
#define SDP_UUID_GENERIC_NETWORKING  		0x1201
#define SDP_UUID_GENERIC_FTP  			0x1202
#define SDP_UUID_GENERIC_AUDIO  		0x1203
#define SDP_UUID_GENERIC_TELEPHONY		0x1204

/*
 ** Attribute identifier codes
 ** Possible values for uint16_t are
 ** listed below. See SDP Spec, section "Service Attribute
 ** Definitions" for more details.
 */
#define SDP_ATTR_SERVICE_RECORD_HANDLE		0x0000
#define SDP_ATTR_SERVICE_CLASSID_LIST		0x0001
#define SDP_ATTR_SERVICE_RECORD_STATE		0x0002
#define SDP_ATTR_SERVICEID			0x0003
#define SDP_ATTR_PROTO_DESC_LIST		0x0004
#define SDP_ATTR_BROWSE_GROUP_LIST		0x0005
#define SDP_ATTR_LANG_BASE_ATTRID_LIST		0x0006
#define SDP_ATTR_SERVICE_INFO_TTL		0x0007
#define SDP_ATTR_SERVICE_AVAILABILITY		0x0008
#define SDP_ATTR_PROFILE_DESC_LIST		0x0009
#define SDP_ATTR_DOC_URL			0x000A
#define SDP_ATTR_EXEC_URL			0x000B
#define SDP_ATTR_ICON_URL			0x000C                      
#define SDP_ATTR_IP_SUBNET			0x0200	//!!!
#define SDP_ATTR_VERSION_NUMBER_LIST		0x0200	//!!!
#define SDP_ATTR_GROUPID			0x0200	//!!!
#define SDP_ATTR_SERVICE_DB_STATE		0x0201

/* PAN */
#define SDP_ATTR_SECURITY_DESC			0x030A
#define SDP_ATTR_NET_ACCESS_TYPE		0x030B
#define SDP_ATTR_MAX_NET_ACCESS_RATE		0x030C
/* HandsFree */
#define SDP_ATTR_SUPPORTED_FEATURES		0x0311
/* OBEX Push */
#define SDP_ATTR_SUPPORTED_FORMATS_LIST		0x0303


/*
 ** These identifiers are based on the SDP
 ** spec stating that "base attribute id of
 ** the primary (universal) language must be 0x0100
 */
#define SDP_ATTR_PRIM_LANG_BASE 		0x0100

#define SDP_ATTR_SERVICE_NAME         		0x0000
#define SDP_ATTR_SERVICE_DESC  			0x0001
#define SDP_ATTR_PROVIDER_NAME 			0x0002

#define SDP_ATTR_SERVICE_NAME_PRIM         	(SDP_ATTR_SERVICE_NAME+SDP_ATTR_PRIM_LANG_BASE)
#define SDP_ATTR_SERVICE_DESC_PRIM  		(SDP_ATTR_SERVICE_DESC+SDP_ATTR_PRIM_LANG_BASE)
#define SDP_ATTR_PROVIDER_NAME_PRIM 		(SDP_ATTR_PROVIDER_NAME+SDP_ATTR_PRIM_LANG_BASE)
/*
 ** Other languages should have theor own offset ex.
 ** #define XXXLangBase yyyy
 ** #define AttrServiceName_XXX	0x0000+XXXLangBase
 ** ..
 */

/* --------------------------------------------------------------- */

/*
 ** The Data representation in SDP PDUs (pg 339 and 340 of BT SDP Spec)
 **
 ** These are the exact data type+size descriptor values
 ** that go into PDU buffer.
 **
 ** The datatype(leading 5bits) + size descriptor(last 3 bits)
 ** is 8 bits. The size descriptor is critical to extract the
 ** right number of bytes for the data value from the PDU.
 **
 ** For most basic types, the datatype+size descriptor is
 ** straightforward. However for constructed types and strings,
 ** the size of the data is in the next "n" bytes following the
 ** 8 bits (datatype+size) descriptor. Exactly what the "n" is
 ** specified in the 3 bits of the data size descriptor.
 **
 ** TextString and URLString can be of size 2^{8, 16, 32} bytes
 ** DataSequence and DataSequenceAlternates can be of size 2^{8, 16, 32}
 ** The size are computed post-facto in the API and are not known apriori
 */
/*
 ** Permitted values for DTD
 */
#define SDP_DTD_UINT8  			0x08
#define SDP_DTD_UINT16			0x09
#define SDP_DTD_UINT32			0x0A
#define SDP_DTD_UINT64			0x0B
#define SDP_DTD_UINT128			0x0C
#define SDP_DTD_INT8			0x10
#define SDP_DTD_INT16			0x11
#define SDP_DTD_INT32			0x12
#define SDP_DTD_INT64			0x13
#define SDP_DTD_INT128			0x14
#define SDP_DTD_AUUID			0x18
#define SDP_DTD_UUID16			0x19
#define SDP_DTD_UUID32			0x1A
#define SDP_DTD_UUID128			0x1C
#define SDP_DTD_STR8			0x25
#define SDP_DTD_STR16			0x26
#define SDP_DTD_STR32        		0x27
#define SDP_DTD_ABOOL			0x28
#define SDP_DTD_SEQ8			0x35
#define SDP_DTD_SEQ16			0x36
#define SDP_DTD_SEQ32			0x37
#define SDP_DTD_ALT8			0x3D
#define SDP_DTD_ALT16			0x3E
#define SDP_DTD_ALT32			0x3F
#define SDP_DTD_URL8			0x45
#define SDP_DTD_URL16			0x46
#define SDP_DTD_URL32			0x47

#define SDP_DTD_NULL			0x00
#define SDP_DTD_UINT			0x01
#define SDP_DTD_INT			0x02
#define SDP_DTD_UUID			0x03
#define SDP_DTD_STR			0x04
#define SDP_DTD_BOOL			0x05
#define SDP_DTD_SEQ			0x06
#define SDP_DTD_ALT			0x07
#define SDP_DTD_URL			0x08

#define SDP_DTD_8			0x00
#define SDP_DTD_16			0x01
#define SDP_DTD_32			0x02
#define SDP_DTD_64			0x03
#define SDP_DTD_128			0x04
#define SDP_DTD_SIZE8			0x05
#define SDP_DTD_SIZE16			0x06
#define SDP_DTD_SIZE32			0x07

#define SDP_DTD_TYPE(dtd)		((dtd)>>3)
#define SDP_DTD_SIZE(dtd)		((dtd)&0x07)
#define SDP_DTD_SETSIZE(dtd, size)	((dtd) = ((dtd)&~0x07) | (size))

/*
 ** The PDU identifiers of SDP packets between client
 ** and server
 */
#define SDP_PDU_ERROR_RSP		0x01
#define SDP_PDU_SEARCH_REQ 		0x02
#define SDP_PDU_SEARCH_RSP 		0x03
#define SDP_PDU_ATTR_REQ 		0x04
#define SDP_PDU_ATTR_RSP 		0x05
#define SDP_PDU_SEARCH_ATTR_REQ 	0x06
#define SDP_PDU_SEARCH_ATTR_RSP 	0x07

/*
 ** Some additions to support service registration.
 ** These are outside the scope of the Bluetooth specification
 */
#define SDP_PDU_REG_REQ 		0x75
#define SDP_PDU_REG_RSP 		0x76
#define SDP_PDU_UPDATE_REQ		0x77
#define SDP_PDU_UPDATE_RSP		0x78
#define SDP_PDU_REMOVE_REQ		0x79
#define SDP_PDU_REMOVE_RSP		0x80


/*
 ** SDP Error codes
 */
#define SDP_ERR_SDP_VERSION		0x0001
#define SDP_ERR_SERVICE_RECORD_HANDLE	0x0002
#define SDP_ERR_REQUEST_SYNTAX		0x0003
#define SDP_ERR_PDU_SIZE		0x0004
#define SDP_ERR_CONT_STATE		0x0005
#define SDP_ERR_RESOURCES		0x0006
/* internal Affix specific */
#define SDP_ERR_INVALID_ARG		0x0101
#define SDP_ERR_NOT_EXIST		0x0102
#define SDP_ERR_SYNTAX			0x0103
#define SDP_ERR_INTERNAL		0x0104
#define SDP_ERR_SERVER			0x0105


#define SDP_REQ_BUF_SIZE		2048
#define SDP_RSP_BUF_SIZE		USHRT_MAX

#define SDP_BASIC_ATTR_PDU_SIZE		32
#define SDP_SEQ_PDU_SIZE 	  	128
#define SDP_UUID_SEQ_PDU_SIZE	  	256

typedef struct {
	char data[16];
} uint128_t ;

typedef uint128_t	int128_t;

typedef struct {
	int	type;
	union {
		uint16_t	uuid16Bit;
		uint32_t	uuid32Bit;
		uint128_t	uuid128Bit;
	} value ;
} uuid_t;      


/*
 ** User visible strings can be in many languages
 ** in addition to the universal language.
 **
 ** Language meta-data includes language code in
 ** ISO639 followed by the encoding format.
 ** The third field in this structure is the
 ** attribute offset for the language. User visible
 ** strings in the specified language can be obtained
 ** at this offset
 */
typedef struct {
	uint16_t	lang;
	uint16_t	encoding;
	uint16_t	offset;
} sdp_lang_t ;

/* ------------------------------------------------------- */




typedef enum operationMode {
	SDP_CLIENT,
	SDP_SERVER
} sdp_mode_t;

/*
 ** Request blob as received from the client
 */
typedef struct {
	int	fd;
	int	mtu;
	char	*data;
	int	len;
} sdp_request_t ;

/*
 ** Proprietary (Nokia) extensions to register/delete/modify
 ** service records
 */
typedef struct {
	uint8_t		pduId;
	uint16_t	transactionId;
	uint16_t 	paramLength;	/* not includeing pduId and transId */
	uint16_t	data[0];
} __PACK__ sdp_hdr_t;

struct sdp_error {
	uint8_t		pduId;
	uint16_t	transactionId;
	uint16_t 	paramLength;	/* not includeing pduId and transId */
	uint16_t	ErrorCode;
	uint8_t		ErrorInfo[0];
} __PACK__;

/*
 ** The service record when encapsulated as a PDU
 ** as per BT spec
 **
 **  Each of the PDU form objects are cached to
 ** avoid repetitive encodings on the server side
 ** They are however set to null on the client side
 */
typedef struct {
	char	*data;
	int	length;
	int	size;
} sdppdu_t;     


/* ---------------------------------------------------- */

/* SDP record state */
enum {
	SDP_STATE_CREATED,
	SDP_STATE_REGISTERED,
	SDP_STATE_UPDATE_READY,
	SDP_STATE_REGISTER_READY,
	SDP_STATE_NOT_EXIST
};

/*
 ** sdpsvc_t is a composite of many attributes not
 ** all of which need be present. References of those
 ** composites not present would be set to NULL
 **
 ** A set of mandatory attributes will be defined and
 ** when set, the sdpsvc_t gets into a "commit ready"
 ** state
 **
 */
typedef struct {
	uint32_t	serviceRecordHandle;
	BD_ADDR		*bda;		/* service owner */
	int		state;
	sdppdu_t	pdu;	/* PDU form and the entire service record */
	slist_t 	*targetPattern;	/* all possible uuid_t */
	slist_t 	*attributeList;
	int		fd;	/* to remove service */
} sdpsvc_t ;

typedef struct sdpdata {
	uint8_t		dtd;
	uint16_t	attrId;
	union {
		int8_t 		int8;
		int16_t 	int16;
		int32_t 	int32;
		int64_t 	int64;
		uint128_t 	int128;
		uint8_t 	uint8;
		uint16_t 	uint16;
		uint32_t 	uint32;
		uint64_t 	uint64;
		uint128_t 	uint128;
		uuid_t		uuid;
		char 		*stringPtr;
		slist_t	 	*dataSeq;
	} value;
	sdppdu_t 	pdu;
	int 		unitSize;
} sdpdata_t;

sdpsvc_t *sdp_create_svc(void);
void sdp_free_svc(sdpsvc_t *svcRec);
void sdp_free_svclist(slist_t **svcList);
void sdp_free_data(sdpdata_t *pData);
void sdp_free_seq(slist_t **pDataSeq);
sdpdata_t *sdp_get_attr(sdpsvc_t *svcRec, uint16_t attrId);
sdpdata_t *sdp_create_data(uint8_t dtd, void *pValue);

static inline int sdp_is_seq(sdpdata_t *data)
{
	return (SDP_DTD_TYPE(data->dtd) == SDP_DTD_SEQ);
}

static inline int sdp_is_alt(sdpdata_t *data)
{
	return (SDP_DTD_TYPE(data->dtd) == SDP_DTD_ALT);
}

static inline int sdp_is_seqalt(sdpdata_t *data)
{
	return (sdp_is_seq(data) || sdp_is_alt(data));
}

char *sdp_error(int err);

__END_DECLS

#endif //SDP_H
