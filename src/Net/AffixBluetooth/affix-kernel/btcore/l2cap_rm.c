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

int l2cap_try_to_send(l2cap_ch *ch);
int l2cap_send_sframe(l2cap_ch *ch,__u16 rcid, __u16 reqseq, __u16 sframetype, __u16 ret_bit);
static int l2cap_release(l2cap_ch *ch);
static inline void __l2ca_put(l2cap_ch *ch);


/* Retransmission and monitors timers */
static inline int l2cap_retransmission_timer_pending(l2cap_ch *ch)
{
	/* Need writing lock on timer !? */
	return timer_pending(&ch->ret_timer);
}

static inline void __l2cap_start_retransmission_timer(l2cap_ch *ch, unsigned long timeout)
{
	if (timeout <= jiffies)
		return;
	if (!mod_timer(&ch->ret_timer, timeout))
		l2ca_hold(ch);
}

static inline void l2cap_start_retransmission_timer(l2cap_ch *ch, unsigned long len)
{
	__l2cap_start_retransmission_timer(ch, len + jiffies);
}

static inline void l2cap_stop_retransmission_timer(l2cap_ch *ch)
{
	if (del_timer(&ch->ret_timer))
		l2ca_put(ch);
}

static inline void __l2cap_start_monitor_timer(l2cap_ch *ch, unsigned long timeout)
{
	if (timeout <= jiffies)
		return;
	if (!mod_timer(&ch->monitor_timer, timeout))
		l2ca_hold(ch);
}

static inline void l2cap_start_monitor_timer(l2cap_ch *ch, unsigned long len)
{
	__l2cap_start_monitor_timer(ch, len + jiffies);
}

static inline void l2cap_stop_monitor_timer(l2cap_ch *ch)
{
	if (del_timer(&ch->monitor_timer))
		l2ca_put(ch); 
}

void l2cap_retransmission_timer(unsigned long data)
{
	l2cap_ch	*ch = (l2cap_ch*)data;
	DBFENTER;
	DBPRT("Reatranmission timer Expired!\n");	
	if (REMOTE_MODE(ch) == L2CAP_FLOW_CONTROL_MODE) {
		DBPRT("FLOW CONTROL MODE\n");
		ch->expected_ack_seq = (ch->expected_ack_seq + 1) % 64;
		if (ch->expected_ack_seq == ch->next_tx_seq) { /* No I-Frames waiting to be sent, and all has been acknowledge */
			l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
		}
		else {
			/* Start retransmkission timer. The timer will be started in the l2cap_try_to_send functio,so no need to restated here.*/
			l2cap_try_to_send(ch); /* This function will send any waiting frame, or exit silently if there is none waiting frame to be transmitted. */
		}
		l2ca_put(ch);
	}
	else { /* LOCAL_MODE(ch) == L2CAP_RETRANSMISSION_MODE */
			/* Update ch->next_tx_frame */
		DBPRT("RETRANSMISSION MODE.\n");
		if (ch->transmit_counter < ch->cfgin.rfc.max_transmit) {
			if ((!test_bit(L2CAP_LOCAL_RDB, &ch->rfc_flags)) && (ch->expected_ack_seq != ch->next_tx_seq)){
				/* We may need locks to keep this assignation safe */
				ch->next_tx_frame = skb_peek(&ch->tx_buffer);
				ch->next_tx_seq = ch->expected_ack_seq;
				ch->transmit_counter++; 
				/* Start retransmission timer. The timer will be started in the l2cap_try_to_send functio,so no need to restated here.*/
				l2cap_try_to_send(ch);
			}
			l2ca_put(ch);
		}
		else { /* TransmitCounter >= MaxTranmit => The channel shall be move to CLOSED state */
			DBPRT("Transmit couter (%d) Max Transmit (%d). CLOSING CHANNEL\n",ch->transmit_counter, ch->cfgin.rfc.max_transmit);
			switch (l2cap_release(ch)) {
				case CON_CLOSED:
				case CON_W4_DISCRSP:
					break;
				default:
					l2ca_disconnect_ind(ch);
					break;
			}
			__l2ca_put(ch);
		}
	}
	DBFEXIT;
}

void l2cap_monitor_timer(unsigned long data)
{
	l2cap_ch	*ch = (l2cap_ch*)data;
	DBFENTER;
#ifndef  __L2CAP_TEST_TIMERS__ /* Use to test the retransmission timer */
	if (!test_bit(L2CAP_LOCAL_REJ_CONDITION, &ch->rfc_flags)) {
		l2cap_send_sframe(ch,ch->rcid,ch->buffer_seq, L2CAP_S_RR, 0); /* Retransmisison bit is set 0 in flow control mode */
	} 
	l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
#endif
	l2ca_put(ch);
	DBFEXIT;
}

/* End retransmission and monitor timers */


static inline __u16 get_reqseq(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	return ((__btoh16(iframe->control) & L2CAP_REQSEQ_MASK)>>8); 
}

