/* Copyright (C) 2005-2007 Datapark corp. All rights reserved.

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

#ifndef _DPS_CHARSETUTILS_H
#define _DPS_CHARSETUTILS_H

extern __C_LINK void* dps_memmove(void *dst0, const void *src0, size_t length);
extern __C_LINK void* dps_memcpy(void *dst0, const void *src0, size_t length);
extern __C_LINK void* dps_strncpy(void *dst0, const void *src0, size_t length);
extern __C_LINK void* dps_strcpy(void *dst0, const void *src0);
extern __C_LINK void* dps_strcat(void *dst0, const void *src0);
extern __C_LINK void* dps_strncat(void *dst0, const void *src0, size_t length);
extern __C_LINK size_t dps_strlen(const char *src);
extern __C_LINK int dps_tolower(int c);
extern __C_LINK void dps_mstr(char *s, const char *src, size_t l1, size_t l2);

#ifndef DPS_NULL2EMPTY
#define DPS_NULL2EMPTY(x)	((x)?(x):"")
#endif

#ifndef DPS_NULL2STR
#define DPS_NULL2STR(x)	        ((x)?(x):"<NULL>")
#endif

typedef struct {
  size_t allocated_size;
  size_t data_size;
  size_t page_size;
  int    freeme;
  char   *data;
} DPS_DSTR;


DPS_DSTR *DpsDSTRInit(DPS_DSTR *dstr, size_t page_size);
void DpsDSTRFree(DPS_DSTR *dstr);
size_t DpsDSTRAppend(DPS_DSTR *dstr, const void *, size_t append_size);
size_t DpsDSTRAppendStr(DPS_DSTR *dstr, const char *);
size_t DpsDSTRAppendStrWithSpace(DPS_DSTR *dstr, const char *);
size_t DpsDSTRAppendUni(DPS_DSTR *dstr, const dpsunicode_t);
size_t DpsDSTRAppendUniStr(DPS_DSTR *dstr, const dpsunicode_t *);
size_t DpsDSTRAppendUniWithSpace(DPS_DSTR *dstr, const dpsunicode_t *data);


#endif /* _DPS_UTILS_H */
