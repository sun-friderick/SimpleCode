/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 - 2004 Nokia Corporation
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
   $Id: bthidctl.c,v 1.14 2004/07/15 16:01:33 hoffmeis Exp $

   bthidctl - Program for controlling devices used by the bluetooth
   hid kernel driver

   Original implementation of bthidctl by
   Anselm Martin Hoffmeister - Rheinische Friedrich-Wilhelms-Universität BONN
   using great parts of the bluez userspace hid tool by Peter Klausler
*/

#include <affix/config.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/errno.h>

#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <termios.h>
#include <dirent.h>
#include <sys/types.h>
#include <dirent.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>
#include <affix/utils.h>

#define PSM_SDP 0x0001

#define	DIRECTORY_HIDDB "/var/spool/affix/hiddb"

static int		sdp_sock = -1;
static unsigned char *	sdp_attr = NULL;
static    int    	sdp_attr_bytes = 0;

static  unsigned short	sdp_tid;         /* transaction id of last SDP request */
static  unsigned long	sdp_handle;

INQUIRY_ITEM		devs[20]; /* For device inquiries, like 'bthidctl connect discovery' */
static int		found_devs = 0;

struct sdp_data {
	enum sdp_data_type {
		sdp_data_nil,
		sdp_data_uint,
		sdp_data_sint,
		sdp_data_uuid,
		sdp_data_str,
		sdp_data_bool,
		sdp_data_seq,
		sdp_data_alt,
		sdp_data_url
	} type;
	int     bytes;
	union {
		unsigned long uint;     /* uint, bool */
		long    sint;           /* sint */
		char    ch [1];         /* str, url, uuid */
		struct sdp_seq {        /* seq, alt */
			int     items;
			struct sdp_data **item;
		} seq;
	} u;
};

struct hidp_ioc iocstruct;

int bthid_channel_connect (BD_ADDR *bda, int psm) {

	int     sock, rv;
	char    buf [32];
	struct sockaddr_affix l2sa;

	if (0 > (sock = socket (PF_AFFIX, SOCK_SEQPACKET, BTPROTO_L2CAP))) {
		fprintf ( stderr, "Cannot create L2CAP socket!\n" );
		return sock;
	}
	l2sa.family = AF_AFFIX;
	l2sa.port = psm;  //check byteorder
	l2sa.bda = *bda;
	_bda2str (buf, &l2sa.bda);
	l2sa.devnum = HCIDEV_ANY;
	if ((rv = connect (sock, (struct sockaddr *) &l2sa, sizeof l2sa))) {
		fprintf ( stderr, "Cannot connect L2CAP socket %d to " \
				 "BD %s PSM %d!\n", sock, buf, psm);
		close (sock);
		return rv;
	}
	return sock;
}


int dohelp ( char * topic ) {
	char	*p = (topic == NULL ? "" : topic );
	if ( 0 == strcmp ( p, "connect" ) ) {
		fprintf ( stdout, "bthidctl connect <bda>|'discovery'\n" \
			"This command is used to add devices to the HID device database (which lives\n" \
			"in " DIRECTORY_HIDDB " or reconnect those that had a connection to another\n" \
			"machine in the meantime. For 'connecting' a device, it must be in range and\n" \
			"ready to connect - many devices have a button you can press to force them into" \
			"a 'connectable' state.\n" \
			"After the device has been added to the database, you can (e.g. after a reboot,\n" \
			"or at any later time) use the 'bthidctl listen' command to request the kernel\n" \
			"to recognize that device again.\n" \
			"The 'bthidctl connect' command needs a parameter, which is either a valid\n" \
			"bluetooth device address ('bda') of the device or 'discovery' which tells\n" \
			"bthidctl to run a discovery and connect all available HID devices in range.\n" );
	} else if ( 0 == strcmp ( p, "listen" ) ) {
		fprintf ( stdout, "bthidctl listen [--active] <bda>|'all'\n" \
			"This command is used to notify the kernel of another device to allow HID\n" \
			"connections to/from. The given Bluetooth address must be registered to the\n" \
			"HID database (use 'bthidctl connect' command) and will be activated by this call.\n" \
			"You can give the string 'all' instead of a Bluetooth address, which will cause\n" \
			"all devices currently in database to be added to the kernel's device list.\n" \
			"This will prove particularly useful for init script usage or similar.\n" \
			"The '--active' option makes the kernel perform an active connection to the\n" \
			"given device (most devices connect from themselves, provided this machine is\n" \
			"the last one they had connection with, when a key is pressed or similar events\n" \
			"occur) - this will only be necessary for non-auto-reconnect capable HID devices\n" \
			"that don't announce their lack of capability.\n" \
			"If you used your HID device on another machine since you 'connect'ed it to this\n" \
			"one, you will have to do a 'bthidctl connect' again to update the HID device's\n" \
			"memory of which machine it is bundled to.\n" );
	} else if ( 0 == strcmp ( p, "disconnect" ) ) {
		fprintf ( stdout, "bthidctl disconnect <bda>|'all'\n" \
			"This command is used to delete devices from the kernel's list of active and\n" \
			"allowed devices. In case a connection to that device is active at time of\n" \
			"command execution, that connection will be terminated. This command has no\n" \
			"impact on the device database, so the device can be reconnected with the\n" \
			"'bthidctl connect' command at a later time.\n" );
	} else if ( 0 == strcmp ( p, "delete" ) ) {
		fprintf ( stdout, "bthidctl delete <bda>\n" \
			"This command removes the given device permanently from the device database.\n" \
			"In case this device is listed as active or connected in the kernel, it will\n" \
			"be disconnected. There is intentionally no 'all' parameter allowed for this\n" \
			"command. You can manually delete database files in case you need this.\n" );
	} else if ( 0 == strcmp ( p, "status" ) ) {
		fprintf ( stdout, "bthidctl status [discovery]\n" \
			"This command is used to retrieve a list of HID devices. It lists\n" \
			"those devices currently connected as well as the ones the kernel\n" \
			"currently has no connection to, but would accept HID events from, as well\n" \
			"as those only listed in the database of known devices, but which currently\n" \
			"are not allowed to connect to the HID daemon.\n" \
			"When specifying 'discovery', a discovery for HID devices will be done first;\n" \
			"any newly found HID devices will be listed along those already known and\n" \
			"those devices which are present in the database.\n" \
			"Discovered devices without database entry are listed without further\n" \
			"verification, so neither name nor type (HID or not HID) of those is known.\n" \
			"Sample output of 'bthidctl status discovery':\n=============\n" \
			"Performing device inquiry for 8 seconds...done: 4 'connectable' devices found.\n" \
			"Bluetooth address  status\n" \
			"00:00:00:c0:ff:ee  ACTIVE   Logitake bluetooth coffee mug thermic sensor\n" \
			"00:00:de:ad:be:ef  STANDBY  Epics input device\n" \
			"00:00:00:af:f1:c5  DATABASE Anymake WithoutAnyPurpose device\n" \
			"01:23:45:67:89:ab  IN RANGE\n" );
			
	} else if ( 0 == strcmp ( p, "help" ) ) {
		fprintf ( stdout, "bthidctl help <topic>\n" \
			"This command is used to retrieve information about bthidctl functions.\n" \
			"As you see this text, you obviously succeeded in using the\n" \
			"  'bthidctl help help'\n" \
			"command. To retrieve a list of available commands, please enter\n" \
			"  'bthidctl help'\n" );
	} else if ( 0 == strcmp ( p, "" ) ) {
		fprintf ( stdout, "bthidctl - Bluetooth Human Interface Device Control Utility for Affix stack\n" \
			"The following commands are valid:\n" \
			"   connect  listen  disconnect  delete  status  help\n" \
			"which will be discussed in detail when you enter 'bthidctl help <command>'\n" );
	} else {
		fprintf ( stdout, "No help for topic '%s'.\n" \
			"Use 'bthidctl help' to get a list of available commands/help topics!\n", p );
	}
	return	0;
}

