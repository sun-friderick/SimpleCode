/* A Bison parser, made by GNU Bison 1.875a.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     OPT_DEVICE = 258,
     OPT_SERVICE = 259,
     OPT_PROFILE = 260,
     OPT_FLAGS = 261,
     OPT_EXEC = 262,
     OPT_PROVIDER = 263,
     OPT_DESCRIPT = 264,
     OPT_SECURITY = 265,
     OPT_COD = 266,
     OPT_NAME = 267,
     OPT_ROLE = 268,
     OPT_PKTTYPE = 269,
     OPT_SCANMODE = 270,
     OPT_PORT = 271,
     OPT_ACTIVE = 272,
     OPT_YES = 273,
     OPT_NO = 274,
     OPT_MANAGEPIN = 275,
     OPT_MANAGEKEY = 276,
     OPT_STARTSVC = 277,
     OPT_INITDEV = 278,
     NUM = 279,
     STRING = 280,
     WORDLIST = 281,
     WORD = 282
   };
#endif
#define OPT_DEVICE 258
#define OPT_SERVICE 259
#define OPT_PROFILE 260
#define OPT_FLAGS 261
#define OPT_EXEC 262
#define OPT_PROVIDER 263
#define OPT_DESCRIPT 264
#define OPT_SECURITY 265
#define OPT_COD 266
#define OPT_NAME 267
#define OPT_ROLE 268
#define OPT_PKTTYPE 269
#define OPT_SCANMODE 270
#define OPT_PORT 271
#define OPT_ACTIVE 272
#define OPT_YES 273
#define OPT_NO 274
#define OPT_MANAGEPIN 275
#define OPT_MANAGEKEY 276
#define OPT_STARTSVC 277
#define OPT_INITDEV 278
#define NUM 279
#define STRING 280
#define WORDLIST 281
#define WORD 282




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 82 "btsrv-parser.y"
typedef union YYSTYPE {
	int	num;
	char	*str;
} YYSTYPE;
/* Line 1240 of yacc.c.  */
#line 96 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



