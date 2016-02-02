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
   $Id: servicedb.c,v 1.19 2003/03/14 08:16:05 kds Exp $

   Contains the implementation of the service repository.
   Has methods to create and clean the repository, besides
   methods to add/find/modify/delete individual entries

   Fixes:
   		Dmitry Kasatkin		: some fixes
*/

#include <affix/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <affix/bluetooth.h>
#include <fcntl.h>
#include <affix/btcore.h>
#include <string.h>

#include <stdint.h>

#include <affix/sdpclt.h>
#include "servicedb.h"

/*
 ** Internal data structure holding service
 ** records. Need not be seen by any other module
 */
slist_t	*serviceList = NULL;


/*
 ** Ordering function called when inserting a service record.
 ** The service repository is a linked list in sorted order
 ** and the service record handle is the sort key
 */
static int orderingFunction(const void * rec1, const void * rec2)
{
	int		order = -1;
	const sdpsvc_t	*svcRec1 = (const sdpsvc_t *)rec1;
	const sdpsvc_t	*svcRec2 = (const sdpsvc_t *)rec2;

	if ((svcRec1 == NULL) || (svcRec2 == NULL)) {
		BTERROR("NULL SVCREC LIST FATAL\n");
	} else
		order = svcRec1->serviceRecordHandle -
			svcRec2->serviceRecordHandle;
	return order;
}

/*
 ** Initialize the service repository
 */
void sdp_init_svcdb(void)
{
	serviceList = NULL;
}

/*
 ** Reset the service repository
 ** Implies deleting all it's contents
 */
void sdp_reset_svcdb(void)
{
	sdp_free_svclist(&serviceList);
}

/*
 ** Add a service record to the repository
 */
int sdp_add_svc(sdpsvc_t *svcRecToAdd)
{
	int	status = -1;

	if (svcRecToAdd == NULL) {
		BTERROR("TRYING TO ADD NULL SVCREC");
		return -1;
	}
	s_list_insert_sorted(&serviceList, svcRecToAdd, orderingFunction);
	DBPRT("Adding svc rec : 0x%lx", (long)svcRecToAdd);
	DBPRT("with handle : 0x%lx", (long)svcRecToAdd->serviceRecordHandle);
	DBPRT("Entries in svcList : %d", s_list_length(serviceList));
	return status;
}

/*
 ** Given a service record handle, find the record associated
 ** with it.
 */
sdpsvc_t * sdp_find_svc(uint32_t svcRecHandle)
{
	sdpsvc_t	tmpRec;
	slist_t 	*tmpList;

	if (serviceList == NULL)
		return NULL;
	tmpRec.serviceRecordHandle = svcRecHandle;
	tmpList = s_list_find_custom(serviceList, &tmpRec, orderingFunction);
	if (tmpList)
		return tmpList->data;
	DBPRT("Couldn't find record for : 0x%x\n", svcRecHandle);
	return NULL;
}


/*
 ** Given a service record handle, remove the service
 ** record from the repository
 */
int sdp_del_svc(uint32_t svcRecHandle)
{
	sdpsvc_t	*svcRec = NULL;

	svcRec = sdp_find_svc(svcRecHandle);
	if (svcRec == NULL) {
		BTERROR("Remove : Couldn't find record for : 0x%x\n", svcRecHandle);
		return -1;
	}
	DBPRT("Removing : 0x%x\n", (uint32_t)svcRec);
	s_list_remove(&serviceList, svcRec);
	return 0;
}

