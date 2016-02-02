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
   $Id: sdpsrv.c,v 1.51 2004/01/16 15:12:50 litos Exp $

   Contains implementation of SDP service registration APIs
 
   Contains utlities for use by SDP service registration clients
   to add attributes to a service record.

   Fixes:
		Manel Guerrero			: service delete
		Dmitry Kasatkin			: int changed to complex type
		Dmitry Kasatkin			: cleanup
		Dmitry Kasatkin			: new simple API
*/

#include <affix/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <affix/sdp.h>
#include <affix/sdpsrv.h>
#include <affix/sdpclt.h>
#include "utils.h"
#include "attr.h"


int sdp_register_service(int srvHandle, sdpsvc_t *svcRec)
{
	char		*pdata;
	int		status = 0;
	char		*requestBuffer;
	char		*responseBuffer;
	int		requestSize;
	int		responseSize;
	sdp_hdr_t	*pduRequestHeader;
	sdp_hdr_t	*pduResponseHeader;
	uint32_t	svcRecHandle;
	
	if (svcRec->state != SDP_STATE_REGISTER_READY)
		return SDP_ERR_INVALID_ARG;
	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}
	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_REG_REQ;
	pduRequestHeader->transactionId = htons(sdp_gen_trans());

	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);                       

	sdp_gen_svc_pdu(svcRec);
	memcpy(pdata, svcRec->pdu.data, svcRec->pdu.length);
	requestSize += svcRec->pdu.length;
	pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));

	status = sdp_send_req_w4_rsp(srvHandle, requestBuffer,
			responseBuffer, requestSize, &responseSize);

	if (status)
		goto exit;
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;
	pdata = responseBuffer + sizeof(sdp_hdr_t);
	if ((pduResponseHeader->pduId == SDP_PDU_REG_RSP)) {
		svcRecHandle = ntohl(__get_u32(pdata));
		svcRec->serviceRecordHandle = svcRecHandle;
		sdp_append_attr(svcRec, SDP_ATTR_SERVICE_RECORD_HANDLE, sdp_put_u32(svcRecHandle));
		svcRec->state = SDP_STATE_REGISTERED;
	} else 	if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP)
		status = pduResponseHeader->data[0];
	else 
		status = SDP_ERR_SERVER;
exit:
	free(requestBuffer);
	free(responseBuffer);
	return status;
}

int sdp_delete_service(int srvHandle, sdpsvc_t *svcRec)
{
	char	*pdata;
	int	status = 0;
	char	*requestBuffer;
	char	*responseBuffer;
	int	requestSize = 0;
	int	responseSize = 0;

	sdp_hdr_t	*pduRequestHeader;
	sdp_hdr_t	*pduResponseHeader;

	uint32_t	svcRecHandle = 0;

	svcRecHandle = svcRec->serviceRecordHandle;
	if ((svcRecHandle == SDP_ATTR_SERVICE_RECORD_HANDLE) ||
			((svcRec->state != SDP_STATE_REGISTERED) &&
			 (svcRec->state != SDP_STATE_CREATED) && /* FIXME POPO TAROM */
			 (svcRec->state != SDP_STATE_UPDATE_READY)))
		return SDP_ERR_INVALID_ARG;

	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_REMOVE_REQ;
	pduRequestHeader->transactionId = htons(sdp_gen_trans());

	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);                       
	__put_u32(pdata, htonl(svcRecHandle));
	requestSize += sizeof(uint32_t);

	pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));
	status = sdp_send_req_w4_rsp(srvHandle, requestBuffer,
			responseBuffer, requestSize, &responseSize);
	if (status)
		goto exit;
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;
	if ((pduResponseHeader->pduId == SDP_PDU_REMOVE_RSP))
		svcRec->state = SDP_STATE_NOT_EXIST;
	else if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP)
		status = pduResponseHeader->data[0];
	else
		status = SDP_ERR_SERVER;
exit:
	free(requestBuffer);
	free(responseBuffer);
	return status;
}

