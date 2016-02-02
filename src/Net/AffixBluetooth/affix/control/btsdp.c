/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001, 2002 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
 
   Original Author:Guruprasad Krishnamurthy <guruprasad.krishnamurthy@nokia.com>

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
   $Id: btsdp.c,v 1.6 2004/03/02 16:15:04 kassatki Exp $

   Fixes:
		Manel Guerrero Zapata <manel.guerrero-zapata@nokia.com>
		Dmitry Kasatkin		: search added, fixes
*/

#include <affix/config.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>

#include <stdint.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>
#include <affix/utils.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>


int srvHandle;

int __sdp_connect(char *target)
{
	int 			status;
	struct sockaddr_affix	sa;

	status = sdp_init(0);
	if (status < 0)
		return status;
	sa.family = PF_AFFIX;
	sa.devnum = hci_devnum(btdev);//HCIDEV_ANY;
	if (strcmp(target, "local") == 0) {
		printf("Opening local connection\n");
		srvHandle = sdp_connect_local();
	} else {
		status = btdev_get_bda(&sa.bda, target);
		if (status) {
			printf("Incorrect address given\n");
			return -1;
		}
		printf("Connecting to host %s ...\n", bda2str(&sa.bda));
		srvHandle = sdp_connect(&sa);
	}
	if (srvHandle < 0)
		return -1;
	return 0;
}

void __sdp_close(void)
{
	if (srvHandle >= 0)
		sdp_close(srvHandle);
}

void print_info_attr(sdpsvc_t *svcRec)
{
	char	*name, *prov, *desc;

	sdp_get_info_attr(svcRec, &name, &prov, &desc);
	if (name)
		printf("Service Name: %s\n", name);
	if (desc)
		printf("Service Description: %s\n", desc);
	if (prov)
		printf("Service Provider: %s\n", prov);
}

static void print_group(sdpsvc_t *svcRec)
{
	uuid_t	*uuid;
	void	*state = NULL;

	printf("Browse Group List: \n");
	do {
		if (sdp_get_subgroup_attr(svcRec, &uuid, &state))
			continue;
		if (sdp_uuidcmp32(uuid, SDP_UUID_PUBLIC_BROWSE_GROUP) == 0)
			printf("  \"PublicBrowseGroup\" (0x%s)\n", sdp_uuid2str(uuid));
		else
			printf("  0x%s\n", sdp_uuid2str(uuid));
	} while (state);
}

static void print_svc_class(sdpsvc_t *svcRec)
{
	uuid_t	*uuid;
	void	*state = NULL;

	printf("Service Class ID List: \n");
	do {
		if (sdp_get_class_attr(svcRec, &uuid, &state))
			continue;
		printf("  \"%s\" (0x%s)\n", sdp_class2str(uuid), sdp_uuid2str(uuid));
	} while (state);
}

static void print_proto_desc(uuid_t *uuid, void *param)
{
	int	i;

	// printf("  Protocol Descriptors:\n");
	printf("  \"%s\" (0x%s)\n", sdp_proto2str(uuid), sdp_uuid2str(uuid));
	/* get following attributes */
	for (i = 1; param; param = s_list_next(param),i++) {
		if (i == 1 && sdp_uuidcmp32(uuid, SDP_UUID_BNEP) == 0) {
			printf("    Version: %#.4x\n", sdp_get_u16(s_list_data(param)));
			/* no more parameters */
			break;
		} else if (i == 1) {
			printf("    Port/Channel: %d\n", sdp_get_u8(s_list_data(param)));
		} else if (i == 2) {
			printf("    Version: %#.4x\n", sdp_get_u16(s_list_data(param)));
		}
	}
}

static void print_access_proto(sdpsvc_t *svcRec)
{
	void	*seq, *s1 = NULL, *s2 = NULL;
	uuid_t	*uuid;
	void	*param;

	printf("Protocol Descriptor List: \n");
	do {
		if (sdp_get_proto_alt_attr(svcRec, &seq, &s1))
			continue;
		do {
			if (sdp_get_proto_attr(svcRec, seq, &uuid, &param, &s2))
				continue;
			print_proto_desc(uuid, param);

		} while (s2);
	} while (s1);
}

static void print_profile_desc(sdpsvc_t *svcRec)
{
	void		*state = NULL;
	uuid_t		*uuid;
	uint16_t	ver;

	printf("Profile Descriptor List: \n");
	do {
		if (sdp_get_profile_attr(svcRec, &uuid, &ver, &state))
			continue;
		printf("  \"%s\" (0x%s)\n", sdp_profile2str(uuid), sdp_uuid2str(uuid));
		printf("    Version: %#.4x\n", ver);
	} while (state);
}

