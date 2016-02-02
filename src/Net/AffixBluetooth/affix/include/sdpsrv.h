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
   $Id: sdpsrv.h,v 1.28 2003/10/06 10:51:24 kds Exp $

   SDP service registration API definitions
 
   Fixes:
		Dmitry Kasatkin		: bug fixes, cleanup, re-arrangement, 
					  continuation state, mtu
*/
	 
#ifndef SDP_SRV_H
#define SDP_SRV_H

#include <affix/sdp.h>

__BEGIN_DECLS


#define SDP_TCP_PORT 3400


/*
 ** Returns the internal state of the service record
 ** pointed to by svcRecHandle
 */
static inline int sdp_get_state(sdpsvc_t *svcRec)
{
	if (svcRec)
		return svcRec->state;
	return SDP_STATE_NOT_EXIST;
}

/*
 * Register a service record.
 */

int sdp_register_service(int srvHandle, sdpsvc_t *svcRec);


/*
 * Delete a service record from the server.
 */

int sdp_delete_service(int srvHandle, sdpsvc_t *svcRec);
int sdp_delete_service_handle(int srvHandle, uint32_t svcRecHandle);

/*
 * Update a service record on the server.
 */

int sdp_update_service(int srvHandle, sdpsvc_t *svcRec);


/*
 * NOTE that none of the sdp_set_XXX() functions below will
 * actually update the SDP server, unless the
 * {register, update}sdpsvc_t() function is invoked.
 * 
 */

int sdp_set_info_attr(sdpsvc_t *svcRec, char *serviceName,
		char *providerName, char *serviceDescription);

sdpdata_t *sdp_set_class_attr(sdpsvc_t *svc);
sdpdata_t *sdp_add_class(sdpdata_t *attr, uint16_t uuid);


sdpdata_t *sdp_set_subgroup_attr(sdpsvc_t *svc);
sdpdata_t *sdp_add_subgroup(sdpdata_t *attr, uint16_t uuid);


sdpdata_t *sdp_set_proto_attr(sdpsvc_t *svc);
/* for alternative protocols */
sdpdata_t *sdp_add_proto_list(sdpdata_t *attr);
sdpdata_t *sdp_add_proto(sdpdata_t *attr, uint16_t uuid, 
		uint16_t portNumber, int portSize, uint16_t version);


sdpdata_t *sdp_set_lang_attr(sdpsvc_t *svcRec);
int sdp_add_lang(sdpdata_t *attr, uint16_t lang, uint16_t encoding, uint16_t offset);
 
sdpdata_t *sdp_set_ttl_attr(sdpsvc_t *svcRec, uint32_t svcTTL);
sdpdata_t *sdp_set_state_attr(sdpsvc_t *svcRec, uint32_t svcRecState);
sdpdata_t *sdp_set_service_attr(sdpsvc_t *svcRec, uint16_t svcUUID);
sdpdata_t *sdp_set_group_attr(sdpsvc_t *svcRec, uint16_t groupUUID);
sdpdata_t *sdp_set_availability_attr(sdpsvc_t *svcRec, uint8_t svcAvail);

sdpdata_t *sdp_set_profile_attr(sdpsvc_t *svc);
int sdp_add_profile(sdpdata_t *attr, uint16_t uuid, uint16_t version);


int sdp_set_url_attr(sdpsvc_t *svcRec, char *clientExecURL, char *docURL, char *iconURL);

/* ---------------------------------------- */

sdpsvc_t *sdp_create_rfcomm_svc(uint16_t svc_class, uint16_t generic_class, uint16_t profile, 
		char *name, char *prov, char *desc, int port);
sdpsvc_t *sdp_create_pan_svc(uint16_t svcClass, slist_t *ptype, uint16_t security, 
					uint16_t type, uint32_t rate);

sdpdata_t *sdp_append_attr(sdpsvc_t *svcRec, uint16_t attrId, sdpdata_t *data);
void sdp_remove_attr(sdpsvc_t *svcRec, uint16_t attrId);

void sdp_set_state(sdpsvc_t *svcRec);

__END_DECLS

#endif //SDP_SRV_H
