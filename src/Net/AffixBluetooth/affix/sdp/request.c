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
   $Id: request.c,v 1.50 2003/11/13 19:23:16 litos Exp $

   Contains the SDP service discovery client request handling code.

   Fixes:
   		Dmitry Kasatkin		- Continuation mechanism, MTU support
*/

#include <affix/config.h>

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>
#include "servicedb.h"
#include "utils.h"
#include "attr.h"
#include "cstate.h"

extern void sdp_update_time(void);
extern slist_t	*serviceList;

/*
 ** Search/Attribute element sequence extractor. Builds
 ** a slist_t whose elements are those found in the 
 ** sequence. The data type of elements found in the
 ** sequence (now populated in slist_t) is returned
 ** in the reference pDataType
 */
int sdp_extr_data_seq(char *pduBuffer, slist_t **svcReqSeq, uint8_t *pDataType)
{
	int	status, bytesScanned;
	char	*pdata;
	uint8_t	seqType;
	int 	length = 0;
	short	numberOfElements;
	int	seqLength;
	slist_t	*pSeq;
	uint8_t	dataType;

	bytesScanned = 0;
	pdata = sdp_extr_seq_dtd(pduBuffer, &seqType, &length, &bytesScanned);
	DBPRT("Seq type : %d", seqType);
	if (!pdata || (SDP_DTD_TYPE(seqType) != SDP_DTD_SEQ)) {
		BTERROR("Unknown seq type");
		return -1;
	}
	DBPRT("Data size : %d", length);
	numberOfElements = 0;
	seqLength = 0; 
	pSeq = NULL;
	for (;;) {
		char	*pElem = NULL;
		int	localSeqLength = 0;

		dataType = __get_u8(pdata);
		DBPRT("Data type : %d", dataType);
		switch (dataType) {
			case SDP_DTD_UINT16 :
				pdata += sizeof(uint8_t);
				seqLength += sizeof(uint8_t);
				pElem = (char *)malloc(sizeof(uint16_t));
				if (!pElem)
					break;
				*(uint16_t *)pElem = ntohs(__get_u16(pdata));
				pdata += sizeof(uint16_t);
				seqLength += sizeof(uint16_t);
				break;
			case SDP_DTD_UINT32 :
				pdata += sizeof(uint8_t);
				seqLength += sizeof(uint8_t);
				pElem = (char *)malloc(sizeof(uint32_t));
				if (!pElem)
					break;
				*(uint32_t *)pElem = ntohl(__get_u32(pdata));
				pdata += sizeof(uint32_t);
				seqLength += sizeof(uint32_t);
				break;
			case SDP_DTD_UUID16 :
			case SDP_DTD_UUID32 :
			case SDP_DTD_UUID128 :
				pElem = (char *)malloc(sizeof(uuid_t));
				if (!pElem)
					break;
				status = __sdp_extr_uuid(pdata, (uuid_t *)pElem, &localSeqLength);
				if (status) {
					free(pElem);
					return status;
				}
				seqLength += localSeqLength;
				pdata += localSeqLength;
				break;
		}
		if (!pElem)
			continue;
		s_list_append(&pSeq, pElem);
		numberOfElements++;
		DBPRT("No of elements : %d", numberOfElements);
		if (seqLength == length)
			break;
		else if (seqLength > length)
			return -1;
	}
	*svcReqSeq = pSeq;
	bytesScanned += seqLength;
	*pDataType = dataType;
	return bytesScanned;
}



/*
 ** Service search request PDU. This methods extracts the "search pattern"
 ** which is a seqquence of UUIDs calls the matching function
 ** to find matching services
 */
