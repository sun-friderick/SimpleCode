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
   $Id: attr.c,v 1.44 2003/04/15 10:47:06 kds Exp $

   Fixes:
   		Dmitry Kasatkin		: cleanup, fixes
*/

#include <affix/config.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>
#include "attr.h"
#include "utils.h"

void sdp_add_uuid_to_pattern(sdpsvc_t *svcRec, uuid_t *uuid)
{
	uuid_t *uuid128;

//	if (sdp_mode == SDP_CLIENT)
//		return;
	if (!svcRec || !uuid)
		return;
	uuid128 = sdp_uuidcpy128(uuid);
	if (!uuid128)
		return;
	if (s_list_find_custom(svcRec->targetPattern, uuid128, (void*)sdp_uuidcmp) == NULL)
		s_list_insert_sorted(&svcRec->targetPattern, uuid128, (void*)sdp_uuidcmp);
	else
		free(uuid128);
}

void sdp_free_data(sdpdata_t *data)
{
	if (SDP_DTD_TYPE(data->dtd) == SDP_DTD_SEQ || 
					SDP_DTD_TYPE(data->dtd) == SDP_DTD_ALT) {
		sdp_free_seq(&data->value.dataSeq);
	} else if (SDP_DTD_TYPE(data->dtd) == SDP_DTD_STR || 
					SDP_DTD_TYPE(data->dtd) == SDP_DTD_URL) {
		/* string type */
		free(data->value.stringPtr);
	}
	free(data);
}

void sdp_free_seq(slist_t **pDataSeq)
{
	slist_t	*dataSeq;
	for (dataSeq = *pDataSeq; dataSeq; dataSeq = s_list_next(dataSeq)) {
		sdp_free_data(dataSeq->data);
	}
	s_list_free(pDataSeq);
}

sdpdata_t *sdp_create_data(uint8_t dtd, void *pValue)
{
	sdpdata_t	*pSeqEntry;
	sdpdata_t	*data = NULL;
	int		strLength = 0;
	slist_t		*dataSeq;

	if (!pValue && SDP_DTD_TYPE(dtd) != SDP_DTD_SEQ && SDP_DTD_TYPE(dtd) != SDP_DTD_ALT)
		return NULL;
	data = (sdpdata_t *)malloc(sizeof(sdpdata_t));
	if (!data)
		return NULL;
	memset(data, 0, sizeof(sdpdata_t));
	data->dtd = dtd;
	data->unitSize = sizeof(uint8_t);
	switch (dtd) {
		case SDP_DTD_UINT8 :
			data->value.uint8 = *(uint8_t *)pValue;
			data->unitSize += sizeof(uint8_t);
			break;
		case SDP_DTD_INT8 :
		case SDP_DTD_ABOOL :
			data->value.int8 = *(int8_t *)pValue;
			data->unitSize += sizeof(int8_t);
			break;
		case SDP_DTD_UINT16 :
			data->value.uint16 = *(uint16_t *)pValue;
			data->unitSize += sizeof(uint16_t);
			break;
		case SDP_DTD_INT16 :
			data->value.int16 = *(int16_t *)pValue;
			data->unitSize += sizeof(int16_t);
			break;
		case SDP_DTD_UINT32 :
			data->value.uint32 = *(uint32_t *)pValue;
			data->unitSize += sizeof(uint32_t);
			break;
		case SDP_DTD_INT32 :
			data->value.int32 = *(int32_t *)pValue;
			data->unitSize += sizeof(int32_t);
			break;
		case SDP_DTD_INT64 :
			data->value.int64 = *(int64_t *)pValue;
			data->unitSize += sizeof(int64_t);
			break;
		case SDP_DTD_UINT64 :
			data->value.uint64 = *(uint64_t *)pValue;
			data->unitSize += sizeof(uint64_t);
			break;
		case SDP_DTD_UINT128 :
			memcpy(&data->value.uint128, pValue, sizeof(uint128_t));
			data->unitSize += sizeof(uint128_t);
			break;
		case SDP_DTD_INT128 :        
			memcpy(&data->value.int128, pValue, sizeof(uint128_t));
			data->unitSize += sizeof(uint128_t);
			break;
		case SDP_DTD_AUUID :
			data->value.uuid = *(uuid_t*)pValue;
			data->dtd = data->value.uuid.type;
			//data->unitSize += sizeof(uint16_t);
			break;
		case SDP_DTD_UUID16 :
			sdp_val2uuid16(&data->value.uuid, *(uint16_t *)pValue);
			data->unitSize += sizeof(uint16_t);
			break;
		case SDP_DTD_UUID32 :
			sdp_val2uuid32(&data->value.uuid, *(uint32_t *)pValue);
			data->unitSize += sizeof(uint32_t);
			break;
		case SDP_DTD_UUID128 :
			sdp_val2uuid128(&data->value.uuid, (uint128_t *)pValue);
			data->unitSize += sizeof(uint128_t);
			break;
		case SDP_DTD_URL8 :
		case SDP_DTD_STR8 :
		case SDP_DTD_URL16 :
		case SDP_DTD_STR16 :
		case SDP_DTD_URL32 :
		case SDP_DTD_STR32 :
			strLength = strlen(pValue) + 1;
			data->unitSize += strLength;
			data->value.stringPtr = (char*)malloc(strLength);
			if (!data->value.stringPtr)
				goto err;
			strcpy(data->value.stringPtr, pValue);
			if (strLength <= UCHAR_MAX) {
				data->unitSize += sizeof(uint8_t);
				SDP_DTD_SETSIZE(data->dtd, SDP_DTD_SIZE8);
			} else if (strLength <= USHRT_MAX){
				data->unitSize += sizeof(uint16_t);
				SDP_DTD_SETSIZE(data->dtd, SDP_DTD_SIZE16);
			} else {
				data->unitSize += sizeof(uint32_t);
				SDP_DTD_SETSIZE(data->dtd, SDP_DTD_SIZE32);
			}
			break;
		case SDP_DTD_ALT8 :
		case SDP_DTD_ALT16 :
		case SDP_DTD_ALT32 :
		case SDP_DTD_SEQ8 :
		case SDP_DTD_SEQ16 :
		case SDP_DTD_SEQ32 :
			data->value.dataSeq = pValue;
			for (dataSeq = pValue; dataSeq; dataSeq = s_list_next(dataSeq)) {
				pSeqEntry = dataSeq->data;
				data->unitSize += pSeqEntry->unitSize;
			}
			break;
		default :
			goto err;
			break;
	}
	return data;
err:
	free(data);
	return NULL;	
}