static inline void set_reqseq(struct sk_buff *skb, __u16 num)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	iframe->control = __htob16(((num<<8) & L2CAP_REQSEQ_MASK) | (__btoh16(iframe->control) & L2CAP_CLEAR_REQSEQ));
}

static inline __u16 get_txseq(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	return ((__btoh16(iframe->control) & L2CAP_TXSEQ_MASK)>>1);
}

static inline void set_txseq(struct sk_buff *skb, __u16 num)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	iframe->control = __htob16(((num<<1) & L2CAP_TXSEQ_MASK) | (__btoh16(iframe->control) & L2CAP_CLEAR_TXSEQ));
}

static inline void set_retransmission_bit(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	iframe->control = __htob16( __btoh16(iframe->control) & L2CAP_SET_RET_DISABLE_BIT);
}

static inline void unset_retransmission_bit(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	iframe->control = __htob16( __btoh16(iframe->control) & L2CAP_UNSET_RET_DISABLE_BIT);
}

static inline __u16 get_retransmission_bit(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	return (__btoh16(iframe->control) & L2CAP_RETRANSMISSION_BIT_MASK);
}

static inline __u16 get_sar(struct sk_buff *skb)
{
	l2cap_iframe_hdr_t	*iframe = (l2cap_iframe_hdr_t *)(skb->data);
	return (SAR(iframe->control));
}
	
static inline int TxWindowFull(l2cap_ch *ch)
{
	DBPRT("TxWindowFull values: Tx Window size (%d) NextTxSeq (%d) ExpectedAckSeq(%d) Num Pending packet (%d)\n", ch->cfgin.rfc.txwindow_size, ch->next_tx_seq, ch->expected_ack_seq,((ch->next_tx_seq - ch->expected_ack_seq + 64) % 64));
	return (ch->cfgin.rfc.txwindow_size <= ((ch->next_tx_seq - ch->expected_ack_seq + 64) % 64));

}

static inline int rx_windowfull(l2cap_ch *ch)
{
	return (ch->cfgin.rfc.txwindow_size <= ((ch->expected_tx_seq - ch->buffer_seq + 64) % 64));
}

static inline void set_fcs(struct sk_buff *skb, __u16 fcs)
{
	//* Note the space for the fcs has been claimed previously with skb_put()
	__put_u16(skb->data + skb->len - 2,__htob16(fcs));
}

static inline void enqueue_tx_buffer(l2cap_ch *ch, struct sk_buff *skb)
{
	if (!ch->next_tx_frame) {
			ch->next_tx_frame = skb;
	}
	skb_queue_tail(&ch->tx_buffer,skb);
}


void l2cap_onedottwo_init(l2cap_ch *ch)
{
	skb_queue_head_init(&(ch->tx_buffer));
	ch->next_tx_frame = NULL;	/* Points to the sk_buff(L2CAP packet) to be transmited next. */
	ch->next_tx_seq = 0;	
	ch->expected_ack_seq = 0;
	/* Receiving buffer */
	skb_queue_head_init(&(ch->rx_buffer));
	ch->buffer_seq = 0;		/* Points to the first sk_buff (L2CAP packet) waiting to be pull out from the upper layer */
	ch->expected_tx_seq = 0;		/* Sequence number of the next packet to be received */ 

	/* Reassembly */
	ch->lframe = NULL;			/* Points to the packet that it is currently being reassembled. */
	ch->expected_reassembly_seq = 0;	/* Initialitation NOT really needed */
	
	/* Local flags. Used mainly to keep track of the state of the RetransmissionDisableBit */
	ch->rfc_flags = 0;
	ch->transmit_counter = 0;
	
	/* l2cap_try_to_send lock */
	spin_lock_init(&ch->xmit_lock);
	/* Initialitation timers */
	init_timer(&ch->ret_timer);
	ch->ret_timer.data = (unsigned long)ch;
	ch->ret_timer.function = l2cap_retransmission_timer;

	init_timer(&ch->monitor_timer);
	ch->monitor_timer.data = (unsigned long)ch;
	ch->monitor_timer.function = l2cap_monitor_timer;
}

void l2cap_onedottwo_close(l2cap_ch *ch)
{
  	spin_unlock_wait(&ch->xmit_lock);	/* unlock xmit_lock */
	skb_queue_purge(&ch->rx_buffer);
	skb_queue_purge(&ch->tx_buffer);
}
__u16 calculate_fcs(struct sk_buff *frame)
{
	l2cap_hdr_t* 	l2cap_frame = (l2cap_hdr_t *)frame->data;
	__u8*  cpos = frame->data;
	__u16	len = l2cap_frame->length + 2,gd = 0x8005,lfsr = 0x0000, bit =0x0000, fcs = 0x0000, apply_gd, ind;
	int j;
	
	
	/*NOTE: len has the total size of frame including length and CID fields minus the FCS field */
	
	for(ind=0; ind< len; ind++){ /* For every byte in the frame (except FCS) do... */
		for(j=0;j<8;j++) { /* From the LSB first apply the algorithm explained in Page 39 of the L2CAP 1.2 Specification */
			bit = (*cpos>>j) & 0x0001;
			apply_gd = (bit ^ (lfsr>>15));
			lfsr = lfsr << 1;
			if (apply_gd) {
				lfsr = lfsr ^ gd;
			}
		}
		cpos++;
	}

	/* The last bit has been entered and now we set the switch to postion 2 (See Figure 3.4, page 39 L2CAP Specification 1.2) to get the FCS value */
	
	for(j=0; j<16; j++)
	{
		fcs = ((lfsr<<j) & 0x8000) | (fcs>>1);
	}
	//DBPRT("L2CAP 1.2:FCS caluclation result : %x\n",fcs);
	return fcs;	
}