static void hid_descriptor (struct sdp_data *classtype, struct sdp_data *classval) {
	// We received a HIDP descriptor. Copy it so that it can be used (iocstruct)
	if (classtype->type != sdp_data_uint ||
		classtype->u.uint != 0x22 /* report descriptor */ ||
		classval->type != sdp_data_str) {
		fprintf (stderr, "Device sent a bad HID descriptor\n");
		return;
	}
	if ( NULL == iocstruct.conn_info.rd_data ) {
		if ( NULL == ( iocstruct.conn_info.rd_data = malloc ( ( 1023 + classval->bytes ) & 0x400 ) ) ) {
			fprintf ( stderr, "Failed to allocate memory for the HID report descriptor!\n" );
			return;
		}
		iocstruct.conn_info.rd_size = classval->bytes;
		memcpy ( iocstruct.conn_info.rd_data, classval->u.ch, classval->bytes );
	}
	return;
}


static void sdp_free (struct sdp_data *s) {
	int     i;
	if (s->type == sdp_data_seq || s->type == sdp_data_alt) {
		for (i = 0; i < s->u.seq.items; i++) {
			sdp_free (s->u.seq.item [i]);
		}
	}
	free (s);
}

static void sdp_add_to_seq (struct sdp_data *s, struct sdp_data *it) {
	s->u.seq.item = realloc (s->u.seq.item, sizeof *s->u.seq.item * (s->u.seq.items + 1));
	s->u.seq.item [s->u.seq.items++] = it;
}

static int sdp_get_data_length (unsigned char *buff, int *idx) {
	int     i = *idx;
	int     tcode;
	int     n;
	tcode = buff [i++];
	if (!(tcode >> 3)) {
		n = 0;  /* nil */
	} else {
		switch (tcode & 7) {
			default:
				n = 1 << (tcode & 7);
				break;
			case 5:
				n = buff [i++];
				break;
			case 6:
				n = buff [i+0] << 8 |
					buff [i+1] << 0;
				i += 2;
				break;
			case 7:
				n = (unsigned long) buff [i+0] << 24 |
					(unsigned long) buff [i+1] << 16 |
					(unsigned long) buff [i+2] <<  8 |
					(unsigned long) buff [i+3] <<  0;
				i += 4;
				break;
		}
	}
	*idx = i;
	return n;
}

static void hid_attr_pnp (unsigned long attr, struct sdp_data *val) {
	// We received in 1st SDP query an attribute. If usable, copy to iocstruct
	switch (attr) { 
		case 0x201:
			if (val->type == sdp_data_uint) {
				iocstruct.conn_info.vendor = val->u.uint & 0xffff;
			}
			break;
		case 0x202:
			if (val->type == sdp_data_uint) {
				iocstruct.conn_info.product = val->u.uint & 0xffff;
			}
			break;
		case 0x203:
			if ( val->type == sdp_data_uint) {
				iocstruct.conn_info.version = (int)(val->u.uint) & 0xffff;
			}
			break;
	}
}

static void hid_attr (unsigned long attr, struct sdp_data *val) {
	// We received in 2nd SDP query an attribute. If usable, copy to iocstruct
	int i;
	char * devname;
	switch (attr) {
		case 0x100: // Device name
			if (val->type == sdp_data_str) {
				devname = strdup (val->u.ch);
				if (devname) {
					strncpy ( iocstruct.conn_info.name, devname, 127 );
					iocstruct.conn_info.name[127] = 0;
				}
			}
			break;
		case 0x101: // device description - not used right now.
		case 0x102: // vendor name - not used right now.
			break;
		case 0x201: // Hid profile version
			if (val->type == sdp_data_uint) {
				//iocstruct.conn_info.version = (int)(val->u.uint) & 0xffff;
				// This won't matter, version is expected to be the USB-type
				// of manufacturer/product/version
				;
			}
			break;
		case 0x20b: // Hid parser version
			if (val->type == sdp_data_uint) {
				iocstruct.conn_info.parser = (int)(val->u.uint) & 0xffff;
			}
			break;
		case 0x203: // country code ("0" means not localized)
			if (val->type == sdp_data_uint) {
				iocstruct.conn_info.country = (int)(val->u.uint) & 0xff;
			}
			break;
		case 0x205: // ReconnectInitiate: If 0, needs active reconnection
			if ( val->type == sdp_data_bool ) {
				if ( ((int)(val->u.uint) & 0xff) == 0 ) {
					// Device needs '--active' flag not only on first connection
					//fprintf ( stdout, "DEBUG: ACTIVE_ADD permanently forced.\n" );
					iocstruct.status = HIDP_STATUS_ACTIVE_ADD;
					;
				}
			}
			break;
		case 0x206: // HID descriptor - check and if possible, use it
			if (val->type == sdp_data_seq) {
				for (i = 0; i < val->u.seq.items; i++) {
					if (val->u.seq.item [i]->type == sdp_data_seq && val->u.seq.item [i]->u.seq.items >= 2) {
						hid_descriptor (val->u.seq.item [i]->u.seq.item [0],
								val->u.seq.item [i]->u.seq.item [1]);
					}
				}
			}
			break;
		default:
			break;
	}
}
	