int sdp_attrcmp(const void *key1, const void *key2)
{
	int		cmp = 0;
	sdpdata_t	*pData1;
	sdpdata_t	*pData2;

	pData1 = (sdpdata_t *)key1;
	pData2 = (sdpdata_t *)key2;
	if (pData1 && pData2) {
		cmp = pData1->attrId - pData2->attrId;
	}
	return cmp;
}

sdpdata_t *sdp_get_attr(sdpsvc_t *svcRec, uint16_t attrId)
{
	sdpdata_t	sdpTemplate;
	slist_t		*pStartElem = NULL;

	if (svcRec->attributeList == NULL)
		return NULL;
	sdpTemplate.attrId = attrId;
	pStartElem = s_list_find_custom(svcRec->attributeList, &sdpTemplate, sdp_attrcmp); 
	if (pStartElem)
		return (sdpdata_t*)pStartElem->data;
	return NULL;
}

sdpdata_t *sdp_append_attr(sdpsvc_t *svcRec, uint16_t attrId, sdpdata_t *data)
{
	sdpdata_t *pData;
	
	if (!data)
		return NULL;
	DBPRT("Adding attr id : 0x%x to attr table", attrId);
	data->attrId = attrId;
	pData = sdp_get_attr(svcRec, attrId);
	if (pData) {
		s_list_remove(&svcRec->attributeList, pData);
		sdp_free_data(pData);
	} 
	s_list_insert_sorted(&svcRec->attributeList, data, sdp_attrcmp);
	sdp_set_state(svcRec);
	return data;
}
void sdp_remove_attr(sdpsvc_t *svcRec, uint16_t attrId)
{
	sdpdata_t	*data;

	if (!svcRec || !svcRec->attributeList)
		return;
	data = sdp_get_attr(svcRec, attrId);
	if (!data)
		return;
	s_list_remove(&svcRec->attributeList, data);
	sdp_free_data(data);
}

