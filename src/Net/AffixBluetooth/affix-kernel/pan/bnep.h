/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Muller Ulrich <ulrich.muller@nokia.com>

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
   $Id: bnep.h,v 1.12 2003/03/04 11:11:23 kds Exp $

   bnep.h - Bluetooth Network Encapsulation Protocol (BNEP) Rev. 0.95a

   Fixes:	
*/

/************************************************************************************************/
/* includes */

#ifndef _BNEP_H
#define _BNEP_H

#include <linux/if_ether.h>
#include <linux/timer.h>
#include <asm/byteorder.h>

#include "pan.h"

/***************************************************************************************/
/* internal types */

/* protocol filters and multicast address filters are implemented as simple arrays of
   sorted start-stop entries, because the number of entries is expected to be very small.
   for a large number of entries, a data structure with a sublinear lookup time should be
   used.
*/


/***************************************************************************************/
/* 2.4 BNEP Header Formats */

typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8	type:7,
		extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8	extension:1,
		type:7;
#else
#error	"Please fix <asm/byteorder.h>"
#endif
	__u8	data[0];
} __PACK__ bnep_hdr_t;

/* 2.4.1 BNEP Type Values */
#define BNEP_GENERAL_ETHERNET			0x00
#define BNEP_CONTROL				0x01
#define BNEP_COMPRESSED_ETHERNET		0x02
#define BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY	0x03
#define BNEP_COMPRESSED_ETHERNET_DEST_ONLY	0x04

/*-------------------------------------------------------------------------------------*/
/* 2.5 BNEP_GENERAL_ETHERNET Packet Type Header Format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		h_dest[ETH_ALEN];	/* destination eth addr	*/
	__u8		h_source[ETH_ALEN];	/* source ether addr	*/
	__u16		h_proto;		/* packet type ID field	*/
} __PACK__ bnep_hdr_ge_t;

/*-------------------------------------------------------------------------------------*/
/* 2.6 BNEP_CONTROL Packet Type Header Format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
} __PACK__ bnep_hdr_control_t;


/* 2.6.1 BNEP Control Type Values */
#define BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD	0x00
#define BNEP_SETUP_CONNECTION_REQUEST_MSG	0x01
#define BNEP_SETUP_CONNECTION_RESPONSE_MSG	0x02
#define BNEP_FILTER_NET_TYPE_SET_MSG		0x03
#define BNEP_FILTER_NET_TYPE_RESPONSE_MSG	0x04
#define BNEP_FILTER_MULTI_ADDR_SET_MSG		0x05
#define BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG	0x06

/* 2.6.2 BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD Control Command Packet */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
	__u8		unknown_control;
} __PACK__ bnep_hdr_ccnu_t;

/* 2.6.3.1 BNEP_SETUP_CONNECTION_REQUEST_MSG setup control message format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
	__u8		uuid_size;
	__u8		uuid[0]; /* destination service uuid (uuid_size bytes), source service uudi (uuid_size bytes) */
} __PACK__ bnep_hdr_scrm_t;

/* 2.6.3.2 BNEP_SETUP_CONNECTION_RESPONSE_MSG response message format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
	__u16		response;
} __PACK__ bnep_hdr_response_t;

/* 2.6.3.2.1 Response Messages */
#define BNEP_SETUP_RSP_SUCCESSFUL		0x0000
#define BNEP_SETUP_RSP_FAIL_INVALID_DEST	0x0001
#define BNEP_SETUP_RSP_FAIL_INVALID_SOURCE	0x0002
#define BNEP_SETUP_RSP_FAIL_SERVICE		0x0003
#define BNEP_SETUP_RSP_NOT_ALLOWED		0x0004

/* 2.6.5.1 BNEP_FILTER_NET_TYPE_SET_MSG filter control message format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
	__u16		list_length;
	__u16		protocol[0]; /* start#1, stop#2, start#2, stop#2, ..., start#N, stop#N */
} __PACK__ bnep_hdr_fntsm_t;

/* 2.6.5.2 BNEP_FILTER_NET_TYPE_RESPONSE_MSG response message format */
/* 2.6.5.2.1 Response Messages */
#define BNEP_FILTER_NET_RSP_SUCCESSFUL		0x0000
#define BNEP_FILTER_NET_RSP_UNSUPPORTED		0x0001
#define BNEP_FILTER_NET_RSP_FAIL_RANGE		0x0002
#define BNEP_FILTER_NET_RSP_FAIL_MAXIMUM	0x0003
#define BNEP_FILTER_NET_RSP_FAIL_SECURITY	0x0004

/* 2.6.6.1 BNEP_FILTER_MULTI_ADDR_SET_MSG filter control message format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		control_type;
	__u16		list_length;
	__u8		addr[ETH_ALEN][0]; /* start#1, stop#2, start#2, stop#2, ..., start#N, stop#N */
} __PACK__ bnep_hdr_fmasm_t;

/* 2.6.6.2 BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG */
/* 2.6.6.2.1 Response Messages */
#define BNEP_FILTER_MULTI_RSP_SUCCESSFUL	0x0000
#define BNEP_FILTER_MULTI_RSP_UNSUPPORTED	0x0001
#define BNEP_FILTER_MULTI_RSP_FAIL_INVALID	0x0002
#define BNEP_FILTER_MULTI_RSP_FAIL_MAXIMUM	0x0003
#define BNEP_FILTER_MULTI_RSP_FAIL_SECURITY	0x0004

/*-------------------------------------------------------------------------------------*/
/* 2.7 BNEP_COMPRESSED_ETHERNET Packet Type Header Format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u16		h_proto;		/* packet type ID field	*/
} __PACK__ bnep_hdr_ce_t;

/*-------------------------------------------------------------------------------------*/
/* 2.8 BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY Packet Type Header Format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		h_source[ETH_ALEN];	/* source ether addr	*/
	__u16		h_proto;		/* packet type ID field	*/
} __PACK__ bnep_hdr_ceso_t;

/*-------------------------------------------------------------------------------------*/
/* 2.9 BNEP_COMPRESSED_ETHERNET_DEST_ONLY Packet Type Header Format */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		h_dest[ETH_ALEN];	/* destination eth addr	*/
	__u16		h_proto;		/* packet type ID field	*/
} __PACK__ bnep_hdr_cedo_t;

/***************************************************************************************/
/* 3.1 Extension Header Overview */

typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u8		type:7,
			extension:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u8		extension:1,
			type:7;
#endif
	__u8		length;
	__u8		data[0];
} __PACK__ bnep_ext_hdr_t;

/* 3.2 Extension Type Values */
#define BNEP_EXT_CONTROL			0x00

/***************************************************************************************/

/* convert ethernet addr to string */
char *ETH_ADDR2str(void *p);

/* initialize new connection to use with bnep */
int bnep_init(struct bnep_con *cl);

/* destroy internal data of a connection */
void bnep_close(struct bnep_con *cl);

/* process bnep packet from l2cap layer */
int bnep_process_lower(struct bnep_con *cl, struct sk_buff *skb);

/* process ethernet packet from upper layer, returns 0 if packet was sent */
int bnep_process_upper(struct pan_dev *bt, struct sk_buff *skb);

/* sets protocol filter by sending control message to all remote devices */
void bnep_set_filter_protocol(struct pan_dev *btdev, protocol_filter* pf);

/* sets multicast filter by sending control message to all remote devices */
void bnep_set_filter_multicast(struct pan_dev *btdev, multicast_filter* mf);

void bnep_send_ethernet_prepare(struct pan_dev *bt, BD_ADDR *bda, struct sk_buff *skb);

/***************************************************************************************/

#endif

