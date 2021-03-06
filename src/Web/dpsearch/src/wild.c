/* Copyright (C) 2003-2005 Datapark corp. All rights reserved.
   Copyright (C) 2000-2002 Lavtech.com corp. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "dps_common.h"
#include "dps_wild.h"
#include "dps_charsetutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>


#if 0

int DpsWildCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; wexp[y]; ++y, ++x) {
	if ((!str[x]) && (wexp[y] != '*'))return -1;
	if (wexp[y] == '*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCmp(&str[x++], &wexp[y])) != 1)return ret;
	    }
	    return -1;
	}else
	if ((wexp[y] != '?') && (str[x] != wexp[y]))return 1;
    }
    return (str[x] != '\0');
}

#else
int DpsWildCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {
      switch(wexp[y]) {
      case '*':
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      case '?':
	break;
      default:
	if (str[x] != wexp[y]) return 1;
	break;
      }
    }
    if (str[x] != '\0') return 1;
    while(wexp[y] == '*' || wexp[y] == '?') y++;
    if (wexp[y] == '\0') return 0;
    return -1;
}

#endif



#if 0

int DpsWildCaseCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; wexp[y]; ++y, ++x) {
	if ((!str[x]) && (wexp[y] != '*'))return -1;
	if (wexp[y] == '*') {
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCaseCmp(&str[x++], &wexp[y])) != 1)return ret;
	    }
	    return -1;
	}else
	if ((wexp[y] != '?') && (dps_tolower(str[x]) != dps_tolower(wexp[y]))) return 1;
    }
    return (str[x] != '\0');
}

#else

int DpsWildCaseCmp(const char *str, const char *wexp) {
    register size_t x, y;

    for (x = 0, y = 0; str[x] && wexp[y]; ++y, ++x) {
      switch(wexp[y]) {
      case '*':
	    while (wexp[++y] == '*');
	    if (!wexp[y])return 0;
	    while (str[x]) {
		register int ret;
		if ((ret = DpsWildCaseCmp(&str[x++], &wexp[y])) != 1) return ret;
	    }
	    return -1;
      case '?':
	break;
      default:
	if (dps_tolower(str[x]) != dps_tolower(wexp[y])) return 1;
	break;
      }
    }
    if (str[x] != '\0') return 1;
    while(wexp[y] == '*' || wexp[y] == '?') y++;
    if (wexp[y] == '\0') return 0;
    return -1;
/*    return (str[x] != '\0') ? 1 : ((wexp[y] == '*' || wexp[y] == '?' || wexp[y] == '\0') ? 0 : -1);*/
}

#endif