int svcBrowse(uuid_t *groupToExtract)
{
	int 		status = -1, i;
	uint16_t	count = 0;
	slist_t		*searchList = NULL;
	slist_t		*attrIdList = NULL;
	slist_t		*svcRecList = NULL;
	sdpsvc_t	*svcRec;

	s_list_append(&searchList, groupToExtract);
	s_list_append_uint(&attrIdList, 0x0000ffff);
	status = sdp_search_attr_req(srvHandle, searchList, 
			RangeOfAttributes, attrIdList, 0xFFFF, &svcRecList, &count);
	s_list_free(&attrIdList);
	s_list_free(&searchList);
	//printf("Status : %d Count : %d, list: %d\n", status, count, s_list_length(svcRecList));
	
	if (status || count == 0)
		return status;

	for (i = 0; i < s_list_length(svcRecList); i++) {
		printf("==============================\n");

		svcRec = (sdpsvc_t *)s_list_nth_data(svcRecList, i);
		print_info_attr(svcRec);
		printf("------------------------------\n");

		printf("SvcRecHdl: 0x%x\n", svcRec->serviceRecordHandle);

		if (!verboseflag) {
			print_svc_class(svcRec);
			print_access_proto(svcRec);
			print_profile_desc(svcRec);
			print_group(svcRec);
		} else {
			// print detailed info
		}

		if (sdp_is_group(svcRec)) {
			uuid_t	*groupId;

			// printf("SvcRec : 0x%x is a group\n", svcRecHandle);
			printf("This is a group.\n");
			groupId = sdp_get_group_attr(svcRec);
			printf("  0x%s\n", sdp_uuid2str(groupId));
			if (sdp_uuidcmp(groupId, groupToExtract) != 0) {
				printf("Extracting it\n");
				svcBrowse(groupId);
			}
		}
	}
	sdp_free_svclist(&svcRecList);
	return status;
}

int svcSearch(slist_t *searchList)
{
	int		status = -1, i;
	uint16_t 	count = 0;
	slist_t 	*svcRecList = NULL;
	slist_t 	*attrIdList = NULL;
	sdpsvc_t	*svcRec;

	s_list_append_uint(&attrIdList, 0x0000ffff);
	status = sdp_search_attr_req(srvHandle, searchList, 
			RangeOfAttributes, attrIdList, 0xFFFF, &svcRecList, &count);
	//printf("Status : %d Count : %d\n", status, count);
	s_list_free(&attrIdList);
	
	if (status || count == 0)
		return status;

	for (i = 0; i < s_list_length(svcRecList); i++) {
		printf("==============================\n");

		svcRec = (sdpsvc_t *)s_list_nth_data(svcRecList, i);
		print_info_attr(svcRec);
		printf("------------------------------\n");

		printf("SvcRecHdl: 0x%x\n", svcRec->serviceRecordHandle);

		print_svc_class(svcRec);
		print_access_proto(svcRec);
		print_profile_desc(svcRec);
			
		if (sdp_is_group(svcRec)) {
			uuid_t	*groupId;

			printf("This is a group.\n");
			groupId = sdp_get_group_attr(svcRec);
			printf("  0x%s\n", sdp_uuid2str(groupId));
		}
	}
	sdp_free_svclist(&svcRecList);
	return status;
}

int cmd_browse(struct command *cmd)
{
	int	status = -1;
	uuid_t 	uuid;

	if (!__argv[optind]) {
		printf("server address is not given\n");
		return -1;
	}
	status = __sdp_connect(__argv[optind]);
	if (status) {
		printf("Connection to SDP server failed: %s\n", sdp_error(status));
		return status;
	}
	sdp_val2uuid16(&uuid, SDP_UUID_PUBLIC_BROWSE_GROUP);
	status = svcBrowse(&uuid);
	__sdp_close();
	return status;
}

int cmd_search(struct command *cmd)
{
	int		status = -1, i;
	slist_t		*svcSearchList = NULL;
	uint16_t	searchArray[] = {
		SDP_UUID_LAN,
		SDP_UUID_DUN,
		SDP_UUID_OBEX_PUSH,
		SDP_UUID_OBEX_FTP,
		SDP_UUID_FAX,
		SDP_UUID_SERIAL_PORT,
		SDP_UUID_IRMC_SYNC,
		SDP_UUID_IRMC_SYNC_CMD,
		SDP_UUID_HEADSET,
		SDP_UUID_HEADSET_AG,
		SDP_UUID_HANDSFREE_AG,
		SDP_UUID_HANDSFREE
	};
	
	if (!__argv[optind]) {
		printf("server address is not given\n");
		return -1;
	}
	status = __sdp_connect(__argv[optind]);
	if (status) {
		printf("Connection to SDP server failed.\n");
		return status;
	}
	for (i = 0; i < (sizeof(searchArray)/sizeof(searchArray[0])); i++) {
		s_list_append_uuid16(&svcSearchList, searchArray[i]);
		status = svcSearch(svcSearchList);
		s_list_destroy(&svcSearchList);
	}
	__sdp_close();
	return status;
}