int invalid_frame_detection(l2cap_ch *ch,struct sk_buff *skb)
{
	l2cap_iframe_hdr_t* 	iframe = (l2cap_iframe_hdr_t *)skb->data;
	__u16 fcs = 0;
	__u16 sdu_len = 0;


	if (skb->len < L2CAP_FRAME_MIN_LEN)	/* I-frame has less than 8 octets */
	{
		DBPRT("L2CAP 1.2: I-frame has less than 8 octets\n");
		return -4;
	}	
	if ((SAR(iframe->control) == L2CAP_SAR_START) && (skb->len < L2CAP_FRAME_MIN_LEN+2)) /* I-frame SAR=01 less than 10 octets */
	{	
		DBPRT("L2CAP 1.2: START of segmented I-frame has less than 10 octets\n");
		return -5;
	}
	sdu_len =  __btoh16(__get_u16(iframe->data));
	if ((SAR(iframe->control) == L2CAP_SAR_START) && (( sdu_len > ch->cfgin.rfc.mps) || (sdu_len <= skb->len - 10 )))	/* SDU Length greater than the maximum payload size */
	{	/* NOTE: We only use this function to detect incoming invalid frames. Our frames are always valid ;) */
		DBPRT("L2CAP 1.2: Frame is larger than the MPS or START Frame is smaller than total frame size. START I-Frame length (%d) SDU Lenght (%d) MPS (%d)\n",skb->len -10, sdu_len,ch->cfgin.rfc.mps);
		return -3;
	}
	fcs = __btoh16(__get_u16(skb->tail -2));
	
	if (fcs != calculate_fcs(skb)) /* FCS error */
	{
		DBPRT("L2CAP 1.2: FCS error. Frame FCS (%x) Calculated FCS (%x)\n",fcs,calculate_fcs(skb));
		return -2;
	}
	return 0;
	
}

int append_unsegmented_hdr(l2cap_ch *ch,struct sk_buff *skb)
{
	/* unsigned char		*tail = NULL;*/
	l2cap_iframe_hdr_t	*hdr = NULL;
	int			err = -1;
	__u16 			sdu_len = skb->len;
	
	if ((skb_headroom(skb) >= 6) && (skb_tailroom(skb) >= 2)) {
		hdr =  (l2cap_iframe_hdr_t*)skb_push(skb, 6);
		hdr->length = __htob16(sdu_len+4);	// SDU Len + CONTROL field len + FCS field len
		hdr->cid = __htob16(ch->rcid);
		hdr->control = __htob16(L2CAP_SAR_UNSEGMENTED);		// The rest of the control field is set when sending the frame 
		skb_put(skb,2); 	/* Make space for FCS */
		err = 0;
	}
	/* else if no space the copy and the data into a bigger skbuff !!!!*/
	return err;
}

struct sk_buff *create_start_segmented_iframe(l2cap_ch *ch,struct sk_buff *sdu)
{ 
	struct sk_buff 		*lskb = NULL;
	l2cap_iframe_hdr_t	*iframe = NULL;
	unsigned char		*ldata = NULL;

	DBFENTER;
	
	lskb = alloc_skb(HCI_SKB_RESERVE + ch->cfgout.mtu,GFP_ATOMIC);	
	if (lskb == NULL) 
		return NULL;
	
	skb_reserve(lskb,HCI_SKB_RESERVE);
	ldata = skb_put(lskb,8);	/* Magic number 8 comes from reserving space for LENGTH,CID,CONTROL, SDU Length fields */
	iframe = (l2cap_iframe_hdr_t *)(ldata);
	iframe->length = __htob16(ch->cfgout.mtu - 4);/* Magic number 4 comes from excluding the length and cid fields from the total length. */
	iframe->cid = __htob16(ch->rcid);
	
	iframe->control = __htob16(L2CAP_SAR_START);	
	__put_u16(iframe->data,__htob16(sdu->len));

	ldata = skb_put(lskb,ch->cfgout.mtu - 10);
	memcpy(ldata, sdu->data, ch->cfgout.mtu - 10);
	
	skb_put(lskb,2); /* Make space for FCS */
	// NOTE: You can not add the FCS yet because you have not set all the values in the control field. They are set at the moment of sending the packet !
	
	DBFEXIT;
	return lskb;
}

