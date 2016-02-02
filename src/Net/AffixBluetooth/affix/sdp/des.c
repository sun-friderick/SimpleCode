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
   $Id: des.c,v 1.35 2003/04/15 10:47:06 kds Exp $

   Extracting attributes from received PDU

   Fixes:
   		Manel Guerrero Zapata <manel.guerrero-zapata@nokia.com>
		Dmitry Kasatkin
*/

#include <affix/config.h>

#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>
#include "utils.h"
#include "attr.h"


sdpdata_t *sdp_extr_attr(char *pdata, int *extractedLength, sdpsvc_t *svcRec);


/*
 ** Extract the sequence type, it's length
 ** and return a pointer to the beginning of
 ** sequence
 XXXSDP - where StringSize???
 */
char *sdp_extr_seq_dtd(char *dstBuffer, uint8_t *dataType, 
				int *sequenceSize, int *bytesScanned)
{
	char	*pdata;
	uint8_t	localDataType;

	pdata = dstBuffer;
	localDataType = __get_u8(pdata);
	pdata += sizeof(uint8_t);
	*bytesScanned += sizeof(uint8_t);
	*dataType = localDataType;
	switch (SDP_DTD_SIZE(localDataType)) {
		case SDP_DTD_SIZE8:
			*sequenceSize = __get_u8(pdata);
			pdata += sizeof(uint8_t);
			*bytesScanned += sizeof(uint8_t);
			break;
		case SDP_DTD_SIZE16:
			*sequenceSize = ntohs(__get_u16(pdata));
			pdata += sizeof(uint16_t);
			*bytesScanned += sizeof(uint16_t);
			break;
		case SDP_DTD_SIZE32:
			*sequenceSize = ntohl(__get_u32(pdata));
			pdata += sizeof(uint32_t);
			*bytesScanned += sizeof(uint32_t);
			break;
		default :
			BTERROR("Unknown sequence type, aborting\n");
			pdata = NULL;
			break;
	}
	return pdata;
}

sdpdata_t *sdp_extr_seq(char *pdata, int *extractedLength, sdpsvc_t *svcRec)
{
	int		sequenceLength = 0;
	int		localExtractedLength = 0;
	sdpdata_t	*currentNode;
	int		seqElementCount = 0;
	char		*localSeqPtr;
	sdpdata_t	*pDataSeq;
	int		attributeLength = 0;
	slist_t		*dataSeq = NULL;
	uint8_t 	dtd;

	DBPRT("Extracting sequence ");
	localSeqPtr = sdp_extr_seq_dtd(pdata, &dtd, &sequenceLength, extractedLength);
	DBPRT("Sequence Type : 0x%x length : 0x%x", dtd, sequenceLength);

	if ((localSeqPtr == NULL) || (sequenceLength == 0))
		return NULL;
	while (localExtractedLength < sequenceLength) {
		attributeLength = 0;
		currentNode = sdp_extr_attr(localSeqPtr, &attributeLength, svcRec);
		if (!currentNode)
			break;
		seqElementCount++;
		s_list_append(&dataSeq, currentNode);
		localSeqPtr += attributeLength;
		localExtractedLength += attributeLength;
		DBPRT("LocalExtractedLength : %d SequenceLength : %d", localExtractedLength, sequenceLength);
	}
	if (localExtractedLength != sequenceLength) {
		DBPRT("Error in extracting sequence, terminating");
		sdp_free_seq(&dataSeq);
		return NULL;
	}
	/* create root element */
	pDataSeq = malloc(sizeof(sdpdata_t));
	if (!pDataSeq) {
		sdp_free_seq(&dataSeq);
		return NULL;
	}
	memset(pDataSeq, 0, sizeof(sdpdata_t));
	pDataSeq->value.dataSeq = dataSeq;
	*extractedLength += localExtractedLength;
	return pDataSeq;
}

