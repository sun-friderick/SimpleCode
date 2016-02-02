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
   $Id: cstate.h,v 1.9 2003/03/14 09:18:56 kds Exp $

   Fixes:
   		Dmitry Kasatkin		: cleanup, fixes
*/


#ifndef SDP_CSTATE_H
#define SDP_CSTATE_H

#include <stdint.h>

#include <affix/sdp.h>

/* Continuation state on client side */
typedef struct {
	uint8_t		length;
	uint8_t		data[16];
} __PACK__ sdp_cs_t;

int sdp_copy_cstate(char *pdata, sdp_cs_t *pCState);

/* Continuation state on server side */
typedef struct {
	uint8_t	length;
	long	timestamp;
	union {
		uint16_t	maxBytesSent;
		uint16_t	lastIndexSent;
	} value;
} sdp_cstate_t;

typedef struct {
	int		fd;		/* owner */
	long 		timestamp;	/* id */
	sdppdu_t 	responseBuffer;	/* actuall data */
} sdp_csbuffer_t;

sdp_cstate_t *sdp_get_cstate(char *buffer);

long sdp_add_rsp_cscache(int fd, sdppdu_t *response);
sdppdu_t *sdp_get_rsp_cscache(int fd, sdp_cstate_t *pCState);
void sdp_del_rsp_cscache(int fd, sdp_cstate_t *pCState);

void sdp_print_cstate(sdp_cstate_t *pCState);

int sdp_set_cstate(sdppdu_t *pdu, sdp_cstate_t *pCState);

void sdp_cleanup_cscache(void);

void sdp_init_cscache(void);

#endif 