void sdp_set_seq_length(char *sequenceStartPtr, int length)
{
	uint8_t dtd;

	dtd = __get_u8(sequenceStartPtr);
	switch (SDP_DTD_SIZE(dtd)) {
		case SDP_DTD_SIZE8:
			__put_u8(sequenceStartPtr + 1, length);
			break;
		case SDP_DTD_SIZE16:
			__put_u16(sequenceStartPtr + 1, htons(length));
			break;
		case SDP_DTD_SIZE32:
			__put_u32(sequenceStartPtr + 1, htonl(length));
			break;
	}
}

int sdp_set_dtd(sdppdu_t *pdu, uint8_t dtd)
{
	char	*pdata;
	int	length = 0;
	int	originalSize = 0;

	if (pdu == NULL)
		return 0;
	originalSize = pdu->length;
	pdata = pdu->data + pdu->length;
	__put_u8(pdata, dtd);
	pdata += sizeof(uint8_t);
	pdu->length += sizeof(uint8_t);
	switch (SDP_DTD_SIZE(dtd)) {
		case SDP_DTD_SIZE8:
			pdu->length += sizeof(uint8_t);
			break;
		case SDP_DTD_SIZE16:
			pdu->length += sizeof(uint16_t);
			break;
		case SDP_DTD_SIZE32:
			pdu->length += sizeof(uint32_t);
			break;
	}
	length = pdu->length - originalSize;
	return length;
}


void sdp_set_attr_id(sdppdu_t *pdu, uint16_t svcAttrId)
{
	char *pdata;

	if (pdu == NULL)
		return;
	pdata = pdu->data;
	/* Data type for SvcAttrId */
	__put_u8(pdata, SDP_DTD_UINT16);
	pdata += sizeof(uint8_t);
	pdu->length = sizeof(uint8_t);
	__put_u16(pdata, htons(svcAttrId));
	pdata += sizeof(uint16_t);
	pdu->length += sizeof(uint16_t);
}

/*
 ** Generate the attribute sequence pdu form
 ** from slist_t elements. Return length of attr seq
 */
int __sdp_gen_seq_pdu(char *dstBuffer, slist_t *seq, uint8_t dataType)
{
	sdpdata_t	*pDataSeq;
	sdppdu_t	pdu;
	int		seqLength = 0;
	
	memset(&pdu, 0, sizeof(sdppdu_t));
	pdu.data = (char*)malloc(SDP_UUID_SEQ_PDU_SIZE);
	if (!pdu.data)
		return -1;
	pdu.size = SDP_UUID_SEQ_PDU_SIZE;
	pDataSeq = sdp_create_seq();
	for (; seq; seq = s_list_next(seq)) {
		if (SDP_DTD_TYPE(dataType) == SDP_DTD_UUID)
			sdp_append_uuid(pDataSeq, s_list_data(seq));
		else if (dataType == SDP_DTD_UINT16)
			sdp_append_u16(pDataSeq, s_list_uint(seq));
		else /* SDP_DTD_UINT32 */
			sdp_append_u32(pDataSeq, s_list_uint(seq));
	}
	DBPRT("Data Seq : %p\n", pDataSeq);
	seqLength = sdp_gen_pdu(&pdu, pDataSeq);
	DBPRT("Copying : %d\n", pdu.length);
	memcpy(dstBuffer, pdu.data, pdu.length);
	sdp_free_data(pDataSeq);
	free(pdu.data);
	return seqLength;
}

int sdp_gen_uuid_seq_pdu(char *dstBuffer, slist_t *seq)
{
	uuid_t	*uuid = (uuid_t*)s_list_data(seq);
	return __sdp_gen_seq_pdu(dstBuffer, seq, uuid->type);
}

int sdp_gen_attr_seq_pdu(char *dstBuffer, slist_t *seq, uint8_t dataType)
{
	return __sdp_gen_seq_pdu(dstBuffer, seq, dataType);
}