int sdp_delete_service_handle(int srvHandle, uint32_t svcRecHandle)
{
	char	*pdata;
	int	status = 0;
	char	*requestBuffer;
	char	*responseBuffer;
	int	requestSize = 0;
	int	responseSize = 0;
	sdp_hdr_t	*pduRequestHeader;
	sdp_hdr_t	*pduResponseHeader;

	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}

	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_REMOVE_REQ;
	pduRequestHeader->transactionId = htons(sdp_gen_trans());

	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);                       
	__put_u32(pdata, htonl(svcRecHandle));
	requestSize += sizeof(uint32_t);

	pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));
	status = sdp_send_req_w4_rsp(srvHandle, requestBuffer,
			responseBuffer, requestSize, &responseSize);
	if (status)
		goto exit;
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;
	if ((pduResponseHeader->pduId == SDP_PDU_REMOVE_RSP))
		/* nothing */;
	else if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP)
		status = pduResponseHeader->data[0];
	else
		status = SDP_ERR_SERVER;
exit:
	free(requestBuffer);
	free(responseBuffer);
	return status;
}

int sdp_update_service(int srvHandle, sdpsvc_t *svcRec)
{
	char *pdata;
	int status = 0;
	char *requestBuffer;
	char *responseBuffer;
	int requestSize;
	int responseSize;
	sdp_hdr_t *pduRequestHeader;
	sdp_hdr_t *pduResponseHeader;
	uint32_t svcRecHandle;

	svcRecHandle = svcRec->serviceRecordHandle;
	if ((svcRecHandle == SDP_ATTR_SERVICE_RECORD_HANDLE) ||
			((svcRec->state != SDP_STATE_UPDATE_READY)))
		return SDP_ERR_INVALID_ARG;

	requestBuffer = (char *)malloc(SDP_REQ_BUF_SIZE);
	if (!requestBuffer)
		return -1;
	responseBuffer = (char *)malloc(SDP_RSP_BUF_SIZE);
	if (!responseBuffer) {
		free(requestBuffer);
		return -1;
	}
	pduRequestHeader = (sdp_hdr_t *)requestBuffer;
	pduRequestHeader->pduId = SDP_PDU_UPDATE_REQ;
	pduRequestHeader->transactionId = htons(sdp_gen_trans());

	pdata = (char *)(requestBuffer + sizeof(sdp_hdr_t));
	requestSize = sizeof(sdp_hdr_t);                       

	__put_u32(pdata, htonl(svcRecHandle));
	requestSize += sizeof(uint32_t);
	pdata += sizeof(uint32_t);

	sdp_gen_svc_pdu(svcRec);
	memcpy(pdata, svcRec->pdu.data, svcRec->pdu.length);
	requestSize += svcRec->pdu.length;

	pduRequestHeader->paramLength = htons(requestSize - sizeof(sdp_hdr_t));
	status = sdp_send_req_w4_rsp(srvHandle, requestBuffer,
			responseBuffer, requestSize, &responseSize);
	DBPRT("Send req status : %d\n", status);
	if (status)
		goto exit;
	pduResponseHeader = (sdp_hdr_t *)responseBuffer;
	if ((pduResponseHeader->pduId == SDP_PDU_UPDATE_RSP)) {
		svcRec->state = SDP_STATE_REGISTERED;
	} else 	if (pduResponseHeader->pduId == SDP_PDU_ERROR_RSP)
		status = pduResponseHeader->data[0];
	else
		status = SDP_ERR_SERVER;
exit:
	free(requestBuffer);
	free(responseBuffer);
	return status;
}

/* ------------------------------------------------------------------------ */

/*
 ** NOTE that none of the sdp_set_XXX() functions below will
 ** actually update the SDP server, unless the
 ** sdp_{register,update}_service() function is invoked.
 **
 */