struct sk_buff *create_segmented_iframe(l2cap_ch *ch, __u16 control,unsigned char *sdu, __u16 sdu_len)
{
	struct sk_buff 	*lskb = NULL;
	unsigned char	*ldata = NULL;
	l2cap_iframe_hdr_t *iframe;
	
	DBFENTER;
	lskb = alloc_skb(HCI_SKB_RESERVE + sdu_len + 8,GFP_ATOMIC);	/* Magic number 8 comes from adding the Lenght,CID, Cntrol and FCS fields */
	if (lskb == NULL)
		return NULL;
	
	skb_reserve(lskb, HCI_SKB_RESERVE);
	ldata = skb_put(lskb,6);	/* Magic number 6 comes from reserving space for Length,CID and Control fields) */		
	iframe = (l2cap_iframe_hdr_t *)(ldata);
	iframe->length = __htob16(sdu_len + 4);	/* Magic number 4 comes from the FCS + CONTROL fields. They are also counted with the payload */
	iframe->cid = __htob16(ch->rcid);
	
	iframe->control = __htob16(control);

	ldata = skb_put(lskb,sdu_len);
	memcpy(ldata, sdu, sdu_len);

	skb_put(lskb,2); /* Make space for FCS */
	// NOTE: You can not add the FCS yet because you have not set all the values in the control field. They are set at the moment of sending the packet !	   	   

	DBFEXIT;
	return lskb;
}


void l2cap_reassembly_sdu(l2cap_ch *ch, struct sk_buff *skb)
{
	l2cap_iframe_hdr_t *iframe = (l2cap_iframe_hdr_t *)(skb->data);
	__u16	sdu_len;
	__u16	control = __btoh16(iframe->control);
	__u16	length = __btoh16(iframe->length);
	
	DBFENTER;

	switch (SAR(control))
	{
		case L2CAP_SAR_START:
			sdu_len = __btoh16(__get_u16(iframe->data));
			//DBPRT("Start L2CAP SDU frame.SDU length: %d\n", sdu_len);
			if (ch->lframe != NULL) {
				DBPRT("Two START frames without END frames ! DISCARDING PREVIOUS FRAME\n");
				kfree_skb(ch->lframe);
				/* We continue as the discarded lframe never existed */
			}
			ch->lframe = alloc_skb(sdu_len,GFP_ATOMIC);
			if (ch->lframe == NULL) return; /* No memory then we ignore the frame */
			memcpy(skb_put(ch->lframe,(length - 6)),(iframe->data+2),(length - 6));
			ch->expected_reassembly_seq = (get_txseq(skb) + 1) % 64;
			break;
		case L2CAP_SAR_CONTINUATION:
		case L2CAP_SAR_END:
			//DBPRT("Continuation/End L2CAP SDU frame\n");		
			if (ch->lframe == NULL) {
				DBPRT("CONTINUATION/END frame without START frame ! DISCARDING current frame\n");
				return;
			}
	
			if (ch->expected_reassembly_seq != get_txseq(skb)) {
				DBPRT("Reassembly iframe out of sequence (Freeing L2CAP lframe)\n");
				kfree_skb(ch->lframe);
				ch->lframe = NULL;
				return;
			}
			if (skb_tailroom(ch->lframe) >= (length - 4)) {
				memcpy(skb_put(ch->lframe, (length - 4)), iframe->data, (length - 4));
				ch->expected_reassembly_seq = (ch->expected_reassembly_seq + 1) % 64;
			}
			else {
				DBPRT("L2CAP lframe size mismatch. Dropping packet (==>freeing L2CAP lframe)\n");
				kfree_skb(ch->lframe);
				ch->lframe = NULL;
			}
			break;
		default:
			DBPRT("ERROR Unexpected control frame: %x\n",control);
			break;
	}
	
	DBFEXIT;
}

struct sk_buff *l2cap_build_sframe(__u16 rcid,__u16 reqseq, __u16 sframetype, __u16 ret_bit)
{
	struct sk_buff 		*lskb = NULL;
	l2cap_sframe_hdr_t	*sframe = NULL;
	unsigned char		*ldata = NULL;
	
	lskb = alloc_skb(HCI_SKB_RESERVE + L2CAP_SFRAME_HDR_LEN,GFP_ATOMIC);	
	if (lskb) {
		skb_reserve(lskb, HCI_SKB_RESERVE);
		ldata = skb_put(lskb,L2CAP_SFRAME_HDR_LEN);
		sframe = (l2cap_sframe_hdr_t *)(ldata);
		sframe->length = __htob16(4);	/* Magic number 4 comes from excluding the length and cid fields from the total length. */
		sframe->cid = __htob16(rcid);
		sframe->control = __htob16(reqseq<<8 | sframetype | ret_bit | L2CAP_SFRAME_TYPE);	
		sframe->fcs = __htob16(calculate_fcs(lskb));
	}
	return lskb;
}