int sdp_gen_seq_pdu(sdppdu_t *dstPDUForm, sdpdata_t *data)
{
	sdpdata_t	*localSDPData;
	slist_t		*dataSeq;
	int		length = 0;

	for (dataSeq = sdp_get_seq(data); dataSeq; dataSeq = s_list_next(dataSeq)) {
		localSDPData = dataSeq->data;
		length += sdp_gen_pdu(dstPDUForm, localSDPData);
#ifdef CONFIG_AFFIX_DEBUG
		sdp_print_pdu(dstPDUForm);
#endif
	}
	return length;
}

int sdp_gen_pdu(sdppdu_t *dstPDUForm, sdpdata_t *data)
{
	int		pduSize = 0;
	int 		length = 0;
	unsigned char 	*pSrc = NULL;
	unsigned char 	sequenceType = 0;
	unsigned char 	sequenceAlternatesType = 0;
	uint8_t		dtd;
	uint8_t 	savedDTD;
	char 		*sequenceStartPtr = NULL;
	uint16_t 	uint16nbo;
	uint32_t 	uint32nbo;

	if (!data || !dstPDUForm)
		return 0;
	dtd = data->dtd;
	sequenceStartPtr = dstPDUForm->data + dstPDUForm->length;
	pduSize = sdp_set_dtd(dstPDUForm, dtd);
	switch (dtd) {
		case SDP_DTD_UINT8 :
			pSrc = &data->value.uint8;
			length = sizeof(uint8_t);
			break;
		case SDP_DTD_UINT16 :
			uint16nbo = htons(data->value.uint16);
			pSrc = (unsigned char *)&uint16nbo;
			length = sizeof(uint16_t);
			break;
		case SDP_DTD_UINT32 :
			uint32nbo = htonl(data->value.uint32);
			pSrc = (unsigned char *)&uint32nbo;
			length = sizeof(uint32_t);
			break;
		case SDP_DTD_UINT64 :
			pSrc = (unsigned char *)&data->value.uint64;
			length = sizeof(uint64_t);
			break;
		case SDP_DTD_UINT128 :
			pSrc = (unsigned char *)&data->value.uint128;
			length = sizeof(uint128_t);
			break;
		case SDP_DTD_INT8 :
		case SDP_DTD_ABOOL :
			pSrc = (unsigned char *)&data->value.int8;
			length = sizeof(int8_t);
			break;
		case SDP_DTD_INT16 :
			uint16nbo = htons(data->value.int16);
			pSrc = (unsigned char *)&uint16nbo;
			length = sizeof(int16_t);
			break;
		case SDP_DTD_INT32 :
			uint32nbo = htonl(data->value.int32);
			pSrc = (unsigned char *)&uint32nbo;
			length = sizeof(int32_t);
			break;
		case SDP_DTD_INT64 :
			pSrc = (unsigned char *)&data->value.int64;
			length = sizeof(int64_t);
			break;
		case SDP_DTD_INT128 :
			pSrc = (unsigned char *)&data->value.int128;
			length = sizeof(uint128_t);
			break;
		case SDP_DTD_STR8 :
		case SDP_DTD_URL8 :
		case SDP_DTD_STR16 :
		case SDP_DTD_STR32 :
		case SDP_DTD_URL16 :
		case SDP_DTD_URL32 :
			pSrc = (unsigned char *)data->value.stringPtr;
			length = strlen(data->value.stringPtr);
			sdp_set_seq_length(sequenceStartPtr, length);
			break;
		case SDP_DTD_SEQ8 :
		case SDP_DTD_SEQ16 :
		case SDP_DTD_SEQ32 :
			sequenceType = 1;
			length = sdp_gen_seq_pdu(dstPDUForm, data);
			sdp_set_seq_length(sequenceStartPtr, length);
			break;
		case SDP_DTD_ALT8:
		case SDP_DTD_ALT16 :
		case SDP_DTD_ALT32 :
			sequenceAlternatesType = 1;
			savedDTD = data->dtd;
			data->dtd = SDP_DTD_SEQ8;
			length = sdp_gen_pdu(dstPDUForm, data);
			data->dtd = savedDTD;
			sdp_set_seq_length(sequenceStartPtr, length);
			break;
		case SDP_DTD_UUID16 :
			uint16nbo = htons(data->value.uuid.value.uuid16Bit);
			pSrc = (unsigned char *)&uint16nbo;
			length = sizeof(uint16_t);
			break;
		case SDP_DTD_UUID32 :
			uint32nbo = htonl(data->value.uuid.value.uuid32Bit);
			pSrc = (unsigned char *)&uint32nbo;
			length = sizeof(uint32_t);
			break;
		case SDP_DTD_UUID128 :
			pSrc = (unsigned char *)&data->value.uuid.value.uuid128Bit;
			length = sizeof(uint128_t);
			break;
		default :
			break;
	}
	if ((sequenceType == 0) && (sequenceAlternatesType == 0)) {
		if (pSrc) {
			memcpy((dstPDUForm->data + dstPDUForm->length), pSrc, length);
			dstPDUForm->length += length;
		} else
			DBPRT("Gen PDU : Cant copy from NULL source or dest\n");
	}
	pduSize += length;
	return pduSize;
}


