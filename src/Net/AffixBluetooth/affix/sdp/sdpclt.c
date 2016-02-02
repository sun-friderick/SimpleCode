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
   $Id: sdpclt.c,v 1.59 2004/02/26 13:56:47 kassatki Exp $

   Contains the implementation of the SDP service discovery API
 
   Fixes:

   		Dmitry Kasatkin		- Continuation Mechanism
		Dmitry Kasatkin		- Port Request
		Dmitry Kasatkin		- cleaning
		Dmitry Kasatkin		- new simple API
*/
	 
#include <affix/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include "utils.h"
#include "cstate.h"
#include "attr.h"


/*
** This is a service search request. 
**
** INPUT :
**
**   slist_t *svcSearchList
**     Singly linked list containing elements of the search
**     pattern. Each entry in the list is a uuid_t (DataTypeSDP_DTD_UUID16)
**     of the service to be searched
**
**   uint16_t maxSvcRecordCount
**      A 16 bit integer which tells the service, the maximum
**      entries that the client can handle in the response. The
**      server is obliged not to return > maxSvcRecordCount entries
**
** OUTPUT :
**
**   int return value
**     E_OK 
**       The request completed successfully. This does not
**       mean the requested services were found
**     E_FAILURE
**       On any failure
**     E_TIMEOUT 
**       The request completed unsuccessfully due to a timeout
**
**   slist_t **svcHandleList
**     This variable is set on a successful return if there are
**     non-zero service handles. It is a singly linked list of
**     service record handles (uint32_t)
**
**   uint16_t *handleCount
**     This is a pointer to a 16 bit integer, which is set to 
**     indicate the number of service record handles present in
**     svcHandleList
*/
int sdp_search_req(
		int srvHandle,
		slist_t *svcSearchList, 
		uint16_t maxSvcRecordCount,
		slist_t **svcResponseList,
		uint16_t *handleCountInResponse)
{
	int status = 0;
	int requestSize = 0, _requestSize;
	int responseSize = 0;
	int seqLength = 0;
	char *pdata, *_pdata;
	char *requestBuffer =  NULL;
	char *responseBuffer =  NULL;
	sdp_hdr_t *pduRequestHeader;
	sdp_hdr_t *pduResponseHeader;
	sdp_cs_t *pCState = NULL;
	//sdppdu_t concatenatedResponseBuffer;
	int responseLength = 0;

	*handleCountInResponse = 0;

	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_SEARCH_REQ;
	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);

	/*
	 ** Add service class IDs for search
	 */
	seqLength = sdp_gen_uuid_seq_pdu(pdata, svcSearchList);

	DBPRT("Data seq added : %d\n", seqLength);

	/*
	 ** Now set the length and increment the pointer
	 */
	requestSize += seqLength;
	pdata += seqLength;

	/*
	 ** Specify the maximum svc rec count that
	 ** client expects
	 */
	__put_u16(pdata, htons(maxSvcRecordCount));
	requestSize += sizeof(uint16_t);
	pdata += sizeof(uint16_t);

	_requestSize = requestSize;
	_pdata = pdata;
	*svcResponseList = NULL;	//important

	do {
		int bytesScanned = 0;
		int totalServiceRecordCount, currentServiceRecordCount;

		/*
		 ** Add continuation state or NULL (first time)
		 */
		requestSize = _requestSize + sdp_copy_cstate(_pdata, pCState);

		/*
		 ** Set the request header's param length
		 */
		pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));
		pduRequestHeader->transactionId = htons(sdp_gen_trans());
		/*
		 ** Send the request, wait for response and if
		 ** not error, set the appropriate values
		 ** and return
		 */
		status = sdp_send_req_w4_rsp(srvHandle, requestBuffer, 
				responseBuffer, requestSize, &responseSize);

		if (status)
			break;

		pduResponseHeader = (sdp_hdr_t *)responseBuffer;
		if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP) {
			status = pduResponseHeader->data[0];	// ErorrCode
			break;
		}

		responseLength = ntohs(pduResponseHeader->paramLength);
		pdata = responseBuffer + sizeof(sdp_hdr_t);
		/*
		 ** Net service record match count
		 */
		totalServiceRecordCount = 0;
		currentServiceRecordCount = 0;
		totalServiceRecordCount = ntohs(__get_u16(pdata));
		pdata += sizeof(uint16_t);
		bytesScanned += sizeof(uint16_t);
		currentServiceRecordCount = ntohs(__get_u16(pdata));
		pdata += sizeof(uint16_t);
		bytesScanned += sizeof(uint16_t);

		DBPRT("Total svc count : %d\n", totalServiceRecordCount);
		DBPRT("Current svc count : %d\n", currentServiceRecordCount);
		DBPRT("ResponseLength : %d\n", responseLength);

		if (currentServiceRecordCount == 0)
			break;
		status = sdp_extr_svc_handles(pdata, svcResponseList, 
				currentServiceRecordCount, &bytesScanned);
		if (status)
			break;
		
		*handleCountInResponse = currentServiceRecordCount;
		/*
		 ** Check if we have sdp_cstate_t set, if yes,
		 ** set the pCState pointer
		 */
		DBPRT("BytesScanned : %d\n", bytesScanned);
		if (responseLength > bytesScanned) {
			uint8_t cStateLength = 0;

			pdata = responseBuffer + sizeof(sdp_hdr_t) + bytesScanned;
			cStateLength = __get_u8(pdata);
			if (cStateLength > 0) {
				pCState = (sdp_cs_t *)pdata;
				DBPRT("Continuation state present\n");
				DBPRT("sdp_cs_t length : %d\n", cStateLength);
			} else
				pCState = NULL;
		}
	} while (pCState);
	
	free(requestBuffer);
	free(responseBuffer);
	//if (concatenatedResponseBuffer.data != NULL)
	//free(concatenatedResponseBuffer.data);
	return status;
}

