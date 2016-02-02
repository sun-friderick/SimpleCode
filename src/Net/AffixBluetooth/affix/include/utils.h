/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

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
   $Id: utils.h,v 1.37 2004/03/02 14:21:30 kassatki Exp $

   General purpose utilites 

   Fixes:
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

#ifndef _BTUTILS_H
#define _BTUTILS_H

#include <stdint.h>
#include <sys/stat.h>

#include <affix/btcore.h>
#include <affix/sdp.h>
#include <affix/sdpclt.h>


__BEGIN_DECLS

extern int		 active_socket;
extern int 		readonlyflag;
extern int 		writeonlyflag;
extern int 		quitflag;
extern int 		crlfflag;


int get_speed(int size, struct timeval *tv_start, struct timeval *tv_end,
		long int *rsec, long int *rusec, double *speed);


/* IO */
int affix_dup2std(int fd);
void affix_open_pipes(char *prog);
void affix_wait_for_children(void);
void affix_do_io(void);

/* SDP helpers */
int sdp_find_port_by_name(struct sockaddr_affix *saddr, char *svc);
int sdp_find_port_by_id(struct sockaddr_affix *saddr, uint16_t svc_id);

__END_DECLS

#endif	/* _BTUTILS_H */