/*
 ** Sets the length of a data element sequence
 ** The size is either an 8 bit quantity or a 16 bit
 ** quantity. This method is called after the actual 
 ** data is appended to the data element sequence and
 ** PDU length is computed
 */
void sdp_set_seq_length_pdu(sdppdu_t *pdu)
{
	char	*pdata;
	uint8_t	dataType;

	pdata = pdu->data;
	dataType = __get_u8(pdata);
	pdata += sizeof(uint8_t);
	switch (dataType) {
		case SDP_DTD_SEQ8 :
			__put_u8(pdata, pdu->length - 
				sizeof(uint8_t) - sizeof(uint8_t));
			break;
		case SDP_DTD_SEQ16 :
			__put_u16(pdata, htons(pdu->length - 
					sizeof(uint8_t) - sizeof(uint16_t)));
			break;
		case SDP_DTD_SEQ32 :
			__put_u32(pdata, htonl(pdu->length - 
					sizeof(uint8_t) - sizeof(uint32_t)));
			break;
		default :
			break;
	}
}

/*
 ** This function appends data to the PDU form in the buffer "dst"
 ** from source "src". The data length is also computed and set.
 ** Should the PDU form length exceed 2^8, then sequence type is
 ** set accordingly and the data is memmove()'d.
 */
int sdp_append_pdu(sdppdu_t *dst, sdppdu_t *src)
{
	uint8_t	dataType;
	char	*pdata;

	pdata = dst->data;
	dataType = __get_u8(pdata);

	DBPRT("Append src size : %d", src->length);
	DBPRT("Append dst size : %d", dst->length);
	DBPRT("Dst buffer size : %d", dst->size);

	if (!dst || !dst->data)
		return 0;
	if ((dst->length + src->length) > dst->size) {
		/*
		 ** Need to realloc !
		 */
		int neededSize = SDP_PDU_CHUNK_SIZE * ((src->length/SDP_PDU_CHUNK_SIZE) + 1);
		dst->data = (char *)realloc(dst->data, (dst->size + neededSize));
		DBPRT("Realloc'ing : %d", neededSize);
		if (dst->data == NULL) {
			BTERROR("Realloc fails \n");
		}
		dst->size += neededSize;
	}
	if ((dst->length == 0) && (dataType == 0)) {
		//create initial sequence
		__put_u8(pdata, SDP_DTD_SEQ8);
		pdata += sizeof(uint8_t);
		dst->length += sizeof(uint8_t);
		/*
		 ** Space for sequence size
		 */
		pdata += sizeof(uint8_t);
		dst->length += sizeof(uint8_t);
	}
	memcpy((dst->data + dst->length), src->data, src->length);
	dst->length += src->length;

	dataType = *(uint8_t *)dst->data;
	if ((dst->length > UCHAR_MAX) && (dataType == SDP_DTD_SEQ8)) {
		/*
		 ** Need to memmove
		 */
		short offset = sizeof(uint8_t) + sizeof(uint8_t);
		memmove((dst->data + offset + 1), (dst->data + offset),
				(dst->length - offset));
		pdata = dst->data;
		__put_u8(pdata, SDP_DTD_SEQ16);
		pdata += sizeof(uint8_t);
		dst->length += 1;
	}
	sdp_set_seq_length_pdu(dst);
	return src->length;
}