int l2cap_send_sframe(l2cap_ch *ch,__u16 rcid, __u16 reqseq, __u16 sframetype, __u16 ret_bit)
{
	struct sk_buff	*skb = NULL;
	int		err = 0;

	DBFENTER;
	skb = l2cap_build_sframe(rcid, reqseq, sframetype, ret_bit);
	if (!skb)
		return -ENOMEM;
	DBPRT("send L2CAP S-FRAME, len = %d\n", skb->len);
	//DBDUMP(skb->data, skb->len);
	err = lp_send_data(ch->con, skb);
	if (err)
		kfree_skb(skb);	
	DBFEXIT;
	return err;
}

void l2cap_ack_frame(l2cap_ch *ch)
{
	l2cap_iframe_hdr_t 	*iframe = NULL;
	__u16 	control = 0;
	__u16 	ret_bit = 0;
	
	if (ch->next_tx_frame) {
		iframe = (l2cap_iframe_hdr_t *)(ch->next_tx_frame->data);
		control = __btoh16(iframe->control);
		/* Just case delete previous values of the fields */
		control = control & L2CAP_CLEAR_REQSEQ;
		control = control & L2CAP_UNSET_RET_DISABLE_BIT;
		/* Set the new values */
		control = control | ch->buffer_seq<<8 | ret_bit;
		iframe->control = __htob16(control);
		/*l2cap_try_to_send(ch);*/
	}
	else {
		l2cap_send_sframe(ch,ch->rcid,ch->buffer_seq, L2CAP_S_RR, ret_bit);
	}
	/* Always that we ACK frame we have to restart the monitor timer, unless the retransmission timer is on.*/
	if (ch->expected_ack_seq == ch->next_tx_seq) {
		l2cap_stop_monitor_timer(ch);
		l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
	}
}
 
struct sk_buff *l2cap_reassembly(l2cap_ch *ch,struct sk_buff *pdu)
{
	struct sk_buff *lskb = NULL;
	l2cap_iframe_hdr_t 	*iframe;
	__u16	control;
	int 	ack_partially_reassembled_frame = 0;

	if (!pdu) return NULL;
	
	iframe = (l2cap_iframe_hdr_t *)(pdu->data);
	control = __htob16(iframe->control);

	//DBPRT("PDU in reassembly function. PDU Length(%d)\n",pdu->len);
	//DBDUMP(pdu->data,pdu->len);
	
	switch(SAR(control)) {
		case L2CAP_SAR_UNSEGMENTED:
			if (!skb_queue_empty(&ch->rx_buffer)) {
				/* We don't queue unsegmented frames. Stalled segmented frames are in the queue, therefore we flush the queue */
				skb_queue_purge(&ch->rx_buffer);
			}
			/* NOTE: Pre-condition: The frame has not been queue in the rx_buffer !*/ 
			skb_pull(pdu,L2CAP_IFRAME_HDR_LEN);
			skb_trim(pdu,pdu->len - L2CAP_FCS_LEN);	/* Litos NOTE: Rememeber here you have give the new lenght, not the length of data to be cut away !*/ 
			lskb = pdu;
			ch->buffer_seq = ch->expected_tx_seq;			/* NOTE: There MUST not be any outstading packet in rx_buffer. */
			DBPRT("UNSEGMENTED SDU in reassembly function. PDU Length(%d)\n",lskb->len);
			//DBDUMP(lskb->data,lskb->len);
			break;
		case L2CAP_SAR_END:
			skb_queue_tail(&ch->rx_buffer,pdu);
			while((lskb = skb_dequeue(&ch->rx_buffer)) != NULL) {	/* Get head skb from rx_buffer until queue empty. */
				l2cap_reassembly_sdu(ch,lskb);			/* Call the 'real' reassembly function */
				skb_unlink(lskb);				/* REALLY Dequeue the frame from rx_buffer. */
				kfree_skb(lskb);				/* Destroy the frame (it is not needed anymore) */
			}
			lskb = ch->lframe;
			ch->lframe = NULL;
			ch->buffer_seq = ch->expected_tx_seq;			/* NOTE: There is only one outstanding segmented packet in rx_buffer. */
			break;
		default:
			skb_queue_tail(&ch->rx_buffer,pdu);
			if (rx_windowfull(ch)) {
				DBPRT("Rx window full\n");
				while((lskb = skb_dequeue(&ch->rx_buffer)) != NULL) {	// * Get head skb from rx_buffer until queue empty.
					l2cap_reassembly_sdu(ch,lskb);			// * Call the 'real' reassembly function
					skb_unlink(lskb);				// * REALLY Dequeue the frame from rx_buffer.
					kfree_skb(lskb);				// * Destroy the fram
				}
				ch->buffer_seq = ch->expected_tx_seq;   /* We free some slots for incoming packets. */
				if (ch->lframe->len > L2CAP_MAX_MPS) { /* If the current reassembly packet is to big then we discard it.(Something has gone wrong!)*/	
					kfree_skb(ch->lframe);
					ch->lframe = NULL;
				}
				ack_partially_reassembled_frame = 1;	/* What ever the situation we ACK those packets */
			}
			break;
	}
	if ((lskb) || (ack_partially_reassembled_frame)) { 	/* 1st CASE for ACK: Unsegmeted packet or Fully reassembled packet (lskb != NULL)
		     						 * 2nd CASE for ACK: Patially reassembled packet (ack_partially_reassembled_packet).
		     						 * This case (2nd) happens when rx_queue full and we want to free some space in the queue ! */
#ifndef __L2CAP_TEST_TIMERS__
		/* To check retransmission mechanism, disable this function call */
		l2cap_ack_frame(ch);
#endif
	}
	return lskb;
}

