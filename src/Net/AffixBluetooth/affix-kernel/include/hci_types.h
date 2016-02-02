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
   $Id: hci_types.h,v 1.38 2004/03/10 08:01:34 chineape Exp $

   Host Controller Interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		


#ifndef _HCI_TYPES_H
#define _HCI_TYPES_H

#ifndef __KERNEL__
#include <sys/types.h>
#endif
#include <linux/types.h>

#ifdef __PACK__
#undef __PACK__
#endif
#define __PACK__	__attribute__ ((packed))


#ifdef  __cplusplus
extern "C" {
#endif

typedef struct {
	__u8	bda[6];
}__PACK__ BD_ADDR;

#define BDADDR_ANY	(BD_ADDR){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}

static inline int bda_equal(BD_ADDR *a1, BD_ADDR *a2)
{
	return memcmp(a1, a2, 6) == 0;
}

static inline int bda_zero(BD_ADDR *a1)	
{
	return bda_equal(a1, &BDADDR_ANY);
}


/* UART */
#define UART_HCI_COMMAND	0x01
#define UART_HCI_ACL		0x02
#define UART_HCI_SCO		0x03
#define UART_HCI_EVENT		0x04


#define HCI_COMMAND		UART_HCI_COMMAND
#define HCI_ACL			UART_HCI_ACL
#define HCI_SCO			UART_HCI_SCO
#define HCI_EVENT		UART_HCI_EVENT

#define HCI_PKT_OUTGOING	0x80

/* Affix specific packets */
#define HCI_MGR			0x10

/* UART protocols */
#define HCI_UART_H4		0x01
#define HCI_UART_TLP		0x02
#define HCI_UART_BCSP		0x03


#define HCI_ACL_HDR_LEN		4
#define HCI_SCO_HDR_LEN		3
#define HCI_CMD_HDR_LEN		3
#define HCI_EVENT_HDR_LEN	2

#define HCI_CMD_LEN(cmd)	(sizeof(cmd)-HCI_CMD_HDR_LEN)

/* Bluetooth types */

#define HCI_GIAC		0x9E8B33
#define HCI_LIAC		0x9E8B00

#define HCI_HANDLE(h)		(__btoh16(h)&0x0FFF)
#define HCI_PB_FLAG(h)		(__btoh16(h)&0x3000)
#define HCI_BC_FLAG(h)		(__btoh16(h)&0xC000)
		
#define	HCI_PB_MORE		0x1000
#define	HCI_PB_FIRST		0x2000
#define	HCI_BC_ACTIVE		0x4000
#define	HCI_BC_PICONET		0x8000
#define	HCI_BC_PP		0x0000


/* packet types */
#define HCI_PT_ACL		0xFF1F
#define HCI_PT_DM1		0x0008
#define HCI_PT_DH1		0x0010
#define HCI_PT_SCO		0x00E0
#define HCI_PT_HV1		0x0020
#define HCI_PT_HV2		0x0040
#define HCI_PT_HV3		0x0080
#define HCI_PT_DM3		0x0400
#define HCI_PT_DH3		0x0800
#define HCI_PT_DM5		0x4000
#define HCI_PT_DH5		0x8000

/* scan modes */
#define	HCI_SCAN_OFF		0x00
#define HCI_SCAN_INQUIRY	0x01
#define	HCI_SCAN_PAGE		0x02
#define	HCI_SCAN_BOTH		0x03


/* link types */
#define HCI_LT_SCO		0x00
#define HCI_LT_ACL		0x01

/* key types */
#define HCI_KT_COMBINATION	0x00
#define HCI_KT_LOCAL_UNIT	0x01
#define HCI_KT_REMOTE_UNIT	0x02

/* PIN types */
#define HCI_PIN_VARIABLE	0x00
#define HCI_PIN_FIXED		0x01


#define BD_LAP_SIZE		3


#define HCI_COD_SERVICE		0xFFE000
#define HCI_COD_MAJOR		0x001F00
#define HCI_COD_MINOR		0x0000FC
#define HCI_COD_TYPE		0x000003

/*
  Service classes
*/
#define HCI_COD_NETWORKING	0x020000
#define HCI_COD_RENDERING	0x040000
#define HCI_COD_CAPTURING	0x080000
#define HCI_COD_TRANSFER	0x100000
#define HCI_COD_AUDIO		0x200000
#define HCI_COD_TELEPHONY	0x400000
#define HCI_COD_INFORMATION	0x800000

/*
  Major
*/
#define HCI_COD_MISC		0x000000
#define HCI_COD_COMPUTER	0x000100
#define HCI_COD_PHONE		0x000200
#define HCI_COD_LAP		0x000300
#define HCI_COD_MAUDIO		0x000400
#define HCI_COD_PERIPHERAL	0x000500

/* minor for Computer */
#define HCI_COD_DESKTOP		0x000004
#define HCI_COD_SERVER		0x000008
#define HCI_COD_LAPTOP		0x00000C
#define HCI_COD_HANDPC		0x000010
#define HCI_COD_PALMPC		0x000014

/* minor for phone */
#define HCI_COD_CELLULAR	0x000004
#define HCI_COD_CORDLESS	0x000008
#define HCI_COD_SMART		0x00000C
#define HCI_COD_MODEM		0x000010

/* minor for LAP */
#define HCI_COD_LAP_LOAD0	0x000000
#define HCI_COD_LAP_LOAD1	0x000020
#define HCI_COD_LAP_LOAD2	0x000040
#define HCI_COD_LAP_LOAD3	0x000060
#define HCI_COD_LAP_LOAD4	0x000080
#define HCI_COD_LAP_LOAD5	0x0000A0
#define HCI_COD_LAP_LOAD6	0x0000C0
#define HCI_COD_LAP_LOAD7	0x0000E0


/* minor for audio */
#define HCI_COD_HEADSET		0x000004


typedef struct {
	BD_ADDR		bda;
	__u8		PS_Repetition_Mode;
	__u8		PS_Period_Mode;
	__u8		PS_Mode;
	__u32		Class_of_Device:24;
	__u16		Clock_Offset;
}__PACK__ INQUIRY_ITEM;

#ifdef CONFIG_AFFIX_BT_1_2

typedef struct {
	BD_ADDR		bda;
	__u32	Transmit_Bandwidth;
	__u32	Receive_Bandwidth;
	__u16	Max_Latency;
	__u16	Content_Format:10;
	__u8	Retransmission_Effort;
	__u16	Packet_Type;
}__PACK__ ACCEPT_SYNC_CON_REQ;

typedef struct {
	__u32	Transmit_Bandwidth;
	__u32	Receive_Bandwidth;
	__u16	Max_Latency;
	__u16	Voice_Setting:10;
	__u8	Retransmission_Effort;
	__u16	Packet_Type;
}__PACK__ SYNC_CON_REQ;

typedef struct {
	__u16		Connection_Handle:12;
	BD_ADDR		bda;
	__u8		Link_Type;
	__u8		Transmission_Interval;
	__u8		Retransmission_window;
	__u16		Rx_Packet_Length;
	__u16		Tx_Packet_Length;
	__u8		Air_Mode;
}__PACK__ SYNC_CON_RES;

typedef struct {
	__u8	Flags;
	__u8	Flow_direction;
	__u8	Service_Type;
	__u32	Token_Rate;
	__u32	Token_Bucket_Size;
	__u32	Peak_Bandwidth;
	__u32	Access_Latency;
}__PACK__ HCI_FLOW;

typedef struct {
	BD_ADDR		bda;
	__u8		PS_Repetition_Mode;
	__u8		PS_Period_Mode;
	__u32		Class_of_Device:24;
	__u16		Clock_Offset;
	__u8		RSSI;
}__PACK__ INQUIRY_RSSI_ITEM;

#endif

typedef struct {
	__u8	EventCode;
	__u8	Length;
	__u8	Data[0];
}__PACK__ HCI_Event_Packet_Header;

typedef struct {
	__u16	OpCode;
	__u8	Length;
	__u8	Data[0];
}__PACK__ HCI_Command_Packet_Header;

typedef struct {
	__u16		Connection_Handle;	/* 12 + 2 + 2 bits */
	__u16		Length;
	__u8		Data[0];
}__PACK__ HCI_ACL_Packet_Header;

typedef struct {
	__u16		Connection_Handle;	/* 12 bits */
	__u8		Length;
	__u8		Data[0];
}__PACK__ HCI_SCO_Packet_Header;


typedef HCI_Event_Packet_Header		hci_event_hdr_t;
typedef HCI_Command_Packet_Header	hci_cmd_hdr_t;
typedef HCI_ACL_Packet_Header		hci_acl_hdr_t;
typedef HCI_SCO_Packet_Header		hci_sco_hdr_t;


/* ************ Event packets *************** */

struct Inquiry_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u8				Num_Responses;
}__PACK__;