int __sdp_search_req(
		struct sockaddr_affix *sa,
		slist_t *svcSearchList, 
		uint16_t maxSvcRecordCount,
		slist_t **svcResponseList,
		uint16_t *handleCountInResponse)
{
	int status = 0;
	int srvHandle;

	srvHandle = sdp_connect(sa);
	if (srvHandle < 0)
		return srvHandle;
	status  = sdp_search_req(srvHandle, svcSearchList, 
			maxSvcRecordCount, svcResponseList, handleCountInResponse);
	sdp_close(srvHandle);
	return status;
}



/*
** This is a service attribute request. 
**
** INPUT :
**
**   uint32_t svcHandle
**     The handle of the service for which the attribute(s) are
**     requested
**
**   sdp_attrreq_t attrReqType
**     Attribute identifiers are 16 bit unsigned integers specified
**     in one of 2 ways described below :
**     IndividualAttributes - 16bit individual identifiers
**        They are the actual attribute identifiers in ascending order
**
**     RangeOfAttributes - 32bit identifier range
**        The high-order 16bits is the start of range
**        the low-order 16bits are the end of range
**        0x0000 to 0xFFFF gets all attributes
**
**   slist_t *attrIDList
**     Singly linked list containing attribute identifiers desired.
**     Every element is either a uint16_t(attrSpec = IndividualAttributes)  
**     or a uint32_t(attrSpec=RangeOfAttributes)
**
**   uint16_t maxAttrIDByteCount
**     The byte count of the number of attribute IDs specified
**     in the request list
**
** OUTPUT :
**   int return value
**     E_OK 
**       The request completed successfully. This does not
**       mean the requested services were found
**     E_TIMEOUT 
**       The request completed unsuccessfully due to a timeout
**
**   uint16_t *maxAttrResponseByteCount
**     This is a pointer to a 16 bit integer, which is set to 
**     indicate the number of bytes of attributes returned. 
**     This pointer is set on successful return
**
*/