/*
 ** Extract a sequence of service record handles from a PDU buffer
 ** and add the entries to a slist_t. Note that the service record
 ** handles are not in "data element sequence" form, but just like
 ** an array of service handles
 */
int sdp_extr_svc_handles(char *pduBuffer, slist_t **svcReqSeq, 
					int currentHandleCount, int *bytesScanned)
{
	char	*pdata = pduBuffer;
	int 	i;

	for (i = 0; i != currentHandleCount; i++) {
		s_list_append_uint(svcReqSeq, ntohl(__get_u32(pdata)));
		pdata += sizeof(uint32_t);
		*bytesScanned += sizeof(uint32_t);
	}
	return 0;
}

sdpdata_t *sdp_extr_int(char *pdata, int *extractedLength)
{
	sdpdata_t	*data;
	uint8_t		dtd;

	DBPRT("Extracting integer");
	data = (sdpdata_t *)malloc(sizeof(sdpdata_t));
	if (!data)
		return NULL;
	memset(data, 0, sizeof(sdpdata_t));
	
	dtd = __get_u8(pdata);
	pdata += sizeof(uint8_t);
	*extractedLength += sizeof(uint8_t);

	switch (SDP_DTD_SIZE(dtd)) {
		case SDP_DTD_8:
			*extractedLength += sizeof(uint8_t);
			data->value.uint8 = __get_u8(pdata);
			break;
		case SDP_DTD_16:
			*extractedLength += sizeof(uint16_t);
			data->value.uint16 = ntohs(__get_u16(pdata));
			break;
		case SDP_DTD_32:
			*extractedLength += sizeof(uint32_t);
			data->value.uint32 = ntohl(__get_u32(pdata));
			break;
		case SDP_DTD_64:
			*extractedLength += sizeof(uint64_t);
			data->value.uint64 = __be64_to_cpu(__get_u64(pdata));
			break;
			/*
			 ** Not clear if NBO rules apply here and if yes, how
			 */
		case SDP_DTD_128:
			*extractedLength += sizeof(uint128_t);
			memcpy(&data->value, pdata, sizeof(uint128_t));
			break;
		default :
			free(data);
			data = NULL;
	}
#ifdef CONFIG_AFFIX_DEBUG
	if (data)
		DBPRT("Ingeter : 0x%x", data->value.uint32);
#endif
	return data;
}

int __sdp_extr_uuid(char *pduBuffer, uuid_t *uuid, int *bytesScanned)
{
	uint8_t 	type;
	char		*pdata;

	pdata = pduBuffer; 
	type = __get_u8(pdata);
	pdata += sizeof(uint8_t);
	*bytesScanned += sizeof(uint8_t);

	if (type == SDP_DTD_UUID16) {
		sdp_val2uuid16(uuid, ntohs(__get_u16(pdata)));
		*bytesScanned += sizeof(uint16_t);
		pdata += sizeof(uint16_t);
	} else if (type == SDP_DTD_UUID32) {
		sdp_val2uuid32(uuid, ntohl(__get_u32(pdata)));
		*bytesScanned += sizeof(uint32_t);
		pdata += sizeof(uint32_t);
	} else if (type == SDP_DTD_UUID128) {
		sdp_val2uuid128(uuid, (uint128_t*)pdata);
		*bytesScanned += sizeof(uint128_t);
		pdata += sizeof(uint128_t);
	} else {
		BTERROR("Unknown data type : %d expecting a svc uuid_t\n", type);
		return -1;
	}
	return 0;
}

sdpdata_t *sdp_extr_uuid(char *pdata, int *extractedLength, sdpsvc_t *svcRec)
{
	int		status;
	sdpdata_t	*data;

	DBPRT("Extracting uuid_t");
	data = (sdpdata_t*)malloc(sizeof(sdpdata_t));
	if (!data)
		return NULL;
	memset(data, 0, sizeof(sdpdata_t));
	status = __sdp_extr_uuid(pdata, &data->value.uuid, extractedLength);
	if (status) {
		free(data);
		return NULL;
	}
	sdp_add_uuid_to_pattern(svcRec, &data->value.uuid);
	return data;
}