struct Inquiry_Result_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Num_Responses;
	INQUIRY_ITEM			Results[0];
}__PACK__;


struct Connection_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	BD_ADDR				bda;
	__u8				Link_Type;
	__u8				Encryption_Mode;
}__PACK__;

struct Connection_Request_Event {
	HCI_Event_Packet_Header		hdr;
	BD_ADDR				bda;
	__u32				Class_of_Device:24;
	__u8				Link_Type;
}__PACK__;

struct Disconnection_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Reason;
}__PACK__;


struct Command_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Num_HCI_Command_Packets;
	__u16				Command_Opcode;
	__u8				Return_Parameters[0];
}__PACK__;

struct Command_Complete_Status {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Data[0];
}__PACK__;


struct Command_Status_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u8				Num_HCI_Command_Packets;
	__u16				Command_Opcode;
}__PACK__;

struct Flush_Occured_Event {
	HCI_Event_Packet_Header		hdr;
	__u16				Connection_Handle;
}__PACK__;
  

typedef struct {
	__u16		Connection_Handle;
	__u16		HC_Number_Of_Completed_Packets;
}__PACK__ Completed_Packets;

struct Number_Of_Completed_Packets {
	HCI_Event_Packet_Header		hdr;
	__u8				Number_Of_Handles;
	Completed_Packets		Result[0];
}__PACK__;


struct Connection_Packet_Type_Changed_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u16				Packet_Type;
}__PACK__;


struct Authentication_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
}__PACK__;

struct Encryption_Change_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Encryption_Enable;
}__PACK__;

struct Change_Connection_Link_Key_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
}__PACK__;

struct Link_Key_Notification_Event {
	HCI_Event_Packet_Header		hdr;
	BD_ADDR				bda;
	__u8				Link_Key[16];
	__u8				Key_Type;
}__PACK__;

struct Master_Link_Key_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Key_Flag;
}__PACK__;

struct Remote_Name_Request_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	BD_ADDR				bda;
	__u8				Name[248];
}__PACK__;

struct Role_Change_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	BD_ADDR				bda;
	__u8				New_Role;
}__PACK__;

struct Link_Key {
	BD_ADDR		bda;
	__u8		Key[16];
}__PACK__;

struct Return_Link_Keys_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Num_Keys;
	struct Link_Key			Link_Keys[0];
}__PACK__;

struct PIN_Code_Request_Event {
	HCI_Event_Packet_Header		hdr;
	BD_ADDR				bda;
}__PACK__;

struct Link_Key_Request_Event {
	HCI_Event_Packet_Header		hdr;
	BD_ADDR				bda;
}__PACK__;


struct Mode_Change_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Current_Mode;
	__u16				Interval;
}__PACK__;

