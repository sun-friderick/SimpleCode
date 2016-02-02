/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001,2002 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   Original Author: Imre Deak <ext-imre.deak@nokia.com>

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
   $Id: btsrv.h,v 1.6 2004/03/17 12:06:06 kassatki Exp $

   Header file for the BT server user space daemon 

   Fixes:
   		Imre Deak <ext-imre.deak@nokia.com>
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

#ifndef BTSRV_H
#define BTSRV_H

#include <affix/utils.h>

#define MAX_PROFILE_NAME_LEN     50
#define MAX_CMD_LEN		256

#define MAX_SERVICE_NUM		32
#define MAX_DEVICE_NUM		8

#define SRV_FLAG_SOCKET		0x01
#define SRV_FLAG_RFCOMM_TTY	0x02
#define SRV_FLAG_STD		0x04

struct btservice;

typedef int (*sdpreg_func_t)(struct btservice *svc);

struct btprofile {
	char		*name;//[MAX_PROFILE_NAME_LEN];
	uint16_t	svc_class;
	uint16_t	generic_class;
	uint16_t	profile;
	int		proto;
	sdpreg_func_t	reg_func;
};


/*
 * data structure to keep
 * - configuration info for services
 *
 */
struct btservice {
	/* service stuff */
	int		running;	/* service state */
	int		active;		/* will be running */
        int     	port;
	int		srv_fd;
        char    	cmd[MAX_CMD_LEN];
	int		flags;
        char    	*name;		/* service name */
	char		*prov;		/* service provider */
	char		*desc;		/* service description */
	int		security;	/* security level mask */
	uint32_t	cod;		/* additional device class bits */
	struct btprofile	*profile;
#if defined(CONFIG_AFFIX_SDP)
	sdpsvc_t	*svcRec;
#endif
};

/*
 * data structure to keep 
 * - configuration info for devices
 * - device status
 *
 */
struct btdevice {
	char		name[IFNAMSIZ];
	int		valid;
	char		*btname;	/* device name */
	uint32_t	cod;		/* device class */
	int		scan;
	int		security;	/* security mode */
	int		pkt_type;
	int		role;
};


extern struct btservice	services[];
extern struct btprofile	profiles[];
extern struct btdevice	devices[];
extern char		*config_file;
extern int		initdev;
extern int		startsvc;
extern int		managepin;
extern int		managekey;


int btsrv_read_config(char *config_file, int *service_num, int *device_num);
int btsrv_read_config_buf(char *config, int *service_num, int *device_num);
char *btsrv_format_cmd(const char *cmd, int conid, int line, BD_ADDR *bd_addr, int srv_ch);

int sdpreg_init(void);
void sdpreg_cleanup(void);
int sdpreg_register(struct btservice *svc);
int sdpreg_unregister(struct btservice *svc);

#if defined(CONFIG_AFFIX_SDP)
int sdpreg_rfcomm(struct btservice *svc);
int sdpreg_pan(struct btservice *svc);
#else
#define sdpreg_rfcomm NULL
#define sdpreg_pan NULL
#endif

#endif	/*	BTSRV_H */