static int sdp_getattr (unsigned char *cont) {
	// Perform a ServiceSearchAttributeRequest (both for PNP and HIDP query)
	// Use the latest retrieved SDPHandle for this.
	char    buf [64];
	int     n = 0, plen, clen, i;

	if (!cont) cont = "\0";
	
	sdp_tid++;

	buf [n++] = 0x04;       /* SDP_ServiceAttributeRequest */
	buf [n++] = sdp_tid >> 8;
	buf [n++] = sdp_tid;
	plen = n, n += 2;               /* 2-byte parameter length */
	buf [n++] = sdp_handle >> 24;   /* 32-bit handle, big-endian */
	buf [n++] = sdp_handle >> 16;
	buf [n++] = sdp_handle >> 8;
	buf [n++] = sdp_handle;
	buf [n++] = 1024 >> 8;          /* max byte count */
	buf [n++] = (char) 1024;
	buf [n++] = sdp_data_seq << 3 | 0x05;   /* Data element sequence */
	buf [n++] = 5;                  /* 5 bytes in sequence */
	buf [n++] = sdp_data_uint << 3 | 2;     /* 4-byte unsigned int */
	buf [n++] = 0x0000 >> 8;        /* range 0x0000-0xffff */
	buf [n++] = 0x0000;
	buf [n++] = 0xffff >> 8;
	buf [n++] = 0xffff & 0xff;	/* I assume 0xffff was meant here, not 0000 */
	buf [n++] = clen = *cont++;     /* continuation state */
	for (i = 0; i < clen; i++)  buf [n++] = *cont++;
	buf [plen] = (n - (plen + 2)) >> 8;
	buf [plen+1] = n - (plen + 2);

	if ( 0 > (n = send (sdp_sock, buf, n, 0))) {
		fprintf (stderr, "SDP send failed!\n");
		return -1;
	}
	//fprintf ( stdout, "." ); fflush ( stdout );
	return 0;
}

static struct sdp_data * sdp_scan (unsigned char *data, int *idx) {

	int     i, lim;
	struct sdp_data *s = NULL, *it;
	enum sdp_data_type tcode = data [*idx] >> 3;
	int     len = sdp_get_data_length (data, idx);

	switch (tcode) {
		default:
		case sdp_data_nil:
			s = malloc (sizeof *s);
			memset (s, 0, sizeof *s);
			s->type = tcode;
			s->bytes = len;
			*idx += len;
			break;
		case sdp_data_uint:
		case sdp_data_sint:
		case sdp_data_bool:
			s = malloc (sizeof *s);
			memset (s, 0, sizeof *s);
			s->type = tcode;
			s->bytes = len;
			for (i = 0; i < len; i++)
				s->u.uint = s->u.uint << 8 | data [*idx + i];
			*idx += len;
			break;
		case sdp_data_uuid:
		case sdp_data_str:
		case sdp_data_url:
			s = malloc (sizeof *s + len + 1);
			memset (s, 0, sizeof *s + len + 1);
			s->type = tcode;
			s->bytes = len;
			for (i = 0; i < len; i++)
				s->u.ch [i] = data [*idx + i];
			*idx += len;
			break;
		case sdp_data_seq:
		case sdp_data_alt:
			s = malloc (sizeof *s);
			memset (s, 0, sizeof *s);
			s->type = tcode;
			s->bytes = len;
			lim = *idx + len;
			while (*idx < lim) {
				it = sdp_scan (data, idx);
				sdp_add_to_seq (s, it);
			}
			break;
	}

	return s;
}



int sdp_resp_pnp (unsigned char *data, int nbytes) {
	// Response handler for the 1st (PNP) SDP query
	int     id, parmlen;
	int     total_ct, curr_ct, attr_bytes;
	int     i;
	unsigned short tid;
        struct sdp_data *s;
	
	id = data [0];
	tid = data [1] << 8 | data [2];         /* SDP is big-endian */
	parmlen = data [3] << 8 | data [4];
	data += 5;
	if (nbytes < 5) {
		//fprintf ( stdout, "NOTICE: Device without PNP record (short read at %d bytes)", nbytes);
		return 2;
	}
	if (parmlen > nbytes - 5) {
		//fprintf( stdout, "NOTICE: PNP-record invalid (bad param " 
		//		"length %d; PDU size %d, id 0x%x, tid 0x%x)\n",
		//		parmlen, nbytes, id, tid);
		return 2;
	}
	if (id == 0x01 /* SDP_ErrorResponse */) {
		//if (parmlen < 2)
		//	fprintf ( stdout, "NOTICE: Retrieval of PNP info record yielded unspecific error!\n");
		//else
		//	fprintf ( stdout, "NOTICE: Condition 0x%x during PNP info record retrieval!\n", data [0] << 8 | data [1]);
		return 2;
	}
	if (id == 0x03 /* SDP_ServiceSearchResponse */) {
		if (parmlen < 9) {
			//fprintf ( stdout, "NOTICE: No PNP record (SDP_ServiceSearchResponse too short: %d bytes, parm len %d)!\n", nbytes, parmlen);
			return 2;
		}
		if (tid != sdp_tid) {
			fprintf ( stdout, "NOTICE: PNP record transaction has wrong ID (0x%x != 0x%x) - ignore PNP info!\n",
					tid, sdp_tid);
			return 2;
		}
		total_ct = data [0] << 8 | data [1];
		curr_ct = data [2] << 8 | data [3];
		if (total_ct < 1) {
			//fprintf ( stdout, "NOTICE: No PNP information for this device could be retrieved!\n");
			return 2;
		}
		if (total_ct > 1 || curr_ct != 1) {
			fprintf ( stdout, "NOTICE: Invalid PNP SDP response: too many handles (total %d, curr %d)!\n", total_ct, curr_ct);
			return 2;
		}
		sdp_handle = (unsigned long) data [4] << 24 |
			(unsigned long) data [5] << 16 |
			(unsigned long) data [6] <<  8 |
			(unsigned long) data [7] <<  0;
		sdp_getattr (NULL);
		return	-2;
	}
	if (id != 0x05 /* SDP_ServiceAttributeResponse */) {
		fprintf ( stderr, "The SDP query for PNP info gave an unexpected SDP response id=0x%x!\n", id);
		return 1;
	}
	attr_bytes = data [0] << 8 | data [1];
	data += 2;
	sdp_attr = realloc (sdp_attr, sdp_attr_bytes + attr_bytes);
	memcpy (sdp_attr + sdp_attr_bytes, data, attr_bytes);
	sdp_attr_bytes += attr_bytes;

	if (data [attr_bytes]) {
		/* There's a continuation, go get it */
		sdp_getattr (&data [attr_bytes]);
		i = recv (sdp_sock, data, 1024, 0); // 1024 == hardcoded length of "data" buffer
		return sdp_resp_pnp ( data, i );
	}
	i = 0;
	s = sdp_scan (sdp_attr, &i);
	free (sdp_attr);
	sdp_attr = NULL;
	
	if (i != sdp_attr_bytes) {
		fprintf ( stderr, "The PNP-Info (SDP attribute data element sequence) " \
				"has bad length (%d != %d)!\n", i, sdp_attr_bytes);
		sdp_free (s);
		return 1;
	}

	if (!s) {
		fprintf ( stdout, "NOTICE: PNP info: Invalid SDP record ignored!\n");
		return 2;
	}
	if (s->type != sdp_data_seq) {
		fprintf ( stderr, "The SDP attributes returned (answering the query for PNP info) are not "
				"a data element sequence!\n");
		sdp_free (s);
		return 1;
	}
	if (s->u.seq.items & 1) {
		fprintf ( stderr, "The SDP attribute data element sequence (answering the query for PNP info) " \
				"has an odd number of elements!\n");
	}
        for (i = 0; i < s->u.seq.items - 1; i += 2) {
		if (s->u.seq.item [i]->type == sdp_data_uint) {
				hid_attr_pnp (s->u.seq.item [i]->u.uint,
						s->u.seq.item [i+1]);
		} else {
			fprintf ( stderr, "The SDP attribute data element "
					"sequence (answering the query for PNP info) has non-integer value in "
					"item[%d]!\n", i);
		}
	}
	sdp_free (s);
	return	0;
}