struct QoS_Violation_Event {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

#ifdef CONFIG_AFFIX_BT_1_2

// COMMANDS BT_1_2

struct  Create_Connection_Cancel {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
}__PACK__;

struct Remote_Name_Request_Cancel {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
}__PACK__;

struct Read_Remote_Extended_Features {
	 HCI_Command_Packet_Header	hdr;
	 __u16				Connection_Handle:12;
	 __u8				Page_Number;
}__PACK__;

struct Read_LMP_Handle {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle:12;
}__PACK__;

struct Setup_Synchronous_Connection {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle:12;
	__u32				Transmit_Bandwidth;
	__u32				Receive_Bandwidth;
	__u16				Max_Latency;
	__u16				Voice_Setting:10;
	__u8				Retransmission_Effort;
	__u16				Packet_Type;
}__PACK__;

struct Accept_Synchronous_Connection_Request {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u32				Transmit_Bandwidth;
	__u32				Receive_Bandwidth;
	__u16				Max_Latency;
	__u16				Content_Format:10;
	__u8				Retransmission_Effort;
	__u16				Packet_Type;
}__PACK__;

struct Reject_Synchronous_Connection_Request {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Reason;
}__PACK__;

struct Write_Default_Link_Policy_Settings {
	HCI_Command_Packet_Header	hdr;
	__u16				Default_Link_Policy_Settings;
}__PACK__;

struct Flow_Specification {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle:12;
	__u8				Flags;
	__u8				Flow_direction;
	__u8				Service_Type;
	__u32				Token_Rate;
	__u32				Token_Bucket_Size;
	__u32				Peak_Bandwidth;
	__u32				Access_Latency;
}__PACK__;

struct Set_AFH_Host_Channel_Classification {
	HCI_Command_Packet_Header	hdr;
	__u8				AFH_Host_Channel_Classification[10];
}__PACK__;

struct Write_Inquiry_Scan_Type {
	HCI_Command_Packet_Header	hdr;
	__u8				Scan_Type;
}__PACK__;

struct Write_Inquiry_Mode {
	HCI_Command_Packet_Header	hdr;
	__u8				Inquiry_Mode;
}__PACK__;

struct Write_Page_Scan_Type {
	HCI_Command_Packet_Header	hdr;
	__u8				Page_Scan_Type;
}__PACK__;

struct Write_AFH_Channel_Assessment_Mode {
	HCI_Command_Packet_Header	hdr;
	__u8				AFH_Channel_Assessment_Mode;
}__PACK__;

struct Read_Local_Extended_Features {
	HCI_Command_Packet_Header	hdr;
	__u8				Page_Number;
}__PACK__;

struct Read_AFH_Channel_Map {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle:12;
}__PACK__;

struct Read_Clock {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle:12;
	__u8				Which_Clock;
}__PACK__;

// EVENTS BT_1_2
// Command Complete Events

struct Create_Connection_Cancel_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	BD_ADDR				bda;
}__PACK__;

struct Name_Request_Cancel_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	BD_ADDR				bda;
}__PACK__;

struct Read_LMP_Handle_Event {
	struct Command_Complete_Event	hdr;
	__u8 				Status;
	__u16				Connection_Handle:12;
	__u8				LMP_Handle;
	__u32				Reserved;
}__PACK__; 

struct Read_Default_Link_Policy_Settings_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Default_Link_Policy_Settings;
}__PACK__;

struct Write_Default_Link_Policy_Settings_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Set_AFH_Host_Channel_Classification_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Read_Inquiry_Scan_Type_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Inquiry_Scan_Type;
}__PACK__;

struct Write_Inquiry_Scan_Type_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Read_Inquiry_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Inquiry_Mode;
}__PACK__;

struct Write_Inquiry_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Read_Page_Scan_Type_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Page_Scan_Type;
}__PACK__;

struct Write_Page_Scan_Type_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Read_AFH_Channel_Assessment_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				AFH_Channel_Assessment_Mode;
}__PACK__;

struct Write_AFH_Channel_Assessment_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Read_Local_Extended_Features_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Page_Number;
	__u8				Maximum_Page_Number;
	__u64				Extended_LMP_Features;
}__PACK__;

struct Read_AFH_Channel_Map_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle:12;
	__u8				AFH_Mode;
	__u8				AFH_Channel_Map[10];
}__PACK__;

struct Read_Clock_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle:12;
	__u32				Clock:28;
	__u16				Accuracy;
}__PACK__;

// General Events

struct Flow_Specification_Complete_Event {
	HCI_Event_Packet_Header	hdr;
	__u8			Status;	
	__u16			Connection_Handle:12;
	__u8			Flags;
	__u8			Flow_direction;
	__u8			Service_Type;
	__u32			Token_Rate;
	__u32			Token_Bucket_Size;
	__u32			Peak_Bandwidth;
	__u32			Access_Latency;
}__PACK__;

struct Inquiry_Result_RSSI_Event {
	HCI_Event_Packet_Header	hdr;
	__u8			Num_Responses;
	INQUIRY_RSSI_ITEM	Results[0];
}__PACK__;

struct Read_Remote_Extended_Features_Complete_Event {
	HCI_Event_Packet_Header	hdr;
	__u8 			Status;
	__u16			Connection_Handle:12;
	__u8			Page_Number;
	__u8			Maximum_Page_Number;
	__u8			Extended_LMP_Features[8];
}__PACK__;

struct Synchronous_Connection_Complete_Event {
	HCI_Event_Packet_Header	hdr;
	__u8			Status;
	__u16			Connection_Handle:12;
	BD_ADDR			bda;
	__u8			Link_Type;
	__u8			Transmission_Interval;
	__u8			Retransmission_window;
	__u16			Rx_Packet_Length;
	__u16			Tx_Packet_Length;
	__u8			Air_Mode;
}__PACK__;

struct Synchronous_Connection_Changed_Event {
	HCI_Event_Packet_Header	hdr;
	__u8			Status;
	__u16			Connection_Handle:12;
	__u8			Transmission_Interval;
	__u8			Retransmission_window;
	__u16			Rx_Packet_Length;
	__u16			Tx_Packet_Length;
}__PACK__;

#endif

struct Read_Clock_Offset {
	HCI_Command_Packet_Header       hdr;
	__u16                           Connection_Handle;
}__PACK__;

struct Read_Clock_Offset_Event {
	HCI_Event_Packet_Header         hdr;
	__u8                            Status;
	__u16                           Connection_Handle;
	__u16                           Clock_Offset;
}__PACK__;


/* Command Packets .. + Results packets */

/* Link Control */
struct Inquiry {
	HCI_Command_Packet_Header	hdr;
	__u32				LAP:24;
	__u8				Inquiry_Length;
	__u8				Max_Num_Responses;
}__PACK__;

struct Inquiry_Cancel_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;

struct Periodic_Inquiry_Mode {
	HCI_Command_Packet_Header	hdr;
	__u16				Max_Period_Length;
	__u16				Min_Period_Length;
	__u32				LAP:24;
	__u8				Inquiry_Length;
	__u8				Max_Num_Responses;
}__PACK__;


struct Create_Connection {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u16				Packet_Type;
	__u8				PS_Repetition_Mode;
	__u8				PS_Mode;
	__u16				Clock_Offset;
	__u8				Allow_Role_Switch;
}__PACK__;


struct Disconnect {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u8				Reason;
}__PACK__;

struct Add_SCO_Connection {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Packet_Type;
}__PACK__;

struct Accept_Connection_Request {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Role;
}__PACK__;

struct Reject_Connection_Request {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Reason;
}__PACK__;


struct Link_Key_Request_Reply {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Link_Key[16];
}__PACK__;

struct Link_Key_Request_Reply_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	BD_ADDR				bda;
}__PACK__;

