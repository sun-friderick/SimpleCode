%{
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
   $Id: btsrv-parser.y,v 1.3 2004/03/17 12:06:06 kassatki Exp $

   parser for btsrv configuration file

   Fixes:	
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
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
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>

#include "btsrv.h"

//#define YYPARSE_PARAM	param
//#define YYLEX_PARAM	param

int yylex(void);
void yyerror(char *s);
void myyyerror(char *format, ...);

extern int yylineno;

static char		*config_file;
static int	 	svcnum;
static int		devnum;

static struct btdevice		*dev;
static struct btservice		*svc;

int	waitsemi = 0;

%}

/* bison declaration */
//%defines
//%name-prefix="btsrv_"
//%output="btsrv-config.c"

%union {
	int	num;
	char	*str;
}

/* Configuration attribute tokens */
%token	OPT_DEVICE OPT_SERVICE OPT_PROFILE OPT_FLAGS OPT_EXEC OPT_PROVIDER OPT_DESCRIPT
%token	OPT_SECURITY OPT_COD OPT_NAME OPT_ROLE OPT_PKTTYPE OPT_SCANMODE OPT_PORT OPT_ACTIVE
%token	OPT_YES OPT_NO OPT_MANAGEPIN OPT_MANAGEKEY OPT_STARTSVC OPT_INITDEV

%token <num>	NUM
%token <str>	STRING WORDLIST WORD

%type <num>	bool
%type <str>	name
%type <str>	string params

%start config

%%

string:	WORD | STRING ;
params:	WORD | WORDLIST ;
bool:	OPT_YES { $$ = 1; } | OPT_NO { $$ = 0; } ;
name:	'*' { $$ = NULL; } | string ;
	
optend:	{ waitsemi = 1; } ';';


config:	/* empty */
	| entry config
	;

entry:	option optend | device | service
	;


/* ------------------------------------------------------------------- */

option:
	OPT_MANAGEPIN bool { managepin = $2; }
	| OPT_MANAGEKEY bool { managekey = $2; }
	| OPT_STARTSVC bool { startsvc = $2; }
	| OPT_INITDEV bool { initdev = $2; }
	;

device:
	'<' OPT_DEVICE name 
			{
				char	*name = $3;
				if (name == NULL) {
					DBPRT("found common device\n");
					if (devnum)
						devices[devnum] = devices[0];
					dev = &devices[0];
					dev->name[0] = '*';
				} else {
					DBPRT("found device: %s", name);
					/* get device num */
					dev = &devices[devnum];
					strcpy(dev->name, name);
				}
			}
	'>'
		devopts
	'<' '/' OPT_DEVICE '>' { devnum++; }
	;


devopts:	/*empty*/
		| devopt optend devopts
		;

devopt:
	OPT_NAME string
			{
				dev->btname = strdup($2);
			}
	| OPT_COD params
			{
				str2cod($2, &dev->cod);
				DBPRT("cod: %#.6x", dev->cod);
			}
	| OPT_ROLE params
			{
				str2mask(role_map, $2, &dev->role);
				;
			}
	| OPT_SECURITY params
			{
				str2mask(sec_mode_map, $2, &dev->security);
				DBPRT("sec_mode: %#x", dev->security);
			}
	| OPT_SCANMODE params
			{
				str2mask(scan_map, $2, &dev->scan);
				DBPRT("scan_mode: %#x", dev->scan);

			}
	| OPT_PKTTYPE params
			{
				str2mask(pkt_type_map, $2, &dev->pkt_type);
				DBPRT("pkt_type: %#x", dev->pkt_type);
			}
	//| error { /*yyclearin; yyerrok;*/ }
	;



/* ------------------------------------------------------------------------------ */
service:
	'<' OPT_SERVICE WORD 
			{ 
				int	i;
				svc = &services[svcnum];
				DBPRT("found service: [%s]", $3);
				for (i = 0; profiles[i].name; i++) {
					if (strcasecmp(profiles[i].name, $3) == 0)
						break;
				}
				if (profiles[i].name == NULL) {
					BTERROR("Invalid profile name %s in config file %s line %d",
						$3, config_file, yylineno);
					return -1;
				}
				svc->profile = &profiles[i];
			} 
		'>'
		svcopts
	'<' '/' OPT_SERVICE '>'	{ svcnum++; }
	;

svcopts:	/*empty*/
		| svcopt optend svcopts
		;

svcopt:	
	OPT_PROFILE string
			{ 
				DBPRT("found profile: %s", $2); 
			}
	| OPT_FLAGS params
			{ 
				struct affix_tupla	flags[] = {
						{SRV_FLAG_SOCKET, "socket"},
						{SRV_FLAG_RFCOMM_TTY|SRV_FLAG_SOCKET, "tty"},
						{SRV_FLAG_STD, "std"},
						{0, 0}
						};

				DBPRT("found flag list: [%s]", $2);
				if (!str2mask(flags, $2, &svc->flags)) {
					BTERROR("Invalid flags at line %d", yylineno);
					return -1;
				}
			}
	| OPT_EXEC STRING 
			{ 
				strcpy(svc->cmd, $2);
				DBPRT("found exec: %s", svc->cmd);
			}
	| OPT_NAME string
			{
				svc->name = strdup($2);
				DBPRT("Found name: %s\n", svc->name);
			}
	| OPT_PROVIDER string
			{
				svc->prov = strdup($2);
			}
	| OPT_DESCRIPT string
			{
				svc->desc = strdup($2);
			}
	| OPT_COD params
			{
				str2cod_svc($2, &svc->cod);
				DBPRT("cod: %#.6x", svc->cod);
			}
	| OPT_SECURITY params
			{
				str2sec_level($2, &svc->security);
			}
	| OPT_PORT NUM
			{
				svc->port = $2;
			}
					
	| OPT_ACTIVE bool
			{
				DBPRT("Found active: %d\n", $2);
				svc->active = $2;
			}
	//| error { /*yyclearin; yyerrok;*/ }
	;

/* ------------------------------------------------------------------- */

%%

void yyerror(char *s)
{
	BTERROR("reading config file: %s at line %d", s, yylineno);
}

void myyyerror(char *format, ...)
{
	int	n;
	char	str[255];
	va_list	ap;

	va_start(ap, format);
	n = vsnprintf(str, sizeof(str), format, ap);
	va_end(ap);
	yyerror(str);
}
	
int btsrv_read_config(char *file, int *service_num, int *device_num)
{
	int			err;
	extern FILE		*yyin;
	
	config_file = file;
	yyin = fopen(config_file, "r");
	if (!yyin) {
		BTERROR("Unable to open config file %s", config_file);
                return -1;
	}
	svcnum = 0;
	devnum = 0;
	err = yyparse();	/* parse config file */
	*service_num = svcnum;
	*device_num = devnum;
	fclose(yyin);
	yyin = NULL;
	return err;
}

int btsrv_read_config_buf(char *config, int *service_num, int *device_num)
{
	int			err;
	extern FILE		*yyin;
	
	config_file = "<STDIN>";
	yyin = tmpfile();
	if (!yyin) {
		BTERROR("Unable to creat config file\n");
                return -1;
	}
	err = fputs(config, yyin);
	if (err < 0) {
		BTERROR("fputs() error\n");
		fclose(yyin);
		yyin = NULL;
	}
	fseek(yyin, 0, SEEK_SET);
	svcnum = 0;
	devnum = 0;
	err = yyparse();	/* parse config file */
	*service_num = svcnum;
	*device_num = devnum;
	fclose(yyin);
	yyin = NULL;
	return err;
}