void sdp_set_state(sdpsvc_t *svcRec)
{
	DBPRT("Internal state of svcRec : %d", svcRec->state);
	if (svcRec->state == SDP_STATE_CREATED) {
		if ((sdp_get_attr(svcRec, SDP_ATTR_SERVICE_CLASSID_LIST) != NULL) &&
				(sdp_get_attr(svcRec, SDP_ATTR_PROTO_DESC_LIST) != NULL)) {
			svcRec->state = SDP_STATE_REGISTER_READY;
			DBPRT("Internal state of svcRec reg ready : %d", svcRec->state);
		} else if (sdp_get_attr(svcRec, SDP_ATTR_GROUPID) != NULL) {
			/*
			 ** This could be a browse group descriptor, which
			 ** need not contain mandatory attributes
			 */
			svcRec->state = SDP_STATE_REGISTER_READY;
			DBPRT("Internal state of svcRec reg ready : %d", svcRec->state);
		}
	} else if (svcRec->state == SDP_STATE_REGISTERED) {
		svcRec->state = SDP_STATE_UPDATE_READY;
	}
}

int sdp_set_info_attr(sdpsvc_t *svcRec, char *name, char *prov, char *desc) {
	if (!svcRec || (svcRec->state == SDP_STATE_NOT_EXIST))
		return -1;
	sdp_append_attr(svcRec, SDP_ATTR_SERVICE_NAME_PRIM, sdp_put_str(name));
	sdp_append_attr(svcRec, SDP_ATTR_PROVIDER_NAME_PRIM, sdp_put_str(prov));
	sdp_append_attr(svcRec, SDP_ATTR_SERVICE_DESC_PRIM, sdp_put_str(desc));
	sdp_set_state(svcRec);
	return 0;
}


sdpdata_t *sdp_set_class_attr(sdpsvc_t *svc)
{
	return sdp_append_attr(svc, SDP_ATTR_SERVICE_CLASSID_LIST, sdp_create_seq());
}

sdpdata_t *sdp_add_class(sdpdata_t *attr, uint16_t uuid)
{
	return sdp_append_uuid16(attr, uuid);
}


sdpdata_t *sdp_set_subgroup_attr(sdpsvc_t *svc)
{
	return sdp_append_attr(svc, SDP_ATTR_BROWSE_GROUP_LIST, sdp_create_seq());
}

sdpdata_t *sdp_add_subgroup(sdpdata_t *attr, uint16_t uuid)
{
	return sdp_append_uuid16(attr, uuid);
}


sdpdata_t *sdp_set_proto_attr(sdpsvc_t *svc)
{
	return sdp_append_attr(svc, SDP_ATTR_PROTO_DESC_LIST, sdp_create_seq());
}

/* returns alternate list */
sdpdata_t *sdp_add_proto_list(sdpdata_t *attr)
{
	sdpdata_t	*data;

	/* change type to alternative */
	attr->dtd = SDP_DTD_ALT8;
	data = sdp_append_seq(attr);
	return data;
}

sdpdata_t *sdp_add_proto(sdpdata_t *attr, uint16_t uuid, 
		uint16_t portNumber, int portSize, uint16_t version)
{
	sdpdata_t	*root, *data;

	/* create root element */
	root = sdp_append_seq(attr);
	if (!root)
		return NULL;

	data = sdp_append_uuid16(root, uuid);
	if (data == NULL)
		goto err;
	if (portNumber != 0xFFFF) {
		if (portSize == 1) {
			uint8_t	port = portNumber;
			data = sdp_append_u8(root, port);
		} else
			data = sdp_append_u16(root, portNumber);
		if (data == NULL)
			goto err;
	}
	if (version != 0xffff) {
		data = sdp_append_u16(root, version);
		if (data == NULL)
			goto err;
	}
	return root;
err:
	/* destroy root */
	sdp_remove_data(attr, root);
	return NULL;
}


sdpdata_t *sdp_set_lang_attr(sdpsvc_t *svcRec)
{
	return sdp_append_attr(svcRec, SDP_ATTR_LANG_BASE_ATTRID_LIST, sdp_create_seq());
}