struct Link_Key_Request_Negative_Reply {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
}__PACK__;

struct PIN_Code {
	BD_ADDR				bda;
	__u8				Length;
	__u8				Code[16];
}__PACK__;

struct PIN_Code_Request_Reply {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				PIN_Code_Length;
	__u8				PIN_Code[16];
}__PACK__;

struct PIN_Code_Request_Reply_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	BD_ADDR				bda;
}__PACK__;

struct PIN_Code_Request_Negative_Reply {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
}__PACK__;

struct Change_Connection_Packet_Type {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Packet_Type;
}__PACK__;

struct Authentication_Requested {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Set_Connection_Encryption {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u8				Encryption_Enable;
}__PACK__;

struct Change_Connection_Link_Key {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Master_Link_Key {
	HCI_Command_Packet_Header	hdr;
	__u8				Key_Flag;
}__PACK__;

struct Remote_Name_Request {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				PS_Repetition_Mode;
	__u8				PS_Mode;
	__u16				Clock_Offset;
}__PACK__;



/* Link Policy */

struct Hold_Mode {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Hold_Mode_Max_Interval;
	__u16				Hold_Mode_Min_Interval;
}__PACK__;

struct Sniff_Mode {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Sniff_Max_Interval;
	__u16				Sniff_Min_Interval;
	__u16				Sniff_Attempt;
	__u16				Sniff_Timeout;
}__PACK__;

struct Park_Mode {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Beacon_Max_Interval;
	__u16				Beacon_Min_Interval;
}__PACK__;

struct Exit_Mode {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct HCI_QoS{
	__u8				Flags;
	__u8				Service_Type;
	__u32				Token_Rate;
	__u32				Peak_Bandwidth;
	__u32				Latency;
	__u32				Delay_Variation;
}__PACK__;

struct QoS_Setup {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	struct HCI_QoS			QoS;
	
}__PACK__;

struct QoS_Setup_Complete_Event {
	HCI_Event_Packet_Header		hdr;
	__u8				Status;
	__u16				Connection_Handle;
	struct HCI_QoS			QoS;
}__PACK__;

struct Role_Discovery {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Role_Discovery_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Current_Role;
}__PACK__;

struct Role_Switch {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Role;
}__PACK__;

struct Read_Link_Policy {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Read_Link_Policy_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Link_Policy_Settings;
}__PACK__;

struct Write_Link_Policy {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u8				Link_Policy_Settings;
}__PACK__;

struct Write_Link_Policy_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
}__PACK__;

/* HC & BBC */

struct Set_Event_Mask {
	HCI_Command_Packet_Header	hdr;
	__u64				Event_Mask;
}__PACK__;

struct Set_Event_Filter {
	HCI_Command_Packet_Header	hdr;
	__u8				Filter_Type;
	__u8				Filter_Condition_Type;
	__u8				Condition[0];
}__PACK__;

struct Read_PIN_Type_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				PIN_Type;
}__PACK__;

struct Write_PIN_Type {
	HCI_Command_Packet_Header	hdr;
	__u8				PIN_Type;
}__PACK__;

struct Read_Stored_Link_Key {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Read_All_Flag;
}__PACK__;

struct Read_Stored_Link_Key_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Max_Num_Keys;
	__u16				Num_Keys_Read;
}__PACK__;

struct Write_Stored_Link_Key {
	HCI_Command_Packet_Header	hdr;
	__u8				Num_Keys_To_Write;
	struct Link_Key			Link_Keys[0];
}__PACK__;

struct Write_Stored_Link_Key_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Num_Keys_Written;
}__PACK__;

struct Delete_Stored_Link_Key {
	HCI_Command_Packet_Header	hdr;
	BD_ADDR				bda;
	__u8				Delete_All_Flag;
}__PACK__;

struct Delete_Stored_Link_Key_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Num_Keys_Deleted;
}__PACK__;

struct Change_Local_Name {
	HCI_Command_Packet_Header	hdr;
	__u8				Name[248];
}__PACK__;

struct Read_Local_Name_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Name[248];
}__PACK__;


struct Read_Page_Timeout_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Page_Timeout;
}__PACK__;

struct Write_Page_Timeout {
	HCI_Command_Packet_Header	hdr;
	__u16				Page_Timeout;
}__PACK__;


struct Read_Scan_Enable_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Scan_Enable;
}__PACK__;


struct Write_Scan_Enable {
	HCI_Command_Packet_Header	hdr;
	__u8				Scan_Enable;
}__PACK__;


struct Read_Page_Scan_Activity_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Page_Scan_Interval;
	__u16				Page_Scan_Window;
}__PACK__;

struct Write_Page_Scan_Activity {
	HCI_Command_Packet_Header	hdr;
	__u16				Page_Scan_Interval;
	__u16				Page_Scan_Window;
}  __PACK__;


struct Read_Inquiry_Scan_Activity_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Inquiry_Scan_Interval;
	__u16				Inquiry_Scan_Window;
}__PACK__;

struct Write_Inquiry_Scan_Activity {
	HCI_Command_Packet_Header	hdr;
	__u16				Inquiry_Scan_Interval;
	__u16				Inquiry_Scan_Window;
}  __PACK__;


struct Read_Authentication_Enable_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Authentication_Enable;
}__PACK__;

struct Write_Authentication_Enable {
	HCI_Command_Packet_Header	hdr;
	__u8				Authentication_Enable;
}__PACK__;


struct Read_Encryption_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Encryption_Mode;
}__PACK__;

struct Write_Encryption_Mode {
	HCI_Command_Packet_Header	hdr;
	__u8				Encryption_Mode;
}__PACK__;

struct Read_Class_of_Device_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u32				Class_of_Device:24;
}__PACK__;

struct Write_Class_of_Device {
	HCI_Command_Packet_Header	hdr;
	__u32				Class_of_Device:24;
}__PACK__;

struct Read_Voice_Setting_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Voice_Setting;
}__PACK__;

struct Read_Transmit_Power_Level {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
        __u8                            Type;
}__PACK__;

struct Read_Transmit_Power_Level_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__s8				Transmit_Power_Level;
}__PACK__;

struct Write_Voice_Setting {
	HCI_Command_Packet_Header	hdr;
	__u16				Voice_Setting;
}__PACK__;

struct Read_SCO_Flow_Control_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Flow_Control;
}__PACK__;