int sdp_attr_req(
		int srvHandle,
		uint32_t svcHandle,
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrIDByteCount,
		sdpsvc_t **_svcRec,
		uint16_t *maxAttrResponseByteCount)
{
	int		status = 0;
	int 		requestSize = 0, _requestSize;
	int 		responseSize = 0;
	int 		attrListByteCount = 0;
	int 		seqLength = 0;
	char 		*pdata = NULL, *_pdata;
	char 		*requestBuffer =  NULL;
	char 		*responseBuffer =  NULL;
	sdp_hdr_t	*pduRequestHeader;
	sdp_hdr_t	*pduResponseHeader;
	sdp_cs_t	*pCState = NULL;
	uint8_t		cStateLength = 0;
	sdppdu_t	concatenatedResponseBuffer;
	sdpsvc_t	*svcRec = NULL;

	*_svcRec = NULL;
	*maxAttrResponseByteCount = 0;

	if ((attrReqType != IndividualAttributes) && (attrReqType != RangeOfAttributes))
		return SDP_ERR_INVALID_ARG;
	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return SDP_ERR_INVALID_ARG;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return SDP_ERR_INVALID_ARG;
	}

	memset((char *)&concatenatedResponseBuffer, 0, sizeof(sdppdu_t));

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_ATTR_REQ;

	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);

	/*
	 ** Add the service record handle
	 */
	__put_u32(pdata, htonl(svcHandle));
	requestSize += sizeof(uint32_t);
	pdata += sizeof(uint32_t);

	/*
	 ** Add the maxAttrIDByteCount specifying response limit
	 */
	__put_u16(pdata, htons(maxAttrIDByteCount));
	requestSize += sizeof(uint16_t);
	pdata += sizeof(uint16_t);

	/*
	 ** Get attr seq PDU form 
	 */
	seqLength = sdp_gen_attr_seq_pdu(pdata, attrIDList, 
			((attrReqType == IndividualAttributes) ? SDP_DTD_UINT16 : SDP_DTD_UINT32));
	if (seqLength < 0) {
		status = seqLength;
		goto exit;
	} 

	pdata += seqLength;
	requestSize += seqLength;
	DBPRT("Attr list length : %d\n", seqLength);

	// save before Continuation State
	_pdata = pdata;
	_requestSize = requestSize;

	do {
		int responseCount = 0;
		/*
		 ** Add NULL continuation state
		 */
		requestSize = _requestSize + sdp_copy_cstate(_pdata, pCState);

		/*
		 ** Set the request header's param length
		 */
		pduRequestHeader->transactionId = htons(sdp_gen_trans());
		pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));

		status = sdp_send_req_w4_rsp(srvHandle, requestBuffer, 
				responseBuffer, requestSize, &responseSize);

		if (status)
			goto exit;

		pduResponseHeader = (sdp_hdr_t *)responseBuffer;
		if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP) {
			status = pduResponseHeader->data[0];	// ErrorCode
			goto exit;
		}

		pdata = responseBuffer + sizeof(sdp_hdr_t);
		responseCount  = ntohs(__get_u16(pdata));
		attrListByteCount += responseCount;
		pdata += sizeof(uint16_t);

		/*
		 ** Check if we have continuation state set, if yes, we
		 ** need to re-issue request before we parse ..
		 */
		cStateLength = __get_u8(pdata + responseCount);
		DBPRT("Response id : %d\n", pduResponseHeader->pduId);
		DBPRT("Attrlist byte count : %d\n", responseCount);
		DBPRT("sdp_cs_t length : %d\n", cStateLength);
		/*
		 ** This is a split response, need to concatenate the intermediate
		 ** responses as well as the last one which will have cStateLength == 0
		 */
		if ((cStateLength > 0) || (concatenatedResponseBuffer.length != 0)) {
			char *targetPtr = NULL;

			if (cStateLength > 0)
				pCState = (sdp_cs_t *)(pdata + responseCount);
			else
				pCState = NULL;

			/*
			 ** Build concatenated response buffer
			 */
			concatenatedResponseBuffer.data = (char *)realloc(
					concatenatedResponseBuffer.data, 
					concatenatedResponseBuffer.length + responseCount);
			concatenatedResponseBuffer.size = 
				concatenatedResponseBuffer.length + responseCount;
			targetPtr = concatenatedResponseBuffer.data + 
				concatenatedResponseBuffer.length;
			memcpy(targetPtr, pdata, responseCount);
			concatenatedResponseBuffer.length += responseCount;
		}
	} while (pCState);

	if (attrListByteCount > 0) {
		int bytesScanned = 0;

		if (concatenatedResponseBuffer.length != 0) {
			pdata = concatenatedResponseBuffer.data;
		}
		svcRec = sdp_clt_extr_pdu(pdata, svcHandle, &bytesScanned);
		DBPRT("In sdp_attr_req function \n");
		DBPRT("The status of extractServiceRecord is %d\n", status);
		DBPRT("The svcHandle is 0x%x\n", svcHandle);
		if (!svcRec) {
			status = -1;
			goto exit;
		}
		*_svcRec = svcRec;
		*maxAttrResponseByteCount = bytesScanned;
	}
