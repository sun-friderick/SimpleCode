/* -*-   Mode:C; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001, 2002, 2004 Nokia Corporation
   Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
   Original Author: Andre Hebben <hebben@cs.uni-bonn.de>

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
   $Id: affix_hid.h,v 1.5 2004/06/30 16:07:53 hebben Exp $

   affix_hid.h - HID kernel module
   
*/


#ifndef __AFFIX_HID_H
#define __AFFIX_HID_H

#include <linux/list.h>

#include <affix/bluetooth.h>
#include <affix/btdebug.h>
#include <affix/hci.h>
#include <affix/l2cap.h>


#define HIDP_MIN_MTU           48
#define HIDP_DEF_MTU           672

#define HIDP_CTRL_PSM          0x0011
#define HIDP_INTR_PSM          0x0013      /* L2CAP PSMs for hidp control
					      and interrupt channel */

extern l2cap_proto_ops hidp_intr_ops;
extern l2cap_proto_ops hidp_ctrl_ops;

struct hidp_connection {
	struct list_head list;

	BD_ADDR bda;
	struct hidp_conn_info info;

	unsigned long state;
#define HIDP_CONN_CLOSED            0
#define HIDP_CONN_CLOSING           1
#define HIDP_CONN_CONFIG            2
#define HIDP_CONN_OPEN              3

	l2cap_ch *ctrl_ch;                /* L2CAP channel identifiers */
	l2cap_ch *intr_ch;                /* for ctrl and intr channel */

	unsigned long flags;
	unsigned long idle_to;


	struct hid_device *hid;
};

#endif /* __AFFIX_HID_H */