int sdp_add_lang(sdpdata_t *attr, uint16_t lang, uint16_t encoding, uint16_t offset)
{
	sdpdata_t	*data;

	data = sdp_append_u16(attr, lang);
	if (data == NULL)
		return -1;
	data = sdp_append_u16(attr, encoding);
	if (data == NULL)
		return -1;
	data = sdp_append_u16(attr, offset);
	if (data == NULL)
		return -1;
	return 0;
}


sdpdata_t *sdp_set_ttl_attr(sdpsvc_t *svcRec, uint32_t svcTTL)
{
	return sdp_append_attr(svcRec, SDP_ATTR_SERVICE_INFO_TTL, sdp_put_u32(svcTTL));
}


sdpdata_t *sdp_set_state_attr(sdpsvc_t *svcRec, uint32_t svcRecState)
{
	return sdp_append_attr(svcRec, SDP_ATTR_SERVICE_RECORD_STATE, sdp_put_u32(svcRecState));
}


/* 
 * sets the "ServiceID"
 */
sdpdata_t *sdp_set_service_attr(sdpsvc_t *svcRec, uint16_t svcUUID)
{
	return sdp_append_attr(svcRec, SDP_ATTR_SERVICEID, sdp_put_uuid16(svcUUID));
}


/*
 * sets GroupID
 */
sdpdata_t *sdp_set_group_attr(sdpsvc_t *svcRec, uint16_t groupUUID)
{
	return sdp_append_attr(svcRec, SDP_ATTR_GROUPID, sdp_put_uuid16(groupUUID));
}


sdpdata_t *sdp_set_availability_attr(sdpsvc_t *svcRec, uint8_t svcAvail)
{
	return sdp_append_attr(svcRec, SDP_ATTR_SERVICE_AVAILABILITY, sdp_put_u8(svcAvail));
}


sdpdata_t *sdp_set_profile_attr(sdpsvc_t *svc)
{
	return sdp_append_attr(svc, SDP_ATTR_PROFILE_DESC_LIST, sdp_create_seq());
}

int sdp_add_profile(sdpdata_t *attr, uint16_t uuid, uint16_t version)
{
	sdpdata_t	*root, *data;

	root = sdp_append_seq(attr);
	if (root == NULL)
		return -1;
	data = sdp_append_uuid16(root, uuid);
	if (data == NULL)
		goto err;
	data = sdp_append_u16(root, version);
	if (data == NULL)
		goto err;
	return 0;
err:
	sdp_remove_data(attr, root);
	return -1;
}

int sdp_set_url_attr(sdpsvc_t *svcRec, char *clientExecURL, char *docURL, char *iconURL)
{
	if (!svcRec || (svcRec->state == SDP_STATE_NOT_EXIST))
		return -1;
	sdp_append_attr(svcRec, SDP_ATTR_EXEC_URL, sdp_put_url(clientExecURL));
	sdp_append_attr(svcRec, SDP_ATTR_DOC_URL, sdp_put_url(docURL));
	sdp_append_attr(svcRec, SDP_ATTR_ICON_URL, sdp_put_url(iconURL));
	sdp_set_state(svcRec);
	return 0;
}

/* Supported format list attribute 	*/
/* for OBEX Push profile. 		*/
sdpdata_t *sdp_set_supported_format_attr(sdpsvc_t *svc)
{
	return sdp_append_attr(svc, SDP_ATTR_SUPPORTED_FORMATS_LIST, sdp_create_seq());
}

int sdp_add_format(sdpdata_t *attr,uint8_t type)
{
	sdpdata_t	*data;
	data = sdp_append_u8(attr,type);
	if (data == NULL)
		return -1;
	return 0;
}


/* -------------------------------- */