exit:
	free(requestBuffer);
	free(responseBuffer);
	if (concatenatedResponseBuffer.data != NULL)
		free(concatenatedResponseBuffer.data);
	return status;
}

int __sdp_attr_req(
		struct sockaddr_affix *sa,
		uint32_t svcHandle,
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrIDByteCount,
		sdpsvc_t	**svcRec,
		uint16_t *maxAttrResponseByteCount)
{
	int status = 0;
	int srvHandle;

	srvHandle = sdp_connect(sa);
	if (srvHandle < 0)
		return srvHandle;
	status = sdp_attr_req(srvHandle, svcHandle, attrReqType,
			attrIDList, maxAttrIDByteCount, svcRec, maxAttrResponseByteCount);
	sdp_close(srvHandle);
	return status;
}


/*
** This is a service search request combined with the service
** attribute request. First a service class match is done and
** for matching service, requested attributes are extracted
**
** INPUT :
**
**   slist_t *svcSearchList
**     Singly linked list containing elements of the search
**     pattern. Each entry in the list is a uuid_t(DataTypeSDP_DTD_UUID16)
**     of the service to be searched
**
**   AttributeSpecification attrSpec
**     Attribute identifiers are 16 bit unsigned integers specified
**     in one of 2 ways described below :
**     IndividualAttributes - 16bit individual identifiers
**        They are the actual attribute identifiers in ascending order
**
**     RangeOfAttributes - 32bit identifier range
**        The high-order 16bits is the start of range
**        the low-order 16bits are the end of range
**        0x0000 to 0xFFFF gets all attributes
**
**   slist_t *attrIDList
**     Singly linked list containing attribute identifiers desired.
**     Every element is either a uint16_t(attrSpec = IndividualAttributes)  
**     or a uint32_t(attrSpec=RangeOfAttributes)
**
**   uint16_t maxAttrIDByteCount
**     The byte count of the number of attribute IDs specified
**     in the request list
**
** OUTPUT :
**   int return value
**     E_OK 
**       The request completed successfully. This does not
**       mean the requested services were found
**     E_TIMEOUT 
**       The request completed unsuccessfully due to a timeout
**
**   slist_t **svcResponseList
**     This variable is set on a successful return to point to
**     service(s) found. Each element of this list is of type
**     uint32_t (of the services which matched the search list)
**
**   uint16_t *maxAttrResponseByteCount
**     This is a pointer to a 16 bit integer, which is set to 
**     indicate the number of bytes of attributes returned. 
**     This pointer is set on successful return
**
*/
int sdp_search_attr_req(
		int srvHandle,
		slist_t *svcSearchList, 
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrByteCount,
		slist_t **svcResponseList,
		uint16_t *maxAttrResponseByteCount
		)
{
	int status = 0;
	int requestSize = 0, _requestSize;
	int responseSize = 0;
	int seqLength = 0;
	int attrListByteCount = 0;
	char *pdata = NULL, *_pdata;
	char *requestBuffer =  NULL;
	char *responseBuffer =  NULL;
	sdp_hdr_t *pduRequestHeader;
	sdp_hdr_t *pduResponseHeader;
	uint8_t dataType;
	slist_t *svcRecHandleList = NULL;
	sdppdu_t concatenatedResponseBuffer;
	sdp_cs_t	*pCState = NULL;
	int bytesScanned = 0;

	*maxAttrResponseByteCount = 0;

	if ((attrReqType != IndividualAttributes) && (attrReqType != RangeOfAttributes))
		return SDP_ERR_INVALID_ARG;
	
	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}

	memset((char *)&concatenatedResponseBuffer, 0, sizeof(sdppdu_t));

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_SEARCH_ATTR_REQ;

	// generate PDU
	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);

	/*
	 ** Add service class IDs for search
	 */
	seqLength = sdp_gen_uuid_seq_pdu(pdata, svcSearchList);

	DBPRT("Data seq added : %d\n", seqLength);

	/*
	 ** Now set the length and increment the pointer
	 */
	requestSize += seqLength;
	pdata += seqLength;

	/*
	 ** Specify the maximum attr count that client expects
	 */
	__put_u16(pdata, htons(maxAttrByteCount));
	requestSize += sizeof(uint16_t);
	pdata += sizeof(uint16_t);

	DBPRT("Max attr byte count : %d\n", maxAttrByteCount);

	/*
	 ** Get attr seq PDU form 
	 */
	seqLength = sdp_gen_attr_seq_pdu(pdata, attrIDList, 
			((attrReqType == IndividualAttributes) ? SDP_DTD_UINT16 : SDP_DTD_UINT32));
	if (seqLength < 0) {
		status = seqLength;
		goto exit;
	}
	pdata += seqLength;
	DBPRT("Attr list length : %d\n", seqLength);
	requestSize += seqLength;

	// save before Continuation State
	_pdata = pdata;
	_requestSize = requestSize;

	do {
		int responseCount = 0;
		int cStateLength = 0;

		pduRequestHeader->transactionId = htons(sdp_gen_trans());
		/*
		 * Add continuation state
		 * pCState can be null
		 */
		requestSize = _requestSize + sdp_copy_cstate(_pdata, pCState);

		/*
		 ** Set the request header's param length
		 */
		pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));

		status = sdp_send_req_w4_rsp(srvHandle, requestBuffer, 
				responseBuffer, requestSize, &responseSize);

		if (status)
			goto exit;

		pduResponseHeader = (sdp_hdr_t *)responseBuffer;
		if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP) {
			status = pduResponseHeader->data[0];	// ErrorCode
			goto exit;
		}

		pdata = responseBuffer + sizeof(sdp_hdr_t);
		responseCount = ntohs(__get_u16(pdata));
		attrListByteCount += responseCount;
		pdata += sizeof(uint16_t);	// pdata points to attribute list

		/*
		 ** Check if we have continuation state set, if yes, we
		 ** need to re-issue request before we parse ..
		 */
		cStateLength = __get_u8(pdata + responseCount);
		DBPRT("Attrlist byte count : %d\n", attrListByteCount);
		DBPRT("Response byte count : %d\n", responseCount);
		DBPRT("Cstate length : %d\n", cStateLength);
		/*
		 ** This is a split response, need to concatenate the intermediate
		 ** responses as well as the last one which will have cStateLength == 0
		 */
		if ((cStateLength > 0) || (concatenatedResponseBuffer.length != 0)) {
			char *targetPtr = NULL;

			if (cStateLength > 0) 
				pCState = (sdp_cs_t *)(pdata + responseCount);
			else
				pCState = NULL;
			/*
			 ** Build concatenated response buffer
			 */
			concatenatedResponseBuffer.data = (char *)realloc(
					concatenatedResponseBuffer.data, 
					concatenatedResponseBuffer.length + responseCount);
			targetPtr = concatenatedResponseBuffer.data + 
				concatenatedResponseBuffer.length;
			concatenatedResponseBuffer.size = 
				concatenatedResponseBuffer.length + responseCount;
			memcpy(targetPtr, pdata, responseCount);
			concatenatedResponseBuffer.length += responseCount;
		}
	} while (pCState);

	if (attrListByteCount <= 0)
		goto exit;

	if (concatenatedResponseBuffer.length != 0)
		pdata = concatenatedResponseBuffer.data;
	/*
	 ** Response is a sequence of sequence(s) for one or
	 ** more data element sequence(s) representing services
	 ** for which attributes are returned
	 */
	pdata = sdp_extr_seq_dtd(pdata, &dataType, &seqLength, &bytesScanned);

	DBPRT("Bytes scanned : %d\n", bytesScanned);
	DBPRT("Seq length : %d\n", seqLength);
	if (!pdata || seqLength == 0)
		goto exit;
	for (;;) {
		int		localSeqSize;
		sdpsvc_t	*svcRec = NULL;

		localSeqSize = 0;
		svcRec = sdp_clt_extr_pdu(pdata, 0xffffffff, &localSeqSize);
		if (!svcRec) {
			BTERROR("SVC REC is null\n");
			status = -1;
			break;
		}
		bytesScanned += localSeqSize;
		*maxAttrResponseByteCount += localSeqSize;
		pdata += localSeqSize;
		DBPRT("Loc seq length : %d\n", localSeqSize);
		DBPRT("Svc Rec Handle : 0x%x\n", svcRec->serviceRecordHandle);
		DBPRT("Bytes scanned : %d\n", bytesScanned);
		DBPRT("Attrlist byte count : %d\n", attrListByteCount);
		s_list_append(&svcRecHandleList, svcRec);
		if (attrListByteCount == bytesScanned) {
			DBPRT("Successful scan of service attr lists\n");
			*svcResponseList = svcRecHandleList;
			break;
		}
	}
