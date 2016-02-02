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
   $Id: cstate.c,v 1.21 2003/03/14 09:18:56 kds Exp $

   Fixes:
   		Dmitry Kasatkin		: cleanup, fixes
*/

#include <affix/config.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include "cstate.h"
#include "utils.h"

static slist_t	*cacheList;

#ifdef CONFIG_AFFIX_DEBUG

void sdp_print_cstate(sdp_cstate_t *pCState)
{
	DBPRT("[%s-%d] : ", __FUNCTION__, __LINE__);
	DBPRT("sdp_cs_t TS : 0x%lx", pCState->timestamp);
	DBPRT("sdp_cs_t max bytes : %d", pCState->value.maxBytesSent);
	DBPRT("sdp_cs_t last index : %d", pCState->value.lastIndexSent);
}

void sdp_print_cstate_entry(void * value, void * data)
{
	DBPRT("Value : %p", (char *)value);
}

#endif

void sdp_init_cscache(void)
{
	cacheList = NULL;
}

sdp_cstate_t *sdp_get_cstate(char *pdata)
{
	uint8_t 	cStateSize;
	sdp_cstate_t	*pCState;

	pCState = (sdp_cstate_t*)pdata;
	cStateSize = __get_u8(&pCState->length);
	DBPRT("Continuation State size : %d", cStateSize);
	if (cStateSize != 0) {
		DBPRT("Cstate TS : 0x%lx", pCState->timestamp);
		DBPRT("Bytes sent : %d", pCState->value.maxBytesSent);
		return pCState;
	}                 
	return NULL;
}

int __sdp_set_cstate(char *pdata, sdp_cstate_t *pCState)
{
	int	length;

	if (pCState == NULL) {
		__put_u8(pdata, 0);
		pdata += sizeof(uint8_t);
		length = sizeof(uint8_t);
	} else {
		DBPRT("Non null sdp_cs_t id : 0x%lx", pCState->timestamp);
		memcpy(pdata, pCState, sizeof(sdp_cstate_t));
		__put_u8(pdata, sizeof(sdp_cstate_t) - 1);
		length = sizeof(sdp_cstate_t);
	}
	return length;
}

int sdp_set_cstate(sdppdu_t *pdu, sdp_cstate_t *pCState)
{
	int length = 0;
	char *pdata = NULL;

	pdata = pdu->data + pdu->length;
	length = __sdp_set_cstate(pdata, pCState);
	pdu->length += length;
	return length;
}

long sdp_add_rsp_cscache(int fd, sdppdu_t *response)
{
	sdp_csbuffer_t	*csbuf;
	char 		*pdata;

	pdata = (char *)malloc(response->length);
	if (!pdata)
		return -1;
	memcpy(pdata, response->data, response->length);
	
	csbuf = (sdp_csbuffer_t *)malloc(sizeof(sdp_csbuffer_t));
	if (!csbuf) {
		free(pdata);
		return -1;
	}
	memset((char *)csbuf, 0, sizeof(sdp_csbuffer_t));

	csbuf->responseBuffer.data = pdata;
	csbuf->responseBuffer.length = response->length;
	csbuf->responseBuffer.size = response->length;
	/* set owner */
	csbuf->fd = fd;
	/* set id */
	csbuf->timestamp = sdp_get_time();
	DBPRT("CacheList : 0x%lx", (long)cacheList);
	s_list_append(&cacheList, csbuf);
#ifdef CONFIG_AFFIX_DEBUG 
	s_list_foreach(cacheList, sdp_print_cstate_entry, NULL);
#endif
	return csbuf->timestamp;
}

static int __sdp_cscmp(const void *value1, const void *value2)
{
	sdp_csbuffer_t *csb1 = (sdp_csbuffer_t*)value1;
	sdp_csbuffer_t *csb2 = (sdp_csbuffer_t*)value2;

	if (!csb1 || !csb2)
		return -1;
	if (csb1->fd == csb2->fd && 
			(!csb2->timestamp || csb1->timestamp == csb2->timestamp))
		return 0;
	return -1;
}

sdppdu_t *sdp_get_rsp_cscache(int fd, sdp_cstate_t *pCState)
{
	sdppdu_t	*pdu;
	slist_t 	*entry;
	sdp_csbuffer_t	*realRec;
	sdp_csbuffer_t	toFind;

#ifdef CONFIG_AFFIX_DEBUG
	s_list_foreach(cacheList, sdp_print_cstate_entry, NULL);
#endif
	toFind.fd = fd;
	toFind.timestamp = pCState->timestamp;
	entry = s_list_find_custom(cacheList, &toFind, __sdp_cscmp);
	if (!entry)
		return NULL;
	realRec = (sdp_csbuffer_t*)entry->data;
	if (!realRec)
		return NULL;
	pdu = &realRec->responseBuffer;
	DBPRT("Cached response : 0x%lx", (long)pdu);
	return pdu;
}

static int __sdp_del_rsp_cscache(const void *value1, const void *value2)
{
	sdp_csbuffer_t	*pBuffer = (sdp_csbuffer_t*)value1;
	
	if (!value1)
		return -1;
	if (!value2 || __sdp_cscmp(value1, value2) == 0) {
		free(pBuffer->responseBuffer.data);
		free(pBuffer);
		return 0;
	}
	return -1;
}

void sdp_del_rsp_cscache(int fd, sdp_cstate_t *pCState)
{
	sdp_csbuffer_t	toRemove;

	toRemove.fd = fd;
	toRemove.timestamp = pCState ? pCState->timestamp : 0;
	s_list_remove_custom(&cacheList, &toRemove, __sdp_del_rsp_cscache);
}

void sdp_cleanup_cscache(void)
{
	DBPRT("Cache table cleanup");
	s_list_remove_custom(&cacheList, NULL, __sdp_del_rsp_cscache);
	
}

int sdp_copy_cstate(char *pdata, sdp_cs_t *pCState)
{
	int length;

	if (pCState == NULL) {
		__put_u8(pdata, 0);
		length = 1;
	} else {
		__put_u8(pdata++, pCState->length);
		memcpy(pdata, pCState->data, pCState->length);
		length = pCState->length+1;
	}
	return length;
}