/*
 ** Extract strings from the PDU (could be service description
 ** and similar info) 
 */
int __sdp_extr_str(char *buffer, char **dstStringBuffer, int *bytesScanned)
{
	char	*pdata = buffer;
	int 	stringLength = 0;
	uint8_t dataType;

	dataType = __get_u8(pdata);
	pdata += sizeof(uint8_t);
	*bytesScanned += sizeof(uint8_t);
	switch (SDP_DTD_SIZE(dataType)) {
		case SDP_DTD_SIZE8:
			stringLength = __get_u8(pdata);
			pdata += sizeof(uint8_t);
			*bytesScanned += sizeof(uint8_t) + stringLength;
			break;
		case SDP_DTD_SIZE16:
			stringLength = ntohs(__get_u16(pdata));
			pdata += sizeof(uint16_t);
			*bytesScanned += sizeof(uint16_t) + stringLength;
			break;
		case SDP_DTD_SIZE32:
			stringLength = ntohl(__get_u32(pdata));
			pdata += sizeof(uint32_t);
			*bytesScanned += sizeof(uint32_t) + stringLength;
			break;
		default :
			BTERROR("Incorrect size for str/url");
			return -1;
	}
	/*
	 ** Extract the string
	 */
	*dstStringBuffer = (char*)malloc(stringLength + 1);
	if (!(*dstStringBuffer))
		return -1;
	memset(*dstStringBuffer, 0, (stringLength + 1));
	strncpy(*dstStringBuffer, pdata, stringLength);
	DBPRT("Len : %d", stringLength);
	DBPRT("Str : %s", *dstStringBuffer);
	return 0;
}

sdpdata_t *sdp_extr_str(char *pdata, int *extractedLength)
{
	int		status;
	sdpdata_t	*data;

	DBPRT("Extracting string");
	data = (sdpdata_t *)malloc(sizeof(sdpdata_t));
	if (!data)
		return NULL;
	memset(data, 0, sizeof(sdpdata_t));
	status = __sdp_extr_str(pdata, &data->value.stringPtr, extractedLength);
	if (status) {
		free(data);
		return NULL;
	}
	return data;
}


/* MAIN - sdp_extr_attr like sdp_gen_pdu */
sdpdata_t *sdp_extr_attr(char *pdata, int *extractedLength, sdpsvc_t *svcRec)
{
	uint8_t		dataType;
	sdpdata_t 	*pDataElem;
	int 		localExtractedLength = 0;

	dataType = __get_u8(pdata);
	DBPRT("data type: 0x%x", dataType);
	switch (SDP_DTD_TYPE(dataType)) {
		case SDP_DTD_INT:
		case SDP_DTD_UINT:
		case SDP_DTD_BOOL:
			pDataElem = sdp_extr_int(pdata, &localExtractedLength);
			break;
		case SDP_DTD_UUID:
			pDataElem = sdp_extr_uuid(pdata, &localExtractedLength, svcRec);
			break;
		case SDP_DTD_STR:
		case SDP_DTD_URL:
			pDataElem = sdp_extr_str(pdata, &localExtractedLength);
			break;
		case SDP_DTD_SEQ:
			pDataElem = sdp_extr_seq(pdata, &localExtractedLength, svcRec);
			break;
		case SDP_DTD_ALT:
			pDataElem = sdp_extr_seq(pdata, &localExtractedLength, svcRec);
			break;
		default :
			BTERROR("Unknown data descriptor : 0x%x terminating", dataType);
			pDataElem = NULL;
			break;
	}
	if (pDataElem) {
		pDataElem->dtd = dataType;
		*extractedLength += localExtractedLength;
	}
	return pDataElem;
}