exit:
	free(requestBuffer);
	free(responseBuffer);
	if (concatenatedResponseBuffer.data != NULL)
		free(concatenatedResponseBuffer.data);
	return status;
}

int __sdp_search_attr_req(
		struct sockaddr_affix *sa,
		slist_t *svcSearchList, 
		sdp_attrreq_t attrReqType,
		slist_t *attrIDList,
		uint16_t maxAttrByteCount,
		slist_t **svcResponseList,
		uint16_t *maxAttrResponseByteCount
		)
{
	int status = 0;
	int srvHandle;

	srvHandle = sdp_connect(sa);
	if (srvHandle < 0)
		return srvHandle;
	status = sdp_search_attr_req(srvHandle, svcSearchList, attrReqType, attrIDList,
			maxAttrByteCount, svcResponseList, maxAttrResponseByteCount);
	sdp_close(srvHandle);
	return status;
}


/* --------------------------------------------------------------------------*/

/*
   Contains utilities for use by SDP service discovery clients
   to extract individual attributes from a service record
   All utility functions below have a signature 
   int getXXX(uint32_t svcRec, XXX *pXXX)
   The pointer pXXX is set to point at the attribute value if
   it is present in the service record and 0 is returned, else
   E_NOT_EXIST is returned
*/

int sdp_is_proto_alt(sdpsvc_t *svcRec)
{
	sdpdata_t	*attr;

	attr = sdp_get_attr(svcRec, SDP_ATTR_PROTO_DESC_LIST);
	if (!attr)
		return -1;
	if (sdp_is_alt(attr))
		return 1;
	return 0;
}