struct Write_SCO_Flow_Control {
	HCI_Command_Packet_Header	hdr;
	__u8				Flow_Control;
}__PACK__;

struct Read_Hold_Mode_Activity_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Hold_Mode_Activity;
}__PACK__;

struct Write_Hold_Mode_Activity {
	HCI_Command_Packet_Header	hdr;
	__u8				Hold_Mode_Activity;
}__PACK__;

struct Get_Link_Quality {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Get_Link_Quality_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u8				Link_Quality;
}__PACK__;

struct Read_RSSI {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Read_RSSI_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__s8				RSSI;
}__PACK__;

struct Read_Link_Supervision_Timeout {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
}__PACK__;

struct Read_Link_Supervision_Timeout_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
	__u16				Link_Supervision_Timeout;
}__PACK__;

struct Write_Link_Supervision_Timeout {
	HCI_Command_Packet_Header	hdr;
	__u16				Connection_Handle;
	__u16				Link_Supervision_Timeout;
}__PACK__;

struct Write_Link_Supervision_Timeout_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u16				Connection_Handle;
}__PACK__;

struct Read_Number_Of_Supported_IAC_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Num_Supported_IAC;
}__PACK__;

struct Read_Current_IAC_LAP_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Num_Current_IAC;	/* 0x00 - 0x40 */
	struct {__u32 v:24;} __PACK__	IAC_LAP[0];
}__PACK__;

struct Write_Current_IAC_LAP {
	HCI_Command_Packet_Header	hdr;
	__u8				Num_Current_IAC;
	struct {__u32 v:24;} __PACK__	IAC_LAP[0];
}__PACK__;


struct Read_Page_Scan_Period_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Page_Scan_Period_Mode;
}__PACK__;

struct Write_Page_Scan_Period_Mode {
	HCI_Command_Packet_Header	hdr;
	__u8				Page_Scan_Period_Mode;
}__PACK__;

struct Read_Page_Scan_Mode_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Page_Scan_Mode;
}__PACK__;

struct Write_Page_Scan_Mode {
	HCI_Command_Packet_Header	hdr;
	__u8				Page_Scan_Mode;
}__PACK__;

/* Shrirang 16 Oct 2003 */
struct Read_Num_Broadcast_Retransmissions {
	HCI_Command_Packet_Header	hdr;
}__PACK__;

struct Read_Num_Broadcast_Retransmissions_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	__u8				Num_Broadcast_Retran;
}__PACK__;

struct Write_Num_Broadcast_Retransmissions {
	HCI_Command_Packet_Header	hdr;
	__u8				Num_Broad_Retran;
}__PACK__;

struct Write_Num_Broadcast_Retransmissions_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
}__PACK__;
/* */

/* Information */

struct Read_Local_Version_Information_Event {
	struct Command_Complete_Event	hdr;
	__u8	Status;
	__u8	HCI_Version;
	__u16	HCI_Revision;
	__u8	LMP_Version;
	__u16	Manufacture_Name;
	__u16	LMP_Subversion;
}__PACK__;

#define HCI_LMP_ERICSSON		0
#define HCI_LMP_NOKIA			1
#define HCI_LMP_INTEL			2
#define HCI_LMP_IBM			3
#define HCI_LMP_TOSHIBA			4
#define HCI_LMP_3COM			5
#define HCI_LMP_MICROSOFT		6
#define HCI_LMP_LUCENT			7
#define HCI_LMP_MOTOROLA		8
#define HCI_LMP_INFINEION		9
#define HCI_LMP_CSR			10
#define HCI_LMP_SILICONWAVE		11
#define HCI_LMP_DIGIANSWER		12

struct Read_Local_Supported_Features_Event {
	struct Command_Complete_Event	hdr;
	__u8		Status;
	__u64		LMP_Features;
}__PACK__;


/* LMP Features */
/* packets */
#define HCI_LF_PACKETS			0x00000003
#define HCI_LF_3SLOTS			0x00000001
#define HCI_LF_5SLOTS			0x00000002
/* security */
#define HCI_LF_ENCRYPTION		0x00000004
/* timing */
#define HCI_LF_TIMING			0x00000018
#define HCI_LF_SLOT_OFFSET		0x00000008
#define HCI_LF_TIMING_ACCURACY		0x00000010
/* policy */
#define HCI_LF_POLICY			0x000001E0
#define HCI_LF_SWITCH			0x00000020
#define HCI_LF_HOLD_MODE		0x00000040
#define HCI_LF_SNIFF_MODE		0x00000080
#define HCI_LF_PARK_MODE		0x00000100
/* radio */
#define HCI_LF_RADIO			0x00020600
#define HCI_LF_RSSI			0x00000200
#define HCI_LF_CQD_DATARATE		0x00000400
/* audio */
#define HCI_LF_AUDIO			0x0009F800
#define HCI_LF_SCO			0x00000800
#define HCI_LF_HV2			0x00001000
#define HCI_LF_HV3			0x00002000
#define HCI_LF_ULAWLOG			0x00004000
#define HCI_LF_ALAWLOG			0x00008000
#define HCI_LF_CVSD			0x00010000
/* radio */
#define HCI_LF_PAGING_SCHEME		0x00020000
/* power */
#define HCI_LF_POWER_CONTROL		0x00040000
/* audio */
#define HCI_LF_TRANSPARENT_SCO		0x00080000
/* flow */
#define HCI_LF_FLOW_CONTROL0		0x00100000
#define HCI_LF_FLOW_CONTROL1		0x00200000
#define HCI_LF_FLOW_CONTROL2		0x00400000



struct Read_Buffer_Size_Event {
	struct Command_Complete_Event	hdr;
	__u8	Status;
	__u16	HC_ACL_Data_Packet_Length;
	__u8	HC_SCO_Data_Packet_Length;
	__u16	Total_Num_ACL_Data_Packets;
	__u16	Total_Num_SCO_Data_Packets;
}__PACK__;

struct Read_Country_Code_Event {
	struct Command_Complete_Event	hdr;
	__u8	Status;
	__u8	Country_Code;
}__PACK__;


struct Read_BD_ADDR_Event {
	struct Command_Complete_Event	hdr;
	__u8				Status;
	BD_ADDR				bda;
}__PACK__;


// Ericsson specific

struct Ericsson_Write_PCM_Settings {
	HCI_Command_Packet_Header	hdr;
	__u8				PCM_Settings;
}__PACK__;


struct Ericsson_Set_SCO_Data_Path {
	HCI_Command_Packet_Header	hdr;
	__u8				SCO_Data_Path;
}__PACK__;


