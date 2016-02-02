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
   $Id: uuid.c,v 1.40 2004/02/25 16:27:19 kassatki Exp $
   
   Fixes:
   		Manel Guerrero Zapata	- fixing
		Dmitry Kasatkin		- added UUID32 to string conversion
*/

#include <affix/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

#include <affix/sdp.h>
#include <affix/sdpclt.h>


/*
 ** All definitions are based on Bluetooth Assigned Numbers
 ** of the Bluetooth Specification
 **
 **
 ** NOTE : The quotes("") are needed as this value is treated
 ** as a string in uuidHandler.c
 **
 ** NOTE : The conversion of this string to 128bit relies
 ** in preserving the sequence shown below ie. if you need
 ** to change, then preserve the "-" characters in the 
 ** appropriate places. If their positions CHANGE, then you need to
 **
 */
#define SDP_BASE_UUID "00000000-0000-1000-8000-00805F9B34FB"

char sdp_base_uuid[] = { 0x00, 0x00, 0x00, 0x00,   0x00, 0x00,   0x10, 0x00,
			 0x80, 0x00,    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};


struct affix_tupla sdp_proto_map[] = {
	{SDP_UUID_SDP, "SDP"},
	{SDP_UUID_UDP,"UDP"},
	{SDP_UUID_RFCOMM, "RFCOMM"},
	{SDP_UUID_TCP, "TCP"},
	{SDP_UUID_TCS_BIN, "TCS-BIN"},
	{SDP_UUID_TCS_AT, "TCS-AT"},
	{SDP_UUID_OBEX, "OBEX"},	
	{SDP_UUID_IP, "IP"},
	{SDP_UUID_FTP, "FTP"},
	{SDP_UUID_HTTP, "HTTP"},
	{SDP_UUID_WSP, "WSP"},
	{SDP_UUID_BNEP, "BNEP"},
	{SDP_UUID_L2CAP, "L2CAP"},
	{0, NULL, NULL}
};

struct affix_tupla sdp_service_map[] = {
	{SDP_UUID_SDP_SERVER, NULL, "SDP Server"},
	{SDP_UUID_BROWSE_GROUP_DESC, NULL, "Browse Group Descriptor"},
	{SDP_UUID_PUBLIC_BROWSE_GROUP, NULL, "Public Browse Group"},
	{SDP_UUID_SERIAL_PORT, "Serial", "Serial Port"},
	{SDP_UUID_LAN, "LAN", "LAN Access Using PPP"},
	{SDP_UUID_DUN, "DUN", "Dialup Networking"},
	{SDP_UUID_IRMC_SYNC, "Sync", "IrMCSync"},
	{SDP_UUID_OBEX_PUSH, "Push", "Obex Object Push"},
	{SDP_UUID_OBEX_FTP, "FTP", "Obex File Transfer"},
	{SDP_UUID_IRMC_SYNC_CMD, NULL, "IrMCSync Command"},
	{SDP_UUID_HEADSET, "Head", "Headset"},
	{SDP_UUID_CORDLESS_TELEPHONY, NULL, "Cordless Telephony"},
	{SDP_UUID_INTERCOM, NULL, "Intercom"},
	{SDP_UUID_FAX, "Fax", "Fax"},
	{SDP_UUID_HEADSET_AG, "Audio", "Headset Audio Gateway"},
	{SDP_UUID_PNP_INFO, NULL, "PnP Information"},
	{SDP_UUID_GENERIC_NETWORKING, NULL, "Generic Networking"},
	{SDP_UUID_GENERIC_FTP, NULL, "Generic File Transfer"},
	{SDP_UUID_GENERIC_AUDIO, NULL, "Generic Audio"},
	{SDP_UUID_GENERIC_TELEPHONY, NULL, "Generic Telephony"},
	{SDP_UUID_HANDSFREE, NULL, "Handsfree"},
	{SDP_UUID_HANDSFREE_AG, NULL, "Handsfree Audio Gateway"},
	{SDP_UUID_PANU, "PANU", "PAN PANU"},
	{SDP_UUID_NAP, "NAP", "PAN NAP"},
	{SDP_UUID_GN, "GN", "PAN GN"},
	{0, NULL, NULL}
};

/*
 * Prints into a string the Protocol uuid_t
 * coping a maximum of n characters.
 */