void sdp_gen_attr_pdu(void *value, void *userData)
{
	sdpdata_t	*data = (sdpdata_t*)value;
	uint8_t		dtd;
	int		pduFormSize = 0;

	if (!data)
		return;
	dtd = data->dtd;
	if (dtd < SDP_DTD_UUID128)
		pduFormSize = SDP_BASIC_ATTR_PDU_SIZE;
	else
		pduFormSize = SDP_SEQ_PDU_SIZE;
	if (data->pdu.data == NULL) {
		data->pdu.data = (char *)malloc(pduFormSize);
		if (!data->pdu.data)
			return;
		data->pdu.size = pduFormSize;
	}
	data->pdu.length = 0;
	memset(data->pdu.data, 0, data->pdu.size);
	DBPRT("Adding attr id : 0x%x", data->attrId);
	sdp_set_attr_id(&data->pdu, data->attrId);	// attr id
	sdp_gen_pdu(&data->pdu, data);		// attr value
#ifdef CONFIG_AFFIX_DEBUG
	sdp_print_pdu(&data->pdu);
#endif
	/*
	 ** Append to the overall PDU form
	 */
	if (userData != NULL) {
		/* will be a sequence of attributes */
		sdp_append_pdu((sdppdu_t *)userData, &data->pdu);
	}
}

/*
 *  Main function. Called to serialize Service Record to sdppdu_t
 *  MAIN for sdp_register_service
 */
void sdp_gen_svc_pdu(sdpsvc_t *svcRec)
{
	slist_t	*attrList;

	attrList = svcRec->attributeList;
	if (!attrList)
		return;
	if (!svcRec->pdu.data) {
		svcRec->pdu.data = (char *)malloc(SDP_PDU_CHUNK_SIZE);
		if (!svcRec->pdu.data)
			return;
		svcRec->pdu.size = SDP_PDU_CHUNK_SIZE;
	}
	svcRec->pdu.length = 0;
	memset(svcRec->pdu.data, 0, svcRec->pdu.size);
	s_list_foreach(attrList, sdp_gen_attr_pdu, &svcRec->pdu);
	//sdp_set_seq_length_pdu(&svcRec->pdu);
#ifdef CONFIG_AFFIX_DEBUG
	sdp_print_pdu(&svcRec->pdu);
#endif
}

sdpdata_t *sdp_put_u8(uint8_t val)
{
	return sdp_create_data(SDP_DTD_UINT8, &val);
}

sdpdata_t *sdp_append_u8(sdpdata_t *seq, uint8_t val)
{
	return sdp_append_data(seq, sdp_put_u8(val));
}

uint8_t sdp_get_u8(sdpdata_t *data)
{
	return data->value.uint8;
}

sdpdata_t *sdp_put_i8(int8_t val)
{
	return sdp_create_data(SDP_DTD_INT8, &val);
}

sdpdata_t *sdp_append_i8(sdpdata_t *seq, int8_t val)
{
	return sdp_append_data(seq, sdp_put_i8(val));
}

int8_t sdp_get_i8(sdpdata_t *data)
{
	return data->value.int8;
}

sdpdata_t *sdp_put_bool(int8_t val)
{
	return sdp_create_data(SDP_DTD_ABOOL, &val);
}

sdpdata_t *sdp_append_bool(sdpdata_t *seq, int8_t val)
{
	return sdp_append_data(seq, sdp_put_bool(val));
}

int8_t sdp_get_bool(sdpdata_t *data)
{
	return data->value.int8;
}

sdpdata_t *sdp_put_u16(uint16_t val)
{
	return sdp_create_data(SDP_DTD_UINT16, &val);
}

sdpdata_t *sdp_append_u16(sdpdata_t *seq, uint16_t val)
{
	return sdp_append_data(seq, sdp_put_u16(val));
}

uint16_t sdp_get_u16(sdpdata_t *data)
{
	return data->value.uint16;
}

sdpdata_t *sdp_put_i16(int16_t val)
{
	return sdp_create_data(SDP_DTD_INT16, &val);
}

sdpdata_t *sdp_append_i16(sdpdata_t *seq, int16_t val)
{
	return sdp_append_data(seq, sdp_put_i16(val));
}

int16_t sdp_get_i16(sdpdata_t *data)
{
	return data->value.int16;
}

sdpdata_t *sdp_put_u32(uint32_t val)
{
	return sdp_create_data(SDP_DTD_UINT32, &val);
}