int sdp_resp (unsigned char *data, int nbytes) {
	// Response handler for the 2nd (HIDP) SDP query
	int     id, parmlen;
	int     total_ct, curr_ct, attr_bytes;
	int     i;
	unsigned short tid;
        struct sdp_data *s;
	
	id = data [0];
	tid = data [1] << 8 | data [2];         /* SDP is big-endian */
	parmlen = data [3] << 8 | data [4];
	data += 5;

	if (nbytes < 5) {
		fprintf ( stderr, "SDP response invalid (response too small, %d bytes)", nbytes);
		return 1;
	}
	if (parmlen > nbytes - 5) {
		fprintf( stderr, "SDP response invalid (has bad parameter " \
				"length %d (PDU size %d), id 0x%x, tid 0x%x)\n",
				parmlen, nbytes, id, tid);
		return 1;
	}
	if (id == 0x01 /* SDP_ErrorResponse */) {
		if (parmlen < 2)
			fprintf ( stderr, "SDP response claims unspecified error condition while retrieving HID info!\n");
		else
			fprintf ( stderr, "SDP response claims error condition 0x%x while retrieving HID info!\n", data [0] << 8 | data [1]);
		return 1;
	}
	if (id == 0x03 /* SDP_ServiceSearchResponse */) {
		if (parmlen < 9) {
			fprintf ( stderr, "Invalid SDP_ServiceSearchResponse (too short: %d bytes, parm len %d)!\n", nbytes, parmlen);
			return 1;
		}
		if (tid != sdp_tid) {
			fprintf ( stderr, "SPD ServiceSearchResponse for PNP has wrong transaction ID (0x%x != 0x%x)!\n",
					tid, sdp_tid);
			return 1;
		}
		total_ct = data [0] << 8 | data [1];
		curr_ct = data [2] << 8 | data [3];
		if (total_ct < 1) {
			fprintf ( stderr, "This device does not offer HIDP information (no HID descriptor available)!\n");
			return 1;
		}
		if (total_ct > 1 || curr_ct != 1) {
			fprintf ( stderr, "The SDP response for HIDP info contains too many handles (total %d, curr %d)!\n", total_ct, curr_ct);
			return 1;
		}
		sdp_handle = (unsigned long) data [4] << 24 |
			(unsigned long) data [5] << 16 |
			(unsigned long) data [6] <<  8 |
			(unsigned long) data [7] <<  0;
		//fprintf ( stdout, "get HID info.." );
		sdp_getattr (NULL);
		return	-2;
	}
	if (id != 0x05 /* SDP_ServiceAttributeResponse */) {
		fprintf ( stderr, "The SDP query for PNP info gave an unexpected SDP response id=0x%x!\n", id);
		return 1;
	}
	attr_bytes = data [0] << 8 | data [1];
	data += 2;
	sdp_attr = realloc (sdp_attr, sdp_attr_bytes + attr_bytes);
	memcpy (sdp_attr + sdp_attr_bytes, data, attr_bytes);
	sdp_attr_bytes += attr_bytes;

	if (data [attr_bytes]) {
		/* There's a continuation, go get it */
		//fprintf ( stdout, "." ); fflush ( stdout );
		sdp_getattr (&data [attr_bytes]);
		//fprintf ( stdout, "." ); fflush ( stdout );
		i = recv (sdp_sock, data, 1024, 0); // 1024 == hardcoded length of "data" buffer
		return sdp_resp ( data, i );
	}
	i = 0;
	s = sdp_scan (sdp_attr, &i);
	free (sdp_attr);
	sdp_attr = NULL;
	
	if (i != sdp_attr_bytes) {
		fprintf ( stderr, "The PNP-Info (SDP attribute data element sequence) " \
				"has bad length (%d != %d)!\n", i, sdp_attr_bytes);
		sdp_free (s);
		return 1;
	}

	if (!s) {
		fprintf ( stderr, "The SDP query for HIDP info returned no SDP attributes!\n");
		return 1;
	}
	if (s->type != sdp_data_seq) {
		fprintf ( stderr, "The SDP attributes returned (answering the query for HIDP info) are not "
				"a data element sequence!\n");
		sdp_free (s);
		return 1;
	}
	if (s->u.seq.items & 1) {
		fprintf ( stderr, "The SDP attribute data element sequence (answering the query for PNP info) " \
				"has an odd number of elements!\n");
	}
        for (i = 0; i < s->u.seq.items - 1; i += 2) {
		if (s->u.seq.item [i]->type == sdp_data_uint)
			hid_attr (s->u.seq.item [i]->u.uint,
					s->u.seq.item [i+1]);
		else
			fprintf ( stderr, "The SDP attribute data element " \
					"sequence (answering the query for HIDP info) has non-integer value in " \
					"item[%d]!\n", i);
	}
	sdp_free (s);
	return	0;
}

