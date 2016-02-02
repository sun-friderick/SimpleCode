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
   $Id: btdebug.h,v 1.20 2003/03/10 16:18:21 kds Exp $

   BTDEBUG - Debug primitives

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/


#ifndef _AFFIX_BTDEBUG_H
#define _AFFIX_BTDEBUG_H

#include <affix/bluetooth.h>

extern __u32	affix_dbmask;

#ifdef CONFIG_AFFIX_DEBUG
	#define __DBPRT(dbtype, fmt, args...) \
	({ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & (dbtype))) { \
			if ((affix_dbmask & DBFNAME) && ((dbtype) & DBFNAME)) { \
				printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ##args); \
			} else { \
				printk(fmt , ##args); \
			} \
		} \
	})
	#define DBDUMP(data, len) \
	({ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & DBHEXDUMP)) \
			printk_hexdump(data, len); \
	})
	
	#define DBPARSEHCI(data, len, pkttype, dir) \
	({ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & DBPARSE)) \
			printk_parse_hci(data, len, pkttype, dir); \
	})

	#define DBDUMPCHAR(data, len) \
	({ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & DBCHARDUMP)) \
			printk_chardump(data, len); \
	})

	#define DBPARSERFCOMM(data, len, dir) \
	({ \
		if ((affix_dbmask & FILEBIT) && (affix_dbmask & DBPARSE)) \
			printk_parse_rfcomm(data, len, dir); \
	})

#define BTASSERT(expr, func) \
	if (!(expr)) { \
        	printk( "Assertion failed! %s,%s,%s,line=%d\n",\
		#expr,__FILE__,__FUNCTION__,__LINE__); \
		func \
	}


#else /* CONFIG_AFFIX_DEBUG */

#define __DBPRT(dbtype, fmt, args...) 
#define DBDUMP(data, len)
#define DBPARSEHCI(data, len, pkttype, dir)
#define DBPARSERFCOMM(data, len, dir)
#define DBDUMPCHAR(data, len)

#define BTASSERT(expr, func)

#endif /* CONFIG_AFFIX_DEBUG */

#define DBPRT(fmt, args...)	__DBPRT(DBCTRL | DBFNAME, fmt , ##args)
#define _DBPRT(fmt, args...)	__DBPRT(DBCTRL, fmt , ##args)
#define DBFENTER		__DBPRT(DBFUNC | DBFNAME, "Entered\n")
#define DBFEXIT			__DBPRT(DBFUNC | DBFNAME, "Exited\n")


#define BTERROR(fmt, args...)	printk(KERN_ERR "%s: " fmt, __FUNCTION__ , ##args)
#define BTWARN(fmt, args...) 	printk(KERN_WARNING "%s: " fmt, __FUNCTION__ , ##args)
#define BTNOTICE(fmt, args...)	printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ##args)
#define BTINFO(fmt, args...)	printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ##args)
#define BTDEBUG(fmt, args...)	printk(KERN_DEBUG "%s: "  fmt, __FUNCTION__ , ##args)


//      For packet parsing
typedef volatile enum {
        TO_HOSTCTRL = 0,
        FROM_HOSTCTRL
} pkt_dir;

/* some functions to help... */
void printk_hexdump(const __u8 *p, __u32 len);
void printk_chardump(const __u8 *p, int len);

void printk_parse_hci(const __u8 *p, int data_size, int type, pkt_dir dir); 
void printk_parse_rfcomm(const __u8 *p, int len, pkt_dir dir);


#endif