struct sk_buff * l2cap_segmentation_sdu(l2cap_ch *ch, struct sk_buff *sdu)
/* This function segments the SDU and queue the segmented L2CAP packets in the transmission queue (tx_buffer) */
{
	struct sk_buff *lskb = NULL;
	struct sk_buff_head llist;
	__u16 offset = 0;
	
	DBFENTER;

	DBPRT("SDU length: %d\n",sdu->len);
	
	skb_queue_head_init(&llist);

	/* Create segmented START PDU */
	lskb = create_start_segmented_iframe(ch,sdu);
	if (lskb == NULL)
		goto exit;
	skb_queue_tail(&llist, lskb);

	/* Create segmented CONTINUATION PDU */
	offset = lskb->len - 10;
	//DBPRT("Current offset in the SDU: %d\n",offset);
	while (ch->cfgout.mtu - 8 < sdu->len -offset) {
		lskb = create_segmented_iframe( ch, L2CAP_SAR_CONTINUATION ,(unsigned char *)(sdu->data + offset), ch->cfgout.mtu - 8);
		if (lskb == NULL)
			goto exit;
		skb_queue_tail(&llist, lskb);
		offset += ch->cfgout.mtu - 8;
		//DBPRT("Current offset in the SDU: %d\n",offset);
	}

	/* Create segmented END PDU */
	lskb = create_segmented_iframe( ch, L2CAP_SAR_END, (unsigned char *)(sdu->data + offset), (sdu->len - offset));
	if(lskb){
		skb_queue_tail(&llist, lskb);
		/* Add L2CAP packets to the transmission queue (tx_buffer) */
		while (!skb_queue_empty(&llist)){
			/* NOTE: change the following two lines for just skb_dequeue !!! */
			lskb = skb_peek(&llist);
			skb_unlink(lskb);
			enqueue_tx_buffer(ch,lskb);
		}
	}
	
exit:
	skb_queue_purge(&llist);
	kfree_skb(sdu);
	DBFEXIT;
	return lskb;
}

static inline void dequeue_ack_frames(l2cap_ch *ch, __u16 reqseq)
/* NOTE: Reqseq-1 tells up to which frame has been ACK.*/
{
	__u16 num_ack_frames = (reqseq - ch->expected_ack_seq + 64) % 64;
	__u16 ind = 0;
	
	for(ind = 0;((ind < num_ack_frames) && !skb_queue_empty(&ch->tx_buffer));ind++) {
		kfree_skb(skb_dequeue(&ch->tx_buffer));
		DBPRT("Deleting frame txseq (%d)",ind);
	}
	
	if (num_ack_frames > 0)
			ch->transmit_counter = 1;
	
	DBPRT("Number of packet acknowled and removed: (%d) NumAckFrames (%d)\n",ind,num_ack_frames);
}

struct sk_buff *l2cap_process_txseq_flow_control(l2cap_ch *ch, struct sk_buff *skb)
{
	__u16	txseq = get_txseq(skb);
	
	DBFENTER;

	DBPRT("I-Frame value TxSeq (%d). Local values ExpectedTxSeq (%d) BufferSeq(%d) TxWindow(%d)\n",
			txseq, ch->expected_tx_seq, ch->buffer_seq,ch->cfgin.rfc.txwindow_size);
	
	/* TxSeq Sequenece check */
	if (txseq == ch->expected_tx_seq) {
		DBPRT("I-frame in sequence\n");
	
		ch->expected_tx_seq = (ch->expected_tx_seq + 1) % 64;
	
	}
	else if (((txseq - ch->buffer_seq + 64) % 64) <= ch->cfgin.rfc.txwindow_size) { 
		/* If Out of sequence I-frame */
		
		DBPRT("I-Frame out of sequence: TxSeq:(%d) ExpectedTxSeq (%d) BufferSeq(%d)\n",txseq,ch->expected_tx_seq,ch->buffer_seq);
		
		ch->expected_tx_seq = (txseq + 1) % 64;
		
		/* We should signal the reassembly function about missing frames */
		/* Instead we don't signal and we take action here already */
		/* MISSING FRAMES => (We have decided to) Flush queue */
		
		skb_queue_purge(&ch->rx_buffer);
		ch->buffer_seq = ch->expected_tx_seq; /* We update the bufferseq */
	}	
	else {
		/* Duplicated I-frame or Invalid TxSeq then drop silently the packet */
		DBPRT("Packet dropped TxSeq(%d) Expected_Tx_Seq (%d) BufferSeq (%d) TxWindow(%d)\n",
				txseq,ch->expected_tx_seq,ch->buffer_seq,ch->cfgin.rfc.txwindow_size);
		kfree_skb(skb);
		skb = NULL;
	}