/* OpCode Group Field */
#define HCI_GROUP_MASK	0xFC00
#define HCI_CMD_MASK	0x03FF

#define HCI_G_LC	0x0400		/* (1<<10)	*/
#define HCI_G_LP	0x0800
#define HCI_G_HB	0x0C00
#define HCI_G_I		0x1000
#define HCI_G_ST	0x1400
#define HCI_G_T		0x1800
#define HCI_G_V		0xFC00

// Link control

#define HCI_C_INQUIRY				0x0401
#define HCI_C_INQUIRY_CANCEL			0x0402
#define HCI_C_PERIODIC_INQUIRY_MODE		0x0403
#define HCI_C_EXIT_PERIODIC_INQUIRY_MODE	0x0404
#define HCI_C_CREATE_CONNECTION			0x0405
#define HCI_C_DISCONNECT			0x0406
#define HCI_C_ADD_SCO_CONNECTION		0x0407
#define HCI_C_ACCEPT_CONNECTION_REQUEST		0x0409
#define HCI_C_REJECT_CONNECTION_REQUEST		0x040A
#define HCI_C_LINK_KEY_REQUEST_REPLY		0x040B
#define HCI_C_LINK_KEY_REQUEST_NEGATIVE_REPLY	0x040C
#define HCI_C_PIN_CODE_REQUEST_REPLY		0x040D
#define HCI_C_PIN_CODE_REQUEST_NEGATIVE_REPLY	0x040E
#define HCI_C_CHANGE_CONNECTION_PACKET_TYPE	0x040F
#define HCI_C_AUTHENTICATION_REQUESTED		0x0411
#define HCI_C_SET_CONNECTION_ENCRYPTION		0x0413
#define HCI_C_CHANGE_CONNECTION_LINK_KEY	0x0415
#define HCI_C_MASTER_LINK_KEY			0x0417
#define HCI_C_REMOTE_NAME_REQUEST		0x0419
#define HCI_C_READ_REMOTE_SUPPORTED_FEATURES	0x041B
#define HCI_C_READ_REMOTE_VERSION_INFORMATION	0x041D
#define HCI_C_READ_CLOCK_OFFSET			0x041F

// Link policy

#define HCI_C_HOLD_MODE				0x0801
#define HCI_C_SNIFF_MODE			0x0803
#define HCI_C_EXIT_SNIFF_MODE			0x0804
#define HCI_C_PARK_MODE				0x0805
#define HCI_C_EXIT_PARK_MODE			0x0806
#define HCI_C_QOS_SETUP				0x0807
#define HCI_C_ROLE_DISCOVERY			0x0809
#define HCI_C_SWITCH_ROLE			0x080B
#define HCI_C_READ_LINK_POLICY_SETTINGS		0x080C
#define HCI_C_WRITE_LINK_POLICY_SETTINGS	0x080D


// Host controller & baseband

#define HCI_C_SET_EVENT_MASK				0x0C01
#define HCI_C_RESET					0x0C03
#define HCI_C_SET_EVENT_FILTER				0x0C05
#define HCI_C_FLUSH					0x0C08
#define HCI_C_READ_PIN_TYPE				0x0C09
#define HCI_C_WRITE_PIN_TYPE				0x0C0A
#define HCI_C_CREATE_NEW_UNIT_KEY			0x0C0B
#define HCI_C_READ_STORED_LINK_KEY			0x0C0D
#define HCI_C_WRITE_STORED_LINK_KEY			0x0C11
#define HCI_C_DELETE_STORED_LINK_KEY			0x0C12
#define HCI_C_CHANGE_LOCAL_NAME				0x0C13
#define HCI_C_READ_LOCAL_NAME				0x0C14
#define HCI_C_READ_CONNECTION_ACCEPT_TIMEOUT		0x0C15
#define HCI_C_WRITE_CONNECTION_ACCEPT_TIMEOUT		0x0C16
#define HCI_C_READ_PAGE_TIMEOUT				0x0C17
#define HCI_C_WRITE_PAGE_TIMEOUT			0x0C18
#define HCI_C_READ_SCAN_ENABLE				0x0C19
#define HCI_C_WRITE_SCAN_ENABLE				0x0C1A
#define HCI_C_READ_PAGE_SCAN_ACTIVITY			0x0C1B
#define HCI_C_WRITE_PAGE_SCAN_ACTIVITY			0x0C1C
#define HCI_C_READ_INQUIRY_SCAN_ACTIVITY		0x0C1D
#define HCI_C_WRITE_INQUIRY_SCAN_ACTIVITY		0x0C1E
#define HCI_C_READ_AUTHENTICATION_ENABLE		0x0C1F
#define HCI_C_WRITE_AUTHENTICATION_ENABLE		0x0C20
#define HCI_C_READ_ENCRYPTION_MODE			0x0C21
#define HCI_C_WRITE_ENCRYPTION_MODE			0x0C22
#define HCI_C_READ_CLASS_OF_DEVICE			0x0C23
#define HCI_C_WRITE_CLASS_OF_DEVICE			0x0C24
#define HCI_C_READ_VOICE_SETTING			0x0C25
#define HCI_C_WRITE_VOICE_SETTING			0x0C26
#define HCI_C_READ_RETRANSMIT_TIMEOUT			0x0C27
#define HCI_C_WRITE_RETRANSMIT_TIMEOUT			0x0C28
#define HCI_C_READ_NUM_BROADCAST_RETRANSMISSIONS	0x0C29
#define HCI_C_WRITE_NUM_BROADCAST_RETRANSMISSIONS	0x0C2A
#define HCI_C_READ_HOLD_MODE_ACTIVITY			0x0C2B
#define HCI_C_WRITE_HOLD_MODE_ACTIVITY			0x0C2C
#define HCI_C_READ_TRANSMIT_POWER_LEVEL			0x0C2D
#define HCI_C_READ_SCO_FLOW_CONTROL_ENABLE		0x0C2E
#define HCI_C_WRITE_SCO_FLOW_CONTROL_ENABLE		0x0C2F
#define HCI_C_SET_HOST_CONTROLLER_TO_HOST_FLOW_CONTROL	0x0C31
#define HCI_C_HOST_BUFFER_SIZE				0x0C33
#define HCI_C_HOST_NUMBER_OF_COMPLETED_PACKETS		0x0C35
#define HCI_C_READ_LINK_SUPERVISION_TIMEOUT		0x0C36
#define HCI_C_WRITE_LINK_SUPERVISION_TIMEOUT		0x0C37
#define HCI_C_READ_NUMBER_OF_SUPPORTED_IAC		0x0C38
#define HCI_C_READ_CURRENT_IAC_LAP			0x0C39
#define HCI_C_WRITE_CURRENT_IAC_LAP			0x0C3A
#define HCI_C_READ_PAGE_SCAN_PERIOD_MODE		0x0C3B
#define HCI_C_WRITE_PAGE_SCAN_PERIOD_MODE		0x0C3C
#define HCI_C_READ_PAGE_SCAN_MODE			0x0C3D
#define HCI_C_WRITE_PAGE_SCAN_MODE			0x0C3E