int sdp_probe_pnp ( BD_ADDR * bda ) /* Search for PNP info */
{
	int	n = 0, plen;
	unsigned char	buf[32];
	unsigned char	lbuf[1024];
	if ( sdp_sock >= 0 ) {
		close ( sdp_sock );
		if ( sdp_attr ) free ( sdp_attr );
		sdp_attr = NULL;
	}
	if ( ( sdp_sock = bthid_channel_connect ( bda, PSM_SDP ) ) < 0 ) {
		fprintf ( stderr, "Failed to connect SDP channel!\n" );
		return	1;
	}
	sdp_tid = 0;	/* First query at all, so our TransactionID is 0 (increasing) */
	sdp_attr_bytes = n = 0;
	buf [n++] = 0x02;			/* SDP_ServiceSearchRequest */
	buf [n++] = sdp_tid >> 8;
	buf [n++] = sdp_tid;
	plen = n, n += 2;			/* 2 bytes for param len */
	buf [n++] = sdp_data_seq << 3 | 0x05;	/* Data element sequence */
	buf [n++] = 3;				/* 3 bytes in data element sequence */
	buf [n++] = (sdp_data_uuid << 3 | 0x01) & 0xff;	/* 2-byte UUID */
	buf [n++] = SDP_UUID_PNP_INFO >> 8;		/* PNPINFO service UUID */
	buf [n++] = SDP_UUID_PNP_INFO & 0xff;
	buf [n++] = 1 >> 8;			/* One handle max */
	buf [n++] = 1;
	buf [n++] = 0;				/* No continuation code */
	buf [plen] = ( n - (plen + 2) ) >> 8;
	buf [plen+1] = n - (plen + 2);
	if ( ( n = send (sdp_sock, buf, n, 0) ) < 0 ) {
		close ( sdp_sock );
		sdp_sock = -1;
		fprintf ( stderr, "Sending SDP request failed!\n" );
		return	1;
	}
	if ( 1 > ( n = recv (sdp_sock, lbuf, sizeof lbuf, 0) ) ) {
		fprintf ( stderr, "Failed to receive answer for ServiceSearchRequest!\n" );
		return	1;
	}
	n = sdp_resp_pnp ( lbuf, n );
	if ( n == 2 ) {
		// No PNP record retrieved, fake it
		// This is necessary e.g. for the Logitech Presenter - it does not offer
		// a valid PNP record. This info would be nice-to-have in /proc/bus/input/...
		// but if it cannot be retrieved, best-effort is to set it to 0x0000
		iocstruct.conn_info.vendor = 0;
		iocstruct.conn_info.product = 0;
		iocstruct.conn_info.version = 0;
		return	0;
	}
	if ( n != -2 ) {
		fprintf ( stderr, "Failed to send SDP attrib request!\n" );
		return	1;
	}
	if ( 1 > ( n = recv (sdp_sock, lbuf, sizeof lbuf, 0) ) ) {
		fprintf ( stderr, "Failed to receive answer for SDP AttributeRequest!\n" );
		return	1;
	}
	n = sdp_resp_pnp ( lbuf, n );
	return	0;
}
int sdp_probe ( BD_ADDR * bda )
{
	int	n = 0, plen;
	unsigned char	buf[32];
	unsigned char	lbuf[1024];
	if ( sdp_sock >= 0 ) {
		close ( sdp_sock );
		if ( sdp_attr ) free ( sdp_attr );
		sdp_attr = NULL;
	}
	if ( ( sdp_sock = bthid_channel_connect ( bda, PSM_SDP ) ) < 0 ) {
		fprintf ( stderr, "Failed to open a SDP connection to the HID device (bthid_channel_connect)!\n" );
		return	1;
	}
	sdp_attr_bytes = n = 0;
	buf [n++] = 0x02;			/* SDP_ServiceSearchRequest */
	buf [n++] = sdp_tid >> 8;
	buf [n++] = sdp_tid;
	plen = n, n += 2;			/* 2 bytes for param len */
	buf [n++] = sdp_data_seq << 3 | 0x05;	/* Data element sequence */
	buf [n++] = 3;				/* 3 bytes in data element sequence */
	buf [n++] = (sdp_data_uuid << 3 | 0x01) & 0xff;	/* 2-byte UUID */
	buf [n++] = 0x1124 >> 8;		/* HIDP service UUID - seems not to have a #define in AFFIX, so numeric constant used here */
	buf [n++] = 0x1124 & 0xff;		/* seems to be interchangeably usable to UUID PSM_HID_CONTROL (0x0011) */
	buf [n++] = 1 >> 8;			/* One handle max */
	buf [n++] = 1;
	buf [n++] = 0;				/* No continuation code */
	buf [plen] = ( n - (plen + 2) ) >> 8;
	buf [plen+1] = n - (plen + 2);
	if ( ( n = send (sdp_sock, buf, n, 0) ) < 0 ) {
		close ( sdp_sock );
		sdp_sock = -1;
		fprintf ( stderr, "Sending SDP request for HIDP info to the HID device failed!\n" );
		return	1;
	}
	// Now get the ServiceSearchRequest answer...
	if ( 1 > ( n = recv (sdp_sock, lbuf, sizeof lbuf, 0) ) ) {
		fprintf ( stderr, "ServiceSearchRequest for HIDP info of HID device failed!\n" );
		return	1;
	}
	n = sdp_resp ( lbuf, n );
	if ( n != -2 ) {
		fprintf ( stderr, "Failed to send request for HIDP info!\n" );
		return	1;
	}
	if ( 1 > ( n = recv (sdp_sock, lbuf, sizeof lbuf, 0) ) ) {
		fprintf ( stderr, "Failed to receive answer for SDP AttributeRequest from HID device!\n" );
		return	1;
	}
	n = sdp_resp ( lbuf, n );
	return	0;
}

int do_inquiry(int length)
{
	int		fd;
	int		err;
	__u8		num;
	if (!length) length = 8;
	fd = hci_open(btdev);
	if (fd < 0) {
		fprintf(stderr, "Unable to open device %s for inquiry: %s!\n", btdev, strerror(errno));
		return 1;
	}
	fprintf(stdout, "Performing device inquiry for %d seconds...", length); fflush ( stdout );
	err = HCI_Inquiry(fd, length, 20, devs, &num);
	close ( fd );
	if (err) {
		fprintf(stderr, "ERROR: %s!\n", hci_error(err));
		exit(1);
	}
	if (num == 0) {
		fprintf(stdout, "done: No 'connectable' devices found.\n");
	} else {
		fprintf(stdout, "done: %d 'connectable' device%s found.\n", found_devs = num, (num>1?"s":""));
	}
	fflush ( stdout );
	return 0;
}