	DBFEXIT;
	return skb;	
}

struct sk_buff *l2cap_process_reqseq_flow_control(l2cap_ch *ch, struct sk_buff *skb)
{
	l2cap_iframe_hdr_t *iframe;
	__u16	txseq,reqseq,control;

	DBFENTER;
	
	if (!skb) return NULL;
	
	iframe = (l2cap_iframe_hdr_t *)(skb->data);
	txseq = get_txseq(skb);
	reqseq = get_reqseq(skb);
	control = __btoh16(iframe->control);
	
	if (IFRAMETYPE(control) || (SFRAMETYPE(control) && (SUPERVISORY(control) == L2CAP_S_RR))) {
		
		DBPRT("ExpAckSeq (%d) ReqSeq (%d) NextTxSeq(%d)\n", ch->expected_ack_seq, reqseq, ch->next_tx_seq);
		/* ReqSeq check */
		if (((reqseq - ch->expected_ack_seq + 64) % 64 ) <= ((ch->next_tx_seq - ch->expected_ack_seq + 64) % 64)) { 
			l2cap_stop_retransmission_timer(ch);
					
			/* Dequeue from TxQueue and Free the acknoledged frames */
			dequeue_ack_frames(ch,reqseq);
		
			ch->expected_ack_seq = reqseq;
			
			DBPRT("New ExpectedAckSeq (%d)\n",ch->expected_ack_seq);
			
			/* Restart the timers */
			if (reqseq != ch->next_tx_seq) {
				l2cap_start_retransmission_timer(ch,ch->cfgin.rfc.retransmission_timeout*HZ/1000);
			}
			else {
				l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
			}
		}
		else {
			DBPRT("Packet dropped ReqSeq(%d) Expected_Ack_Seq (%d) Next_Tx_Seq (%d) TxWindow(%d) \n",reqseq,ch->expected_ack_seq,ch->next_tx_seq,ch->cfgin.rfc.txwindow_size);
			/* L2CAP Page 102 section 8.5.6.2 ReqSeq Sequence error. The L2CAP entity shall close the channel as a consequence of an ReqSeq Sequence error */
			ENTERSTATE(ch, CON_W4_DISCRSP);
			l2cap_disconnect_req(ch, L2CAP_RTX_TIMEOUT_DISC, __alloc_id(ch), ch->rcid, ch->lcid);

			kfree_skb(skb);
			return NULL;
		}
	}

	if (SFRAMETYPE(control)) {
		kfree_skb(skb);
		skb = NULL;
	}
	
	DBFEXIT;
	return skb;
}

struct sk_buff *l2cap_process_txseq_retransmission(l2cap_ch *ch, struct sk_buff *skb)
{
	__u16	txseq = get_txseq(skb);
	
	DBFENTER;

	DBPRT("I-Frame value TxSeq (%d). Local values ExpectedTxSeq (%d) BufferSeq(%d) TxWindow(%d)\n",txseq, ch->expected_tx_seq, ch->buffer_seq,ch->cfgin.rfc.txwindow_size);
	
	/* TxSeq Sequenece check */
	if (txseq == ch->expected_tx_seq) {
		DBPRT("I-frame in sequence\n");
		ch->expected_tx_seq = (ch->expected_tx_seq + 1) % 64;
		clear_bit(L2CAP_LOCAL_REJ_CONDITION,&ch->rfc_flags);	/* Unset any pending REJ Expeception codition (if any) */
	}
	else {
		if (((txseq - ch->buffer_seq + 64) % 64) <= ch->cfgin.rfc.txwindow_size) { 
			/* If Out of sequence I-frame */
			DBPRT("I-Frame out of sequence: TxSeq:(%d) ExpectedTxSeq (%d) BufferSeq(%d)\n",txseq,ch->expected_tx_seq,ch->buffer_seq);
			/* Check value to set in retransmission disabble bit. */
			if (!test_bit(L2CAP_LOCAL_REJ_CONDITION,&ch->rfc_flags)) {
				l2cap_send_sframe(ch,ch->rcid,ch->expected_tx_seq,L2CAP_S_REJ,0);
				set_bit(L2CAP_LOCAL_REJ_CONDITION,&ch->rfc_flags);
			}
		} /* ELSE: Duplicated I-frame or Invalid TxSeq then drop silently the packet */
		
		DBPRT("Packet dropped TxSeq(%d) Expected_Tx_Seq (%d) BufferSeq (%d) TxWindow(%d)\n",txseq,ch->expected_tx_seq,ch->buffer_seq,ch->cfgin.rfc.txwindow_size);
		kfree_skb(skb);
		skb = NULL;

	}	

	DBFEXIT;
	return skb;
}

struct sk_buff *l2cap_process_reqseq_retransmission(l2cap_ch* ch, struct sk_buff *skb)
{
	l2cap_iframe_hdr_t *iframe;
	__u16 reqseq,control;
	
	DBFENTER;
	
	if (!skb) return NULL;
	