sdpdata_t *sdp_append_u32(sdpdata_t *seq, uint32_t val)
{
	return sdp_append_data(seq, sdp_put_u32(val));
}

uint32_t sdp_get_u32(sdpdata_t *data)
{
	return data->value.uint32;
}

sdpdata_t *sdp_put_i32(int32_t val)
{
	return sdp_create_data(SDP_DTD_INT32, &val);
}

sdpdata_t *sdp_append_i32(sdpdata_t *seq, int32_t val)
{
	return sdp_append_data(seq, sdp_put_i32(val));
}

int32_t sdp_get_i32(sdpdata_t *data)
{
	return data->value.int32;
}

sdpdata_t *sdp_put_u64(uint64_t val)
{
	return sdp_create_data(SDP_DTD_UINT64, &val);
}

sdpdata_t *sdp_append_u64(sdpdata_t *seq, uint64_t val)
{
	return sdp_append_data(seq, sdp_put_u64(val));
}

uint64_t sdp_get_u64(sdpdata_t *data)
{
	return data->value.uint64;
}

sdpdata_t *sdp_put_i64(int64_t val)
{
	return sdp_create_data(SDP_DTD_INT64, &val);
}

sdpdata_t *sdp_append_i64(sdpdata_t *seq, int64_t val)
{
	return sdp_append_data(seq, sdp_put_i64(val));
}

int64_t sdp_get_i64(sdpdata_t *data)
{
	return data->value.int64;
}

sdpdata_t *sdp_put_u128(uint128_t *val)
{
	return sdp_create_data(SDP_DTD_UINT128, val);
}

sdpdata_t *sdp_append_u128(sdpdata_t *seq, uint128_t *val)
{
	return sdp_append_data(seq, sdp_put_u128(val));
}

uint128_t *sdp_get_u128(sdpdata_t *data)
{
	return &data->value.uint128;
}

sdpdata_t *sdp_put_i128(int128_t *val)
{
	return sdp_create_data(SDP_DTD_INT128, val);
}

sdpdata_t *sdp_append_i128(sdpdata_t *seq, int128_t *val)
{
	return sdp_append_data(seq, sdp_put_i128(val));
}

int128_t *sdp_get_i128(sdpdata_t *data)
{
	return &data->value.int128;
}

sdpdata_t *sdp_put_uuid(uuid_t *val)
{
	return sdp_create_data(SDP_DTD_AUUID, val);
}

sdpdata_t *sdp_append_uuid(sdpdata_t *seq, uuid_t *val)
{
	return sdp_append_data(seq, sdp_put_uuid(val));
}

uuid_t *sdp_get_uuid(sdpdata_t *data)
{
	return &data->value.uuid;
}

sdpdata_t *sdp_put_uuid16(uint16_t val)
{
	return sdp_create_data(SDP_DTD_UUID16, &val);
}

sdpdata_t *sdp_append_uuid16(sdpdata_t *seq, uint16_t val)
{
	return sdp_append_data(seq, sdp_put_uuid16(val));
}

sdpdata_t *sdp_put_uuid32(uint32_t val)
{
	return sdp_create_data(SDP_DTD_UUID32, &val);
}

sdpdata_t *sdp_append_uuid32(sdpdata_t *seq, uint32_t val)
{
	return sdp_append_data(seq, sdp_put_uuid32(val));
}

sdpdata_t *sdp_put_uuid128(uint128_t *val)
{
	return sdp_create_data(SDP_DTD_UUID128, val);
}

sdpdata_t *sdp_append_uuid128(sdpdata_t *seq, uint128_t *val)
{
	return sdp_append_data(seq, sdp_put_uuid128(val));
}

sdpdata_t *sdp_put_str(char *str)
{
	return sdp_create_data(SDP_DTD_STR8, str);
}

sdpdata_t *sdp_append_str(sdpdata_t *seq, char *str)
{
	return sdp_append_data(seq, sdp_put_str(str));
}

char *sdp_get_str(sdpdata_t *data)
{
	return data->value.stringPtr;
}

sdpdata_t *sdp_put_url(char *url)
{
	return sdp_create_data(SDP_DTD_URL8, url);
}

