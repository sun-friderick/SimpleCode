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

/* Written by Richard Stallman by simplifying the original so called
   ``semantic'' parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



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




/* Copy the first part of user declarations.  */
#line 1 "btsrv-parser.y"

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



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 82 "btsrv-parser.y"
typedef union YYSTYPE {
	int	num;
	char	*str;
} YYSTYPE;
/* Line 191 of yacc.c.  */
#line 211 "y.tab.c"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 214 of yacc.c.  */
#line 223 "y.tab.c"

#if ! defined (yyoverflow) || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# if YYSTACK_USE_ALLOCA
#  define YYSTACK_ALLOC alloca
# else
#  ifndef YYSTACK_USE_ALLOCA
#   if defined (alloca) || defined (_ALLOCA_H)
#    define YYSTACK_ALLOC alloca
#   else
#    ifdef __GNUC__
#     define YYSTACK_ALLOC __builtin_alloca
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning. */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
# else
#  if defined (__STDC__) || defined (__cplusplus)
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   define YYSIZE_T size_t
#  endif
#  define YYSTACK_ALLOC malloc
#  define YYSTACK_FREE free
# endif
#endif /* ! defined (yyoverflow) || YYERROR_VERBOSE */


#if (! defined (yyoverflow) \
     && (! defined (__cplusplus) \
	 || (YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  short yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (short) + sizeof (YYSTYPE))				\
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  register YYSIZE_T yyi;		\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (0)
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (0)

#endif

#if defined (__STDC__) || defined (__cplusplus)
   typedef signed char yysigned_char;
#else
   typedef short yysigned_char;
#endif

/* YYFINAL -- State number of the termination state. */
#define YYFINAL  19
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   67

/* YYNTOKENS -- Number of terminals. */
#define YYNTOKENS  33
/* YYNNTS -- Number of nonterminals. */
#define YYNNTS  18
/* YYNRULES -- Number of rules. */
#define YYNRULES  44
/* YYNRULES -- Number of states. */
#define YYNSTATES  84

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   282

#define YYTRANSLATE(YYX) 						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const unsigned char yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,    28,     2,     2,     2,     2,    32,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    29,
      30,     2,    31,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const unsigned char yyprhs[] =
{
       0,     0,     3,     5,     7,     9,    11,    13,    15,    17,
      19,    20,    23,    24,    27,    30,    32,    34,    37,    40,
      43,    46,    47,    58,    59,    63,    66,    69,    72,    75,
      78,    81,    82,    93,    94,    98,   101,   104,   107,   110,
     113,   116,   119,   122,   125
};

/* YYRHS -- A `-1'-separated list of the rules' RHS. */
static const yysigned_char yyrhs[] =
{
      40,     0,    -1,    27,    -1,    25,    -1,    27,    -1,    26,
      -1,    18,    -1,    19,    -1,    28,    -1,    34,    -1,    -1,
      39,    29,    -1,    -1,    41,    40,    -1,    42,    38,    -1,
      43,    -1,    47,    -1,    20,    36,    -1,    21,    36,    -1,
      22,    36,    -1,    23,    36,    -1,    -1,    30,     3,    37,
      44,    31,    45,    30,    32,     3,    31,    -1,    -1,    46,
      38,    45,    -1,    12,    34,    -1,    11,    35,    -1,    13,
      35,    -1,    10,    35,    -1,    15,    35,    -1,    14,    35,
      -1,    -1,    30,     4,    27,    48,    31,    49,    30,    32,
       4,    31,    -1,    -1,    50,    38,    49,    -1,     5,    34,
      -1,     6,    35,    -1,     7,    25,    -1,    12,    34,    -1,
       8,    34,    -1,     9,    34,    -1,    11,    35,    -1,    10,
      35,    -1,    16,    24,    -1,    17,    36,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const unsigned short yyrline[] =
{
       0,   103,   103,   103,   104,   104,   105,   105,   106,   106,
     108,   108,   111,   112,   115,   115,   115,   122,   123,   124,
     125,   130,   129,   151,   152,   156,   160,   165,   170,   175,
     181,   194,   193,   214,   215,   219,   223,   238,   243,   248,
     252,   256,   261,   265,   270
};
#endif

#if YYDEBUG || YYERROR_VERBOSE
/* YYTNME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals. */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "OPT_DEVICE", "OPT_SERVICE", "OPT_PROFILE", 
  "OPT_FLAGS", "OPT_EXEC", "OPT_PROVIDER", "OPT_DESCRIPT", "OPT_SECURITY", 
  "OPT_COD", "OPT_NAME", "OPT_ROLE", "OPT_PKTTYPE", "OPT_SCANMODE", 
  "OPT_PORT", "OPT_ACTIVE", "OPT_YES", "OPT_NO", "OPT_MANAGEPIN", 
  "OPT_MANAGEKEY", "OPT_STARTSVC", "OPT_INITDEV", "NUM", "STRING", 
  "WORDLIST", "WORD", "'*'", "';'", "'<'", "'>'", "'/'", "$accept", 
  "string", "params", "bool", "name", "optend", "@1", "config", "entry", 
  "option", "device", "@2", "devopts", "devopt", "service", "@3", 
  "svcopts", "svcopt", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const unsigned short yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,    42,    59,
      60,    62,    47
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const unsigned char yyr1[] =
{
       0,    33,    34,    34,    35,    35,    36,    36,    37,    37,
      39,    38,    40,    40,    41,    41,    41,    42,    42,    42,
      42,    44,    43,    45,    45,    46,    46,    46,    46,    46,
      46,    48,    47,    49,    49,    50,    50,    50,    50,    50,
      50,    50,    50,    50,    50
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const unsigned char yyr2[] =
{
       0,     2,     1,     1,     1,     1,     1,     1,     1,     1,
       0,     2,     0,     2,     2,     1,     1,     2,     2,     2,
       2,     0,    10,     0,     3,     2,     2,     2,     2,     2,
       2,     0,    10,     0,     3,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const unsigned char yydefact[] =
{
      12,     0,     0,     0,     0,     0,     0,    12,    10,    15,
      16,     6,     7,    17,    18,    19,    20,     0,     0,     1,
      13,    14,     0,     3,     2,     8,     9,    21,    31,    11,
       0,     0,    23,    33,     0,     0,     0,     0,     0,     0,
       0,    10,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    10,     5,     4,    28,    26,    25,    27,
      30,    29,     0,    23,    35,    36,    37,    39,    40,    42,
      41,    38,    43,    44,     0,    33,     0,    24,     0,    34,
       0,     0,    22,    32
};

/* YYDEFGOTO[NTERM-NUM]. */
static const yysigned_char yydefgoto[] =
{
      -1,    26,    56,    13,    27,    21,    22,     6,     7,     8,
       9,    30,    40,    41,    10,    31,    52,    53
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -33
static const yysigned_char yypact[] =
{
      15,    28,    28,    28,    28,    47,     4,    15,   -33,   -33,
     -33,   -33,   -33,   -33,   -33,   -33,   -33,   -15,   -18,   -33,
     -33,   -33,    -9,   -33,   -33,   -33,   -33,   -33,   -33,   -33,
      23,    24,    29,    17,    26,    26,     5,    26,    26,    26,
      18,   -33,     5,    26,    31,     5,     5,    26,    26,     5,
      33,    28,    30,   -33,   -33,   -33,   -33,   -33,   -33,   -33,
     -33,   -33,    27,    29,   -33,   -33,   -33,   -33,   -33,   -33,
     -33,   -33,   -33,   -33,    32,    17,    55,   -33,    57,   -33,
      34,    35,   -33,   -33
};

/* YYPGOTO[NTERM-NUM].  */
static const yysigned_char yypgoto[] =
{
     -33,   -28,   -32,    -2,   -33,   -22,   -33,    56,   -33,   -33,
     -33,   -33,    -1,   -33,   -33,   -33,    -8,   -33
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const unsigned char yytable[] =
{
      14,    15,    16,    57,    19,    59,    60,    61,    58,    28,
      23,    65,    24,    25,    64,    69,    70,    67,    68,    63,
      29,    71,    42,    43,    44,    45,    46,    47,    48,    49,
      23,    75,    24,    50,    51,     1,     2,     3,     4,    34,
      35,    36,    37,    38,    39,     5,    11,    12,    62,    73,
      17,    18,    54,    55,    32,    33,    66,    72,    80,    76,
      74,    81,    77,    20,    78,    82,    83,    79
};

static const unsigned char yycheck[] =
{
       2,     3,     4,    35,     0,    37,    38,    39,    36,    27,
      25,    43,    27,    28,    42,    47,    48,    45,    46,    41,
      29,    49,     5,     6,     7,     8,     9,    10,    11,    12,
      25,    53,    27,    16,    17,    20,    21,    22,    23,    10,
      11,    12,    13,    14,    15,    30,    18,    19,    30,    51,
       3,     4,    26,    27,    31,    31,    25,    24,     3,    32,
      30,     4,    63,     7,    32,    31,    31,    75
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const unsigned char yystos[] =
{
       0,    20,    21,    22,    23,    30,    40,    41,    42,    43,
      47,    18,    19,    36,    36,    36,    36,     3,     4,     0,
      40,    38,    39,    25,    27,    28,    34,    37,    27,    29,
      44,    48,    31,    31,    10,    11,    12,    13,    14,    15,
      45,    46,     5,     6,     7,     8,     9,    10,    11,    12,
      16,    17,    49,    50,    26,    27,    35,    35,    34,    35,
      35,    35,    30,    38,    34,    35,    25,    34,    34,    35,
      35,    34,    24,    36,    30,    38,    32,    45,    32,    49,
       3,     4,    31,    31
};

#if ! defined (YYSIZE_T) && defined (__SIZE_TYPE__)
# define YYSIZE_T __SIZE_TYPE__
#endif
#if ! defined (YYSIZE_T) && defined (size_t)
# define YYSIZE_T size_t
#endif
#if ! defined (YYSIZE_T)
# if defined (__STDC__) || defined (__cplusplus)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# endif
#endif
#if ! defined (YYSIZE_T)
# define YYSIZE_T unsigned int
#endif

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrlab1


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { 								\
      yyerror ("syntax error: cannot back up");\
      YYERROR;							\
    }								\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

/* YYLLOC_DEFAULT -- Compute the default location (before the actions
   are run).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)         \
  Current.first_line   = Rhs[1].first_line;      \
  Current.first_column = Rhs[1].first_column;    \
  Current.last_line    = Rhs[N].last_line;       \
  Current.last_column  = Rhs[N].last_column;
#endif

/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (0)

# define YYDSYMPRINT(Args)			\
do {						\
  if (yydebug)					\
    yysymprint Args;				\
} while (0)

# define YYDSYMPRINTF(Title, Token, Value, Location)		\
do {								\
  if (yydebug)							\
    {								\
      YYFPRINTF (stderr, "%s ", Title);				\
      yysymprint (stderr, 					\
                  Token, Value);	\
      YYFPRINTF (stderr, "\n");					\
    }								\
} while (0)

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (cinluded).                                                   |
`------------------------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_stack_print (short *bottom, short *top)
#else
static void
yy_stack_print (bottom, top)
    short *bottom;
    short *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (/* Nothing. */; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yy_reduce_print (int yyrule)
#else
static void
yy_reduce_print (yyrule)
    int yyrule;
#endif
{
  int yyi;
  unsigned int yylineno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %u), ",
             yyrule - 1, yylineno);
  /* Print the symbols being reduced, and their result.  */
  for (yyi = yyprhs[yyrule]; 0 <= yyrhs[yyi]; yyi++)
    YYFPRINTF (stderr, "%s ", yytname [yyrhs[yyi]]);
  YYFPRINTF (stderr, "-> %s\n", yytname [yyr1[yyrule]]);
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (Rule);		\
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YYDSYMPRINT(Args)
# define YYDSYMPRINTF(Title, Token, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   SIZE_MAX < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#if YYMAXDEPTH == 0
# undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined (__GLIBC__) && defined (_STRING_H)
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
#   if defined (__STDC__) || defined (__cplusplus)
yystrlen (const char *yystr)
#   else
yystrlen (yystr)
     const char *yystr;
#   endif
{
  register const char *yys = yystr;

  while (*yys++ != '\0')
    continue;

  return yys - yystr - 1;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined (__GLIBC__) && defined (_STRING_H) && defined (_GNU_SOURCE)
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
#   if defined (__STDC__) || defined (__cplusplus)
yystpcpy (char *yydest, const char *yysrc)
#   else
yystpcpy (yydest, yysrc)
     char *yydest;
     const char *yysrc;
#   endif
{
  register char *yyd = yydest;
  register const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

#endif /* !YYERROR_VERBOSE */



#if YYDEBUG
/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yysymprint (FILE *yyoutput, int yytype, YYSTYPE *yyvaluep)
#else
static void
yysymprint (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  if (yytype < YYNTOKENS)
    {
      YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
# ifdef YYPRINT
      YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
    }
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  switch (yytype)
    {
      default:
        break;
    }
  YYFPRINTF (yyoutput, ")");
}

#endif /* ! YYDEBUG */
/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

#if defined (__STDC__) || defined (__cplusplus)
static void
yydestruct (int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yytype, yyvaluep)
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  /* Pacify ``unused variable'' warnings.  */
  (void) yyvaluep;

  switch (yytype)
    {

      default:
        break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM);
# else
int yyparse ();
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
# if defined (__STDC__) || defined (__cplusplus)
int yyparse (void *YYPARSE_PARAM)
# else
int yyparse (YYPARSE_PARAM)
  void *YYPARSE_PARAM;
# endif
#else /* ! YYPARSE_PARAM */
#if defined (__STDC__) || defined (__cplusplus)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  register int yystate;
  register int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  short	yyssa[YYINITDEPTH];
  short *yyss = yyssa;
  register short *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  register YYSTYPE *yyvsp;



#define YYPOPSTACK   (yyvsp--, yyssp--)

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* When reducing, the number of symbols on the RHS of the reduced
     rule.  */
  int yylen;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed. so pushing a state here evens the stacks.
     */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack. Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	short *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow ("parser stack overflow",
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyoverflowlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyoverflowlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	short *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyoverflowlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YYDSYMPRINTF ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */
  YYDPRINTF ((stderr, "Shifting token %s, ", yytname[yytoken]));

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;


  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  yystate = yyn;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 6:
#line 105 "btsrv-parser.y"
    { yyval.num = 1; }
    break;

  case 7:
#line 105 "btsrv-parser.y"
    { yyval.num = 0; }
    break;

  case 8:
#line 106 "btsrv-parser.y"
    { yyval.str = NULL; }
    break;

  case 10:
#line 108 "btsrv-parser.y"
    { waitsemi = 1; }
    break;

  case 17:
#line 122 "btsrv-parser.y"
    { managepin = yyvsp[0].num; }
    break;

  case 18:
#line 123 "btsrv-parser.y"
    { managekey = yyvsp[0].num; }
    break;

  case 19:
#line 124 "btsrv-parser.y"
    { startsvc = yyvsp[0].num; }
    break;

  case 20:
#line 125 "btsrv-parser.y"
    { initdev = yyvsp[0].num; }
    break;

  case 21:
#line 130 "btsrv-parser.y"
    {
				char	*name = yyvsp[0].str;
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
    break;

  case 22:
#line 147 "btsrv-parser.y"
    { devnum++; }
    break;

  case 25:
#line 157 "btsrv-parser.y"
    {
				dev->btname = strdup(yyvsp[0].str);
			}
    break;

  case 26:
#line 161 "btsrv-parser.y"
    {
				str2cod(yyvsp[0].str, &dev->cod);
				DBPRT("cod: %#.6x", dev->cod);
			}
    break;

  case 27:
#line 166 "btsrv-parser.y"
    {
				str2mask(role_map, yyvsp[0].str, &dev->role);
				;
			}
    break;

  case 28:
#line 171 "btsrv-parser.y"
    {
				str2mask(sec_mode_map, yyvsp[0].str, &dev->security);
				DBPRT("sec_mode: %#x", dev->security);
			}
    break;

  case 29:
#line 176 "btsrv-parser.y"
    {
				str2mask(scan_map, yyvsp[0].str, &dev->scan);
				DBPRT("scan_mode: %#x", dev->scan);

			}
    break;

  case 30:
#line 182 "btsrv-parser.y"
    {
				str2mask(pkt_type_map, yyvsp[0].str, &dev->pkt_type);
				DBPRT("pkt_type: %#x", dev->pkt_type);
			}
    break;

  case 31:
#line 194 "btsrv-parser.y"
    { 
				int	i;
				svc = &services[svcnum];
				DBPRT("found service: [%s]", yyvsp[0].str);
				for (i = 0; profiles[i].name; i++) {
					if (strcasecmp(profiles[i].name, yyvsp[0].str) == 0)
						break;
				}
				if (profiles[i].name == NULL) {
					BTERROR("Invalid profile name %s in config file %s line %d",
						yyvsp[0].str, config_file, yylineno);
					return -1;
				}
				svc->profile = &profiles[i];
			}
    break;

  case 32:
#line 211 "btsrv-parser.y"
    { svcnum++; }
    break;

  case 35:
#line 220 "btsrv-parser.y"
    { 
				DBPRT("found profile: %s", yyvsp[0].str); 
			}
    break;

  case 36:
#line 224 "btsrv-parser.y"
    { 
				struct affix_tupla	flags[] = {
						{SRV_FLAG_SOCKET, "socket"},
						{SRV_FLAG_RFCOMM_TTY|SRV_FLAG_SOCKET, "tty"},
						{SRV_FLAG_STD, "std"},
						{0, 0}
						};

				DBPRT("found flag list: [%s]", yyvsp[0].str);
				if (!str2mask(flags, yyvsp[0].str, &svc->flags)) {
					BTERROR("Invalid flags at line %d", yylineno);
					return -1;
				}
			}
    break;

  case 37:
#line 239 "btsrv-parser.y"
    { 
				strcpy(svc->cmd, yyvsp[0].str);
				DBPRT("found exec: %s", svc->cmd);
			}
    break;

  case 38:
#line 244 "btsrv-parser.y"
    {
				svc->name = strdup(yyvsp[0].str);
				DBPRT("Found name: %s\n", svc->name);
			}
    break;

  case 39:
#line 249 "btsrv-parser.y"
    {
				svc->prov = strdup(yyvsp[0].str);
			}
    break;

  case 40:
#line 253 "btsrv-parser.y"
    {
				svc->desc = strdup(yyvsp[0].str);
			}
    break;

  case 41:
#line 257 "btsrv-parser.y"
    {
				str2cod_svc(yyvsp[0].str, &svc->cod);
				DBPRT("cod: %#.6x", svc->cod);
			}
    break;

  case 42:
#line 262 "btsrv-parser.y"
    {
				str2sec_level(yyvsp[0].str, &svc->security);
			}
    break;

  case 43:
#line 266 "btsrv-parser.y"
    {
				svc->port = yyvsp[0].num;
			}
    break;

  case 44:
#line 271 "btsrv-parser.y"
    {
				DBPRT("Found active: %d\n", yyvsp[0].num);
				svc->active = yyvsp[0].num;
			}
    break;


    }

/* Line 999 of yacc.c.  */
#line 1384 "y.tab.c"

  yyvsp -= yylen;
  yyssp -= yylen;


  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (YYPACT_NINF < yyn && yyn < YYLAST)
	{
	  YYSIZE_T yysize = 0;
	  int yytype = YYTRANSLATE (yychar);
	  char *yymsg;
	  int yyx, yycount;

	  yycount = 0;
	  /* Start YYX at -YYN if negative to avoid negative indexes in
	     YYCHECK.  */
	  for (yyx = yyn < 0 ? -yyn : 0;
	       yyx < (int) (sizeof (yytname) / sizeof (char *)); yyx++)
	    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	      yysize += yystrlen (yytname[yyx]) + 15, yycount++;
	  yysize += yystrlen ("syntax error, unexpected ") + 1;
	  yysize += yystrlen (yytname[yytype]);
	  yymsg = (char *) YYSTACK_ALLOC (yysize);
	  if (yymsg != 0)
	    {
	      char *yyp = yystpcpy (yymsg, "syntax error, unexpected ");
	      yyp = yystpcpy (yyp, yytname[yytype]);

	      if (yycount < 5)
		{
		  yycount = 0;
		  for (yyx = yyn < 0 ? -yyn : 0;
		       yyx < (int) (sizeof (yytname) / sizeof (char *));
		       yyx++)
		    if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
		      {
			const char *yyq = ! yycount ? ", expecting " : " or ";
			yyp = yystpcpy (yyp, yyq);
			yyp = yystpcpy (yyp, yytname[yyx]);
			yycount++;
		      }
		}
	      yyerror (yymsg);
	      YYSTACK_FREE (yymsg);
	    }
	  else
	    yyerror ("syntax error; also virtual memory exhausted");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror ("syntax error");
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
	 error, discard it.  */

      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        {
	  /* Pop the error token.  */
          YYPOPSTACK;
	  /* Pop the rest of the stack.  */
	  while (yyss < yyssp)
	    {
	      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
	      yydestruct (yystos[*yyssp], yyvsp);
	      YYPOPSTACK;
	    }
	  YYABORT;
        }

      YYDSYMPRINTF ("Error: discarding", yytoken, &yylval, &yylloc);
      yydestruct (yytoken, &yylval);
      yychar = YYEMPTY;

    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*----------------------------------------------------.
| yyerrlab1 -- error raised explicitly by an action.  |
`----------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;

      YYDSYMPRINTF ("Error: popping", yystos[*yyssp], yyvsp, yylsp);
      yydestruct (yystos[yystate], yyvsp);
      yyvsp--;
      yystate = *--yyssp;

      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  YYDPRINTF ((stderr, "Shifting error token, "));

  *++yyvsp = yylval;


  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*----------------------------------------------.
| yyoverflowlab -- parser overflow comes here.  |
`----------------------------------------------*/
yyoverflowlab:
  yyerror ("parser stack overflow");
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  return yyresult;
}


#line 280 "btsrv-parser.y"


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