int sdp_get_proto_alt_attr(sdpsvc_t *svcRec, void **seq, void **state)
{
	sdpdata_t	*attr;
	slist_t		*list;

	if (state && *state) {
		list = *state;
		goto get;
	}
	attr = sdp_get_attr(svcRec, SDP_ATTR_PROTO_DESC_LIST);
	if (!attr)
		return SDP_ERR_NOT_EXIST;
	if (sdp_is_seq(attr)) {
		/* not an alternate  - consider as only one alternative */
		*seq = attr;
		if (state)
			*state = NULL;
		return 0;
	}
	for (list = sdp_get_seq(attr); list; list = s_list_next(list)) {
get:
		*seq = s_list_data(list);	/* seq data element */
		if (state)
			*state = s_list_next(list);
		return 0;
	}
	if (state)
		*state = NULL;
	return -1;
}

int sdp_get_proto_attr(sdpsvc_t *svcRec, void *alt, uuid_t **uuid, void **param, void **state)
{
	sdpdata_t	*attr;
	slist_t		*list;

	if (state && *state) {
		list = *state;
		goto get;
	}
	if (!alt) {
		attr = sdp_get_attr(svcRec, SDP_ATTR_PROTO_DESC_LIST);
		if (!attr)
			return SDP_ERR_NOT_EXIST;
	} else {
		attr = alt;
	}
	for (list = sdp_get_seq(attr); list; list = s_list_next(list)) {
get:
		/* 
		 * first element - protocol UUID
		 */
		*param = sdp_get_seq(s_list_data(list));
		if (!*param)
			continue;
		*uuid = sdp_get_uuid(s_list_data(*param));
		/* next parameters */
		*param = s_list_next(*param);
		if (state)
			*state = s_list_next(list);
		return 0;
	}
	if (state)
		*state = NULL;
	return -1;
}

int sdp_get_uuid_attr(sdpsvc_t *svcRec, uint16_t attrID, uuid_t **uuid, void **state)
{
	sdpdata_t	*data;
	slist_t		*list;

	if (state && *state) {
		list = *state;
		goto get;
	}
	data = sdp_get_attr(svcRec, attrID);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	for (list = sdp_get_seq(data); list; list = s_list_next(list)) {
get:
		*uuid = sdp_get_uuid(s_list_data(list));
		if (state)
			*state = s_list_next(list);
		return 0;
	}
	if (state)
		*state = NULL;
	return -1;
}

uuid_t *sdp_get_service_attr(sdpsvc_t *svcRec)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_SERVICEID);
	if (!data)
		return NULL;
	return sdp_get_uuid(data);
}

uuid_t *sdp_get_group_attr(sdpsvc_t *svcRec)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_GROUPID);
	if (!data)
		return NULL;
	return sdp_get_uuid(data);
}