/* MAIN extract Attributes */
int sdp_extr_attrs(sdpsvc_t *svcRec, char *localSeqPtr, int sequenceLength)
{
	int		extractStatus = -1;
	int		localExtractedLength = 0;
	uint8_t 	dtd;
	int		attrValueLength;
	int		attrSize;
	uint16_t 	attrId;
	sdpdata_t	*pAttrValue;

	while (localExtractedLength < sequenceLength) {
		DBPRT("Extract PDU, sequenceLength : %d localExtractedLength : %d",
				sequenceLength, localExtractedLength);
		attrSize = 0;
		attrValueLength = 0;
		
		dtd = __get_u8(localSeqPtr);
		attrSize = sizeof(uint8_t);
		attrId = ntohs(__get_u16(localSeqPtr + attrSize));
		attrSize += sizeof(uint16_t);
		DBPRT("DTD of attrId : %d Attr id : 0x%x ", dtd, attrId);
		pAttrValue = sdp_extr_attr((localSeqPtr+attrSize), &attrValueLength, svcRec);
		DBPRT("Attr id : 0x%x attrValueLength : %d", attrId, attrValueLength);
		attrSize += attrValueLength;
		if (!pAttrValue) {
			DBPRT("Terminating extraction of attributes");
			break;
		}
		localExtractedLength += attrSize;
		localSeqPtr += attrSize;
		sdp_append_attr(svcRec, attrId, pAttrValue);
		extractStatus = 0;
		DBPRT("Extract PDU, sequenceLength : %d localExtractedLength : %d",
				sequenceLength, localExtractedLength);
	}
	if (extractStatus == 0) {
#ifdef CONFIG_AFFIX_DEBUG
		DBPRT("Successful extracting of Svc Rec attributes");
		sdp_print_svc(svcRec->attributeList);
#endif
		return sequenceLength;
	}
	return 0;
}


/* 
 * Extract PDU for Client
 */
sdpsvc_t *sdp_clt_extr_pdu(char *pdata, uint32_t handleExpected, int *bytesScanned)
{
	int localExtractedLength = 0;
	char *localSeqPtr = NULL;
	uint8_t dtd;
	int sequenceLength = 0;
	sdpsvc_t *svcRec = NULL;
	uint16_t lookAheadAttrId = 0xffff;
	uint32_t svcRecHandle = 0xffffffff;

	localSeqPtr = sdp_extr_seq_dtd(pdata, &dtd, &sequenceLength, 
			bytesScanned);
	lookAheadAttrId = ntohs(__get_u16(localSeqPtr + sizeof(uint8_t)));
	DBPRT("Look ahead attr id : %d", lookAheadAttrId);
	/*
	 ** Check if the PDU already has a service record handle,
	 ** int which case this may be a update request
	 */
	if (lookAheadAttrId == SDP_ATTR_SERVICE_RECORD_HANDLE) {
		svcRecHandle = ntohl(__get_u32(localSeqPtr + 
					sizeof(uint8_t) +
					sizeof(uint16_t) +
					sizeof(uint8_t)));
		DBPRT("SvcRecHandle : 0x%x", svcRecHandle);
	}
	svcRec = sdp_create_svc();
	if (svcRec == NULL)
		return NULL;
	svcRec->attributeList = NULL;
	if (lookAheadAttrId == SDP_ATTR_SERVICE_RECORD_HANDLE) {
		svcRec->serviceRecordHandle = svcRecHandle;
	} else {
		if (handleExpected != 0xffffffff) {
			svcRec->serviceRecordHandle = handleExpected;
		} else {
			BTINFO("ServiceRecordHandle not defined");
		}
	}
	localExtractedLength = sdp_extr_attrs(svcRec, localSeqPtr, sequenceLength);
	if (localExtractedLength >= 0)
		*bytesScanned += localExtractedLength;
	return svcRec;
}