// Informational

#define HCI_C_READ_LOCAL_VERSION_INFORMATION	0x1001
#define HCI_C_READ_LOCAL_SUPPORTED_FEATURES	0x1003
#define HCI_C_READ_BUFFER_SIZE			0x1005
#define HCI_C_READ_COUNTRY_CODE			0x1007
#define HCI_C_READ_BD_ADDR			0x1009

// Status

#define HCI_C_READ_FAILED_CONTACT_COUNTER	0x1401
#define HCI_C_RESET_FAILED_CONTACT_COUNTER	0x1402
#define HCI_C_GET_LINK_QUALITY			0x1403
#define HCI_C_READ_RSSI				0x1405

// Testing commands

#define HCI_C_READ_LOOPBACK_MODE		0x1801
#define HCI_C_WRITE_LOOPBACK_MODE		0x1802
#define HCI_C_ENABLE_DEVICE_UNDER_TEST_MODE	0x1803

// Ericsson specific  FC
#define HCI_C_ERICSSON_WRITE_PCM_SETTINGS	0xFC07
#define HCI_C_ERICSSON_SET_SCO_DATA_PATH	0xFC1D


// Events

#define HCI_E_INQUIRY_COMPLETE				0x01
#define HCI_E_INQUIRY_RESULT				0x02
#define HCI_E_CONNECTION_COMPLETE			0x03
#define HCI_E_CONNECTION_REQUEST			0x04
#define HCI_E_DISCONNECTION_COMPLETE			0x05
#define HCI_E_AUTHENTICATION_COMPLETE			0x06
#define HCI_E_REMOTE_NAME_REQUEST_COMPLETE		0x07
#define HCI_E_ENCRYPTION_CHANGE				0x08
#define HCI_E_CHANGE_CONNECTION_LINK_KEY_COMPLETE	0x09
#define HCI_E_MASTER_LINK_KEY_COMPLETE			0x0a
#define HCI_E_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE	0x0b
#define HCI_E_READ_REMOTE_VERSION_INFORMATION_COMPLETE	0x0c
#define HCI_E_QOS_SETUP_COMPLETE			0x0d
#define HCI_E_COMMAND_COMPLETE				0x0e
#define HCI_E_COMMAND_STATUS				0x0f
#define HCI_E_HARDWARE_ERROR				0x10
#define HCI_E_FLUSH_OCCURRED				0x11
#define HCI_E_ROLE_CHANGE				0x12
#define HCI_E_NUMBER_COMPLETED_PACKETS			0x13
#define HCI_E_MODE_HANGE				0x14
#define HCI_E_RETURN_LINK_KEYS				0x15
#define HCI_E_PIN_CODE_REQUEST				0x16
#define HCI_E_LINK_KEY_REQUEST				0x17
#define HCI_E_LINK_KEY_NOTIFICATION			0x18
#define HCI_E_LOOPBACK_COMMAND				0x19
#define HCI_E_DATA_BUFFER_OVERFLOW			0x1a
#define HCI_E_MAX_SLOTS_CHANGE				0x1b
#define HCI_E_READ_CLOCK_OFFSET_COMPLETE		0x1c
#define HCI_E_CONNECTION_PACKET_TYPE_CHANGED		0x1d
#define HCI_E_QOS_VIOLATION				0x1e
#define HCI_E_PAGE_SCAN_MODE_CHANGE_EVENT		0x1f
#define HCI_E_PAGE_SCAN_REPETITION_MODE_CHANGE_EVENT	0x20
#define HCI_E_TCI					0xFE
#define HCI_E_VENDOR					0xFF

#define HCI_E_LAST					0x20

//#define HCI_E_

// Error codes

#define HCI_ERR_SUCCESS					0x00
#define HCI_ERR_UNKNOWN_COMMAND				0x01
#define HCI_ERR_NO_CONNECTION				0x02
#define HCI_ERR_HARDWARE_FAILURE			0x03
#define HCI_ERR_PAGE_TIMEOUT				0x04
#define HCI_ERR_AUTHENTICATION_FAILURE			0x05
#define HCI_ERR_KEY_MISSING				0x06
#define HCI_ERR_MEMORY_FULL				0x07
#define HCI_ERR_CONNECTION_TIMEOUT			0x08
#define HCI_ERR_MAX_NUMBER_OF_CONNECTIONS		0x09
#define HCI_ERR_MAX_NUMBER_OF_SCO_CONNECTIONS		0x0a
#define HCI_ERR_MAX_NUMBER_OF_ACL_CONNECTIONS		0x0b
#define HCI_ERR_COMMAND_DISALLOWED			0x0c
#define HCI_ERR_HOST_REJECTED_LIMIT			0x0d
#define HCI_ERR_HOST_REJECTED_SECURITY			0x0e
#define HCI_ERR_HOST_REJECTED_PERSONAL			0x0f
#define HCI_ERR_HOST_TIMEOUT				0x10
#define HCI_ERR_UNSUPPORTED_FEATURE			0x11
#define HCI_ERR_INVALID_PARAMETERS			0x12
#define HCI_ERR_OTHER_END_TERMINATED_USER		0x13
#define HCI_ERR_OTHER_END_TERMINATED_RESOURCES		0x14
#define HCI_ERR_OTHER_END_TERMINATED_POWEROFF		0x15
#define HCI_ERR_CONNECTION_TERMINATED			0x16
#define HCI_ERR_REPETED_ATTEMPTS			0x17
#define HCI_ERR_PAIRING_NOT_ALLOWED			0x18
#define HCI_ERR_UNKNOWN_LMP_PDU				0x19
#define HCI_ERR_UNSUPPORTED_LMP_FEATURE			0x1a
#define HCI_ERR_SCO_OFFSET_REJECTED			0x1b
#define HCI_ERR_SCO_INTERVAL_REJECTED			0x1c
#define HCI_ERR_SCO_AIR_MODE_REJECTED			0x1d
#define HCI_ERR_INVALID_LMP_PARAMETERS			0x1e
#define HCI_ERR_UNSPECIFIED_ERROR			0x1f
#define HCI_ERR_UNSUPPORTED_LMP_PARAMETER_VALUE		0x20