char *sdp_uuid2msg(struct affix_tupla *message, uuid_t *uuid)
{
	if (!uuid)
		return "uuid is NULL";

	switch (uuid->type) {
		case SDP_DTD_UUID16:
			return val2str(message, uuid->value.uuid16Bit);
		case SDP_DTD_UUID32:
			return val2str(message, uuid->value.uuid32Bit);
		case SDP_DTD_UUID128:
			return "Error: This is uuid128";
		default:
			return "Enum type of uuid_t not set.";
	}
}

char *sdp_proto2str(uuid_t *uuid)
{
	return sdp_uuid2msg(sdp_proto_map, uuid);
}

char *sdp_class2str(uuid_t *uuid)
{
	return sdp_uuid2msg(sdp_service_map, uuid);
}

char *sdp_profile2str(uuid_t *uuid)
{
	return sdp_uuid2msg(sdp_service_map, uuid);
}


/*
 * Prints into a string the uuid_t
 * coping a maximum of n characters.
 */
int _sdp_uuid2str(uuid_t *uuid, char *str, size_t n)
{
	if (uuid == NULL)
		return -2;

	switch (uuid->type) {
		case SDP_DTD_UUID16:
			snprintf(str, n, "%.4x", uuid->value.uuid16Bit);
			break;
		case SDP_DTD_UUID32:
			snprintf(str, n, "%.8x", uuid->value.uuid32Bit);
			break;
		case SDP_DTD_UUID128: {
				       unsigned int data0;
				       unsigned short data1;
				       unsigned short data2;
				       unsigned short data3;
				       unsigned int data4;
				       unsigned short data5;

				       memcpy(&data0, &uuid->value.uuid128Bit.data[0], 4);
				       memcpy(&data1, &uuid->value.uuid128Bit.data[4], 2);
				       memcpy(&data2, &uuid->value.uuid128Bit.data[6], 2);
				       memcpy(&data3, &uuid->value.uuid128Bit.data[8], 2);
				       memcpy(&data4, &uuid->value.uuid128Bit.data[10], 4);
				       memcpy(&data5, &uuid->value.uuid128Bit.data[14], 2);

				       snprintf(str, n, "%.8x-%.4x-%.4x-%.4x-%.8x%.4x",
						       ntohl(data0), ntohs(data1), ntohs(data2),
						       ntohs(data3), ntohl(data4), ntohs(data5));
			       }
			       break;
		default:
			       snprintf(str, n, "Enum type of uuid_t not set.");
			       return -1;
	}

	return 0;	// OK
}
char *sdp_uuid2str(uuid_t *uuid)
{
	static char	str[2][MAX_LEN_UUID_STR];
	static int 	num = 0; 

	num = 1 - num; /* switch buf */
	_sdp_uuid2str(uuid, str[num], MAX_LEN_UUID_STR);
	return str[num];
}

int sdp_uuid2val(uuid_t *uuid)
{
	if (uuid == NULL)
		return 0;
	if (uuid->type == SDP_DTD_UUID16)
		return uuid->value.uuid16Bit;
	else
		return uuid->value.uuid32Bit;
	return 0;
}


/*
 ** Function prints the uuid_t in hex as per defined syntax -
 **
 ** 4bytes-2bytes-2bytes-2bytes-6bytes
 **
 ** There is some ugly code, including hardcoding, but
 ** that is just the way it is converting 16 and 32 bit
 ** UUIDs to 128 bit as defined in the SDP doc
 **
 */
#ifdef CONFIG_AFFIX_DEBUG

void sdp_print_uuid(uuid_t *uuid)
{
	char	*str;
	
	str = sdp_uuid2str(uuid);
	switch (uuid->type) {
		case SDP_DTD_UUID16:
			DBPRT("  uint16_t : 0x%s", str);
			break;
		case SDP_DTD_UUID32:
			DBPRT("  uint32_t : 0x%s", str);
			break;
		case SDP_DTD_UUID128:
			DBPRT("  uint128_t : 0x%s", str);
			break;
		default:
			DBPRT("%s", str);
			break;
	}
}

#endif


void sdp_val2uuid16(uuid_t *uuid, uint16_t value16Bit)
{
	if (uuid != NULL) {
		memset(uuid, 0, sizeof(uuid_t));
		uuid->type = SDP_DTD_UUID16;
		uuid->value.uuid16Bit = value16Bit;
	}
}

void sdp_val2uuid32(uuid_t *uuid, uint32_t value32Bit)
{
	if (uuid != NULL) {
		memset(uuid, 0, sizeof(uuid_t));
		uuid->type = SDP_DTD_UUID32;
		uuid->value.uuid32Bit = value32Bit;
	}
}