	iframe = (l2cap_iframe_hdr_t *)(skb->data);
	reqseq = get_reqseq(skb);
	control = __btoh16(iframe->control);

	
	/* Check and handling of the retransminssion disable bit */
	if (get_retransmission_bit(skb)) { /* Retransmission Disable Bit ON (1) */
		if (!test_bit(L2CAP_LOCAL_RDB,&ch->rfc_flags)) {
			l2cap_stop_retransmission_timer(ch);
			l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
			set_bit(L2CAP_LOCAL_RDB,&ch->rfc_flags);
		}
	}
	else {	/* Retranmission Disable Bit OFF (0) */
		if (test_bit(L2CAP_LOCAL_RDB,&ch->rfc_flags)) {
			clear_bit(L2CAP_LOCAL_RDB,&ch->rfc_flags);
			l2cap_stop_monitor_timer(ch);
		}
	} 

	DBPRT("ExpAckSeq (%d) ReqSeq (%d) NextTxSeq(%d)\n", ch->expected_ack_seq, reqseq, ch->next_tx_seq);
	/* ReqSeq check */ 
	if (((reqseq - ch->expected_ack_seq + 64) % 64 ) <= ((ch->next_tx_seq - ch->expected_ack_seq + 64) % 64)) { 			
		
		dequeue_ack_frames(ch,reqseq);
		ch->expected_ack_seq = reqseq;
		DBPRT("Stopping retransmission time\n");
		l2cap_stop_retransmission_timer(ch);

		/* Now we update the ExpectedAckSeq */
		
		if (SFRAMETYPE(control) && (SUPERVISORY(control) != L2CAP_S_RR)) {
			DBPRT("REJ frame dectected control field (%d)",control);
			ch->next_tx_seq = reqseq;
			if (!skb_queue_empty(&ch->tx_buffer)) {
				ch->next_tx_frame = skb_peek(&ch->tx_buffer);
			}
		}
		/* ELSE (IFRAMETYPE(control) || (SFRAMETYPE(control) && (SUPERVISORY(control) == L2CAP_S_RR))) */
		
		DBPRT("New ExpectedAckSeq (%d) NextTxFrame (%d)\n",ch->expected_ack_seq, ch->next_tx_seq);

		/* restart the timers, if retransmission disabled keep the monitor timer running */
		if (!test_bit(L2CAP_LOCAL_RDB,&ch->rfc_flags)) {
			if (ch->expected_ack_seq != ch->next_tx_seq) {
				l2cap_start_retransmission_timer(ch,ch->cfgin.rfc.retransmission_timeout*HZ/1000);
			}
			else {
				l2cap_start_monitor_timer(ch,ch->cfgin.rfc.monitor_timeout*HZ/1000);
			}
		}

	}
	else {
		DBPRT("Packet dropped ReqSeq(%d) Expected_Ack_Seq (%d) Next_Tx_Seq (%d) TxWindow(%d) \n",
				reqseq,ch->expected_ack_seq,ch->next_tx_seq,ch->cfgin.rfc.txwindow_size);
		
		/* L2CAP Page 102 section 8.5.6.2 ReqSeq Sequence error. The L2CAP entity shall close the channel as a consequence of an ReqSeq Sequence error */
		ENTERSTATE(ch, CON_W4_DISCRSP);
		l2cap_disconnect_req(ch, L2CAP_RTX_TIMEOUT_DISC, __alloc_id(ch), ch->rcid, ch->lcid);
		
		kfree_skb(skb);
		return NULL;
	}

	if (SFRAMETYPE(control)) {
		kfree_skb(skb);
		skb = NULL;
	}

	return skb;

}

struct sk_buff* l2cap_retransmission_flowcontrol(l2cap_ch* ch, struct sk_buff *skb)
{
	l2cap_iframe_hdr_t *iframe;
	__u16	control;

	DBFENTER;
	
	if (!invalid_frame_detection(ch,skb)) {
		
		iframe = (l2cap_iframe_hdr_t *)(skb->data);
		control = __btoh16(iframe->control);
		
		if (IFRAMETYPE(control)) {
			if (LOCAL_MODE(ch) == L2CAP_RETRANSMISSION_MODE) {
				skb = l2cap_process_txseq_retransmission(ch,skb);
			}
			else { /* (LOCAL_MODE(ch) == L2CAP_FLOW_CONTROL_MODE) */
				/* TO-DO: ASSERT THAT*/
				skb = l2cap_process_txseq_flow_control(ch,skb);
			}
		}
		
		if(REMOTE_MODE(ch) == L2CAP_RETRANSMISSION_MODE) {
			skb = l2cap_process_reqseq_retransmission(ch,skb);
		}
		else { /* (LOCAL_MODE(ch) == L2CAP_FLOW_CONTROL_MODE) */
			/* TO-DO: ASSERT THAT*/
			skb = l2cap_process_reqseq_flow_control(ch,skb);
		}
	}
	else {
		DBPRT("Invalid I-frame detected\n");
		kfree_skb(skb);
		skb = NULL;
	}

	DBFEXIT;
	return skb;
}