int dolisten ( char * strbda, char doactive ) {
	// doactive is set to != 0 if an active connection is requested (like needed when first
	// connecting that device, so that it creates a fixation onto the host)
	int	fd;
	char	filenam[] = { DIRECTORY_HIDDB "/01234560123456" };
	int	i;
	for ( i = 0; i < 6; ++i ) {
		filenam[(2*i)+23] = strbda[(3*i)  ];
		filenam[(2*i)+24] = strbda[(3*i)+1];
	}
	if ( strlen ( strbda ) != 17 ) {
		fprintf ( stderr, "Bluetooth address not accepted [%s]!\n", strbda );
		return	1;
	}
	filenam[35] = 0;
	if ( 0 > (fd = open ( filenam, O_RDONLY ) ) ) {
		fprintf ( stderr, "Failed to read database file [%s] (%s)! (Did you do a 'bthidctl connect' before?)\n", filenam, strerror(errno) );
		return	1;
	}
	if ( sizeof(iocstruct) != read ( fd, (void *)&iocstruct, sizeof(iocstruct) ) ) {
		fprintf ( stderr, "Failed to read from database file [%s] (%s)!\n", filenam, strerror(errno) );
		close ( fd );
		return	1;
	}
	i = iocstruct.conn_info.rd_size;
	if ( ( i < 1 ) || ( i > 65535 ) ) {
		fprintf ( stderr, "Descriptor size seems invalid, aborting!\n" );
		close ( fd );
		return	1;
	}
	if ( NULL == ( iocstruct.conn_info.rd_data = malloc ( ( i + 1023 ) & 0x400 ) ) ) {
		fprintf ( stderr, "Failed to allocate memory for HID descriptor!\n" );
		close ( fd );
		return	1;
	}
	if ( i != read ( fd, iocstruct.conn_info.rd_data, i ) ) {
		fprintf ( stderr, "Failed to read the HID descriptor from file!\n" );
		close ( fd );
		return	1;
	}
	close ( fd );
	if ( 0 == str2bda ( &iocstruct.saddr.bda, strbda ) ) {
		fprintf ( stderr, "Malformed Bluetooth address: [%s]!\n", strbda );
		return	1;
	}
	if ( doactive ) { // Force active connection, for example on first contact
		iocstruct.status |= HIDP_STATUS_ACTIVE_ADD;
	}
	if ( hci_ioctl ( BTIOC_HIDP_MODIFY, &iocstruct ) < 0 ) {
		fprintf ( stderr, "Error %d while submitting info to the kernel (ioctl failed: %s)!\n", errno, strerror(errno) );
		return	1;
	}
	free ( iocstruct.conn_info.rd_data );
	iocstruct.conn_info.rd_data = NULL;
	fprintf ( stdout, "Successfully added [%s] to kernel device list%s.\n", strbda, \
			doactive ? " and requested immediate connection" : "");	
	return	0;
}

int dodisconnect ( char * strbda ) {
	if ( 0 == str2bda ( &iocstruct.saddr.bda, strbda ) ) {
		fprintf ( stderr, "Malformed Bluetooth address: [%s]!\n", strbda );
		return	1;
	}

	if ( hci_ioctl ( BTIOC_HIDP_DELETE, &iocstruct ) < 0 ) {
		fprintf ( stderr, "Error (%s) submitting info to the kernel (ioctl failed)!\n", strerror(errno) );
		return	1;
	}
	fprintf ( stdout, "Disconnected device [%s].\n", strbda );
	return	0;
}

int dodelete ( char * strbda ) {
	char	filenam[] = { DIRECTORY_HIDDB "/01234560123456" };
	BD_ADDR	bda;
	char *	bdap;
	int	i;
	if ( 0 == str2bda ( &bda, strbda ) ) {
		fprintf ( stderr, "Malformed Bluetooth address: [%s]!\n", strbda );
		return	1;
	}
	dodisconnect ( strbda ); // Just in case it was connected, try to disconnect.
	bdap = bda2str ( &bda );
	if ( NULL == bdap ) {
		fprintf ( stderr, "Failed to convert bluetooth address to string!" );
		return	1;
	}
	bdap[2] = bdap[5] = bdap[8] = bdap[11] = bdap[14] = bdap[17] = 0;
	sprintf ( filenam, "%s/%s%s%s%s%s%s", DIRECTORY_HIDDB,
		bdap, bdap + 3, bdap + 6, bdap + 9, bdap + 12, bdap + 15 );
	for ( bdap = filenam + strlen ( DIRECTORY_HIDDB ) + 1; *bdap != 0; ++bdap )
		*bdap = tolower ( *bdap );
	i = unlink ( filenam );
	if ( ( i == ENOENT ) || ( i == 0 ) ) {
		fprintf ( stdout, "Successfully removed device from database.\n" );
		return	0;
	}
	fprintf ( stderr, "Deleting device [%s] failed: Removing database file failed!\n", strbda );
	return	1;
}

