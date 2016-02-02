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
   $Id: servicedb.h,v 1.11 2003/03/14 08:16:05 kds Exp $

   Contains methods to add/find/modify/delete entries in the service
   repository

   Fixes:
   		Dmitry Kasatkin		: cleanup, fixes
*/

#ifndef SDP_SERVICE_DB_H
#define SDP_SERVICE_DB_H

#include <affix/sdp.h>

/*
** Cleans up the service table
**
*/
void sdp_init_svcdb(void);

/*
** Purge the DB of all service record(s).
** Happens when SDP server is about to
** shutdown
*/
void sdp_reset_svcdb(void);

/*
** Add a new service record to the repository
*/
int sdp_add_svc(sdpsvc_t *svcRecord);

/*
** Find a service record given it's handle
*/
sdpsvc_t *sdp_find_svc(uint32_t svcHandle);

/*
** Remove a service record given it's handle
*/
int sdp_del_svc(uint32_t svcHandle);

#endif //SDP_SERVICE_DB_H