sdpdata_t *sdp_append_url(sdpdata_t *seq, char *str)
{
	return sdp_append_data(seq, sdp_put_url(str));
}

char *sdp_get_url(sdpdata_t *data)
{
	return data->value.stringPtr;
}

sdpdata_t *sdp_create_seq(void)
{
	return sdp_create_data(SDP_DTD_SEQ8, NULL);
}

sdpdata_t *sdp_append_seq(sdpdata_t *seq)
{
	return sdp_append_data(seq, sdp_create_seq());
}

slist_t *sdp_get_seq(sdpdata_t *data)
{
	return data->value.dataSeq;
}

sdpdata_t *sdp_create_alt(void)
{
	return sdp_create_data(SDP_DTD_ALT8, NULL);
}

sdpdata_t *sdp_append_alt(sdpdata_t *seq)
{
	return sdp_append_data(seq, sdp_create_alt());
}


#ifdef CONFIG_AFFIX_DEBUG

void sdp_print_pdu(sdppdu_t *pdu)
{
	if (pdu->length == 0)
		DBPRT("NULL buffer");
	BTDUMP(pdu->data, pdu->length);
}

void sdp_print_int(sdpdata_t *pData)
{
	if (pData != NULL)
		DBPRT("Integer : 0x%x", pData->value.uint32);
}

void sdp_print_str(sdpdata_t *pData)
{
	if (pData != NULL)
		DBPRT("String : %s", pData->value.stringPtr);
}

void sdp_print_seq(slist_t *dataSeq)
{
	sdpdata_t		*data;

	for (;dataSeq; dataSeq = s_list_next(dataSeq)) {
		data = dataSeq->data;
		sdp_print_data(data);
	}
}

void sdp_print_data(sdpdata_t *data)
{
	uint8_t dtd;

	dtd = data->dtd;
	switch (dtd) {
		case SDP_DTD_ABOOL :
		case SDP_DTD_UINT8 :
		case SDP_DTD_UINT16 :
		case SDP_DTD_UINT32 :
		case SDP_DTD_UINT64 :
		case SDP_DTD_UINT128 :
		case SDP_DTD_INT8 :
		case SDP_DTD_INT16 :
		case SDP_DTD_INT32 :
		case SDP_DTD_INT64 :
		case SDP_DTD_INT128 :
			sdp_print_int(data);
			break;

		case SDP_DTD_UUID16 :
		case SDP_DTD_UUID32 :
		case SDP_DTD_UUID128 :
			DBPRT("uuid_t");
			sdp_print_uuid(&data->value.uuid);
			break;

		case SDP_DTD_STR8 :
		case SDP_DTD_STR16 :
		case SDP_DTD_STR32 :
			DBPRT("Text : %s", data->value.stringPtr);
			break;
		case SDP_DTD_URL8 :
		case SDP_DTD_URL16 :
		case SDP_DTD_URL32 :
			DBPRT("URL : %s", data->value.stringPtr);
			break;

		case SDP_DTD_SEQ8 :
		case SDP_DTD_SEQ16 :
		case SDP_DTD_SEQ32 :
			sdp_print_seq(data->value.dataSeq);
			break;

		case SDP_DTD_ALT8 :
		case SDP_DTD_ALT16 :
		case SDP_DTD_ALT32 :
			DBPRT("Data Sequence Alternates");
			sdp_print_seq(data->value.dataSeq);
			break;
	}
}

void sdp_print_attr(void * value, void * userData)
{
	sdpdata_t *data = NULL;
	uint16_t attrId;

	DBPRT("[%s-%d] : ", __FUNCTION__, __LINE__);
	data = (sdpdata_t *)value;
	attrId = data->attrId;
	DBPRT("=====================================");
	DBPRT("ATTRIBUTE IDENTIFIER : 0x%x", attrId);
	DBPRT("ATTRIBUTE VALUE PTR : 0x%x", (uint32_t)value);
	if (data != NULL) {
		sdp_print_data(data);
	} else
		DBPRT("NULL value");
	DBPRT("=====================================");
}

void sdp_print_svc(slist_t *svcAttrList)
{
	s_list_foreach(svcAttrList, sdp_print_attr, NULL);
}

#endif	// CONFIG_AFFIX_DEBUG