int dostatus ( char dodiscovery )
{
	// List all devices that the kernel currently knows about
	struct hidp_ioc_getlist * hic;
	int	i;
	DIR	*dir;
	struct dirent *dire;
	char	buf[1024];
	int     status;
	BD_ADDR bda;
	int	fd;
	if ( NULL == ( hic = malloc ( sizeof ( struct hidp_ioc_getlist ) + ( sizeof ( struct hidp_ioc ) * HIDP_MAX_DEVICES ) ) ) ) {
		fprintf ( stderr, "Memory allocation failed - cannot retrieve status\n" );
		return	1;
	}
	hic->count = HIDP_MAX_DEVICES;
	hic->left = 0;
	if ( hci_ioctl ( BTIOC_HIDP_GET_LIST, hic ) < 0 ) {
		fprintf ( stderr, "Error submitting info to the kernel (ioctl failed): %d [%s]\n", errno, strerror(errno) );
		return	1;
	}
	if ( hic->count > HIDP_MAX_DEVICES ) {
		fprintf ( stderr, "Kernel managed to screw our device list.... claims %d devices in list! Abort.\n", hic->count );
		return	1;
	}
	found_devs = 0;
	if ( dodiscovery ) {
		if ( do_inquiry ( 0 ) ) { // 0 => use default inq length (like 8 seconds)
			found_devs = 0;
		}
	}
	// Read in devices from database
	if ( NULL == ( dir = opendir ( DIRECTORY_HIDDB ) ) ) {
		fprintf ( stderr, "Error: Cannot read database directory\n" );
		return	1;
	}
	fprintf ( stdout, "Bluetooth address  status   Device identifier%s\n", dodiscovery?" (only for devices in database)":"" );
	while ( NULL != ( dire = readdir ( dir ) ) ) {
		if ( strlen ( dire->d_name ) == 12 ) {
			for ( i = 2; i < 17; i+=3 ) buf[i] = ':';
			for ( i = 0; i < 12; ++i )  buf[(3*i)/2] = dire->d_name[i];
			buf[17] = 0;
			// Now check if it's in the list of kernel-known devices
			if ( str2bda ( &bda, buf ) ) { // Address is OK
				status = 0;
				for ( i = 0; i < hic->count; ++i ) {
					if ( 0 == memcmp ( &bda, &(hic->list[i].saddr.bda), sizeof ( bda ) ) ) {
						status = hic->list[i].status | HIDP_STATUS_ACTIVE_ADD; // Used for 'is kernel device'
						i = hic->count;
					}
				}
				// Now check discovered devices
				for ( i = 0; i < found_devs; ++i ) {
					if ( 0 == memcmp ( &bda, &devs[i].bda, sizeof ( bda ) ) ) {
						// Remove from the list, as it's _not_ an unknown/new device
						if ( i < ( found_devs - 1 ) ) {
							memcpy ( &devs[found_devs - 1].bda, &devs[i].bda, sizeof(bda) );
						}
						--found_devs;
					}
				}
				// Read in device information
				sprintf ( buf + 32, "%s/%s", DIRECTORY_HIDDB, dire->d_name );
				if ( 0 > (fd = open ( buf + 32, O_RDONLY ) ) ) {
					fprintf ( stderr, "Failed to read database file [%s] (%s)!\n", buf + 32, strerror(errno) );
					return	1;
				}
				if ( sizeof(iocstruct) != read ( fd, (void *)&iocstruct, sizeof(iocstruct) ) ) {
					fprintf ( stderr, "Failed to read from database file [%s] (%s)!\n", buf + 32, strerror(errno) );
					close ( fd );
					return	1;
				}
				close ( fd );
				// iocstruct now contains the additional info we wanted
				fprintf ( stdout, "%s  %s %s\n", buf, ((status & HIDP_STATUS_CONNECTED) ? "ACTIVE  " : ((status & HIDP_STATUS_ACTIVE_ADD) ? "STANDBY " : "DATABASE" ) ), iocstruct.conn_info.name );
			}
		}
	}
	closedir ( dir );
	for ( i = 0; i < found_devs; ++i ) {
		fprintf ( stdout, "%s  IN RANGE\n", bda2str ( &devs[i].bda ) );
	}
	//fprintf ( stdout, "Device list (%d entries):\n", hic->count );
	//for ( i = 0; i < hic->count; ++i ) {
	//	fprintf ( stdout, "[%s]\n", bda2str ( &(hic->list[i].saddr.bda) ));
	//}
	return	0;
}
	

int doconnect ( char * strbda )
{
	// Add the given BDA to the device database (or refresh record)
	// Must be in range to do so, of course, as SDP queries are involved
	char	filenam[] = { DIRECTORY_HIDDB "/01234560123456" };
	char *	bdap;
	BD_ADDR bda;
	int	i;
	if ( 0 == str2bda ( &bda, strbda ) ) {
		fprintf ( stderr, "Malformed Bluetooth address: [%s]\n", strbda );
		return	1;
	}
	// Trying to do the connection, retrieve via SDP the HID profile information
	if ( 0 == str2bda ( &iocstruct.saddr.bda, strbda ) ) return 1;
	iocstruct.status = 0;
	iocstruct.conn_info.vendor = 0;
	iocstruct.conn_info.product = 0;
	iocstruct.conn_info.version = 0;
	if ( sdp_probe_pnp ( &bda ) ) {
		fprintf ( stderr, "\nERROR: SDP device PNP probe for [%s] failed\n" \
				"This can be related to the device not being 'connectable', or\n" \
				"not being in radio range. Check address, and don't forget to press\n" \
				"'connect' button on the bottom of your bluetooth HID device.\n", strbda );
		return	1;
	}
	iocstruct.conn_info.country = 0;
	iocstruct.conn_info.flags = 0;
	iocstruct.conn_info.idle_to = 0;
	iocstruct.conn_info.parser = 0;
	iocstruct.conn_info.rd_size = 0;
	iocstruct.conn_info.rd_data = NULL;
	strcpy(iocstruct.conn_info.name, "<Generic Bluetooth HID Device>");
	iocstruct.status &= HIDP_STATUS_ACTIVE_ADD;
	if ( sdp_probe ( &bda ) ) {
		fprintf ( stderr, "SDP probe for HID record of device [%s] failed!\n", strbda );
		return	1;
	}
	fprintf ( stdout, "Identified device: Vendor ID[%04x:%04x], Name[%s].\n", iocstruct.conn_info.vendor,
			iocstruct.conn_info.product, iocstruct.conn_info.name );
	bdap = bda2str ( &bda );
	if ( NULL == bdap ) {
		return	1;
	} 
	bdap[2] = bdap[5] = bdap[8] = bdap[11] = bdap[14] = bdap[17] = 0;
	if ( rmkdir ( DIRECTORY_HIDDB, 0775 ) != 0 ) {
		fprintf ( stderr, "Failed to create directory " DIRECTORY_HIDDB " !\n" );
		return	1;
	}
	sprintf ( filenam, "%s/%s%s%s%s%s%s", DIRECTORY_HIDDB,
		bdap, bdap + 3, bdap + 6, bdap + 9, bdap + 12, bdap + 15 );
	for ( bdap = filenam + strlen ( DIRECTORY_HIDDB ) + 1; *bdap != 0; ++bdap )
		*bdap = tolower ( *bdap );
	if ( iocstruct.conn_info.rd_data == NULL ) {
		fprintf ( stderr, "Failed to retrieve a HID descriptor via SDP!\n" );
		return	1;
	}
	umask ( 002 ); // Create file ug_writeable.
	if ( 0 > ( i = open ( filenam, O_CREAT | O_TRUNC | O_RDWR, 0600 ) ) ) {
		fprintf ( stderr, "Failed to open database file [%s] for writing - check directory\n" \
				"exists and permissions are correct for " DIRECTORY_HIDDB " !\n", filenam );
		return	1;
	}
	if ( sizeof ( iocstruct ) != write ( i, (void *)&iocstruct, sizeof(iocstruct) ) ) {
		fprintf ( stderr, "Failed to write to file [%s] (though opening succeeded)!\n", strerror(errno) );
		return	1;
	}
	if ( iocstruct.conn_info.rd_size != write ( i, iocstruct.conn_info.rd_data, iocstruct.conn_info.rd_size ) ) {
		fprintf ( stderr, "Failed to write to file [%s] (though opening succeeded)!\n", strerror(errno) );
		return	1;
	}
	close ( i );

	fprintf ( stdout, "Successfully added device [%s] to database.\n", bda2str ( &bda ) );
	return	0;
}