//#define HCI_ERR_


/*
 * Mask used to identify the event(s) that the
 * user chooses to handle.
 */
#define NO_EVENT_MASK					0x00000000
#define INQUIRY_COMPLETE_MASK				0x00000001
#define INQUIRY_RESULT_MASK				0x00000002
#define CONNECTION_COMPLETE_MASK			0x00000004
#define CONNECTION_REQUEST_MASK				0x00000008
#define DISCONNECTION_COMPLETE_MASK			0x00000010
#define AUTHENTICATION_COMPLETE_MASK			0x00000020
#define REMOTE_NAME_REQUEST_COMPLETE_MASK		0x00000040
#define ENCRYPTION_CHANGE_MASK				0x00000080
#define CHANGE_CONNECTION_LINK_KEY_COMPLETE_MASK	0x00000100
#define MASTER_LINK_KEY_COMPLETE_MASK			0x00000200
#define READ_REMOTE_SUPPORTED_FEATURES_COMPLETE_MASK	0x00000400
#define READ_REMOTE_VERSION_INFORMATION_COMPLETE_MASK	0x00000800
#define QOS_SETUP_COMPLETE_MASK				0x00001000
#define COMMAND_COMPLETE_MASK				0x00002000
#define COMMAND_STATUS_MASK				0x00004000
#define HARDWARE_ERROR_MASK				0x00008000
#define FLUSH_OCCURRED_MASK				0x00010000
#define ROLE_CHANGE_MASK				0x00020000
#define NUMBER_OF_COMPLETE_PACKETS_MASK			0x00040000
#define MODE_CHANGE_MASK				0x00080000
#define RETURN_LINK_KEYS_MASK				0x00100000
#define PIN_CODE_REQUEST_MASK				0x00200000
#define LINK_KEY_REQUEST_MASK				0x00400000
#define LINK_KEY_NOTIFICATION_MASK			0x00800000
#define LOOPBACK_COMMAND_MASK				0x01000000
#define DATA_BUFFER_OVERFLOW_MASK			0x02000000
#define MAX_SLOTS_CHANGE_MASK				0x04000000
#define READ_CLOCK_OFFSET_COMPLETE_MASK			0x08000000
#define CONNECTION_PACKET_TYPE_CHANGED_MASK		0x10000000
#define QOS_VIOLATION_MASK				0x20000000
#define PAGE_SCAN_MODE_CHANGE_MASK			0x40000000
#define PAGE_SCAN_REPETITION_MODE_CHANGE_MASK		0x80000000

#ifndef CONFIG_AFFIX_BT_1_2

#define ALL_EVENTS_MASK					0xffffffff

#else

#define ALL_EVENTS_MASK					0x00001FFFFFFFFFFFLL

#endif

#ifdef CONFIG_AFFIX_BT_1_2

//
// HCI Commands
//

// Link Control Commands
#define HCI_C_CREATE_CONNECTION_CANCEL			0X0408
#define HCI_C_NAME_REQUEST_CANCEL			0x041A

#define HCI_C_READ_REMOTE_EXTENDED_FEATURES		0x041C
#define HCI_C_READ_LMP_HANDLE				0x0427
#define HCI_C_SETUP_SYNCHRONOUS_CONNECTION		0x0428
#define HCI_C_ACCEPT_SYNCHRONOUS_CONNECTION_REQUEST	0x0429
#define HCI_C_REJECT_SYNCHRONOUS_CONNECTION_REQUEST	0x0430

// Link Policy Commands
#define HCI_C_READ_DEFAULT_LINK_POLICY_SETTINGS		0x080E
#define HCI_C_WRITE_DEFAULT_LINK_POLICY_SETTINGS	0x080F
#define HCI_C_FLOW_SPECIFICATION			0x0810			

// Controller & Baseband Commands
#define HCI_C_SET_AFH_HOST_CHANNEL_CLASSIFICATION	0x0C3F
#define HCI_C_READ_INQUIRY_SCAN_TYPE			0x0C42
#define HCI_C_WRITE_INQUIRY_SCAN_TYPE			0x0C43
#define HCI_C_READ_INQUIRY_MODE				0x0C44
#define HCI_C_WRITE_INQUIRY_MODE			0x0C45
#define HCI_C_READ_PAGE_SCAN_TYPE 			0x0C46
#define HCI_C_WRITE_PAGE_SCAN_TYPE			0x0C47
#define HCI_C_READ_AFH_CHANNEL_ASSESSMENT_MODE		0x0C48
#define HCI_C_WRITE_AFH_CHANNEL_ASSESSMENT_MODE		0x0C49

// Informational Parameters
#define HCI_C_READ_LOCAL_EXTENDED_FEATURES		0x1004

// Status Parameters 
#define HCI_C_READ_CLOCK				0x1407
#define HCI_C_READ_AFH_CHANNEL_MAP			0x1406

//
// HCI EVENTS
//

// Quality of Service
#define HCI_E_FLOW_SPECIFICATION_COMPLETE		0x21
// Device Discovery
#define HCI_E_INQUIRY_RESULT_RSSI			0x22
// Remote Information
#define HCI_E_READ_REMOTE_EXTENDED_FEATURES_COMPLETE	0x23
// Synchronous Connections
#define HCI_E_SYNCHRONOUS_CONNECTION_COMPLETE		0x2C
#define HCI_E_SYNCHRONOUS_CONNECTION_CHANGED		0x2D

//
// HCI EVENTS MASK
//
						
#define FLOW_SPECIFICATION_COMPLETE_MASK		0x0000000100000000LL
#define INQUIRY_RESULT_WITH_RSSI_MASK			0X0000000200000000LL
#define READ_REMOTE_EXTENDED_FEATURES_COMPLETE_MASK	0x0000000400000000LL
#define SYNCHRONOUS_CONNECTION_COMPLETE_MASK		0X0000080000000000LL
#define SYNCHRONOUS_CONNECTION_CHANGED_MASK		0x0000100000000000LL

#endif


#ifdef  __cplusplus
}
#endif

#endif