void sdp_val2uuid128(uuid_t *uuid, uint128_t *pValue128Bit)
{
	if ((uuid != NULL) && (pValue128Bit != NULL)) {
		memset(uuid, 0, sizeof(uuid_t));
		uuid->type = SDP_DTD_UUID128;
		memcpy(&uuid->value.uuid128Bit, pValue128Bit, sizeof(uint128_t));
	}
}

/*
 ** 128 to 16 bit and 32 to 16 bit uuid_t conversion functions
 ** yet to be implemented. Note that the input is in NBO in
 ** both 32 and 128 bit UUIDs and conversion is needed
 */
static inline void sdp_uuid2uuid128(uuid_t *uuid128, uuid_t *uuid)
{
	uint32_t	data;
	
	memcpy(&uuid128->value.uuid128Bit, sdp_base_uuid, 16);
	uuid128->type = SDP_DTD_UUID128;

	data = ntohl(uuid128->value.uuid32Bit);
	if (uuid->type == SDP_DTD_UUID16)
		data += uuid->value.uuid16Bit;
	else
		data += uuid->value.uuid32Bit;
	uuid128->value.uuid32Bit = htonl(data);
}

uuid_t *sdp_uuidcpy128(uuid_t *uuid)
{
	uuid_t *uuid128 = NULL;

	uuid128 = (uuid_t *)malloc(sizeof(uuid_t));
	if (!uuid128)
		return NULL;
	memset(uuid128, 0, sizeof(uuid_t));
	switch (uuid->type) {
		case SDP_DTD_UUID128:
			*uuid128 = *uuid;
			break;
		default:
			sdp_uuid2uuid128(uuid128, uuid);
			break;
	}
	return uuid128;
}


/*
 ** The matching process is defined as "each and every uuid_t
 ** specified in the "search pattern" must be present in the
 ** "target pattern". Here "search pattern" is the set of UUIDs
 ** specified by the service discovery client and "target pattern"
 ** is the set of UUIDs present in a service record. 
 ** 
 ** Return 1 if each and every uuid_t in the search
 ** pattern exists in the target pattern
 */
int sdp_match_uuid(slist_t *searchPattern, slist_t *targetPattern)
{
	int	i;
	int	exists = 0;
	slist_t	*pList;
	void	*sData;

	/*
	 ** The target is a sorted list, so we need not look
	 ** at all elements to confirm existence of an element
	 ** from the search pattern
	 */
	if (!searchPattern || !targetPattern)
		return 0;

	if (s_list_length(targetPattern) < s_list_length(searchPattern))
		return 0;

	for (i = 0; i < s_list_length(searchPattern); i++) {
		sData = s_list_nth_data(searchPattern, i);
		if (sData == NULL)
			continue;
		exists = 0;	/* reset status */
		pList = s_list_find_custom(targetPattern, sData, (void*)sdp_uuidcmp);
		if (pList)
			exists = 1;
		else	/* not found - quit */
			break;
	}
	return exists;
}


/*
 ** uuid_t comparison function
 **
 ** Returns 
 ** 0 if uuidValue1 == uuidValue2
 ** -1 if uuidValue1 < uidValue2
 ** 1 if uuidValue1 > uidValue2
 **    
 */
int sdp_uuidcmp(uuid_t *u1, uuid_t *u2)
{
	int	status;
	uuid_t	*u;
	
	if (u1->type == SDP_DTD_UUID128) {
		if (u2->type == SDP_DTD_UUID128)
			return memcmp(&u1->value, &u2->value, sizeof(uint128_t));
		else {
			u = sdp_uuidcpy128(u2);
			if (!u)
				return -1;
			status = memcmp(&u1->value, &u->value, sizeof(uint128_t));
			free(u);
			return status;
		}
	} else {
		if (u2->type == SDP_DTD_UUID128) {
			u = sdp_uuidcpy128(u1);
			if (!u)
				return -1;
			status = memcmp(&u->value, &u2->value, sizeof(uint128_t));
			free(u);
			return status;
		} else
			return sdp_uuid2val(u1) - sdp_uuid2val(u2);
	}
}

int sdp_uuidcmp32(uuid_t *u1, uint32_t u2)
{
	uuid_t	u2_32;

	sdp_val2uuid32(&u2_32, u2);
	return sdp_uuidcmp(u1, &u2_32);
}