int sdp_is_group(sdpsvc_t *svcRec)
{
	sdpdata_t *data = sdp_get_attr(svcRec, SDP_ATTR_GROUPID);
	if (data && (svcRec->serviceRecordHandle != SDP_ATTR_SERVICE_RECORD_HANDLE))
		return 1;
	return 0;
}

int sdp_get_state_attr(sdpsvc_t *svcRec, uint32_t *state)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_RECORD_STATE);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	*state = sdp_get_u32(data);
	return 0;
}

int sdp_get_availability_attr(sdpsvc_t *svcRec, uint8_t *avail)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_AVAILABILITY);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	*avail = sdp_get_u8(data);
	return 0;
}

int sdp_get_ttl_attr(sdpsvc_t *svcRec, uint32_t *ttl)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_INFO_TTL);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	*ttl = sdp_get_u32(data);
	return 0;
}

int sdp_get_lang_attr(sdpsvc_t *svcRec, slist_t **langSeq)
{
	return 0; 
}

/*
 * get profile descriptor
 *
 * state - store next descriptor ptr
 */
int sdp_get_profile_attr(sdpsvc_t *svcRec, uuid_t **uuid, uint16_t *ver, void **state)
{
	sdpdata_t		*attr;
	slist_t			*list, *param;

	if (state && *state) {
		list = *state;
		goto get_desc;
	}
	attr = sdp_get_attr(svcRec, SDP_ATTR_PROFILE_DESC_LIST);
	if (!attr)
		return SDP_ERR_NOT_EXIST;
	for (list = sdp_get_seq(attr); list; list = s_list_next(list)) {
		/* get parameters */
get_desc:
		/* first param - UUID */
		param = sdp_get_seq(s_list_data(list));
		if (!param)
			continue;
		*uuid = sdp_get_uuid(s_list_data(param));
		/* next - version */
		param = s_list_next(param);
		if (!param)
			continue;
		*ver = sdp_get_u16(s_list_data(param));
		if (state)
			*state = s_list_next(list);
		return 0;
	}
	if (state)
		*state = NULL;
	return -1;
}

int sdp_get_info_attr(sdpsvc_t *svcRec, char **name, char **prov, char **desc)
{
	sdpdata_t	*data;
	
	if (name) {
		data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_NAME_PRIM);
		if (data)
			*name = sdp_get_str(data);
		else
			*name = NULL;
	}
	if (prov) {
		data = sdp_get_attr(svcRec, SDP_ATTR_PROVIDER_NAME_PRIM);
		if (data)
			*prov = sdp_get_str(data);
		else
			*prov = NULL;
	}
	if (name) {
		data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_DESC_PRIM);
		if (data)
			*desc = sdp_get_str(data);
		else
			*desc = NULL;
	}
	return 0;
}

int sdp_get_url_attr(sdpsvc_t *svcRec, char **doc, char **exec, char **icon)
{
	sdpdata_t	*data;
	
	if (doc) {
		data = sdp_get_attr(svcRec, SDP_ATTR_DOC_URL);
		if (data)
			*doc = sdp_get_str(data);
		else
			*doc = NULL;
	}
	if (exec) {
		data = sdp_get_attr(svcRec, SDP_ATTR_EXEC_URL);
		if (data)
			*exec = sdp_get_str(data);
		else
			*exec = NULL;
	}
	if (icon) {
		data = sdp_get_attr(svcRec, SDP_ATTR_ICON_URL);
		if (data)
			*icon = sdp_get_str(data);
		else
			*icon = NULL;
	}
	return 0;
}

/*
 * SDP server attributes
 */
int sdp_get_dbstate_attr(sdpsvc_t *svcRec, uint32_t *state)
{
	sdpdata_t	*data;
	
	data = sdp_get_attr(svcRec, SDP_ATTR_SERVICE_DB_STATE);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	*state = sdp_get_u32(data);
	return 0;
}

int sdp_get_version_attr(sdpsvc_t *svcRec, uint16_t *ver, void **state)
{
	sdpdata_t	*data;
	slist_t		*list;

	if (state && *state) {
		list = *state;
		goto get;
	}
	data = sdp_get_attr(svcRec, SDP_ATTR_VERSION_NUMBER_LIST);
	if (!data)
		return SDP_ERR_NOT_EXIST;
	for (list = sdp_get_seq(data); list; list = s_list_next(list)) {
get:
		*ver = sdp_get_u16(s_list_data(list));
		if (state)
			*state = s_list_next(list);
		return 0;
	}
	if (state)
		*state = NULL;
	return -1;
}

