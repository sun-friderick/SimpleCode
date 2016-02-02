/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 - 2004 Nokia Corporation
   Authors:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
   		Imre Deak <ext-imre.deak@nokia.com>

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
   $Id: btsrv-sdp.c,v 1.7 2004/03/19 15:55:05 kassatki Exp $

   Registering SDP records with the local SDP server.  

   Fixes:
   		Imre Deak <ext-imre.deak@nokia.com>
		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <affix/config.h>

#include <stdlib.h>
#include <string.h>

#include <affix/btcore.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>
#include <affix/sdpsrv.h>

#include "btsrv.h"

static int	srvHandle = -1;

int sdpreg_init(void)
{
	if (srvHandle >= 0)
		return 0;
	// initialize the SDP internal data structures
	if (sdp_init(SDP_SVC_PROVIDER) != 0)
		return -1;
	srvHandle = sdp_connect_local();
	if (srvHandle < 0)
		return srvHandle;
	return 0;
}

void sdpreg_cleanup(void)
{
	if (srvHandle < 0)
		return;
	sdp_close(srvHandle);
	sdp_cleanup();
	srvHandle = -1;
}

int sdpreg_register(struct btservice *svc)
{
	if (svc->profile->reg_func && svc->profile->reg_func(svc) < 0) {
		BTERROR("Unable to register SDP record for service %s", svc->name);
		return -1;
	}
	return 0;
}

int sdpreg_unregister(struct btservice *svc)
{
	int		status;
	sdpsvc_t	*svcRec = svc->svcRec;

	if (!svcRec)
		return 0;
	status = sdp_delete_service(srvHandle, svcRec);
	switch (status) {
		case 0:
			BTINFO("service %s deleted", svc->name);
			break;
		case SDP_ERR_INVALID_ARG:
			BTERROR("Error: You cannot delete service %s", svc->name);
			break;
		default:
			BTERROR("Something went wrong. sdp_delete_service() returned: %d\n", status);
			break;
	}
	sdp_free_svc(svcRec);
	svc->svcRec = NULL;
	return 0;
}

int sdpreg_rfcomm(struct btservice *svc)
{
	int		status = -1;
	sdpsvc_t	*svcRec;

	svcRec = sdp_create_rfcomm_svc(svc->profile->svc_class, svc->profile->generic_class, svc->profile->profile,
			svc->name, svc->prov, svc->desc, svc->port);
	if (!svcRec) {
		BTERROR("sdp_create_svc failed");
		return -1;
	}
	status = sdp_register_service(srvHandle, svcRec);
	if (status != 0) {
		BTERROR("sdp_register_service failed");
		sdp_free_svc(svcRec);
		return -1;
	}
	svc->svcRec = svcRec;
	return 0;
}

int sdpreg_pan(struct btservice *svc)
{
	int		status = -1;
	sdpsvc_t	*svcRec;
	sdpdata_t	*attr;
	slist_t		*ptype = NULL;

	/*
	 ** First create a service record handle
	 */
	s_list_append_uint(&ptype, 0x800); 
	s_list_append_uint(&ptype, 0x806);
	svcRec = sdp_create_pan_svc(svc->profile->svc_class, ptype, 0x0000, 0x0005, 0x100);
	s_list_free(&ptype);
	if (svcRec == NULL)
		return -1;
	/* add language attribute */
	attr = sdp_set_lang_attr(svcRec);
	if (attr == NULL) {
		BTERROR("setLanguageBase failed");
		sdp_free_svc(svcRec);
		return -1;
	}
	status = sdp_add_lang(attr, 0, 0, 0);
	if (status) {
		sdp_free_svc(svcRec);
		return -1;
	}
	/* set informational attibutes */		
	status = sdp_set_info_attr(svcRec, svc->name, svc->prov, svc->desc);
	if (status != 0) {
		sdp_free_svc(svcRec);
		return -1;
	}
	status = sdp_register_service(srvHandle, svcRec);
	if (status != 0) {
		BTERROR("sdp_register_service failed");
		sdp_free_svc(svcRec);
		return -1;
	}
	svc->svcRec = svcRec;
	return 0;
}

/*
	  ipSubnetValuePtr[0] = (char *)malloc(32);
	  sprintf(ipSubnetValuePtr[0], "192.168.0");
	  ipSubnetValuePtr[1] = (char *)malloc(32);
	  sprintf(ipSubnetValuePtr[1], "192.168.1");

	  SDP_ATTR_IP_SUBNET
*/

