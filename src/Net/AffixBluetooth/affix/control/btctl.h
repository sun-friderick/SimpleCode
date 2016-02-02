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
   $Id: btctl.h,v 1.30 2004/03/02 15:59:39 kassatki Exp $

   Header file for the BT server user space daemon 

   Fixes:
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

#ifndef BTCTL_H
#define BTCTL_H

#include <affix/utils.h>

extern char		**argv;
extern int		argc;
extern char		btdev[IFNAMSIZ];
//extern int		linkmode;
extern int		ftpmode;
extern int		uart_rate;
extern char		*uart_map;

int cmd_prompt(struct command *_cmd);
int cmd_help(struct command *_cmd);
void usage(void);

#if 0
/* btctl-sdp.c */
int cmd_browse(struct command *bcmd);
int cmd_search(struct command *bcmd);

/* btctl-obex.c */
int cmd_open(struct command *bcmd);
int cmd_close(struct command *bcmd);
int cmd_ls(struct command *bcmd);
int cmd_get(struct command *bcmd);
int cmd_put(struct command *bcmd);
int cmd_push(struct command *bcmd);
int cmd_rm(struct command *bcmd);
int cmd_cd(struct command *bcmd);
int cmd_mkdir(struct command *bcmd);
#endif

/* btctl-dev.c */
#define CMD_INITDEV	0x00
#define CMD_UPDEV	0x01
#define CMD_DOWNDEV	0x02
int cmd_initdev(struct command *bcmd);
int cmd_capture(struct command *bcmd);

#define CMD_INIT_UART	0x00
#define CMD_OPEN_UART	0x01
#define CMD_CLOSE_UART	0x02
int cmd_uart(struct command *bcmd);

/* btctl-audio.c */
int cmd_audio(struct command *bcmd);
int cmd_play(struct command *bcmd);
int cmd_record(struct command *bcmd);
int cmd_addsco(struct command *bcmd);

/* PAN */
#define CMD_PAN_INIT	0
#define CMD_PAN_STOP	1
int cmd_pan_init(struct command *bcmd);
int cmd_pan_discovery(struct command *cmd);
int cmd_pan_connect(struct command *cmd);
int cmd_pan_disconnect(struct command *cmd);
int cmd_panctl(struct command *bcmd);



#endif	/*	BTCTL_H */