sdpsvc_t *sdp_create_rfcomm_svc(uint16_t svc_class, uint16_t generic_class, uint16_t profile, 
		char *name, char *prov, char *desc, int port)
{
	sdpsvc_t	*svcRec;
	sdpdata_t	*attr, *param;
	//va_list		ap;

	//va_start(ap, port);
	//va_end(ap);
	
	svcRec = sdp_create_svc();
	if (svcRec == NULL) {
		BTERROR("sdp_create_svc failed");
		return NULL;
	}
	/* set service class list*/
	attr = sdp_set_class_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_class_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_class(attr, svc_class);
	if (generic_class)
		sdp_add_class(attr, generic_class);

	/* set profiles list */
	attr = sdp_set_profile_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_profile_attr() failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_profile(attr, profile, 0x0100);

	/* set group */
	attr = sdp_set_subgroup_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_subgroup_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_subgroup(attr, SDP_UUID_PUBLIC_BROWSE_GROUP);
	attr = sdp_set_proto_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_proto_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	param = sdp_add_proto(attr, SDP_UUID_L2CAP, 0xffff, 0, 0xffff);
	param = sdp_add_proto(attr, SDP_UUID_RFCOMM, port, 1, 0xffff);
	if (svc_class == SDP_UUID_OBEX_FTP 
		|| svc_class == SDP_UUID_OBEX_PUSH) {
		param = sdp_add_proto(attr, SDP_UUID_OBEX, 0xffff, 0, 0xffff);
	}
	if (svc_class == SDP_UUID_OBEX_PUSH){
		attr = sdp_set_supported_format_attr(svcRec);
		if (attr == NULL) {
			BTERROR("sdp_set_proto_attr failed");
			sdp_free_svc(svcRec);
			return NULL;
		}
		sdp_add_format(attr,0xFF);
	}
	
	if (sdp_set_info_attr(svcRec, name, prov, desc)) {
		BTERROR("sdp_set_info_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	return svcRec;
}

sdpsvc_t *sdp_create_pan_svc(uint16_t svcClass, slist_t *ptype, uint16_t security, 
						uint16_t type, uint32_t rate)
{
	int		status;
	sdpsvc_t	*svcRec;
	sdpdata_t	*attr, *param, *dataSeq;
	slist_t		*pSeq;

	svcRec = sdp_create_svc();
	if (svcRec == NULL) {
		BTERROR("sdp_create_svc failed");
		return NULL;
	}

	attr = sdp_set_subgroup_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_subgroup_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_subgroup(attr, SDP_UUID_PUBLIC_BROWSE_GROUP);

	attr = sdp_set_proto_attr(svcRec);
	if (attr == NULL) {
		BTERROR("sdp_set_proto_attr failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_proto(attr, SDP_UUID_L2CAP, BNEP_PSM, 2, 0xffff);
	param = sdp_add_proto(attr, SDP_UUID_BNEP, 0xffff, 0, 0x0100);
	/* add supported Network Packet Type */
	if (ptype) {
		/* create root parameter */
		dataSeq = sdp_append_seq(param);
		/* fill data */
		for (pSeq = ptype; pSeq; pSeq = s_list_next(pSeq))
			sdp_append_u16(dataSeq, s_list_uint(pSeq));
	}
	/* set service class list*/
	attr = sdp_set_class_attr(svcRec);
	if (attr == NULL) {
		BTERROR("setServiceClassID failed");
		sdp_free_svc(svcRec);
		return NULL;
	}
	sdp_add_class(attr, svcClass);

	/* set profiles list */
	attr = sdp_set_profile_attr(svcRec);
	if (attr == NULL) {
		BTERROR("No memory for ProfileDescriptor");
		sdp_free_svc(svcRec);
		return NULL;
	}
	status = sdp_add_profile(attr, svcClass, 0x0100);
	if (status) {
		sdp_free_svc(svcRec);
		return NULL;
	}
	
	sdp_append_attr(svcRec, SDP_ATTR_SECURITY_DESC, sdp_put_u16(security));

	/* NAP only */
	if (svcClass == SDP_UUID_NAP) {
		sdp_append_attr(svcRec, SDP_ATTR_NET_ACCESS_TYPE, sdp_put_u16(type));
		sdp_append_attr(svcRec, SDP_ATTR_MAX_NET_ACCESS_RATE, sdp_put_u32(rate));
	}
	/* NAP & GN IPv4/6 subnet info - sequence of strings */
	if (svcClass == SDP_UUID_NAP || svcClass == SDP_UUID_GN) {

	}
	return svcRec;
}