int main ( int argc, char * argv[] )
{
	int	i = 1;
	char	buf[20];
	DIR *	directory;
	struct dirent * direntry;
	if ( argc < 2 ) {
		return	dohelp(NULL);
	}
	if ( 0 == strcasecmp ( argv[i], "help" ) ) {
		return	dohelp((i+1) == argc?"":argv[i+1]);
	}
	if ( 0 == strcasecmp ( argv[i], "connect" ) ) {
		if ( (argc - 1) == i ) {
			fprintf ( stderr, "bthidctl connect <bda>   -- additional parameter needed\n" );
			return	1;
		}
		if ( ( 0 == strncasecmp ( argv[i+1], "discover", 8 ) ) ||  // This catches 'discovery' as well
		     ( 0 == strncasecmp ( argv[i+1], "all"     , 3 ) ) ) { // 'all' possible as short form
			if ( do_inquiry ( 0 ) ) { // 0 => use default inq length (like 8 seconds)
				return	1;
			}
			for ( i = 0; i < found_devs; ++i ) {
				if ( doconnect ( bda2str ( &devs[i].bda ) ) ) {
					fprintf ( stdout, "Skipping device [%s], continuing.\n", bda2str ( &devs[i].bda ) );
				} else {
					dolisten ( bda2str ( &devs[i].bda ), 1 );
				}
				dolisten ( bda2str ( &devs[i].bda ), 1 );
			}
			fprintf ( stdout, "Finished adding all devices in range.\n" );
			return	0;
		}
		if ( 0 == doconnect ( argv[i+1] ) ) return dolisten ( argv[i+1], 1 );
	}
	if ( 0 == strcasecmp ( argv[i], "listen" ) ) {
		if ( (argc - 1) == i ) {
			fprintf ( stderr, "bthidctl listen [--active] <bda|ALL>   -- if you want to reconnect all\n" \
				 "                     devices in database, use   'bthidctl listen ALL'\n" );
			return	1;
		}
		iocstruct.status = 0;
		if ( 0 == strcasecmp ( argv[i+1], "--active" ) ) {
			if ( argc == i ) {
				fprintf ( stderr, "ERROR: You need to specify an address along the '--active' flag.\n" );
				return	1;
			}
			++i;
			iocstruct.status = HIDP_STATUS_ACTIVE_ADD;
		}
		if ( 0 == strcasecmp ( argv[i+1], "ALL" ) ) {
			if ( NULL == ( directory = opendir ( DIRECTORY_HIDDB ) ) ) {
				fprintf ( stderr, "Cannot scan database directory for device information files\n" );
				return	1;
			}
			while ( NULL != ( direntry = readdir ( directory ) ) ) {
				if ( strlen ( direntry->d_name ) == 12 ) {
					for ( i = 0; i < 6; ++i ) {
						buf[(3*i)  ] = direntry->d_name[(2*i)  ];
						buf[(3*i)+1] = direntry->d_name[(2*i)+1];
						buf[(3*i)+2] = ':';
					}
					buf[17] = 0;
					dolisten ( buf, 0 );
				}
			}
			closedir ( directory );
			if ( errno != 0 ) {
				fprintf ( stderr, "Error %d occured (%s) while adding all database devices to kernel device list\n",
						errno, strerror(errno) );
			}
			return	1;
		}
		return	dolisten ( argv[i+1], 0 );
	}
	if ( 0 == strcasecmp ( argv[i], "disconnect" ) ) {
		if ( ( argc - 1 ) == i ) {
			fprintf ( stderr, "bthidctl disconnect <bda|ALL>  -- if you want to disconnect all devices\n" \
				 "                                  in database, use  bthidctl disconnect ALL\n" );
			return	1;
		}
		if ( 0 == strcasecmp ( argv[i+1], "ALL" ) ) {
			struct	hidp_ioc_getlist * hic;
			int	i, j;
			if ( NULL == ( hic = malloc ( sizeof ( struct hidp_ioc_getlist ) + ( sizeof ( struct hidp_ioc ) * HIDP_MAX_DEVICES ) ) ) ) {
				fprintf ( stderr, "Memory allocation failed - cannot retrieve status (for disconnect all)\n" );
				return	1;
			}
			hic->count = HIDP_MAX_DEVICES;
			hic->left = 0;
			if ( hci_ioctl ( BTIOC_HIDP_GET_LIST, hic ) < 0 ) {
				fprintf ( stderr, "Error submitting info to the kernel (ioctl failed): %d [%s]\n", errno, strerror(errno) );
				return	1;
			}
			if ( hic->count > HIDP_MAX_DEVICES ) {
				fprintf ( stderr, "Kernel managed to screw our device list.... claims %d devices in list! Abort.\n", hic->count );
				return	1;
			}
			for ( i = j = 0; i < hic->count; ++i ) {
				j += dodisconnect ( bda2str ( &(hic->list[i].saddr.bda) ) );
			}
			fprintf ( stdout, "Disconnected %d device(s).\n", hic->count );
			return	0;
		}
		return	dodisconnect ( argv[i+1] );
	}
	if ( 0 == strcasecmp ( argv[i], "delete" ) ) {
		if ( ( argc - 1 ) == i ) {
			fprintf ( stderr, "Argument needed. Usage:   bthidctl delete <bda>\n" );
			return	1;
		}
		return	dodelete ( argv[i+1] );
	}
	if ( 0 == strcasecmp ( argv[i], "status" ) ) {
		if ( ( argc - 1 ) > i ) if ( strncasecmp ( argv[i+1], "discover", 8 ) == 0 ) return dostatus (1);
		return	dostatus (0);
	}
	fprintf ( stderr, "Invalid arguments given, use    bthidctl help    to learn more!\n" );
	return	1;
}