/* -------------------------------  new ------------------------ */

int sdp_get_rfcomm_port(sdpsvc_t *svcRec)
{
	sdpdata_t	*attr, *data;
	slist_t		*alt, *list, *param;
	uuid_t		*uuid;

	attr = sdp_get_attr(svcRec, SDP_ATTR_PROTO_DESC_LIST);
	if (!attr)
		return SDP_ERR_NOT_EXIST;

	if (sdp_is_seq(attr)) {
		/* no alt  */
		alt = NULL;
		data = attr;
		goto dlist;
	} else {
		/* cycle around alternatives */
		for (alt = sdp_get_seq(attr); alt; alt = s_list_next(alt)) {
			data = s_list_data(alt);
dlist:
			/* cycle around protocol descriptors */
			for (list = sdp_get_seq(data); list; list = s_list_next(list)) {
				/* 
				 * first element - protocol UUID
				 */
				param = sdp_get_seq(s_list_data(list));
				if (!param)
					continue;
				uuid = sdp_get_uuid(s_list_data(param));
				if (sdp_uuidcmp32(uuid, SDP_UUID_RFCOMM) == 0) {
					/* next param - port */
					param = s_list_next(param);
					if (!param)
						return -1;
					return sdp_get_u8(s_list_data(param));
				}
			}
			if (!alt)
				break;
		}
	}
	return -1;
}

int sdp_find_uuid(sdpsvc_t *svcRec,  uint32_t uuid32)
{
	int		status = 0;
	slist_t		*searchPattern = NULL;
	uuid_t		uuid;

	sdp_val2uuid32(&uuid, uuid32);
	s_list_append(&searchPattern, &uuid);
	if (!sdp_match_uuid(searchPattern, svcRec->targetPattern))
		status = -1;
	s_list_free(&searchPattern);
	return status;
}

/* some new service functions */
uuid_t *s_list_append_uuid16(slist_t **list, uint16_t uuid16)
{
	uuid_t	*uuid;
	
	uuid = malloc(sizeof(uuid_t));
	if (!uuid)
		return NULL;
	sdp_val2uuid16(uuid, uuid16);
	s_list_append(list, uuid);
	return uuid;
}

uuid_t *s_list_append_uuid32(slist_t **list, uint32_t uuid32)
{
	uuid_t	*uuid;
	
	uuid = malloc(sizeof(uuid_t));
	if (!uuid)
		return NULL;
	sdp_val2uuid32(uuid, uuid32);
	s_list_append(list, uuid);
	return uuid;
}

int sdp_find_port(struct sockaddr_affix *saddr, uint16_t ServiceID)
{
	int		sch, err;
	uint16_t	count;
	slist_t		*searchList = NULL;
	slist_t		*attrList = NULL;
	slist_t		*svcList = NULL;
	sdpsvc_t	*svcRec;

	/* search for service ServiceID */
	s_list_append_uuid16(&searchList, ServiceID);
	/* set attributes to find */
	s_list_append_uint(&attrList, SDP_ATTR_SERVICE_RECORD_HANDLE);
	s_list_append_uint(&attrList, SDP_ATTR_PROTO_DESC_LIST);

	err = __sdp_search_attr_req(saddr, searchList, 
			IndividualAttributes, attrList, 0xffff, &svcList, &count);
	s_list_destroy(&searchList);
	s_list_free(&attrList);
	if (err) {
		fprintf(stderr, "%s\n", sdp_error(err));
		return -1;
	}
	if (count == 0) {
		fprintf(stderr, "no services found\n");
		return -1;
	}
	svcRec = s_list_dequeue(&svcList);	// get first
	sdp_free_svclist(&svcList);
	sch = sdp_get_rfcomm_port(svcRec);
	sdp_free_svc(svcRec);
	if (sch > 0) {
		fprintf(stderr, "Service found on channel: %d\n", sch);
		return sch;
	} else if (sch == 0) {
		fprintf(stderr, "Service is not available\n");
		return -1;
	} else {
		fprintf(stderr, "Unable to get service channel\n");
		return -2;
	}
	return -1;
}


