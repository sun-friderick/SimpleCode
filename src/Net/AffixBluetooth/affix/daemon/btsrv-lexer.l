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
   $Id: btsrv-lexer.l,v 1.3 2004/02/12 16:23:09 kassatki Exp $

   Lexical Analizer

   Fixes:	
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <stdio.h>

#include "btsrv.h"
#include "btsrv-parser.h"

int yywrap(void);

char		str[128];
extern int 	waitsemi;

%}

%option yylineno
%array

%s PARAMS

hex		0x[0-9a-fA-F]+
num		[0-9]+
comment		\#.*\n
word		[a-zA-Z\_][a-zA-Z0-9\_]*
wordlist	{word}([ ]*\,*[ ]*{word})*
fname		[A-Za-z0-9\_\.\-]+
path		(\/{fname})+
string		\"[^\"]*\"


%%

[ \t]+		;

{comment}	;

<INITIAL>{
yes		return OPT_YES;
no		return OPT_NO;
managekey	return OPT_MANAGEKEY;
managepin	return OPT_MANAGEPIN;
startsvc	return OPT_STARTSVC;
initdev		return OPT_INITDEV;
device		return OPT_DEVICE;
service		return OPT_SERVICE;
profile		return OPT_PROFILE;
port		return OPT_PORT;
active		return OPT_ACTIVE;
flags		{ BEGIN PARAMS; return OPT_FLAGS; }
exec		return OPT_EXEC;
provider	return OPT_PROVIDER;
descript	return OPT_DESCRIPT;
security	{ BEGIN PARAMS; return OPT_SECURITY; }
class		{ BEGIN PARAMS; return OPT_COD; }
name		return OPT_NAME;
scan		{ BEGIN PARAMS; return OPT_SCANMODE; }
role		{ BEGIN PARAMS; return OPT_ROLE; }
pkt_type	{ BEGIN PARAMS; return OPT_PKTTYPE; }
}

{hex}		{ sscanf(yytext, "%x", &yylval.num); return NUM; }
{num}		{ sscanf(yytext, "%d", &yylval.num); return NUM; }

{word}		{
			DBPRT("WORD: [%s]", yytext);
			yylval.str = yytext;
			return WORD;
		}

<PARAMS>{wordlist}	{ 
			DBPRT("WORDLIST: [%s]", yytext);
			yylval.str = yytext;
			BEGIN(INITIAL);
			return WORDLIST;
		}

{string}	{
			DBPRT("STRING: [%s]", yytext);
			yylval.str = yytext+1;
			yytext[yyleng-1] = '\0';
			return STRING;
		}
	
[;\n]		{ 
			/* end of params */
			BEGIN(INITIAL);
			if (waitsemi) {
				waitsemi = 0;
				return ';';
			}
		}

.		return *yytext;

%%

int yywrap(void)
{
	return 1;
}