int _sdp_search_req(sdp_request_t *requestPdu, sdppdu_t *pdu)
{
	int		status = 0, i;
	char		*pdata;
	int		bytesScanned = 0;
	slist_t		*searchPattern = NULL;
	int		maxExpectedServiceRecordCount, realServiceRecordCount;
	uint8_t		dataType;
	sdp_cstate_t	*pCState = NULL;
	char		*pCacheBuffer = NULL;
	int		handleSize = 0;
	long		cStateId = -1;
	short		responseCount = 0;
	short		*pTotalRecordCount;
	short		*pCurrentRecordCount;
	int		MTU;

	pdata = requestPdu->data + sizeof(sdp_hdr_t);
	bytesScanned = sdp_extr_data_seq(pdata, &searchPattern, &dataType);
	if ((bytesScanned < 0) || ((bytesScanned + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(sdp_hdr_t)) > requestPdu->len)){ 
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	pdata += bytesScanned;
	maxExpectedServiceRecordCount = ntohs(__get_u16(pdata));
	DBPRT("Expected count: %d", maxExpectedServiceRecordCount);
	DBPRT("Bytes scanned : %d", bytesScanned);
	pdata += sizeof(uint16_t);
	//bytesScanned += sizeof(uint16_t); //Fix

	/*
	 ** Check if continuation state exists, if yes attempt
	 ** to get response remainder from cache, else send error
	 */
	pCState = sdp_get_cstate(pdata);
	if ((pCState) && ((pCState->length + bytesScanned + sizeof(uint16_t) + sizeof(sdp_hdr_t)) > requestPdu->len)){
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	
	MTU = requestPdu->mtu - sizeof(sdp_hdr_t) - sizeof(uint16_t) - sizeof(uint16_t) - sizeof(sdp_cstate_t);
	realServiceRecordCount = btmin(maxExpectedServiceRecordCount, (MTU >> 2));
	DBPRT("MTU: %d, realrec: %d", MTU, realServiceRecordCount);
	/*
	 ** Make space in the response buffer for total
	 ** and current record counts
	 */
	pdata = pdu->data;

	/*
	 ** Total service record count = 0
	 */
	pTotalRecordCount = (short *)pdata;
	__put_u16(pdata, 0);
	pdata += sizeof(uint16_t);
	pdu->length += sizeof(uint16_t);

	/*
	 ** current service record count = 0
	 */
	pCurrentRecordCount = (short *)pdata;
	__put_u16(pdata, 0);
	pdata += sizeof(uint16_t);
	pdu->length += sizeof(uint16_t);

	if (pCState == NULL) {
		/*
		 ** For every element in the sdpsvc_t DB,
		 ** do a pattern search
		 */
		int index = 0;
		handleSize = 0;
		for (;;) {
			void * data;
			sdpsvc_t *svcRec;

			data = s_list_nth_data(serviceList, index);
			if (data != NULL) {
				svcRec = (sdpsvc_t *)data;
				DBPRT("Checking svcRec : 0x%x", svcRec->serviceRecordHandle);
				if (sdp_match_uuid(searchPattern, svcRec->targetPattern)) {
					responseCount++; 
					__put_u32(pdata, htonl(svcRec->serviceRecordHandle));
					pdata += sizeof(uint32_t);
					handleSize += sizeof(uint32_t);
				}
				index++;
			} else
				break;
		}
		DBPRT("Match count : %d", responseCount);
		/*
		 ** Add "handleSize" to pdu buffer size
		 */
		pdu->length += handleSize;
		__put_u16(pTotalRecordCount,  htons(responseCount));
		__put_u16(pCurrentRecordCount, htons(responseCount));

		if (responseCount > realServiceRecordCount) {
			/* 
			 ** We need to cache the response and generate a 
			 ** continuation state 
			 */
			cStateId = sdp_add_rsp_cscache(requestPdu->fd, pdu);
			/*
			 ** Subtract the handleSize since we now send only
			 ** a subset of handles
			 */
			pdu->length -= handleSize;
		} else {
			/*
			 ** NULL continuation state
			 */
			sdp_set_cstate(pdu, NULL);
		}
	}

	/*
	 ** Under both the conditions below, the response buffer is not
	 ** built up yet
	 */
	if ((pCState != NULL) || (cStateId != -1)) {
		short lastIndex = 0;

		if (pCState != NULL) {
			sdppdu_t *pCache;
			/*
			 ** Get the previous sdp_cstate_t and obtain
			 ** the cached response
			 */
			pCache = sdp_get_rsp_cscache(requestPdu->fd, pCState);
			if (!pCache) {
				status = -1;
				goto exit;
			}
			pCacheBuffer = pCache->data;
			responseCount = ntohs(__get_u16(pCacheBuffer));
			/*
			 ** Get the index of the last sdpsvc_t sent
			 */
			lastIndex = pCState->value.lastIndexSent;
		} else {
			pCacheBuffer = pdu->data;
			lastIndex = 0;
		}
		/*
		 ** Set the local buffer pointer to beyond the
		 ** current record count. And increment the cached
		 ** buffer pointer to beyond the counters ..
		 */
		pdata = (char *)pCurrentRecordCount + sizeof(uint16_t);

		/*
		 ** There is the toatalCount and the currentCount 
		 ** fields to increment beyond
		 */
		pCacheBuffer += (2 * sizeof(uint16_t));

		if (pCState == NULL) {
			handleSize = realServiceRecordCount << 2; //sizeof(uint32_t); 
			i = realServiceRecordCount;
		} else {
			handleSize = 0;
			for (i = lastIndex; (i-lastIndex) < realServiceRecordCount &&
					i < responseCount; i++) {
				__put_u32(pdata, __get_u32(((uint32_t*)pCacheBuffer) + i));
				pdata += sizeof(uint32_t);
				handleSize += sizeof(uint32_t);
			}
		}

		/*
		 ** Add "handleSize" to pdu buffer size
		 */
		pdu->length += handleSize;
		__put_u16(pTotalRecordCount,  htons(responseCount));
		__put_u16(pCurrentRecordCount, htons(i-lastIndex));

		if (i == responseCount) {
			/*
			 ** Set "null" continuationState & and remove it from cache
			 */
			sdp_set_cstate(pdu, NULL);
			sdp_del_rsp_cscache(requestPdu->fd, pCState);
		} else {
			/*
			 ** There is more to it .. set the lastIndexSent to
			 ** the new value and move on ..
			 */
			sdp_cstate_t newState;

			DBPRT("Setting non-NULL sdp_cs_t");
			memset(&newState, 0, sizeof(sdp_cstate_t));
			if (pCState != NULL) {
				memcpy(&newState, pCState, sizeof(sdp_cstate_t));
				newState.value.lastIndexSent = i;
			} else {
				newState.timestamp = cStateId;
				newState.value.lastIndexSent = i;
			}
			sdp_set_cstate(pdu, &newState);
		}
	}
exit:
	if (searchPattern)
		s_list_destroy(&searchPattern);
	return status;
}


/*
 ** This function generates the PDU form of a service record for
 ** a specified set of attributes and not the entire set. This is
 ** driven by a service discovery client which asks for attributes
 ** of a service record by speicifying a subset of attribute 
 ** identifiers
 */
void sdp_gen_svc_pdu_by_attr(sdpsvc_t *svcRec, uint16_t attrId, sdppdu_t *attrSubsetPDUForm)
{
	sdpdata_t *pData;

	pData = (sdpdata_t*)sdp_get_attr(svcRec, attrId);
	if (!pData || !pData->pdu.data || !pData->pdu.length)
		return;
	sdp_append_pdu(attrSubsetPDUForm, &pData->pdu);
}


/*
 ** Extract attribute identifiers from the request PDU.
 ** Clients could request a subset of attributes (by id)
 ** from a service record, instead of the whole set. The
 ** requested identifiers are present in the PDU form of
 ** the request
 */
int sdp_gen_svc_pdu_by_attrs(
		sdpsvc_t *svcRec,
		slist_t *attrSeq,
		uint8_t attrDataType,
		sdppdu_t *pdu,
		int maxExpectedAttrSize
		)
{
	int 	status = 0;
	int 	attrLength;
	int 	index = 0;

	if (!svcRec)
		return SDP_ERR_SERVICE_RECORD_HANDLE;

	if (!attrSeq) {
		DBPRT("Attribute sequence is NULL");
		return 0;
	}
#ifdef CONFIG_AFFIX_DEBUG
	if (attrSeq != NULL) {
		DBPRT("Entries in attr seq : %d", s_list_length(attrSeq));
	}
	DBPRT("AttrDataType : %d", attrDataType);
#endif

	/*
	 ** Set the maximum expected size before hand
	 */
	pdu->size = maxExpectedAttrSize;

	/*
	 ** This is a request for the entire attribute range
	 */
	if (svcRec->pdu.data == NULL) {
		DBPRT("Generating svc rec pdu form");
		sdp_gen_svc_pdu(svcRec);
	}

	attrLength = 0;

	if (attrDataType == SDP_DTD_UINT16) {
		void	*pAttr;
		// Fix for empty AttributeLists (Litos) 13-11-2003
		int currentlength = pdu->length;
		// End litos fix
		for (index = 0; (pAttr = s_list_nth_data(attrSeq, index)); index++) {
			uint16_t	attrId = 0;

			attrId = *(uint16_t *)pAttr;
			DBPRT("Attr id requested : 0x%x", attrId);
			sdp_gen_svc_pdu_by_attr(svcRec, attrId, pdu);
		}
		// Fix for empty AttributeLists (Litos) 13-11-2003
	
		if (currentlength == pdu->length){
		//No attributes to be returned. So we have to return an empty sequence !!!!
			DBPRT("Creating empty Data Sequence\n");
			__put_u8(pdu->data,SDP_DTD_SEQ8);
			__put_u8((pdu->data+sizeof(uint8_t)),0); //Size of the empty sequence is 0
			pdu->length+=sizeof(uint8_t) + sizeof(uint8_t);
			DBPRT("Pdu length: %d\n Pdu size:%d\n", pdu->length,pdu->size);
		}
		// End litos fix
		
	} else if (attrDataType == SDP_DTD_UINT32) {
			uint8_t	completeAttrSetBuilt = 0;
			void	*pAttr;
			for (index = 0; (pAttr = s_list_nth_data(attrSeq, index)); index++) {
				uint32_t 	attrRange = 0;
				uint16_t 	lowIdLim = 0;
				uint16_t 	highIdLim = 0;

				attrRange = *(uint32_t *)pAttr;
				lowIdLim = ((0xffff0000 & attrRange) >> 16);
				highIdLim = 0x0000ffff & attrRange;

				DBPRT("attr range : 0x%x", attrRange);
				DBPRT("Low id : 0x%x", lowIdLim);
				DBPRT("High id : 0x%x", highIdLim);
				if ((lowIdLim == 0x0000) &&
						(highIdLim == 0xffff) &&
						(completeAttrSetBuilt == 0) &&
						(svcRec->pdu.length <= pdu->size)) {
					completeAttrSetBuilt = 1;
					/*
					 ** Create a copy of it
					 */
					DBPRT("Copy attr pdu form : %d", svcRec->pdu.length);
					memcpy(pdu->data, svcRec->pdu.data, 
							svcRec->pdu.length);
					pdu->length = svcRec->pdu.length;
				} else {
					/*
					 ** Not the entire range of attributes, pick and choose
					 ** attributes requested
					 */
					// Fix for empty AttributeLists (Litos) 13-11-2003
					int currentlength = pdu->length;
					// End litos fix
			
					int attrId;
					for (attrId = lowIdLim; attrId <= highIdLim; attrId++)
						sdp_gen_svc_pdu_by_attr(svcRec, attrId, pdu);

					// Fix for empty AttributeLists (Litos) 13-11-2003
					if (currentlength == pdu->length){
						//No attributes to be returned. So we have to return an empty sequence !!!!
						__put_u8(pdu->data,SDP_DTD_SEQ8);
						__put_u8((pdu->data+sizeof(uint8_t)),0); //Size of the empty sequence is 0
						pdu->length+=sizeof(uint8_t) + sizeof(uint8_t);
					}
					// End litos fix
		
				}
			}
	} else {
		status = SDP_ERR_REQUEST_SYNTAX;
		BTERROR("Unexpected data type : 0x%x", attrDataType);
		BTERROR("Expect uint16_t or uint32_t");
	}
	return status;
}


/*
 ** A request for the attributes of a service record.
 ** First check if the service record (specified by
 ** service record handle) exists, then call the attribute
 ** streaming function
 */
int _sdp_attr_req(sdp_request_t *requestPdu, sdppdu_t *pdu)
{
	char		*pdata;
	int		status = 0;
	sdpsvc_t	*svcRec;
	int		maxResponseSize;
	uint32_t	svcRecHandle;
	sdp_cstate_t	*pCState;
	char		*pResponse = NULL;
	short 		continuationStateSize = 0;
	slist_t		*attrSeq = NULL;
	uint8_t		attrDataType = 0;
	int		bytesScanned = 0;

	pdata = requestPdu->data + sizeof(sdp_hdr_t);
	svcRecHandle = ntohl(__get_u32(pdata));
	pdata += sizeof(uint32_t);
	maxResponseSize = ntohs(__get_u16(pdata));
	pdata += sizeof(uint16_t);

	/*
	 ** Extract the attribute list
	 */
	bytesScanned = sdp_extr_data_seq(pdata, &attrSeq, &attrDataType);
	/*
	 * Sencond condition checks : 
	 * 			SDP Header: PDU ID (1 byte) + Transaction ID (2 bytes) + Parameter Length (2 bytes)
	 * 			+
	 * 			Parameter 1: service record handle (4 bytes)
	 * 			+
	 * 			Parameter 2: maximum attribute byte count (2 bytes)
	 *			+
	 *			Paramenter 3: Attribute ID list (variable length) 
	 *			+
	 *			Paramenter 4: Continuation State (1 byte) (We suppose the minimum continuation state size here)
	 *			
	 *			bigger than (>)
	 *			
	 *			Parameter Length value.
	 */
	if ((bytesScanned < 0) || ((sizeof(sdp_hdr_t) + sizeof(uint32_t) + sizeof(uint16_t) + bytesScanned+ sizeof(uint8_t)) > requestPdu->len)){ 
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	pdata += bytesScanned;

	/*
	 ** Check if continuation state exists, if yes attempt
	 ** to get response remainder from cache, else send error
	 */
	pCState = sdp_get_cstate(pdata);
	if ((pCState) && ((pCState->length + bytesScanned + sizeof(sdp_hdr_t) + sizeof(uint32_t) + sizeof(uint16_t)) > requestPdu->len)){
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	
	DBPRT("SvcRecHandle : 0x%x", svcRecHandle);
	DBPRT("maxResponseSize : %d", maxResponseSize);

	maxResponseSize = btmin(maxResponseSize,
			(requestPdu->mtu-sizeof(sdp_hdr_t)-sizeof(uint16_t)-sizeof(sdp_cstate_t)));
	/*
	 * pull header for AttributeList byte count
	 */
	pdu->data += sizeof(uint16_t);
	pdu->size -= sizeof(uint16_t);

	if (pCState) {
		sdppdu_t *pCache = NULL;
		short bytesSent = 0;

		pCache = sdp_get_rsp_cscache(requestPdu->fd, pCState);
		DBPRT("Obtained cached response : %p", pCache);
		if (pCache != NULL) {
			pResponse = pCache->data;
			bytesSent = btmin(maxResponseSize, 
					(pCache->length - pCState->value.maxBytesSent));
			memcpy(pdu->data, (pResponse + pCState->value.maxBytesSent), 
					bytesSent);
			pdu->length += bytesSent;
			pCState->value.maxBytesSent += bytesSent;
			DBPRT("Response size : %d sending now : %d bytes sent so far : %d", 
					pCache->length, bytesSent, pCState->value.maxBytesSent);
			if (pCState->value.maxBytesSent == pCache->length) {
				/* remove from cache */
				continuationStateSize = sdp_set_cstate(pdu, NULL);
				sdp_del_rsp_cscache(requestPdu->fd, pCState);
			} else
				continuationStateSize = sdp_set_cstate(pdu, pCState);
		} else {
			status = -1;
			BTERROR("NULL cache buffer and non-NULL continuation state");
		}
	} else {
		svcRec = sdp_find_svc(svcRecHandle);
		//SDPXXX: is pdata correct ?????
		status = sdp_gen_svc_pdu_by_attrs(svcRec, attrSeq, attrDataType, pdu, pdu->size);
		//BTINFO("length: %d, max: %d\n", pdu->length, maxResponseSize);
		if (pdu->length > maxResponseSize) {
			sdp_cstate_t newState;

			memset((char *)&newState, 0, sizeof(sdp_cstate_t));
			newState.timestamp = sdp_add_rsp_cscache(requestPdu->fd, pdu);
			/*
			 ** Reset the buffer size to the maximum expected and
			 ** set the sdp_cstate_t
			 */
			DBPRT("Creating continuation state of size : %d", pdu->length);
			pdu->length = maxResponseSize;
			newState.value.maxBytesSent = maxResponseSize;
			continuationStateSize = sdp_set_cstate(pdu, &newState);
		} else
			continuationStateSize = sdp_set_cstate(pdu, NULL);
	}
	// push header
	pdu->data -= sizeof(uint16_t);
	pdu->size += sizeof(uint16_t);
	if (status != 0) {
		__put_u16(pdu->data, htons(status));
		pdu->length = sizeof(uint16_t);
	} else {
		/*
		 * set attribute list byte count
		 */
		__put_u16(pdu->data, htons(pdu->length - continuationStateSize));
		pdu->length += sizeof(uint16_t);
	}
exit:
	if (attrSeq)
		s_list_destroy(&attrSeq);
	return status;
}


/*
 ** The single request for a combined "service search" and
 ** "attribute extraction"
 */
int _sdp_search_attr_req(sdp_request_t *requestPdu, sdppdu_t *pdu)
{
	int		status = 0;
	char		*pdata;
	int		bytesScanned = 0, bytesScanned2 = 0;
	slist_t		*searchPattern = NULL;
	int		maxExpectedAttrSize;
	uint8_t		dataType;
	sdp_cstate_t	*pCState = NULL;
	char		*pResponse = NULL;
	short		continuationStateSize = 0;
	slist_t		*attrSeq = NULL;
	uint8_t		attrDataType = 0;
	int		index = 0;
	int		responseCount = 0;
	sdppdu_t	localPDUForm;

	DBPRT("In svc srch attr request");
	pdata = requestPdu->data + sizeof(sdp_hdr_t);

	bytesScanned = sdp_extr_data_seq(pdata, &searchPattern, &dataType);
	DBPRT("Bytes scanned : %d", bytesScanned);
	if ((bytesScanned < 0) || ((sizeof(sdp_hdr_t) + bytesScanned + sizeof(uint16_t) + sizeof(uint8_t)) > requestPdu->len)){ 
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	
	pdata += bytesScanned;
	maxExpectedAttrSize = ntohs(__get_u16(pdata));
	pdata += sizeof(uint16_t);
	DBPRT("Max Attr expected : %d", maxExpectedAttrSize);

	/*
	 ** Extract the attribute list
	 */
	bytesScanned2 = sdp_extr_data_seq(pdata, &attrSeq, &attrDataType);
	if ((bytesScanned2 < 0) || ((sizeof(sdp_hdr_t) + bytesScanned + sizeof(uint16_t) + bytesScanned2 + sizeof(uint8_t)) > requestPdu->len)){ 
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	pdata += bytesScanned2;
	
	/*
	 ** Check if continuation state exists, if yes attempt
	 ** to get response remainder from cache, else send error
	 */
	pCState = sdp_get_cstate(pdata);	//this is my Continuation Information
	if ((pCState) && ((pCState->length + bytesScanned + bytesScanned2 + sizeof(sdp_hdr_t) + sizeof(uint16_t)) > requestPdu->len)){
		status = SDP_ERR_REQUEST_SYNTAX;
		goto exit;
	}
	
	localPDUForm.data = malloc(USHRT_MAX);
	if (!localPDUForm.data){
		status = -1;
		goto exit;
	}
	localPDUForm.length = 0;
	localPDUForm.size = USHRT_MAX;

	maxExpectedAttrSize = btmin(maxExpectedAttrSize,
			(requestPdu->mtu-sizeof(sdp_hdr_t)-sizeof(sdp_cstate_t)-sizeof(uint16_t)));
	/*
	 * pull header for AttributeList byte count
	 */
	pdu->data += sizeof(uint16_t);
	pdu->size -= sizeof(uint16_t);

	if (pCState == NULL) {
		/*
		 * No Continuation State -> Create New Response
		 */
		// Litos Fix
		int currentlength = pdu->length;
		// End litos fix
		for (;;) {
			void * data;
			sdpsvc_t *svcRec;

			localPDUForm.length = 0;
			memset(localPDUForm.data, 0, USHRT_MAX);

			data = s_list_nth_data(serviceList, index);
			if (!data)
				break;
			svcRec = (sdpsvc_t *)data;
			if (sdp_match_uuid(searchPattern, svcRec->targetPattern)) {
				responseCount++;
				status = sdp_gen_svc_pdu_by_attrs(svcRec, attrSeq, attrDataType, 
									&localPDUForm, localPDUForm.size);
				DBPRT("Response count : %d", responseCount);
				DBPRT("Local PDU size : %d", localPDUForm.length);
				if (status == -1) {
					DBPRT("Extract attr from svc rec returns err");
					break;
				}
				if (pdu->length + localPDUForm.length < pdu->size) {
					// to be sure no relocations
					sdp_append_pdu(pdu, &localPDUForm);
				} else {
					BTERROR("Relocation needed");
					break;
				}
				DBPRT("Net PDU size : %d", pdu->length);
			}
			index++;
		}
		// Litos fix
		if (currentlength == pdu->length){
			// Check if the response is empty. If it is then we must return an empty DataSequence !
			__put_u8(pdu->data,SDP_DTD_SEQ8);		// DataSequence
			pdu->length += sizeof(uint8_t);
			//NOTE: The size of the DataSequence is set later in the code ! (in this case the value will be 0)
		}
		// End litos fix

		
		if (pdu->length > maxExpectedAttrSize) {
			sdp_cstate_t newState;

			memset((char *)&newState, 0, sizeof(sdp_cstate_t));
			newState.timestamp = sdp_add_rsp_cscache(requestPdu->fd, pdu);
			/*
			 ** Reset the buffer size to the maximum expected and
			 ** set the sdp_cstate_t
			 */
			pdu->length = maxExpectedAttrSize;
			newState.value.maxBytesSent = maxExpectedAttrSize;
			continuationStateSize = sdp_set_cstate(pdu, &newState);
		} else
			continuationStateSize = sdp_set_cstate(pdu, NULL);
	} else {
		/*
		 * Continuation State exists -> get from cache
		 */
		sdppdu_t *pCache;
		uint16_t bytesSent = 0;

		/*
		 ** Get the cached response from the buffer
		 */
		pCache = sdp_get_rsp_cscache(requestPdu->fd, pCState);
		if (pCache != NULL) {
			pResponse = pCache->data;
			bytesSent = btmin(maxExpectedAttrSize, 
					(pCache->length - pCState->value.maxBytesSent));
			memcpy(pdu->data, (pResponse + pCState->value.maxBytesSent), 
					bytesSent);
			pdu->length += bytesSent;
			pCState->value.maxBytesSent += bytesSent;
			if (pCState->value.maxBytesSent == pCache->length) {
				/* remove response from cache as well */
				continuationStateSize = sdp_set_cstate(pdu, NULL);
				sdp_del_rsp_cscache(requestPdu->fd, pCState);
			} else
				continuationStateSize = sdp_set_cstate(pdu, pCState);
		} else {
			status = -1;
			DBPRT("Non null continuation state, but null cache buffer");
		}
	}
	if ((responseCount == 0) && (pCState == NULL)) {
		//we've not found anything - create an empty sequence
		sdp_append_pdu(pdu, &localPDUForm);
	}
	// push header
	pdu->data -= sizeof(uint16_t);
	pdu->size += sizeof(uint16_t);
	if (status != 0) {
		__put_u16(pdu->data, htons(status));
		pdu->length = sizeof(uint16_t);
	} else {
		/*
		 * set attribute list byte count
		 */
		__put_u16(pdu->data, htons(pdu->length - continuationStateSize));
		pdu->length += sizeof(uint16_t);
	}
	free(localPDUForm.data);
exit:
	if (searchPattern)
		s_list_destroy(&searchPattern);
	if (attrSeq)
		s_list_destroy(&attrSeq);
	return status;
}

/* 
  MAIN in service registraion
 */
sdpsvc_t *sdp_srv_extr_pdu(char *pdata, uint32_t handleExpected, int *bytesScanned)
{
	int		localExtractedLength = 0;
	char		*localSeqPtr = NULL;
	uint8_t		dtd;
	int		sequenceLength = 0;
	sdpsvc_t	*svcRec = NULL;

	localSeqPtr = sdp_extr_seq_dtd(pdata, &dtd, &sequenceLength, bytesScanned);

	/*
	 ** Check if the PDU already has a service record handle,
	 ** int which case this may be a update request
	 */
	if (handleExpected != 0xffffffff)
		svcRec = sdp_find_svc(handleExpected);
	
	if (svcRec == NULL) {
		svcRec = sdp_create_svc();
		svcRec->attributeList = NULL;
	}
	localExtractedLength = sdp_extr_attrs(svcRec, localSeqPtr, sequenceLength);
	if (localExtractedLength >= 0)
		*bytesScanned += localExtractedLength;
	return svcRec;
}

/*
 ** Request to create a brand new service record
 ** The newly created service record is added to the
 ** service repository
 * MAIN in service registraion
 */
int _sdp_register_service(sdp_request_t *sdpReq, sdppdu_t *response)
{
	char		*pdata;
	int 		bytesScanned = 0;
	sdpsvc_t	*svcRec;
	sdp_hdr_t 	*sdpPduHeader;
	int 		length = 0;
	sdpdata_t 	*browseAttrData = NULL;

	/*
	 ** Extract buffer contents
	 */
	sdpPduHeader = (sdp_hdr_t *)sdpReq->data;
	pdata = sdpReq->data + sizeof(sdp_hdr_t);

	/*
	 ** Extract and save image of PDU, since we need
	 ** it when clients request thsi attribute
	 */
	svcRec = sdp_srv_extr_pdu(pdata, 0xffffffff, &bytesScanned);
	if (!svcRec)
		return SDP_ERR_REQUEST_SYNTAX;

	svcRec->serviceRecordHandle = (uint32_t)svcRec;
	sdp_add_svc(svcRec);
	sdp_append_attr(svcRec, SDP_ATTR_SERVICE_RECORD_HANDLE, sdp_put_u32(svcRec->serviceRecordHandle));
	/*
	 ** Also check if the browse group descriptor is NULL,
	 ** ensure that the record belongs to the ROOT group
	 */
	if ((browseAttrData = sdp_get_attr(svcRec, SDP_ATTR_BROWSE_GROUP_LIST)) == NULL)
		sdp_add_uuid16_to_pattern(svcRec, SDP_UUID_PUBLIC_BROWSE_GROUP);

	sdp_gen_svc_pdu(svcRec);
	sdp_update_time();

	/*
	 ** Build a response buffer
	 */
	pdata = response->data;
	__put_u32(pdata, htonl(svcRec->serviceRecordHandle));
	DBPRT("Sending back svcRecHandle : 0x%x", svcRec->serviceRecordHandle);
	length += sizeof(uint32_t);
	response->length = length;
	
	/* set owner of the service */
	svcRec->fd = sdpReq->fd;
	return 0;
}


/*
 ** Register a service record (this means updating the
 ** contents of a newly created service record or updating
 ** the contents of an existing record)
 */
int _sdp_update_service(sdp_request_t *sdpReq, sdppdu_t *response)
{
	char		*pdata;
	uint32_t	svcRecHandle;
	sdpsvc_t	*svcRecNew = NULL;
	sdpsvc_t	*svcRecOld = NULL;
	sdp_hdr_t	*sdpPduHeader;
	int 		bytesScanned = 0;

	/*
	 ** Extract service record handle
	 */
	sdpPduHeader = (sdp_hdr_t *)sdpReq->data;
	pdata = sdpReq->data + sizeof(sdp_hdr_t);
	svcRecHandle = ntohl(__get_u32(pdata));
	DBPRT("Svc Rec Handle : 0x%x\n", svcRecHandle);
	pdata += sizeof(uint32_t);

	svcRecOld = sdp_find_svc(svcRecHandle);
	DBPRT("SvcRecOld : 0x%x\n", (uint32_t)svcRecOld);
	if (!svcRecOld)
		return SDP_ERR_SERVICE_RECORD_HANDLE;
		
	svcRecNew = sdp_srv_extr_pdu(pdata, svcRecHandle, &bytesScanned);
	if ((svcRecNew != NULL) && 
			(svcRecHandle == svcRecNew->serviceRecordHandle)) {
		sdp_gen_svc_pdu(svcRecNew);
		sdp_update_time();
	} else {
		DBPRT("SvcRecHandle : 0x%x\n", svcRecHandle);
		DBPRT("SvcRecHandleNew : 0x%x\n", svcRecNew->serviceRecordHandle);
		DBPRT("SvcRecNew : 0x%x\n", (uint32_t)svcRecNew);
		DBPRT("SvcRecOld : 0x%x\n", (uint32_t)svcRecOld);
		DBPRT("Failure to update, restore old value\n");
		if (svcRecNew)
			sdp_free_svc(svcRecNew);
		return SDP_ERR_REQUEST_SYNTAX;
	}
	return 0;
}


/*
 ** Remove a registered service record
 */
int _sdp_delete_service(sdp_request_t *sdpReq, sdppdu_t *response)
{
	char *pdata;
	int status = 0;
	uint32_t svcRecHandle;
	sdpsvc_t *svcRec;

	/*
	 ** Extract service record handle
	 */
	pdata = sdpReq->data + sizeof(sdp_hdr_t);
	svcRecHandle = ntohl(__get_u32(pdata));
	pdata += sizeof(uint32_t);

	svcRec = sdp_find_svc(svcRecHandle);
	if (!svcRec) {
		DBPRT("Could not find record : 0x%x\n", svcRecHandle);
		return SDP_ERR_SERVICE_RECORD_HANDLE;
	}
	status = sdp_del_svc(svcRecHandle);
	sdp_free_svc(svcRec);
	if (status == 0)
		sdp_update_time();
	return status;
}


/*
 ** Top level request processor. Calls the appropriate processing
 ** function based on request type. Handles service registration
 ** client requests also
 */
int _sdp_process_req(sdp_request_t *request)
{
	int			status;
	sdp_hdr_t		*requestPduHeader;
	sdp_hdr_t		*responsePduHeader = NULL;
	sdppdu_t		responsePdu;
	char			*bufferStart = NULL;
	int			len;
	struct sdp_error	err_rsp;

	DBPRT("In process req");
	requestPduHeader = (sdp_hdr_t*)request->data;

	len = sizeof(sdp_hdr_t) + ntohs(requestPduHeader->paramLength);
	if (len != request->len) {
		status = SDP_ERR_PDU_SIZE;
		goto exit;
	}

/*
 * Test HERE if the data element sequence size match with the total message length 
 * If does not return an SDP_ERROR with status SDP_ERR_REQUEST_SYNTAX
 * NOTE: Check in the _sdp_* how the length of the request with current length of the message processed if bigger then SDP_ERR_REQUEST_SINTAX
 */
	
	bufferStart = malloc(USHRT_MAX);
	if (!bufferStart) {
		status = -1;
		goto exit;
	}
	memset(bufferStart, 0, USHRT_MAX);	//FIXME: necessary???
	responsePdu.data = bufferStart + sizeof(sdp_hdr_t);
	responsePdu.length = 0;
	responsePdu.size = USHRT_MAX - sizeof(sdp_hdr_t);
	responsePduHeader = (sdp_hdr_t*)bufferStart;

	switch (requestPduHeader->pduId) {
		case SDP_PDU_SEARCH_REQ :
			DBPRT("Got a svc srch req");
			status = _sdp_search_req(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_SEARCH_RSP;
			break;
		case SDP_PDU_ATTR_REQ :
			DBPRT("Got a svc attr req");
			status = _sdp_attr_req(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_ATTR_RSP;
			break;
		case SDP_PDU_SEARCH_ATTR_REQ :
			DBPRT("Got a svc srch attr req");
			status = _sdp_search_attr_req(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_SEARCH_ATTR_RSP;
			break;

		/* Service Registration */
		case SDP_PDU_REG_REQ :
			DBPRT("Service register request");
			status = _sdp_register_service(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_REG_RSP;
			break;
		case SDP_PDU_UPDATE_REQ :
			DBPRT("Service update request");
			status = _sdp_update_service(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_UPDATE_RSP;
			break;
		case SDP_PDU_REMOVE_REQ :
			DBPRT("Service removal request");
			status = _sdp_delete_service(request, &responsePdu);
			responsePduHeader->pduId = SDP_PDU_REMOVE_RSP;
			break;

		default :
			BTERROR("Unknown PDU ID : 0x%x received", requestPduHeader->pduId);
			status = SDP_ERR_REQUEST_SYNTAX;
			break;
	}
exit:	
	if (status) {
		/* set error code */
		if (!bufferStart) {
			responsePdu.data = (void*)&err_rsp.ErrorCode;
			responsePdu.size = sizeof(uint16_t);
			responsePduHeader = (sdp_hdr_t*)&err_rsp;
		}
		responsePduHeader->pduId = SDP_PDU_ERROR_RSP;
		if (status < 0)
			__put_u16(responsePdu.data, htons(SDP_ERR_RESOURCES));
		else
			__put_u16(responsePdu.data, htons(status));
		responsePdu.length = sizeof(uint16_t);
	}

	DBPRT("Status : %d", status);
	DBPRT("Sending a response back");
	responsePduHeader->transactionId = requestPduHeader->transactionId;
	responsePduHeader->paramLength = htons(responsePdu.length);

	responsePdu.length += sizeof(sdp_hdr_t);
	responsePdu.data -= sizeof(sdp_hdr_t);

	len = send(request->fd, responsePdu.data, responsePdu.length, 0);
	DBPRT("Bytes Sent : %d, requested: %d", len, responsePdu.length);
	if (bufferStart)
		free(bufferStart);
	if (len < 0)
		return len;
	return status;
}

/*
 * Remove services by RequestSocket
 *
 */
int _sdp_disc_req(int fd)
{
	int	i, status;

	sdp_del_rsp_cscache(fd, NULL);
start_again:
	for (i = 0; i < s_list_length(serviceList); i++) {
		sdpsvc_t *svcRec;

		svcRec = (sdpsvc_t *)s_list_nth_data(serviceList, i);
		DBPRT("fd: %d, svc->fd: %d\n", fd, svcRec->fd);
		if (svcRec->fd == fd) {
			status = sdp_del_svc(svcRec->serviceRecordHandle);
			sdp_free_svc(svcRec);
			goto start_again;
		}
	}
	return 0;	
}

