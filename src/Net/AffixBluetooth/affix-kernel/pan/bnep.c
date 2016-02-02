/* -*-   Mode:C; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Original Author: Muller Ulrich <ulrich.muller@nokia.com>

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
   $Id: bnep.c,v 1.51 2004/05/17 16:11:10 kassatki Exp $

   bnep.c - Bluetooth Network Encapsulation Protocol (BNEP) Rev. 0.95a

   Fixes:
   		Dmitry Kasatkin
*/


/* 
   TODO: - security
*/


/* kernel includes: */
#include <linux/config.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/inet.h>
#include <linux/vmalloc.h>
#include <linux/etherdevice.h>
#include <asm/checksum.h>

/* other includes: */
#define FILEBIT	DBPAN

#include "bnep.h"
#include "pan.h"

/* constant */

static const ETH_ADDR eth_broadcast = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/* helper */

/* convert ethernet addr to string */
char *ETH_ADDR2str(void *p)
{
	unsigned char *addr = (unsigned char*) p;
	static unsigned char buf[2][18];
	static int num = 0; /* we use function with up to two calls in one printk statement, this requires
			that there are two buffers in memory */

	num = 1 - num; /* switch buf */
	sprintf(buf[num], "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	return buf[num];
}

__u16 role2uuid(int role)
{
	switch(role & AFFIX_PAN_ROLE) {
	case AFFIX_PAN_PANU:
		return __cpu_to_be16(AFFIX_PAN_UUID_PANU);
	case AFFIX_PAN_NAP:
		return __cpu_to_be16(AFFIX_PAN_UUID_NAP);
	case AFFIX_PAN_GN:
		return __cpu_to_be16(AFFIX_PAN_UUID_GN);
	default:
		return 0;
	}
}

/* protocol filter */

/* process a list of protocol filter entries (range start - range stop format), network byte order:
   do not change settings unless response returns success,
   returns BNEP_FILTER_NET_TYPE_RESPONSE_MSG return code (host byte order)
*/
__u16 bnep_filter_protocol_process(struct bnep_con *cl, int length, __u16 *protocol)
{
	int i, j, k;
	__u16 filter[PROTOCOL_FILTER_MAX + 1][2];	/* in host byte order, one additional element for sorting */

	if (!cl->filter_protocol_admitted)	/* not allowed */
		return BNEP_FILTER_NET_RSP_FAIL_SECURITY;

	if (!PROTOCOL_FILTER_MAX)		/* not supported */
		return BNEP_FILTER_NET_RSP_UNSUPPORTED;

	if (length > PROTOCOL_FILTER_MAX)	/* too large */
		return BNEP_FILTER_NET_RSP_FAIL_MAXIMUM;

	if (!length) {				/* no filter at all */
		cl->pf.count = 0;
		DBPRT("resetting protocol filter\n");
		return BNEP_FILTER_NET_RSP_SUCCESSFUL;
	}

	/* now process list to local filter array
	   - adapt byte order
	   - sort array for range start with insertion sort (quadratic runtime is ok as
	     MAX_FILTER_PROTOCOL is expected to be very small) */
	for (i = 0; i < length; i++) {
		filter[PROTOCOL_FILTER_MAX + 1][0] = __be16_to_cpu(*(protocol++));
		filter[PROTOCOL_FILTER_MAX + 1][1] = __be16_to_cpu(*(protocol++));

		j = i; /* j will be new position in sorted field */
		while ((j > 0) && (filter[j - 1][0] > filter[PROTOCOL_FILTER_MAX + 1][0]))
			j--;

		if (i != j)
			for (k = i; k > j; k--) {
				filter[k][0] = filter[k - 1][0];
				filter[k][1] = filter[k - 1][1];
			}

		filter[j][0] = filter[PROTOCOL_FILTER_MAX + 1][0];
		filter[j][1] = filter[PROTOCOL_FILTER_MAX + 1][1];
	}

	/* check for overlap/consistency
	   if entries are not disjoint, this is not really a problem, but the specification forbids it */
	for (i = 0; i < length; i++) {
		if (filter[i][0] > filter[i][1]) /* range start > range stop */
			return BNEP_FILTER_NET_RSP_FAIL_RANGE;

		if ((i + 1 < length) && (filter[i][1] >= filter[i+1][0])) /* not disjoint */
			return BNEP_FILTER_NET_RSP_FAIL_RANGE;
	}

	/* if everything is ok, update filter and return success */
	for (i = 0; i < length; i++) {
		cl->pf.protocol[i][0] = filter[i][0];
		cl->pf.protocol[i][1] = filter[i][1];
	}
	cl->pf.count = length;
	DBPRT("applying new protocol filter with %d entries\n", length);

	return BNEP_FILTER_NET_RSP_SUCCESSFUL;
}

/* test if protocol (network byte order) shall be filtered out,
   return 0 if protocol is not filtered out
*/
int bnep_filter_protocol_test(struct bnep_con *cl, __u16 protocol)
{
	int i;

	DBPRT("testing protocol 0x%04x\n", __be16_to_cpu(protocol));

	if (!PROTOCOL_FILTER_MAX || !cl->pf.count)
		return 0;

	protocol = __be16_to_cpu(protocol);
	i = 0;
	while ((i < cl->pf.count) && (protocol <= cl->pf.protocol[i][1])) { /* filter is sorted */
		if ((protocol >= cl->pf.protocol[i][0]) && (protocol <= cl->pf.protocol[i][1]))
			return 0;
		i++;
	}

	DBPRT("skipping packet\n");
	return 1;
}

/* multicast filter */

/* compare two ethernet addresses (network byte order: most significant byte first),
   return -1 if addr1 < addr2
   return  0 if addr1 = addr2
   return  1 if addr1 > addr 2
*/
int eth_compare(ETH_ADDR *addr1, ETH_ADDR *addr2)
{
	__u8 *a1 = (__u8*) addr1;
	__u8 *a2 = (__u8*) addr2;
	int i;

	for (i = 0; i < 6; i++) {
		if (*a1 < *a2)
			return -1;
		else if (*(a1++) < *(a2++))
			return 1;
	}
	return 0;
}

/* process a list of multicast filter entries (address start - address end format), network byte order:
   do not change setting unless response returns success,
   returns BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG return code (host byte order)
*/
__u16 bnep_filter_multicast_process(struct bnep_con *cl, int length, ETH_ADDR *addr)
{
	int i;
	multicast_filter mf;

	if (!cl->filter_multicast_admitted)	/* not allowed */
		return BNEP_FILTER_MULTI_RSP_FAIL_SECURITY;

	if (!MULTICAST_FILTER_MAX)		/* not supported */
		return BNEP_FILTER_MULTI_RSP_UNSUPPORTED;

	if (length > MULTICAST_FILTER_MAX)	/* too large */
		return BNEP_FILTER_MULTI_RSP_FAIL_MAXIMUM;

	if (!length) {				/* no filter at all */
		cl->mf.count = 0;
		DBPRT("resetting protocol filter\n");
		return BNEP_FILTER_MULTI_RSP_SUCCESSFUL;
	}

	/* now copy list to local array */
 	for (i = 0; i < length; i++) {
		memcpy(&mf.multicast[i][0], addr++, sizeof(ETH_ADDR));
		memcpy(&mf.multicast[i][1], addr++, sizeof(ETH_ADDR));

		if (eth_compare(&mf.multicast[i][0], &mf.multicast[i][1]) == 1) /* start > stop */
			return BNEP_FILTER_MULTI_RSP_FAIL_INVALID;
	}
	mf.count = length;

	/* if everything is ok, update filter and return success */
	memcpy(&cl->mf, &mf, sizeof(multicast_filter));
	DBPRT("applying new multicast filter with %d entries\n", length);

	return BNEP_FILTER_MULTI_RSP_SUCCESSFUL;
}

/* test if multicast address shall be filtered out,
   return 0 if address is not filtered out */
int bnep_filter_multicast_test(struct bnep_con *cl, ETH_ADDR *addr)
{
	int i;

	DBPRT("testing multicast address %s\n", ETH_ADDR2str(addr));

	if (!MULTICAST_FILTER_MAX || !cl->mf.count)
		return 0;

	if (!memcmp(addr, eth_broadcast, 6)) /* Ethernet Broadcast is never filtered out */
		return 0;

	i = 0;
	while (i < cl->mf.count) {
		if (eth_compare(&cl->mf.multicast[i][0], addr) != -1 &&
		    eth_compare(&cl->mf.multicast[i][1], addr) != 1)
			return 0;

		i++;
	}

	DBPRT("skipping packet\n");
	return 1;
}

/* helper */

/* allocate skbuff to be sent to lower layer */
struct sk_buff *bnep_alloc_skb(unsigned int size)
{
	struct sk_buff	*skb;
	
	skb = alloc_skb(L2CAP_SKB_RESERVE + size, GFP_ATOMIC);
	if (!skb)
		return NULL;
	skb_reserve(skb, L2CAP_SKB_RESERVE);
	return skb;
}

/* send packets to remote bnep layer */

/* send command not understood message */
int bnep_command_not_understood(struct bnep_con *cl, __u8 unknown_control)
{
	struct sk_buff *skb;
	bnep_hdr_ccnu_t *hdr;

	if ((skb = bnep_alloc_skb(sizeof(bnep_hdr_ccnu_t))) == NULL)
		return -ENOMEM;

	hdr = (bnep_hdr_ccnu_t*) skb_put(skb, sizeof(bnep_hdr_ccnu_t));
	hdr->type = BNEP_CONTROL;
	hdr->extension = 0;
	hdr->control_type = BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD;
	hdr->unknown_control = unknown_control;

	return l2ca_send_data(cl->ch, skb);
}

/* send response message (response in host byte order) */
int bnep_response(struct bnep_con *cl, __u8 control_type, __u16 response)
{
	struct sk_buff *skb;
	bnep_hdr_response_t *hdr;

	if ((skb = bnep_alloc_skb(sizeof(bnep_hdr_response_t))) == NULL)
		return -ENOMEM;

	hdr = (bnep_hdr_response_t*) skb_put(skb, sizeof(bnep_hdr_response_t));
	hdr->type = BNEP_CONTROL;
	hdr->extension = 0;
	hdr->control_type = control_type;
	hdr->response = __cpu_to_be16(response);

	DBPRT("sending response type 0x%02x code 0x%04x\n", control_type, response);

	return l2ca_send_data(cl->ch, skb);
}

/* send setup connection request message */
int bnep_setup_connection_request(struct bnep_con *cl)
{
	struct sk_buff *skb;
	bnep_hdr_scrm_t *hdr;
	int result;
	__u16 dest_UUID;
	__u16 source_UUID;

	DBFENTER;

	source_UUID = role2uuid(cl->btdev->role);
	dest_UUID = role2uuid(cl->btdev->peer_role);
	if ((skb = bnep_alloc_skb( sizeof(bnep_hdr_scrm_t) + 2 * sizeof(dest_UUID) )) == NULL)
		return -ENOMEM;

	hdr = (bnep_hdr_scrm_t*) skb_put(skb, sizeof(bnep_hdr_scrm_t) + 2 * sizeof(dest_UUID) );
	hdr->type = BNEP_CONTROL;
	hdr->extension = 0;
	hdr->control_type = BNEP_SETUP_CONNECTION_REQUEST_MSG;
	hdr->uuid_size = sizeof(dest_UUID);
	memcpy(&hdr->uuid[0], &dest_UUID, sizeof(dest_UUID));
	memcpy(&hdr->uuid[sizeof(dest_UUID)], &source_UUID, sizeof(source_UUID));

	mod_timer(&cl->timer_setup, jiffies + 10 * HZ); /* range is 1 - 30 secs, default is 10 */

	DBPRT("len: %d\n", skb->len);
	if ((result = l2ca_send_data(cl->ch, skb)) != 0) /* error */
		del_timer(&cl->timer_setup);

	return result;
}

/* process setup connection request timeout: send setup connection request message again */
void bnep_setup_connection_request_timeout(unsigned long p)
{
	struct bnep_con *cl = (struct bnep_con*) p;

	DBPRT("timeout\n");
	if (!cl->setup_complete)
		if (bnep_setup_connection_request( (struct bnep_con*) p)) {
			/* error */
			// FIXME l2ca_disconnect_req(cl->ch);
		}
}

/* send filter net type set msg, pf == NULL -> reset filter, returns 0 on success */
int bnep_filter_net_type_set(struct bnep_con *cl, protocol_filter *pf)
{
	struct sk_buff *skb;
	bnep_hdr_fntsm_t *hdr;
	int result, size, list_length = 0;

	DBFENTER;

	if (pf)
		list_length = pf->count;

	size = sizeof(bnep_hdr_fntsm_t) + 4 * list_length;
	if (size > PAN_MTU)
		return -ENOMEM;

	if ((skb = bnep_alloc_skb(size)) == NULL)
		return -ENOMEM;

	hdr = (bnep_hdr_fntsm_t*) skb_put(skb, size);
	hdr->type = BNEP_CONTROL;
	hdr->extension = 0;
	hdr->control_type = BNEP_FILTER_NET_TYPE_SET_MSG;
	hdr->list_length = __cpu_to_be16(4 * list_length);
	if (list_length)
		memcpy(&hdr->protocol, &pf->protocol, 4 * list_length);

	mod_timer(&cl->timer_filter, jiffies + 10 * HZ); /* range is 1 - 30 secs, default is 10 */

	DBPRT("len: %d\n", skb->len);
	if ((result = l2ca_send_data(cl->ch, skb)) != 0)
		del_timer(&cl->timer_filter);

	return result;
}

/* send filter multi addr set msg, mf == NULL -> reset filter, returns 0 on success */
int bnep_filter_multi_addr_set(struct bnep_con *cl, multicast_filter *mf)
{
	struct sk_buff *skb;
	bnep_hdr_fmasm_t *hdr;
	int result, size, list_length = 0;

	DBFENTER;

	if (mf)
		list_length = mf->count;

	size = sizeof(bnep_hdr_fmasm_t) + 12 * list_length;
	if (size > PAN_MTU)
		return -ENOMEM;

	if ((skb = bnep_alloc_skb(size)) == NULL)
		return -ENOMEM;

	hdr = (bnep_hdr_fmasm_t*) skb_put(skb, size);
	hdr->type = BNEP_CONTROL;
	hdr->extension = 0;
	hdr->control_type = BNEP_FILTER_MULTI_ADDR_SET_MSG;
	hdr->list_length = __cpu_to_be16(12 * list_length);
	if (list_length)
		memcpy(&hdr->addr, &mf->multicast, 12 * list_length);

	mod_timer(&cl->timer_filter, jiffies + 10 * HZ); /* range is 1 - 30 secs, default is 10 */

	DBPRT("len: %d\n", skb->len);
	if ((result = l2ca_send_data(cl->ch, skb)) != 0)
		del_timer(&cl->timer_filter);

	return result;
}

/* set protocol/multicast filter */

/* process filter set timeout: send filter set message again */
void bnep_filter_set_timeout(unsigned long p)
{
	struct bnep_con *cl = (struct bnep_con*) p;

	switch(cl->filter_protocol_pending) {
	case filter_error:	/* filter could not be set and was reset */
	case filter_pending:	/* we are waiting for filter response */
	case filter_updated:	/* filter has changed and must be updated */
		DBPRT("transmitting protocol filter\n");
		if (bnep_filter_net_type_set(cl, &cl->btdev->pf)) { /* error */
			cl->filter_protocol_pending = filter_error;
			if (cl->filter_multicast_pending == filter_updated)
				cl->filter_multicast_pending = filter_pending; /* send now, see below */
		} else
			return;
	}

	switch(cl->filter_multicast_pending) {
	case filter_error:	/* filter could not be set and was reset */
	case filter_pending:	/* we are waiting for filter response */
	case filter_updated:	/* filter has changed and must be updated */
		DBPRT("transmitting multicast filter\n");
		if (bnep_filter_multi_addr_set(cl, &cl->btdev->mf)) /* error */
			cl->filter_multicast_pending = filter_error;
		return;
	}

	DBPRT("timer was pending\n");
}

/* sends filter_set message to all devices that:
   - have a connection that is already setup
   - have not a pending filter response messages */
void bnep_set_filter_update(struct pan_dev *btdev)
{
	struct bnep_con *cl;

	btl_read_lock(&btdev->connections);
	btl_for_each (cl, btdev->connections) {
		if (cl->setup_complete && !(timer_pending(&cl->timer_filter))) {
			/* ...that are setup and have no filter timer pending */
			if (cl->filter_protocol_pending == filter_updated) {
				cl->filter_protocol_pending = filter_pending; /* we are waiting for filter response */
				bnep_filter_net_type_set(cl, &btdev->pf);
			} else if (cl->filter_multicast_pending == filter_updated) {
				cl->filter_multicast_pending = filter_pending; /* we are waiting for filter response */
				bnep_filter_multi_addr_set(cl, &btdev->mf);
			}
		}
	}
	btl_read_unlock(&btdev->connections);
}

/* sets protocol filter by sending control message to all remote devices */
void bnep_set_filter_protocol(struct pan_dev *btdev, protocol_filter* pf)
{
	struct bnep_con	*cl;

	/* update local copy */
	DBPRT("setting filter to %d entries\n", pf->count);
	memcpy(&btdev->pf, pf, sizeof(protocol_filter));

	/* mark all remote devices to get updated */
	btl_read_lock(&btdev->connections);
	btl_for_each (cl, btdev->connections) {
		if (cl->filter_protocol_pending != filter_unsupported) /* remote device does not support filters */
			cl->filter_protocol_pending = filter_updated; /* filter has changed and must be updated */
	}
	btl_read_unlock(&btdev->connections);

	bnep_set_filter_update(btdev);
}

/* sets multicast filter by sending control message to all remote devices */
void bnep_set_filter_multicast(struct pan_dev *btdev, multicast_filter* mf)
{
	struct bnep_con *cl;

	/* update local copy */
	DBPRT("setting filter to %d entries\n", mf->count);
	memcpy(&btdev->mf, mf, sizeof(multicast_filter));

	/* mark all remote devices to get updated */
	btl_read_lock(&btdev->connections);
	btl_for_each (cl, btdev->connections) {
		cl->filter_multicast_pending = filter_updated; /* filter has changed and must be updated */
	}
	btl_read_unlock(&btdev->connections);

	bnep_set_filter_update(btdev);
}

/* process control messages from remote layer
   return size of processed header or negative value if header is invalid */

int bnep_process_command_not_understood(struct bnep_con *cl, bnep_hdr_ccnu_t *hdr)
{
	DBPRT("remote rejected control type 0x%02x\n", hdr->unknown_control);

	switch(hdr->unknown_control) { /* if remote rejects basic command, close connection */
	case BNEP_SETUP_CONNECTION_REQUEST_MSG:
	case BNEP_SETUP_CONNECTION_RESPONSE_MSG:
	case BNEP_FILTER_NET_TYPE_SET_MSG:
	case BNEP_FILTER_NET_TYPE_RESPONSE_MSG:
	case BNEP_FILTER_MULTI_ADDR_SET_MSG:
	case BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG:
		// FIXME: l2ca_disconnect_req(cl->ch);
		/* the driver may disconnect immediatley and make con, cl and cl invalid,
		   so it is important not to access them anymore. returning an error code will ensure this. */
		return -EFAULT;
	}

	return sizeof(bnep_hdr_ccnu_t);
}

int bnep_process_setup_connection_request(struct bnep_con *cl, bnep_hdr_scrm_t *hdr, unsigned int length)
{
	int result = sizeof(bnep_hdr_scrm_t) + 2 * hdr->uuid_size;

	if (result != length) {
		DBPRT("wrong size: %d instead of %d\n", length, result);
		return -EFAULT;
	}
	if (hdr->uuid_size < 2 || hdr->uuid_size > 16) {
		DBPRT("invalid UUID size: %d\n", hdr->uuid_size);
		bnep_response(cl, BNEP_SETUP_CONNECTION_RESPONSE_MSG, BNEP_SETUP_RSP_FAIL_SERVICE);
		return -EFAULT;
	}

	//TODO: evaluate UUIDs

	bnep_response(cl, BNEP_SETUP_CONNECTION_RESPONSE_MSG, BNEP_SETUP_RSP_SUCCESSFUL);
	if (cl->setup_complete) {
		DBPRT("received repeated setup request msg\n");
	} else {
		DBPRT("setup request received, sending local filter settings to remote device\n");
		cl->setup_complete = 1; /* must be set before updating filter */
		bnep_set_filter_update(cl->btdev);
		pan_check_link(cl->btdev, 1);
	}

	return result;
}

int bnep_process_setup_connection_response(struct bnep_con *cl, bnep_hdr_response_t *hdr)
{
	del_timer(&cl->timer_setup);

	if (__be16_to_cpu(hdr->response) != BNEP_SETUP_RSP_SUCCESSFUL) {
		//FIXME: l2ca_disconnect_req(cl->ch);
		/* the driver may disconnect immediatley and make con, cl and cl invalid,
		   so it is important not to access them anymore. returning an error code will ensure this. */
		return -EFAULT;
	}

	if (cl->setup_complete) {
		DBPRT("received repeated setup response msg\n");
	} else {
		DBPRT("sending local filter settings to remote device\n");
		cl->setup_complete = 1; /* must be set before updating filter */
		bnep_set_filter_update(cl->btdev);
		pan_check_link(cl->btdev, 1);
	}

	return sizeof(bnep_hdr_response_t);
}

int bnep_process_filter_protocol_set(struct bnep_con *cl, bnep_hdr_fntsm_t *hdr, unsigned int length)
{
	int result = sizeof(bnep_hdr_fntsm_t) + __be16_to_cpu(hdr->list_length);
	__u16 response;

	if ((result != length) || (__be16_to_cpu(hdr->list_length) % 4)) { /* hdr->list_length must be a multiple of 4 */
		DBPRT("invalid size: %d\n", __be16_to_cpu(hdr->list_length));
		return -EFAULT;
	}

	response = bnep_filter_protocol_process(cl, __be16_to_cpu(hdr->list_length) / 4, (__u16*) &hdr->protocol);
	bnep_response(cl, BNEP_FILTER_NET_TYPE_RESPONSE_MSG, response);

	return length;
}

int bnep_process_filter_protocol_response(struct bnep_con *cl, bnep_hdr_response_t *hdr)
{
	DBPRT("remote response: 0x%04x\n", __be16_to_cpu(hdr->response));

	del_timer(&cl->timer_filter);

	switch(cl->filter_protocol_pending) {
	case filter_error: /* filter could not be set and was reset */
		if (__be16_to_cpu(hdr->response) != BNEP_FILTER_NET_RSP_SUCCESSFUL)
			BTERROR("PAN: remote side could not reset protocol filter\n");
		break;
	case filter_pending: /* we are waiting for filter response */
		switch(__be16_to_cpu(hdr->response)) {
		case BNEP_FILTER_NET_RSP_SUCCESSFUL:
			cl->filter_protocol_pending = filter_done; /* filter is set */
			break;
		case BNEP_FILTER_NET_RSP_UNSUPPORTED:
			cl->filter_protocol_pending = filter_unsupported;
			break;
		default: /* unsuccessful and possibly an old filter is active -> reset filter */
			cl->filter_protocol_pending = filter_error;
			bnep_filter_net_type_set(cl, NULL); /* on error, just nothing happens */
			break;
		}
		break;
	case filter_updated: /* filter has changed and must be updated */
		DBPRT("updating protocol filter\n");
		cl->filter_protocol_pending = filter_pending; /* we are waiting for filter response */
		if (bnep_filter_net_type_set(cl, &cl->btdev->pf)) /* error */
			cl->filter_protocol_pending = filter_error; /* filter could not be set and was reset */
		break;
	default:
		DBPRT("unexpected response\n");
	}

	/* check if multicast filter was updated and can be sent now */
	if ((cl->filter_multicast_pending == filter_updated) && !(timer_pending(&cl->timer_filter))) {
		cl->filter_multicast_pending = filter_pending; /* we are waiting for filter response */
		bnep_filter_multi_addr_set(cl, &cl->btdev->mf);
	}

	return sizeof(bnep_hdr_response_t);
}

int bnep_process_filter_multicast_set(struct bnep_con *cl, bnep_hdr_fmasm_t *hdr, unsigned int length)
{
	int result = sizeof(bnep_hdr_fntsm_t) + __be16_to_cpu(hdr->list_length);
	__u16 response;

	if ((result != length) || (__be16_to_cpu(hdr->list_length % 12))) { /* hdr->list_length must be a multiple of 12 */
		DBPRT("invalid size: %d\n", hdr->list_length);
		return -EFAULT;
	}

	response = bnep_filter_multicast_process(cl, __be16_to_cpu(hdr->list_length) / 12, (ETH_ADDR*) &hdr->addr);
	bnep_response(cl, BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG, response);

	return length;
}

int bnep_process_filter_multicast_response(struct bnep_con *cl, bnep_hdr_response_t *hdr)
{
	DBPRT("remote response: 0x%04x\n", __be16_to_cpu(hdr->response));

	del_timer(&cl->timer_filter);

	switch(cl->filter_multicast_pending) {
	case filter_error: /* filter could not be set and was reset */
		if (__be16_to_cpu(hdr->response) != BNEP_FILTER_MULTI_RSP_SUCCESSFUL)
			BTERROR("PAN: remote side could not reset multicast filter\n");
		break;
	case filter_pending: /* we are waiting for filter response */
		switch(__be16_to_cpu(hdr->response)) {
		case BNEP_FILTER_MULTI_RSP_SUCCESSFUL:
			cl->filter_multicast_pending = filter_done; /* filter is set */
			break;
		case BNEP_FILTER_MULTI_RSP_UNSUPPORTED:
			cl->filter_multicast_pending = filter_unsupported;
			break;
		default: /* unsuccessful and possibly an old filter is active -> reset filter */
			cl->filter_multicast_pending = filter_error; /* filter could not be set and was reset */
			bnep_filter_multi_addr_set(cl, NULL); /* on error, just nothing happens */
			break;
		}
		break;
	case filter_updated: /* filter has changed and must be updated */
		DBPRT("updating multicast filter\n");
		cl->filter_multicast_pending = filter_pending; /* we are waiting for filter response */
		if (bnep_filter_multi_addr_set(cl, &cl->btdev->mf)) /* error */
			cl->filter_multicast_pending = filter_error; /* filter could not be set and was reset */
		break;
	default:
		DBPRT("unexpected response\n");
	}

	/* check if protocol filter was updated and can be send now */
	if ((cl->filter_protocol_pending == filter_updated) && !(timer_pending(&cl->timer_filter))) {
		cl->filter_protocol_pending = filter_pending; /* we are waiting for filter response */
		bnep_filter_net_type_set(cl, &cl->btdev->pf);
	}

	return sizeof(bnep_hdr_response_t);
}


/* process control packet
   extension is set when control command is from bnep header (not extension header) and packet contains extensions
   returns size of processed header of negative value if packet has to be dropped */
int bnep_process_control(struct bnep_con *cl, bnep_hdr_control_t *hdr, unsigned int length, int extension)
{
	if (length < sizeof(bnep_hdr_control_t)) /* not able to read type field */
		goto error_length;

	switch(hdr->control_type) {
	case BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD:
		if (extension)
			goto error_extension;
		if (length < sizeof(bnep_hdr_ccnu_t))
			goto error_length;
		return bnep_process_command_not_understood(cl, (bnep_hdr_ccnu_t*) hdr);
	case BNEP_SETUP_CONNECTION_REQUEST_MSG:
		if (length < sizeof(bnep_hdr_scrm_t))
			goto error_length;
		return bnep_process_setup_connection_request(cl, (bnep_hdr_scrm_t*) hdr, length);
	case BNEP_SETUP_CONNECTION_RESPONSE_MSG:
		if (length < sizeof(bnep_hdr_response_t))
			goto error_length;
		return bnep_process_setup_connection_response(cl, (bnep_hdr_response_t*) hdr);
	case BNEP_FILTER_NET_TYPE_SET_MSG:
 		if (length < sizeof(bnep_hdr_fntsm_t))
			goto error_length;
		return bnep_process_filter_protocol_set(cl, (bnep_hdr_fntsm_t*) hdr, length);
	case BNEP_FILTER_NET_TYPE_RESPONSE_MSG:
		if (length < sizeof(bnep_hdr_response_t))
			goto error_length;
		return bnep_process_filter_protocol_response(cl, (bnep_hdr_response_t*) hdr);
	case BNEP_FILTER_MULTI_ADDR_SET_MSG:
		if (length < sizeof(bnep_hdr_fmasm_t))
			goto error_length;
		return bnep_process_filter_multicast_set(cl, (bnep_hdr_fmasm_t*) hdr, length);
	case BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG:
		if (length < sizeof(bnep_hdr_response_t))
			goto error_length;
		return bnep_process_filter_multicast_response(cl, (bnep_hdr_response_t*) hdr);
	default:
		bnep_command_not_understood(cl, hdr->control_type);
		return -EFAULT; /* packet has to be dropped because we dont know header size */
	}

error_length:
	DBPRT("control packet is to small (type 0x%02x, %d bytes)\n", hdr->control_type, length);
	return -EFAULT;

error_extension:
	DBPRT("command not understood packet contains extension\n");
	return -EFAULT;
}

/* send ethernet frame to remote device */

/* check if remote device is receiver (either multicast listener or unicast destination) of ethernet frame:
   returns 0 if it is */
int bnep_send_ethernet_check(struct bnep_con *cl, struct sk_buff *skb)
{
	struct ethhdr *eth = (struct ethhdr*) skb->data;

	if (!cl || (cl->state != configured) || (cl == NULL) || !(cl->setup_complete) )
		return 1;

	if (ethbdacmp(&cl->ch->bda, &eth->h_source) == 0) // do not send to yourself
		return 1;

	if (*eth->h_dest&1) { /* multicast */
		if (bnep_filter_multicast_test(cl, (ETH_ADDR*) &eth->h_dest)) /* address is filtered out */
			return 1;
	} else {
		if (ethbdacmp(&cl->ch->bda, &eth->h_dest)) /* remote is not unicast destination */
			return 1;
	}

	/* check protocol filter */
	if (bnep_filter_protocol_test(cl, eth->h_proto))
		return 1;

	return 0;
}

/*
 * prepares transmission of ethernet frame: changes ethernet header to bnep header
 */
void bnep_send_ethernet_prepare(struct pan_dev *btdev, BD_ADDR *bda, struct sk_buff *skb)
{
	struct ethhdr	eth;
	int 		source, dest;

	memcpy(&eth, skb->data, ETH_HLEN);
	skb_pull(skb, ETH_HLEN);

	/* build bnep header with maximum compression */
	source = ethbdacmp(&btdev->bdaddr, &eth.h_source);
	if (bda)
		dest = ethbdacmp(bda, &eth.h_dest);
	else
		dest = 1;

	if (!source) {
		if (!dest) { /* compress: skip source and dest */
			bnep_hdr_ce_t *hdr = (bnep_hdr_ce_t*) skb_push(skb, sizeof(bnep_hdr_ce_t));
			hdr->type = BNEP_COMPRESSED_ETHERNET;
			hdr->extension = 0;
			hdr->h_proto = eth.h_proto;
		} else { /* compress: skip only source */
			bnep_hdr_cedo_t *hdr = (bnep_hdr_cedo_t*) skb_push(skb, sizeof(bnep_hdr_cedo_t));
			hdr->type = BNEP_COMPRESSED_ETHERNET_DEST_ONLY;
			hdr->extension = 0;
			hdr->h_proto = eth.h_proto;
			memcpy(&hdr->h_dest, &eth.h_dest, ETH_ALEN);
		}
	} else {
		if (!dest) { /* compress: skip only dest */
			bnep_hdr_ceso_t *hdr = (bnep_hdr_ceso_t*) skb_push(skb, sizeof(bnep_hdr_ceso_t));
			hdr->type = BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY;
			hdr->extension = 0;
			hdr->h_proto = eth.h_proto;
			memcpy(&hdr->h_source, &eth.h_source, ETH_ALEN);
		} else { /* dont compress */
			bnep_hdr_ge_t *hdr = (bnep_hdr_ge_t*) skb_push(skb, sizeof(bnep_hdr_ge_t));
			hdr->type = BNEP_GENERAL_ETHERNET;
			hdr->extension = 0;
			hdr->h_proto = eth.h_proto;
			memcpy(&hdr->h_source, &eth.h_source, ETH_ALEN);
			memcpy(&hdr->h_dest, &eth.h_dest, ETH_ALEN);
		}
	}
	DBPRT("prepared packet: source: %d, dest: %d, type: %d\n",
			source, dest, ((bnep_hdr_ge_t*)skb->data)->type);
}

/* test if packet to specified receivers should be sent with broadcast (result != 0) */
int bt_broadcast_test(struct bcast_list *cl, __u32 len)
{
	return 0;
}

/* sends a packet to several receivers */
int __bnep_send_multicast(struct pan_dev *btdev, struct bcast_list *bl, struct sk_buff *skb_source)
{
	int i, err = 0;

	if (bl->counter == 0) {
		dev_kfree_skb(skb_source);
		return 0;
	}
	if (bt_broadcast_test(bl, skb_source->len)) {
		/* use broadcast? */
		DBPRT("sending with *****BROADCAST*****\n");
		return pan_DataWriteBroadcast(BNEPPSM, skb_source);
	}
	for (i = 0; i < bl->counter; i++) {
                struct sk_buff *skb;
		if (i + 1 == bl->counter) /* last transmission */
			skb = skb_source;
		else {
			skb = skb_copy(skb_source, GFP_ATOMIC);
			if (!skb) {
				DBPRT("skb_copy() failed\n");
				continue;	/* try next */
			}
		}
		DBPRT("sending to %s\n", bda2str(&bl->con[i]->bda));
		if (pan_cb(skb)->outgoing)
			pan_skb_set_owner_w(skb, btdev);
		bnep_send_ethernet_prepare(btdev, &bl->con[i]->bda, skb);
		err = l2ca_send_data(bl->con[i], skb);
		if (err) {
			DBPRT("error %d\n", err);
		}
	}
	return err;
}

int bnep_send_multicast(struct pan_dev *btdev, struct sk_buff *skb)
{
	struct bcast_list	bl;
	struct bnep_con 	*cl_p, *cl_f = NULL;

	bl.counter = 0;
	btl_read_lock(&btdev->connections);
	btl_for_each (cl_p, btdev->connections) {
		if (!bnep_send_ethernet_check(cl_p, skb)) { /* device is interested in frame */
			bl.con[bl.counter++] = cl_p->ch;
			cl_f = cl_p; /* remember one target */
		}
	}
	btl_read_unlock(&btdev->connections);
	if (bl.counter) {
		struct sk_buff *skb_c = skb_copy(skb, GFP_ATOMIC);

		if (skb_c)
			__bnep_send_multicast(btdev, &bl, skb_c);
	}
	return bl.counter;
}

/* send ethernet frame to l2cap layer if connection is configured and setup
   returns 0 on successfull transmission, skb_source is not modified */
int bnep_send_unicast(struct pan_dev *btdev, struct bnep_con *cl, struct sk_buff *skb)
{
	int	err;

	if (!cl || (cl->state != configured) || !cl->setup_complete)
		return -EFAULT;
	DBPRT("sending packet to %s, len: %d\n", bda2str(&cl->ch->bda), skb->len);
	pan_skb_set_owner_w(skb, btdev);
	bnep_send_ethernet_prepare(btdev, &cl->ch->bda, skb);
	err = l2ca_send_data(cl->ch, skb);
	if (err)
		DBPRT("error\n");

	return err;
}

/* receive packet fom lower layer */
void bnep_process_lower_ethernet(struct bnep_con *cl, struct ethhdr *eth, struct sk_buff *skb)
{
	struct sk_buff	*skb_new;
	int 		head_needed = ETH_HLEN;
	int		err;

	if (cl->btdev->role != AFFIX_PAN_PANU) /* we may have to send the packet to other remote devices */
		head_needed = L2CAP_SKB_RESERVE + sizeof(bnep_hdr_ge_t); /* maximum possible header size */

	DBPRT("incoming packet from %s to %s protocol 0x%04x size %d\n", ETH_ADDR2str(&eth->h_source),
		ETH_ADDR2str(&eth->h_dest), __be16_to_cpu(eth->h_proto), skb->len + ETH_HLEN);

	skb_new = skb_realloc_headroom(skb, head_needed);
	dev_kfree_skb(skb);
	skb = skb_new;
	if (!skb)
		return;

	skb_push(skb, ETH_HLEN);
	memcpy(skb->data, eth, ETH_HLEN);

	/* here we have ethernet packet */
	pan_cb(skb)->outgoing = 0;

	if (cl->btdev->role != AFFIX_PAN_PANU) {
		/* we may have to send the packet to other remote devices */
		err = bnep_send_multicast(cl->btdev, skb);
		if (err < 0) {
			dev_kfree_skb(skb);
			return;
		}
		/* if the frame is an unicast frame and either 
		 * it was sent to a remote device or 
		 * we are not a bridge and it is not for ourself, than drop the frame
		 */
		//DBPRT("counter: %d, role: %d, comp: %d\n", bl.counter, cl->btdev->role,
		//		ethbdacmp(&cl->btdev->bdaddr, &eth->h_dest));
		if (!(*eth->h_dest & 0x01)) { /* unicast */
			if (err || ((cl->btdev->role == AFFIX_PAN_GN) &&
						ethbdacmp(&cl->btdev->bdaddr, &eth->h_dest))) {
				if (!err)
					DBPRT("no target\n");
				dev_kfree_skb(skb);
				return;
			}
		}
	}
	/* send to upper */
	DBPRT("sending to upper\n");
	pan_net_receive(skb, cl->btdev);
}

/* process bnep packet from l2cap layer */
int bnep_process_lower(struct bnep_con *cl, struct sk_buff *skb)
{
	bnep_hdr_t		*hdr = (bnep_hdr_t*) skb->data;
	struct ethhdr		eth;
	unsigned char 		*payload; /* position of payload or extension header */

	if (skb->len < sizeof(bnep_hdr_t))
		goto error;

	DBPRT("len: %d, type 0x%02x\n", skb->len, hdr->type);

	/* before processing a bnep packet containing an ethernet frame, process all extension headers.
	   this allows us to modify/destroy the packet when processing an ethernet frame.
	   any packet containing a reserved header packet type must be dropped. */
	payload = skb->data;

	switch (hdr->type) {
		case BNEP_GENERAL_ETHERNET:
			payload += sizeof(bnep_hdr_ge_t);
			break;
		case BNEP_COMPRESSED_ETHERNET:
			payload += sizeof(bnep_hdr_ce_t);
			break;
		case BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY:
			payload += sizeof(bnep_hdr_ceso_t);
			break;
		case BNEP_COMPRESSED_ETHERNET_DEST_ONLY:
			payload += sizeof(bnep_hdr_cedo_t);
			break;
			/* control packets can be processed immediatly */
		case BNEP_CONTROL: 
			{
				int result = bnep_process_control(cl, (bnep_hdr_control_t*) hdr, skb->len, hdr->extension);
				if (result < 0)
					goto error;

				payload += result;
				break;
			}
		default:
			DBPRT("dropping unknown BNEP packet, type 0x%02x\n", hdr->type);
			goto error;
	}

	if (hdr->extension) {
		bnep_ext_hdr_t *ext_hdr;
		int extension;

		do {
			ext_hdr = (bnep_ext_hdr_t*) payload;
			if ((unsigned char*) ext_hdr + sizeof(bnep_ext_hdr_t) + ext_hdr->length > skb->tail) {
				DBPRT("packet header is to small\n");
				goto error;
			}

			/* is there another extension following? */
			extension = ext_hdr->extension;

			switch(ext_hdr->type)
			case BNEP_EXT_CONTROL: {
				/* bnep extension control headers are like bnep control headers, but without
				   packet type/extension field. the header is set to the extension headers length field,
				   so that the control_type field matches with the extension headers data field */
				bnep_process_control(cl, (bnep_hdr_control_t*) &ext_hdr->length, ext_hdr->length + 1, 0); 
				break;
			default:
				DBPRT("unknown extension header 0x%02x\n", ext_hdr->type);

			}
			/* we always know the length of the extension header, even if the extension header 
			   type itself is unknown */
			payload += sizeof(bnep_ext_hdr_t) + ext_hdr->length;

		} while(extension);
	}

	skb_pull(skb, payload - skb->data);

	/* now, all controls/extension headers are processed. process ethernet frame */
	switch (hdr->type) {
		case BNEP_GENERAL_ETHERNET:
			{
				bnep_hdr_ge_t *hdr_ge = (bnep_hdr_ge_t*) hdr;

				memcpy(&eth.h_dest, &hdr_ge->h_dest, ETH_ALEN);
				memcpy(&eth.h_source, &hdr_ge->h_source, ETH_ALEN);
				eth.h_proto = hdr_ge->h_proto;
				break;
			}
		case BNEP_COMPRESSED_ETHERNET:
			{
				bnep_hdr_ce_t *hdr_ce = (bnep_hdr_ce_t*) hdr;

				bda2eth(&eth.h_dest, &cl->btdev->bdaddr); /* local */
				bda2eth(&eth.h_source, &cl->ch->bda); /* remote */
				eth.h_proto = hdr_ce->h_proto;
				break;
			}
		case BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY:
			{
				bnep_hdr_ceso_t *hdr_ceso = (bnep_hdr_ceso_t*) hdr;

				bda2eth(&eth.h_dest, &cl->btdev->bdaddr); /* local */
				memcpy(&eth.h_source, &hdr_ceso->h_source, ETH_ALEN); /* remote */
				eth.h_proto = hdr_ceso->h_proto;
				break;
			}
		case BNEP_COMPRESSED_ETHERNET_DEST_ONLY:
			{
				bnep_hdr_cedo_t *hdr_cedo = (bnep_hdr_cedo_t*) hdr;

				memcpy(&eth.h_dest, &hdr_cedo->h_dest, ETH_ALEN); /* local */
				bda2eth(&eth.h_source, &cl->ch->bda); /* remote */
				eth.h_proto = hdr_cedo->h_proto;
				break;
			}
		default:
			dev_kfree_skb(skb);
			return 0;
	}
#if 0 // MULTICAST HACK for Symbian PAN
	{
		struct iphdr	*iph = (void*)skb->data;

		if (iph->daddr == 0xfaffffef) {
			char eth_maddr[] = { 0x01, 0x00, 0x5e, 0x7f, 0xff, 0xfa };
			BTINFO("MULTICAST!!!\n");
			memcpy(eth.h_dest, eth_maddr, 6);
			//iph->ttl = 4;
			//iph->check = 0;
			//iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
		}
	}
#endif
	bnep_process_lower_ethernet(cl, &eth, skb);
	return 0;
error:
	dev_kfree_skb(skb);
	return -EFAULT;
}

/* receive packet fom upper layer */

/* process ethernet packet from upper layer */
int bnep_process_upper(struct pan_dev *btdev, struct sk_buff *skb)
{
	struct sk_buff	*skb_new;
	int		err;
#ifdef CONFIG_AFFIX_DEBUG
	struct ethhdr	*eth = (struct ethhdr*) skb->data;
#endif

	DBPRT("outgoing packet from %s to %s protocol 0x%04x size %d\n", ETH_ADDR2str(&eth->h_source),
	 	ETH_ADDR2str(&eth->h_dest), __be16_to_cpu(eth->h_proto), skb->len);

	skb_new = skb_realloc_headroom(skb, L2CAP_SKB_RESERVE + sizeof(bnep_hdr_ge_t));
	dev_kfree_skb(skb);
	skb = skb_new;
	if (!skb)
		return -ENOMEM;

	pan_cb(skb)->outgoing = 1;
	
	if (btdev->role == AFFIX_PAN_PANU) {
		err = bnep_send_unicast(btdev, __btl_first(btdev->connections), skb);
		if (err < 0) 
			dev_kfree_skb(skb);
	} else {
		err = bnep_send_multicast(btdev, skb);
		dev_kfree_skb(skb);
	}
	return err < 0 ? err : 0;
}


/* initialize new connection to use with bnep */
int bnep_init(struct bnep_con *cl)
{
	// TODO: make this configurable
	cl->filter_protocol_admitted = 1;	/* remote side is allowed to set protocol filter */
	cl->filter_multicast_admitted = 1;	/* remote side is allowed to set multicast filter */

	if (cl->btdev->pf.count) {
		/* remote side has not yet accepted local protocol filter */
		cl->filter_protocol_pending = filter_updated;
	}
	if (cl->btdev->mf.count) {
		/* remote side has not yet accepted local multicast filter */
		cl->filter_multicast_pending = filter_updated;
	}
	cl->pf.count = 0;		/* default remote filter (disabled) */
	cl->mf.count = 0;		/* default remote filter (disabled) */

	/* init timers */
	init_timer(&cl->timer_setup);
	cl->timer_setup.function = bnep_setup_connection_request_timeout;
	cl->timer_setup.data = (unsigned long) cl;

	init_timer(&cl->timer_filter);
	cl->timer_filter.function = bnep_filter_set_timeout;
	cl->timer_filter.data = (unsigned long) cl;

	if (!l2ca_server(cl->ch)) {
		// client send connection request
		bnep_setup_connection_request(cl);
	}
	return 0;
}

/* destroy internal data of a connection */
void bnep_close(struct bnep_con *cl)
{
	del_timer(&cl->timer_setup);
	del_timer(&cl->timer_filter);
}

