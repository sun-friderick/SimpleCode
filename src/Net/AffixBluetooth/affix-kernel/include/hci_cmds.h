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
   $Id: hci_cmds.h,v 1.93 2004/05/26 13:16:13 chineape Exp $

   Host Controller Interface

   Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/		

/*
	Few commands are not implemented yet:
	
Link
	HCI_ReadRemoteSupportedFeatures
	HCI_ReadRemoteVersionInformation
	
Baseband
	HCI_ReadConnectionAcceptTimeout
	HCI_WriteConnectionAcceptTimeout
	HCI_ReadAutomaticFlushTimeout
	HCI_WriteAutomaticFlushTimeout
	HCI_ReadNumBroadcastRetransmissions
	HCI_WriteNumBroadcastRetransmissions
	HCI_SetHostControllerToHostFlowControl
	HCI_HostBufferSize
	HCI_HostNumberOfCompletedPackets

Status
	HCI_ReadFailedContactCounter
	HCI_ResetFailedContactCounter
	
*/

#ifndef _HCI_CMDS_H
#define _HCI_CMDS_H

int hci_open_dev(struct hci_open *dev)
{
	int	fd, err;
	
	fd = btsys_socket(PF_AFFIX, SOCK_RAW, BTPROTO_HCI);
	if (fd < 0) {
		return fd;
	}
	err = btsys_ioctl(fd, BTIOC_OPEN_HCI, dev);
	if (err) {
		btsys_close(fd);
		return err;
	}
	return fd;
}

int hci_exec_cmd(int fd, __u16 opcode, void *cmd, int len, __u64 mask, int flags, void *event, int elen)
{
	HCI_Command_Packet_Header	*hdr = cmd;
	int	err;

	hdr->OpCode = __htob16(opcode);
	hdr->Length = len - HCI_CMD_HDR_LEN;

	err = btsys_setsockopt(fd, SOL_AFFIX, BTSO_EVENT_MASK, &mask, sizeof(mask));
	if (err)
		return err;
	err = btsys_send(fd, cmd, len, flags | HCI_REQUEST_MODE | HCI_NO_UART_ENCAP);
	if (err < 0)
		return err;
	if (event) {
		/* get command complete event */
		err = btsys_recv(fd, event, elen, HCI_NO_UART_ENCAP);
		if (err < 0)
			return err;
		err = 0;
	}
	return err;	/* has status */
}

int hci_exec_cmd0(int fd, __u16 opcode, __u64 mask, int flags, void *event, int elen)
/* Executes an HCI command with no parameters */
{
	HCI_Command_Packet_Header	cmd;

	return hci_exec_cmd(fd, opcode, &cmd, sizeof(cmd), mask, flags, event, elen);
}

int hci_exec_cmd1(int fd, __u16 opcode, void *cmd, int len, __u64 mask, int flags)
/* Executes and HCI command without waiting for an event */
{
	return hci_exec_cmd(fd, opcode, cmd, len, mask, flags, NULL, 0);
}

int hci_recv_event_any(int fd, int *devnum, void *event, int size)
{
	int			err;
	struct sockaddr_affix	sa;
	int			len = sizeof(sa);

	err = btsys_recvfrom(fd, event, size, HCI_NO_UART_ENCAP, (void*)&sa, &len);
	if (err < 0)
		return err;
	if (devnum)
		*devnum = sa.devnum;
	return err;
}


/*
 * Link Control
 */

#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_PAN)  || defined(CONFIG_AFFIX_PAN_MODULE)
static inline int ItemLookup(INQUIRY_ITEM *Items, __u8 Num, BD_ADDR *bda)
{
	int	i;
	for (i = 0; i < Num; i++)
		if (bda_equal(&Items[i].bda, bda))
			return i;
	return -1;
}


int HCI_Inquiry(int fd, __u8 Inquiry_Length, __u8 Max_Num_Responses, INQUIRY_ITEM *Items, __u8 *Num_Responses)
{
	int				err, pos;
	unsigned char			buf[HCI_MAX_EVENT_SIZE];
	struct Inquiry			cmd;
	struct Inquiry_Result_Event	*ir = (void*)buf;
	struct Inquiry_Complete_Event	*ic = (void*)buf;
	
	cmd.LAP = __htob24(HCI_GIAC);				// use only GIAC for now
	cmd.Inquiry_Length = Inquiry_Length;
	cmd.Max_Num_Responses = Max_Num_Responses;
	err = hci_exec_cmd1(fd, HCI_C_INQUIRY, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|INQUIRY_RESULT_MASK|INQUIRY_COMPLETE_MASK, 0);
	if (err)
		return err;

	*Num_Responses = 0;
	do {
		err = hci_recv_event(fd, buf, sizeof(buf), 20);
		if (err < 0) {
			return err;
		}
		if (ir->hdr.EventCode == HCI_E_INQUIRY_RESULT) {
			int	i;
			for (i = 0; i < ir->Num_Responses; i++) {
				pos = ItemLookup(Items, *Num_Responses, &ir->Results[i].bda);
				if (pos == -1) {
					// new device
					pos = (*Num_Responses)++;
					Items[pos].bda = ir->Results[i].bda;
				}

				Items[pos].PS_Repetition_Mode = ir->Results[i].PS_Repetition_Mode;
				Items[pos].PS_Period_Mode = ir->Results[i].PS_Period_Mode;
				Items[pos].PS_Mode = ir->Results[i].PS_Mode;
				Items[pos].Class_of_Device = __btoh24(ir->Results[i].Class_of_Device);
				Items[pos].Clock_Offset = __btoh16(ir->Results[i].Clock_Offset);
	 		}
		}
	} while (ic->hdr.EventCode != HCI_E_INQUIRY_COMPLETE);
	hci_event_mask(fd, 0);

	return ic->Status;
}
#endif

#ifdef CONFIG_AFFIX_BT_1_2

int HCI_Inquiry_RSSI(int fd, __u8 Inquiry_Length, __u8 Max_Num_Responses, INQUIRY_RSSI_ITEM *Items, __u8 *Num_Responses)
{
	int				err, pos;
	unsigned char			buf[HCI_MAX_EVENT_SIZE];
	struct Inquiry			cmd;
	struct Inquiry_Complete_Event	*ic = (void*)buf;
	struct Inquiry_Result_RSSI_Event *irssi = (void*)buf;
	
	cmd.LAP = __htob24(HCI_GIAC);				// use only GIAC for now
	cmd.Inquiry_Length = Inquiry_Length;
	cmd.Max_Num_Responses = Max_Num_Responses;
	
	err = hci_exec_cmd1(fd, HCI_C_INQUIRY, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|INQUIRY_COMPLETE_MASK|INQUIRY_RESULT_WITH_RSSI_MASK, 0);
	if (err)
		return err;

	*Num_Responses = 0;
	do {
		err = hci_recv_event(fd, buf, sizeof(buf), 20);
		if (err < 0) {
			return err;
		}
		if (irssi->hdr.EventCode == HCI_E_INQUIRY_RESULT_RSSI) {
			int	i;
			for (i = 0; i < irssi->Num_Responses; i++) {
				pos = ItemLookup((INQUIRY_ITEM*)Items, *Num_Responses, &irssi->Results[i].bda);
				if (pos == -1) {
					// new device
					pos = (*Num_Responses)++;
					Items[pos].bda = irssi->Results[i].bda;
				}
				Items[pos].PS_Repetition_Mode = irssi->Results[i].PS_Repetition_Mode;
				Items[pos].PS_Period_Mode = irssi->Results[i].PS_Period_Mode;
				Items[pos].Class_of_Device = __btoh24(irssi->Results[i].Class_of_Device);
				Items[pos].Clock_Offset = __btoh16(irssi->Results[i].Clock_Offset);
				Items[pos].RSSI = irssi->Results[i].RSSI;
	 		}
		}
	} while (ic->hdr.EventCode != HCI_E_INQUIRY_COMPLETE);
	hci_event_mask(fd, 0);

	return ic->Status;
}
#endif


int __HCI_CreateConnection(int fd, INQUIRY_ITEM *dev, __u16 Packet_Type, __u8 Allow_Role_Switch)
{
	int				err;
	struct Create_Connection	cmd;

	cmd.bda = dev->bda;
	cmd.Packet_Type = __htob16(Packet_Type);
	cmd.PS_Repetition_Mode = dev->PS_Repetition_Mode;
	cmd.PS_Mode = dev->PS_Mode;
	cmd.Clock_Offset = __htob16(dev->Clock_Offset);
	cmd.Allow_Role_Switch = Allow_Role_Switch;

	err = hci_exec_cmd1(fd, HCI_C_CREATE_CONNECTION, &cmd, sizeof(cmd), COMMAND_STATUS_MASK, 0);
	return err;
}

#if !defined(__KERNEL__)
int __HCI_Disconnect(int fd, __u16 Connection_Handle, __u8 Reason)
{
	int			err;
	struct Disconnect	cmd;

	/* setup command parameters */
	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Reason = Reason;
#if 1
	/* may be bug in Nokia BT card */
	err = hci_exec_cmd1(fd, HCI_C_DISCONNECT, &cmd, sizeof(cmd), 0, HCI_SKIP_STATUS);
#else
	err = hci_exec_cmd1(fd, HCI_C_DISOCNNECT, &cmd, sizeof(cmd), COMMAND_STATUS_MASK, 0);
#endif
	return err;
}
#endif

int HCI_Disconnect(int fd, __u16 Connection_Handle, __u8 Reason)
{
	int					err;
	struct Disconnect			cmd;
	struct Disconnection_Complete_Event	dce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Reason = Reason;
	
	err = hci_exec_cmd1(fd, HCI_C_DISCONNECT, &cmd, sizeof(cmd),
			COMMAND_STATUS_MASK|DISCONNECTION_COMPLETE_MASK, 0);
	if (err)
		return err;

	for (;;) {
		err = hci_recv_event(fd, &dce, sizeof(dce), 5);
		if (err < 0) {
			return err;
		}
		if (__btoh16(dce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			return dce.Status;
		}
	}
	return 0;	
}

int __HCI_AddSCOConnection(int fd, __u16 Connection_Handle, __u16 Packet_Type)
{
	int				err;
	struct Add_SCO_Connection	cmd;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Packet_Type = __htob16(Packet_Type);

	err = hci_exec_cmd1(fd, HCI_C_ADD_SCO_CONNECTION, &cmd, sizeof(cmd), COMMAND_STATUS_MASK, 0);
	return err;
}

int __HCI_AcceptConnectionRequest(int fd, BD_ADDR *bda, __u8 Role)
{
	int					err;
	struct Accept_Connection_Request	cmd;

	cmd.bda = *bda;
	cmd.Role = Role;
	err = hci_exec_cmd1(fd, HCI_C_ACCEPT_CONNECTION_REQUEST, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK, 0);
	return err;
}

int __HCI_RejectConnectionRequest(int fd, BD_ADDR *bda, __u8 Reason)
{
	int					err;
	struct Reject_Connection_Request	cmd;

	cmd.bda = *bda;
	cmd.Reason = Reason;
	err = hci_exec_cmd1(fd, HCI_C_REJECT_CONNECTION_REQUEST, &cmd, sizeof(cmd),
			COMMAND_STATUS_MASK, 0);
	return err;
}

int __HCI_AuthenticationRequested(int fd, __u16 Connection_Handle)
{
	int					err;
	struct Authentication_Requested		cmd;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd1(fd, HCI_C_AUTHENTICATION_REQUESTED, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK, 0);
	return err;
}


/*
 * XXX: Multiple Command Complete
*/
int HCI_LinkKeyRequestReply(int fd, BD_ADDR *bda, __u8 *Link_Key)
{
	int					err;
	struct Link_Key_Request_Reply		cmd;
	struct Link_Key_Request_Reply_Event	cce;

	cmd.bda = *bda;
	memcpy(cmd.Link_Key, Link_Key, 16);
	err = hci_exec_cmd(fd, HCI_C_LINK_KEY_REQUEST_REPLY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	return cce.Status;
}

/*
  XXX: Multiple Command Complete
*/
int HCI_LinkKeyRequestNegativeReply(int fd, BD_ADDR *bda)
{
	int					err;
	struct Link_Key_Request_Negative_Reply	cmd;
	struct Link_Key_Request_Reply_Event	cce;

	cmd.bda = *bda;
	err = hci_exec_cmd(fd, HCI_C_LINK_KEY_REQUEST_NEGATIVE_REPLY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	
	return err ? err : cce.Status;
}

/*
  XXX: Multiple Command Complete
*/
int HCI_PINCodeRequestReply(int fd, BD_ADDR *bda, __u8 PIN_Code_Length, __u8 *PIN_Code)
{
	int					err;
	struct PIN_Code_Request_Reply		cmd;
	struct PIN_Code_Request_Reply_Event	cce;

	cmd.bda = *bda;
	cmd.PIN_Code_Length = PIN_Code_Length;
	memcpy(cmd.PIN_Code, PIN_Code, 16);
	err = hci_exec_cmd(fd, HCI_C_PIN_CODE_REQUEST_REPLY, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	return cce.Status;
}

/*
  XXX:Multiple Command Complete
*/
int HCI_PINCodeRequestNegativeReply(int fd, BD_ADDR *bda)
{
	int					err;
	struct PIN_Code_Request_Negative_Reply	cmd;
	struct PIN_Code_Request_Reply_Event	cce;

	cmd.bda = *bda;
	err = hci_exec_cmd(fd, HCI_C_PIN_CODE_REQUEST_NEGATIVE_REPLY, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	return cce.Status;
}

#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MODULE)
int HCI_ChangeConnectionPacketType(int fd, __u16 Connection_Handle, __u16 Packet_Type)
{
	int						err;
	struct Change_Connection_Packet_Type		cmd;
	struct Connection_Packet_Type_Changed_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Packet_Type = __htob16(Packet_Type);
	err = hci_exec_cmd1(fd, HCI_C_CHANGE_CONNECTION_PACKET_TYPE, &cmd, sizeof(cmd),
			COMMAND_STATUS_MASK|CONNECTION_PACKET_TYPE_CHANGED_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 10);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}
	return 0;	
}
#endif

#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_UPDATE_CLOCKOFFSET)
int __HCI_ReadClockOffset(int fd, __u16 Connection_Handle)
{
	int                                     err;
	struct Read_Clock_Offset                cmd;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd1(fd, HCI_C_READ_CLOCK_OFFSET, &cmd, sizeof(cmd),
		COMMAND_STATUS_MASK | READ_CLOCK_OFFSET_COMPLETE_MASK, 0);
	return err;
}

#endif


/*
 * Link Policy
 */

#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MODULE)
int HCI_SwitchRole(int fd, BD_ADDR *bda, __u8 Role)
{
	int				err;
	struct Role_Switch		cmd;
	struct Role_Change_Event	cce;

	cmd.bda = *bda;
	cmd.Role = Role;
	err = hci_exec_cmd1(fd, HCI_C_SWITCH_ROLE, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|ROLE_CHANGE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 10);
		if (err < 0) {
			return err;
		}

		if (bda_equal(&(cce.bda), bda)) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}

	return 0;	
}
#endif


/*
 * Baseband Commands
 */

int __HCI_Reset(int fd)
{
	int	err;

	err = hci_exec_cmd0(fd, HCI_C_RESET, 0, HCI_SKIP_STATUS, NULL, 0);
	return err;
}


#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MODULE)
int HCI_ReadScanEnable(int fd, __u8 *Scan_Enable)
{
	int				err;
	struct Read_Scan_Enable_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_SCAN_ENABLE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	*Scan_Enable = cce.Scan_Enable;
	return cce.Status;
}
#endif

int HCI_WriteScanEnable(int fd, __u8 Scan_Enable)
{
	int				err;
	struct Write_Scan_Enable	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Scan_Enable = Scan_Enable;
	err = hci_exec_cmd(fd, HCI_C_WRITE_SCAN_ENABLE, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	if (!ccs.Status)
		hci_set_scan(fd, Scan_Enable);
	return ccs.Status;
}

int HCI_WritePageScanActivity(int fd,  __u16 Page_Scan_Interval, __u16 Page_Scan_Window)
{
	int				err;
	struct Write_Page_Scan_Activity	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Page_Scan_Interval = __htob16(Page_Scan_Interval);
	cmd.Page_Scan_Window = __htob16(Page_Scan_Window);
	err = hci_exec_cmd(fd, HCI_C_WRITE_PAGE_SCAN_ACTIVITY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if( err )
		return err;
	return ccs.Status;	
}


int HCI_WriteInquiryScanActivity(int fd, __u16 Inquiry_Scan_Interval, __u16 Inquiry_Scan_Window)
{
	int				err;
	struct Write_Inquiry_Scan_Activity	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Inquiry_Scan_Interval = __htob16(Inquiry_Scan_Interval);
	cmd.Inquiry_Scan_Window = __htob16(Inquiry_Scan_Window);
	err = hci_exec_cmd(fd, HCI_C_WRITE_INQUIRY_SCAN_ACTIVITY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if( err )
		return err;
	return ccs.Status;	
}

int HCI_WriteClassOfDevice(int fd, __u32 Class_of_Device)
{
	int				err;
	struct Write_Class_of_Device	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Class_of_Device = __htob24(Class_of_Device);
	err = hci_exec_cmd(fd, HCI_C_WRITE_CLASS_OF_DEVICE, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ChangeLocalName(int fd, char *Name)
{
	int				err;
	struct Change_Local_Name	cmd;
	struct Command_Complete_Status	ccs;

	strncpy((char*)cmd.Name, Name, 248);
	err = hci_exec_cmd(fd, HCI_C_CHANGE_LOCAL_NAME, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_WriteAuthenticationEnable(int fd, __u8 Authentication_Enable)
{
	int					err;
	struct Write_Authentication_Enable	cmd;
	struct Command_Complete_Status		ccs;

	cmd.Authentication_Enable = Authentication_Enable;
	err = hci_exec_cmd(fd, HCI_C_WRITE_AUTHENTICATION_ENABLE, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_WriteEncryptionMode(int fd, __u8 Encryption_Mode)
{
	int				err;
	struct Write_Encryption_Mode	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Encryption_Mode = Encryption_Mode;
	err = hci_exec_cmd(fd, HCI_C_WRITE_ENCRYPTION_MODE, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_DeleteStoredLinkKey(int fd, BD_ADDR *bda, __u8 Delete_All_Flag, __u16 *Num_Keys_Deleted)
{
	int					err;
	struct Delete_Stored_Link_Key		cmd;
	struct Delete_Stored_Link_Key_Event	cce;

	cmd.bda = *bda;
	cmd.Delete_All_Flag = Delete_All_Flag;
	err = hci_exec_cmd(fd, HCI_C_DELETE_STORED_LINK_KEY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Num_Keys_Deleted = __btoh16(cce.Num_Keys_Deleted);
	return cce.Status;
}

int HCI_ReadTransmitPowerLevel(int fd, __u16 Connection_Handle, __u8 Type, __s8 *Transmit_Power_Level)
{
	int					err;
	struct Read_Transmit_Power_Level 	cmd;
	struct Read_Transmit_Power_Level_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Type = Type;
	err = hci_exec_cmd(fd, HCI_C_READ_TRANSMIT_POWER_LEVEL, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Transmit_Power_Level = cce.Transmit_Power_Level;
	return 0;
}

/*
  XXX: multiple Command Completes
*/
#if !defined(__KERNEL__) || defined(CONFIG_AFFIX_PAN) || defined(CONFIG_AFFIX_PAN_MODULE)
int HCI_WriteLinkSupervisionTimeout(int fd, __u16 Connection_Handle, __u16 Link_Supervision_Timeout)
{
	int						err;
	struct Write_Link_Supervision_Timeout		cmd;
	struct Write_Link_Supervision_Timeout_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Link_Supervision_Timeout = __htob16(Link_Supervision_Timeout);
	err = hci_exec_cmd(fd, HCI_C_WRITE_LINK_SUPERVISION_TIMEOUT, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	return cce.Status;
}
#endif


/*
 * Informational
 */

int HCI_ReadBufferSize(int fd, __u16 *HC_ACL_Data_Packet_Length, __u8 *HC_SCO_Data_Packet_Length,
		       __u16 *Total_Num_ACL_Data_Packets, __u16 *Total_Num_SCO_Data_Packets)
{
	int				err;
	struct Read_Buffer_Size_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_BUFFER_SIZE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*HC_ACL_Data_Packet_Length = __btoh16(cce.HC_ACL_Data_Packet_Length);
	*HC_SCO_Data_Packet_Length = cce.HC_SCO_Data_Packet_Length;
	*Total_Num_ACL_Data_Packets = __btoh16(cce.Total_Num_ACL_Data_Packets);
	*Total_Num_SCO_Data_Packets = __btoh16(cce.Total_Num_SCO_Data_Packets);
	return 0;
}

int HCI_ReadBDAddr(int fd, BD_ADDR *bda)
{
	int				err;
	struct Read_BD_ADDR_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_BD_ADDR, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*bda = cce.bda;
	return 0;
}

int HCI_ReadLocalVersionInformation(int fd, __u8 *HCI_Version, __u16 *HCI_Revision,
				    __u8 *LMP_Version, __u16 *Manufacture_Name, __u16 *LMP_Subversion)
{
	int						err;
	struct Read_Local_Version_Information_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_LOCAL_VERSION_INFORMATION, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*HCI_Version = cce.HCI_Version;
	*HCI_Revision = __btoh16(cce.HCI_Revision);
	*LMP_Version = cce.LMP_Version;
	*Manufacture_Name = __btoh16(cce.Manufacture_Name);
	*LMP_Subversion = __btoh16(cce.LMP_Subversion);
	return 0;
}


int HCI_ReadLocalSupportedFeatures(int fd, __u64 *LMP_Features)
{
	int						err;
	struct Read_Local_Supported_Features_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_LOCAL_SUPPORTED_FEATURES, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*LMP_Features = __btoh64(cce.LMP_Features);
	return 0;
}


/*
 *  Status
 */
 
int HCI_GetLinkQuality(int fd, __u16 Connection_Handle, __u8 *Link_Quality)
{
	int				err;
	struct Get_Link_Quality 	cmd;
	struct Get_Link_Quality_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_GET_LINK_QUALITY, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Link_Quality = cce.Link_Quality;
	return 0;
}

int HCI_ReadRSSI(int fd, __u16 Connection_Handle, __s8 *RSSI)
{
	int			err;
	struct Read_RSSI	cmd;
	struct Read_RSSI_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_READ_RSSI, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*RSSI = cce.RSSI;
	return 0;
}

#ifdef __KERNEL__

/*
 * only kernel space commands
 */

#else

/*
 * only user space commands
 */

/* Link Control Commands */


int HCI_InquiryCancel(int fd)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_INQUIRY_CANCEL, COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;	// return if an error
}

int HCI_PeriodicInquiryMode(int fd, __u16 Max_Period_Length, __u16 Min_Period_Length, __u16 Inquiry_Length, __u8 Max_Num_Responses)
{
	int				err;
	struct Periodic_Inquiry_Mode	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Max_Period_Length = __htob16(Max_Period_Length);
	cmd.Min_Period_Length = __htob16(Min_Period_Length);
	cmd.LAP = __htob24(HCI_GIAC);
	cmd.Inquiry_Length = Inquiry_Length;
	cmd.Max_Num_Responses = Max_Num_Responses;

	err = hci_exec_cmd(fd, HCI_C_PERIODIC_INQUIRY_MODE, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;	// return if an error
}

int HCI_ExitPeriodicInquiryMode(int fd)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_EXIT_PERIODIC_INQUIRY_MODE, 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;	// return if an error
}


int HCI_CreateConnection(int fd, INQUIRY_ITEM *dev, __u16 Packet_Type, __u8 Allow_Role_Switch, __u16 *Connection_Handle, __u8 *Link_Type, __u8 *Encryption_Mode)
{
	int					err;
	struct Create_Connection		cmd;
	struct Connection_Complete_Event	cce;

	cmd.bda = dev->bda;
	cmd.Packet_Type = __htob16(Packet_Type);
	cmd.PS_Repetition_Mode = dev->PS_Repetition_Mode;
	cmd.PS_Mode = dev->PS_Mode;
	cmd.Clock_Offset = __htob16(dev->Clock_Offset);
	cmd.Allow_Role_Switch = Allow_Role_Switch;

	err = hci_exec_cmd1(fd, HCI_C_CREATE_CONNECTION, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|CONNECTION_COMPLETE_MASK, 0);
	if (err)
		return err;

	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 10);
		if (err < 0) {
			return err;
		}

		if (bda_equal(&cce.bda, &dev->bda)) {
			hci_event_mask(fd, 0);	/* remove listener */
			if (cce.Status)
				return cce.Status;
			*Connection_Handle = __btoh16(cce.Connection_Handle);
			*Link_Type = cce.Link_Type;
			*Encryption_Mode = cce.Encryption_Mode;
			break;
		}
	}
	return 0;	
}

int HCI_AcceptConnectionRequest(int fd, BD_ADDR *bda, __u8 Role, __u16 *Connection_Handle, __u8 *Link_Type, __u8 *Encryption_Mode)
{
	int					err;
	struct Accept_Connection_Request	cmd;
	struct Connection_Complete_Event	cce;

	cmd.bda = *bda;
	cmd.Role = Role;
	err = hci_exec_cmd1(fd, HCI_C_ACCEPT_CONNECTION_REQUEST, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|CONNECTION_COMPLETE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 10);
		if (err < 0) {
			return err;
		}

		if (bda_equal(&cce.bda, bda)) {
			hci_event_mask(fd, 0);	/* remove listener */
			*Connection_Handle = __btoh16(cce.Connection_Handle);
			*Link_Type = cce.Link_Type;
			*Encryption_Mode = cce.Encryption_Mode;
			return cce.Status;	
		}
	}
	return 0;	
}

int HCI_RejectConnectionRequest(int fd, BD_ADDR *bda, __u8 Reason)
{
	int					err;
	struct Reject_Connection_Request	cmd;
	struct Connection_Complete_Event	cce;

	cmd.bda = *bda;
	cmd.Reason = Reason;
	err = hci_exec_cmd1(fd, HCI_C_REJECT_CONNECTION_REQUEST, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|CONNECTION_COMPLETE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 10);
		if (err < 0) {
			return err;
		}

		if (bda_equal(&cce.bda, bda)) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}
	return 0;	
}

int HCI_AuthenticationRequested(int fd, __u16 Connection_Handle)
{
	int					err;
	struct Authentication_Requested		cmd;
	struct Authentication_Complete_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd1(fd, HCI_C_AUTHENTICATION_REQUESTED, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|AUTHENTICATION_COMPLETE_MASK, 0);
	if (err)
		return err;
	do {
		err = hci_recv_event(fd, &cce, sizeof(cce), 60);
		if (err < 0) {
			return err;
		}
	} while(__btoh16(cce.Connection_Handle) != Connection_Handle);
	hci_event_mask(fd, 0);	/* remove listener */
	return cce.Status;
}

int HCI_SetConnectionEncryption(int fd, __u16 Connection_Handle, __u8 Encryption_Enable)
{
	int					err;
	struct Set_Connection_Encryption 	cmd;
	struct Encryption_Change_Event		cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Encryption_Enable = Encryption_Enable;
	err = hci_exec_cmd1(fd, HCI_C_SET_CONNECTION_ENCRYPTION, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|ENCRYPTION_CHANGE_MASK, 0);
	if (err)
		return err;
	do {
		err = hci_recv_event(fd, &cce, sizeof(cce), 20);
		if (err < 0) {
			return err;
		}
	} while (__btoh16(cce.Connection_Handle) != Connection_Handle);
	hci_event_mask(fd, 0);	/* remove listener */
	return cce.Status;
}

/*
  May be do not receive Link_Key_Notification at all...
*/

int HCI_ChangeConnectionLinkKey(int fd, __u16 Connection_Handle,
				BD_ADDR *bda, __u8 *Link_Key, __u8 *Key_Type)
{
	int					err;
	unsigned char				buf[HCI_MAX_EVENT_SIZE];
	struct Change_Connection_Link_Key 	cmd;
	struct Change_Connection_Link_Key_Complete_Event		*cce = (void*)buf;
	struct Link_Key_Notification_Event	*lkne = (void*)buf;
	__u64					event_mask;

	cmd.Connection_Handle = __htob16(Connection_Handle);

	event_mask = COMMAND_STATUS_MASK  | CHANGE_CONNECTION_LINK_KEY_COMPLETE_MASK;
	if (bda)
		event_mask |= LINK_KEY_NOTIFICATION_MASK;
	err = hci_exec_cmd1(fd, HCI_C_CHANGE_CONNECTION_LINK_KEY, &cmd, sizeof(cmd), event_mask, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, buf, sizeof(buf), 0);
		if (err < 0) {
			return err;
		}

		if (bda) {
			if (lkne->hdr.EventCode == HCI_E_LINK_KEY_NOTIFICATION) {
			/* geather results */
				memcpy(Link_Key, lkne->Link_Key, 16);
				*Key_Type = lkne->Key_Type;
			} else if (lkne->hdr.EventCode == HCI_E_CHANGE_CONNECTION_LINK_KEY_COMPLETE) {
				if (__btoh16(cce->Connection_Handle) == Connection_Handle) {
					hci_event_mask(fd, 0);	/* remove listener */
					return cce->Status;
				}
			}
		} else {
			if (__btoh16(cce->Connection_Handle) == Connection_Handle) {
				hci_event_mask(fd, 0);	/* remove listener */
				return cce->Status;
			}
		}
	}
	return 0;	
}

/*
  Hmm... One Connection_Handle for all slaves ???!!! XXX:
*/
int HCI_MasterLinkKey(int fd, __u8 Key_Flag, __u16 *Connection_Handle)
{
	int					err;
	struct Master_Link_Key 			cmd;
	struct Master_Link_Key_Complete_Event	cce;

	cmd.Key_Flag = Key_Flag;
	err = hci_exec_cmd1(fd, HCI_C_MASTER_LINK_KEY, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|MASTER_LINK_KEY_COMPLETE_MASK, 0);
	if (err)
		return err;
	do {
		err = hci_recv_event(fd, &cce, sizeof(cce), 0);
		if (err < 0) {
			return err;
		}

	} while (cce.hdr.EventCode != HCI_E_MASTER_LINK_KEY_COMPLETE);
	hci_event_mask(fd, 0);	/* remove listener */
	*Connection_Handle = cce.Connection_Handle;
	return cce.Status;
}


int HCI_RemoteNameRequest(int fd, INQUIRY_ITEM *dev, char *Name)
{
	int					err;
	struct Remote_Name_Request	 	cmd;
	struct Remote_Name_Request_Complete_Event		cce;

	cmd.bda = dev->bda;
	cmd.PS_Repetition_Mode = dev->PS_Repetition_Mode;
	cmd.PS_Mode = dev->PS_Mode;
	cmd.Clock_Offset = __htob16(dev->Clock_Offset);

	strcpy(Name, "unknown");;
	err = hci_exec_cmd1(fd, HCI_C_REMOTE_NAME_REQUEST, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|REMOTE_NAME_REQUEST_COMPLETE_MASK, 0);
	if (err)
		return err;

	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 20);
		if (err < 0) {
			return err;
		}
		if (bda_equal(&cce.bda, &dev->bda)) {
			hci_event_mask(fd, 0);	/* remove listener */
			if (cce.Status)
				return cce.Status;
			strncpy(Name, cce.Name, 248);
			break;
		}
	}
	return 0;	
}

int HCI_ReadClockOffset(int fd, __u16 Connection_Handle, __u16 *ClockOffset)
{
	int                                     err;
	struct Read_Clock_Offset                cmd;
	struct Read_Clock_Offset_Event		cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_READ_CLOCK_OFFSET, &cmd, sizeof(cmd),
		COMMAND_STATUS_MASK | READ_CLOCK_OFFSET_COMPLETE_MASK, 0, &cce, sizeof(cce));
	*ClockOffset = __btoh16(cce.Clock_Offset);
	return err;
}

/*
 * Link Policy commands
 */

int HCI_HoldMode(int fd, __u16 Connection_Handle, __u16 Hold_Mode_Max_Interval, __u16 Hold_Mode_Min_Interval)
{
	int				err;
	struct Hold_Mode		cmd;
	struct Mode_Change_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Hold_Mode_Max_Interval = __htob16(Hold_Mode_Max_Interval);
	cmd.Hold_Mode_Min_Interval = __htob16(Hold_Mode_Min_Interval);

	err = hci_exec_cmd1(fd, HCI_C_HOLD_MODE, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|MODE_CHANGE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 30);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}
}

int HCI_ExitMode(int fd, __u16 opcode, __u16 Connection_Handle, __u8 *Current_Mode, __u16 *Interval)
{
	int				err;
	struct Exit_Mode		cmd;
	struct Mode_Change_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd1(fd, opcode, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|MODE_CHANGE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 30);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			*Current_Mode = cce.Current_Mode;
			*Interval = __btoh16(cce.Interval);
			return cce.Status;
		}
	}
}

int HCI_SniffMode(int fd, __u16 Connection_Handle, __u16 Sniff_Max_Interval, __u16 Sniff_Min_Interval, __u16 Sniff_Attempt, __u16 Sniff_Timeout)
{
	int				err;
	struct Sniff_Mode		cmd;
	struct Mode_Change_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Sniff_Max_Interval = __htob16(Sniff_Max_Interval);
	cmd.Sniff_Min_Interval = __htob16(Sniff_Min_Interval);
	cmd.Sniff_Attempt = __htob16(Sniff_Attempt);
	cmd.Sniff_Timeout = __htob16(Sniff_Timeout);

	err = hci_exec_cmd1(fd, HCI_C_SNIFF_MODE, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|MODE_CHANGE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 30);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}
}

int HCI_ExitSniffMode(int fd, __u16 Connection_Handle, __u8 *Current_Mode, __u16 *Interval)
{
	return HCI_ExitMode(fd, HCI_C_EXIT_SNIFF_MODE, Connection_Handle, Current_Mode, Interval);
}

int HCI_ParkMode(int fd, __u16 Connection_Handle, __u16 Beacon_Max_Interval, __u16 Beacon_Min_Interval)
{
	int				err;
	struct Park_Mode		cmd;
	struct Mode_Change_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Beacon_Max_Interval = __htob16(Beacon_Max_Interval);
	cmd.Beacon_Min_Interval = __htob16(Beacon_Min_Interval);

	err = hci_exec_cmd1(fd, HCI_C_PARK_MODE, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|MODE_CHANGE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd, &cce, sizeof(cce), 30);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			return cce.Status;
		}
	}
}

int HCI_ExitParkMode(int fd, __u16 Connection_Handle, __u8 *Current_Mode, __u16 *Interval)
{
	return HCI_ExitMode(fd, HCI_C_EXIT_PARK_MODE, Connection_Handle, Current_Mode, Interval);
}

int HCI_QoS_Setup(int fd, __u16 Connection_Handle, struct HCI_QoS *Requested_QoS, struct HCI_QoS *Completed_QoS)
{
	int				err;
	struct QoS_Setup		cmd;
	struct QoS_Setup_Complete_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.QoS.Flags = Requested_QoS->Flags;
	cmd.QoS.Service_Type = Requested_QoS->Service_Type;
	cmd.QoS.Token_Rate = __htob32(Requested_QoS->Token_Rate);
	cmd.QoS.Peak_Bandwidth = __htob32(Requested_QoS->Peak_Bandwidth);
	cmd.QoS.Latency = __htob32(Requested_QoS->Latency);
	cmd.QoS.Delay_Variation = __htob32(Requested_QoS->Delay_Variation);

	err = hci_exec_cmd1(fd, HCI_C_QOS_SETUP, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK|QOS_SETUP_COMPLETE_MASK, 0);
	if (err)
		return err;
	for (;;) {
		err = hci_recv_event(fd,&cce, sizeof(cce), 30);
		if (err < 0) {
			return err;
		}
		if (__btoh16(cce.Connection_Handle) == Connection_Handle) {
			hci_event_mask(fd, 0);	/* remove listener */
			Completed_QoS->Flags = cce.QoS.Flags;
			Completed_QoS->Service_Type = cce.QoS.Service_Type;
			Completed_QoS->Token_Rate = __btoh32(cce.QoS.Token_Rate);
			Completed_QoS->Peak_Bandwidth = __btoh32(cce.QoS.Peak_Bandwidth);
			Completed_QoS->Latency = __btoh32(cce.QoS.Latency);
			Completed_QoS->Delay_Variation = __btoh32(cce.QoS.Delay_Variation);
			return cce.Status;
		}
	}
}


int HCI_RoleDiscovery(int fd, __u16 Connection_Handle, __u8 *Current_Role)
{
	int					err;
	struct Role_Discovery			cmd;
	struct Role_Discovery_Event		cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_ROLE_DISCOVERY, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	*Current_Role = cce.Current_Role;
	return cce.Status;
}


int HCI_ReadLinkPolicySettings(int fd, __u16 Connection_Handle, __u8 *Link_Policy_Settings)
{
	int					err;
	struct Read_Link_Policy			cmd;
	struct Read_Link_Policy_Event		cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_READ_LINK_POLICY_SETTINGS, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	*Link_Policy_Settings = cce.Link_Policy_Settings;
	return cce.Status;
}

int HCI_WriteLinkPolicySettings(int fd, __u16 Connection_Handle, __u8 Link_Policy_Settings)
{
	int					err;
	struct Write_Link_Policy		cmd;
	struct Write_Link_Policy_Event		cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Link_Policy_Settings = Link_Policy_Settings;
	err = hci_exec_cmd(fd, HCI_C_WRITE_LINK_POLICY_SETTINGS, &cmd, sizeof(cmd), 
				COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	return cce.Status;
}

/* Shrirang 16 Oct 2003 */
int HCI_Read_Num_Broadcast_Retransmissions(int fd, __u8 *Num)
{
	int					  	err;
	struct Read_Num_Broadcast_Retransmissions_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_NUM_BROADCAST_RETRANSMISSIONS, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Num = cce.Num_Broadcast_Retran;
	
	if (err)
		return err;
	return cce.Status;

}

int HCI_Write_Num_Broadcast_Retransmissions(int fd, __u8 Num)
{
	int					  		err;
	struct Write_Num_Broadcast_Retransmissions 		cmd;
	struct Write_Num_Broadcast_Retransmissions_Event 	cce;

	cmd.Num_Broad_Retran = Num;

	err = hci_exec_cmd(fd, HCI_C_WRITE_NUM_BROADCAST_RETRANSMISSIONS, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	if (err)
		return err;

	return cce.Status;

}
/*  */

/* HC & BB commands */

int HCI_Reset(int fd)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_RESET, COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_SetEventMask(int fd, __u64 Event_Mask)
{
	int				err;
	struct Set_Event_Mask		cmd;
	struct Command_Complete_Status	ccs;

	cmd.Event_Mask = __htob64(Event_Mask);
	err = hci_exec_cmd(fd, HCI_C_SET_EVENT_MASK, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_SetEventFilter(int fd, __u8 Filter_Type, __u8 Filter_Condition_Type, __u8 *Condition, __u8 Condition_Length)
{
	int				err;
	unsigned char			buf[sizeof(struct Set_Event_Filter) + 7];/*XXX max*/
	struct Set_Event_Filter		*cmd = (void*)buf;
	struct Command_Complete_Status	ccs;

	cmd->Filter_Type = Filter_Type;
	cmd->Filter_Condition_Type = Filter_Condition_Type;
	memcpy(cmd->Condition, Condition, Condition_Length);

	err = hci_exec_cmd(fd, HCI_C_SET_EVENT_FILTER, &cmd, sizeof(cmd) + Condition_Length,
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ReadPINType(int fd, __u8 *PIN_Type)
{
	int				err;
	struct Read_PIN_Type_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PIN_TYPE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	*PIN_Type = cce.PIN_Type;
	return cce.Status;
}

int HCI_WritePINType(int fd, __u8 PIN_Type)
{
	int				err;
	struct Write_PIN_Type		cmd;
	struct Command_Complete_Status	ccs;

	cmd.PIN_Type = PIN_Type;

	err = hci_exec_cmd(fd, HCI_C_WRITE_PIN_TYPE, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_CreateNewUnitKey(int fd)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_CREATE_NEW_UNIT_KEY, COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ReadStoredLinkKey(int fd, BD_ADDR *bda, __u8 Read_All_Flag, __u16 *Max_Num_Keys,
			  __u16 *Num_Keys, struct Link_Key *Link_Keys)
{
	int					err;
	unsigned char				buf[HCI_MAX_EVENT_SIZE];
	struct Read_Stored_Link_Key		cmd;
	struct Read_Stored_Link_Key_Event	*cce = (void*)buf;
	struct Return_Link_Keys_Event		*rlke  = (void*)buf;

	cmd.bda = *bda;
	cmd.Read_All_Flag = Read_All_Flag;

	err = hci_exec_cmd(fd, HCI_C_READ_STORED_LINK_KEY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK|RETURN_LINK_KEYS_MASK, HCI_SKIP_STATUS, cce, sizeof(cce));
	if (err)
		return err;
	do {
		err = hci_recv_event(fd, cce, sizeof(*cce), 20);
		if (err < 0) {
			return err;
		}
		if (rlke->hdr.EventCode == HCI_E_RETURN_LINK_KEYS) {
			*Num_Keys = rlke->Num_Keys;
			memcpy(Link_Keys, rlke->Link_Keys, *Num_Keys*sizeof(struct Link_Key));
			Link_Keys += *Num_Keys;
		}

	} while (cce->hdr.hdr.EventCode != HCI_E_COMMAND_COMPLETE);
	hci_event_mask(fd, 0);
	*Max_Num_Keys = __btoh16(cce->Max_Num_Keys);
	*Num_Keys = __btoh16(cce->Num_Keys_Read);
	/*
	 * XXX:	may be compare Num_Keys_Read and total read keys
	*/
	return cce->Status;	
}

int HCI_WriteStoredLinkKey(int fd, __u8 Num_Keys_To_Write, struct Link_Key *Link_Keys,
			   __u8 *Num_Keys_Written)
{
	int					err;
	unsigned char				buf[HCI_MAX_CMD_SIZE];
	struct Write_Stored_Link_Key		*cmd = (void*)buf;
	struct Write_Stored_Link_Key_Event	ccs;
	int					klen;

	klen = Num_Keys_To_Write*sizeof(struct Link_Key);

	cmd->Num_Keys_To_Write = Num_Keys_To_Write;
	memcpy(cmd->Link_Keys, Link_Keys, klen);

	err = hci_exec_cmd(fd, HCI_C_WRITE_STORED_LINK_KEY, &cmd, sizeof(cmd) + klen,
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	*Num_Keys_Written = ccs.Num_Keys_Written;
	return ccs.Status;
}


int HCI_ReadLocalName(int fd, char *Name)
{
	int				err;
	struct Read_Local_Name_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_LOCAL_NAME, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	strncpy(Name, (char*)cce.Name, 248);
	return 0;
}

int HCI_ReadPageTimeout(int fd, __u16 *Page_Timeout)
{
	int				err;
	struct Read_Page_Timeout_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PAGE_TIMEOUT, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	*Page_Timeout = cce.Page_Timeout;
	return cce.Status;
}

int HCI_WritePageTimeout(int fd, __u16 Page_Timeout)
{
	int				err;
	struct Write_Page_Timeout	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Page_Timeout = Page_Timeout;
	err = hci_exec_cmd(fd, HCI_C_WRITE_PAGE_TIMEOUT, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ReadPageScanActivity(int fd,  __u16 *Page_Scan_Interval, __u16 *Page_Scan_Window)
{
	int					err;
	struct Read_Page_Scan_Activity_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PAGE_SCAN_ACTIVITY, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Page_Scan_Interval = __btoh16(cce.Page_Scan_Interval);
	*Page_Scan_Window = __btoh16(cce.Page_Scan_Window);
	return 0;
}

int HCI_ReadInquiryScanActivity(int fd, __u16 *Inquiry_Scan_Interval, __u16 *Inquiry_Scan_Window)
{
	int						err;
	struct Read_Inquiry_Scan_Activity_Event		cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_INQUIRY_SCAN_ACTIVITY, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Inquiry_Scan_Interval = __btoh16(cce.Inquiry_Scan_Interval);
	*Inquiry_Scan_Window = __btoh16(cce.Inquiry_Scan_Window);
	return 0;
}


int HCI_ReadAuthenticationEnable(int fd, __u8 *Authentication_Enable)
{
	int				err;
	struct Read_Authentication_Enable_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_AUTHENTICATION_ENABLE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Authentication_Enable = cce.Authentication_Enable;
	return 0;
}

int HCI_ReadEncryptionMode(int fd, __u8 *Encryption_Mode)
{
	int				err;
	struct Read_Encryption_Mode_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_ENCRYPTION_MODE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Encryption_Mode = cce.Encryption_Mode;
	return 0;
}

int HCI_ReadClassOfDevice(int fd, __u32 *Class_of_Device)
{
	int				err;
	struct Read_Class_of_Device_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_CLASS_OF_DEVICE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Class_of_Device = __btoh24(cce.Class_of_Device);	/* XXX: check it */
	return 0;
}

int HCI_ReadVoiceSetting(int fd, __u16 *Voice_Setting)
{
	int				err;
	struct Read_Voice_Setting_Event	ccs;

	err = hci_exec_cmd0(fd, HCI_C_READ_VOICE_SETTING, 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	*Voice_Setting = __btoh16(ccs.Voice_Setting);
	return ccs.Status;
}

int HCI_ReadSCOFlowControlEnable(int fd, __u8 *Flow_Control)
{
	int				err;
	struct Read_SCO_Flow_Control_Event	ccs;

	err = hci_exec_cmd0(fd, HCI_C_READ_SCO_FLOW_CONTROL_ENABLE,
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	*Flow_Control = ccs.Flow_Control;
	return ccs.Status;
}

int HCI_ReadHoldModeActivity(int fd, __u8 *Hold_Mode_Activity)
{
	int					err;
	struct Read_Hold_Mode_Activity_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_HOLD_MODE_ACTIVITY, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Hold_Mode_Activity = cce.Hold_Mode_Activity;
	return 0;
}

int HCI_WriteHoldModeActivity(int fd, __u8 Hold_Mode_Activity)
{
	int					err;
	struct Write_Hold_Mode_Activity		cmd;
	struct Command_Complete_Status		ccs;

	cmd.Hold_Mode_Activity = Hold_Mode_Activity;
	err = hci_exec_cmd(fd, HCI_C_WRITE_HOLD_MODE_ACTIVITY, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}


/*
 * XXX: multiple command completes
*/
int HCI_ReadLinkSupervisionTimeout(int fd, __u16 Connection_Handle, __u16 *Link_Supervision_Timeout)
{
	int					err;
	struct Read_Link_Supervision_Timeout	cmd;
	struct Read_Link_Supervision_Timeout_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	err = hci_exec_cmd(fd, HCI_C_READ_LINK_SUPERVISION_TIMEOUT, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Link_Supervision_Timeout = __btoh16(cce.Link_Supervision_Timeout);
	return 0;
}

int HCI_ReadNumberOfSupportedIAC(int fd, __u8 *Num_Supported_IAC)
{
	int				err;
	struct Read_Number_Of_Supported_IAC_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_NUMBER_OF_SUPPORTED_IAC, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Num_Supported_IAC = cce.Num_Supported_IAC;
	return 0;
}

int HCI_ReadCurrentIACLAP(int fd, __u8 *Num_Current_IAC, __u32 *IAC_LAP)
{
	int				err, i;
	unsigned char			buf[HCI_MAX_EVENT_SIZE];
	struct Read_Current_IAC_LAP_Event	*cce = (void*)buf;

	err = hci_exec_cmd0(fd, HCI_C_READ_CURRENT_IAC_LAP, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce->Status)
		return cce->Status;
	*Num_Current_IAC = cce->Num_Current_IAC;
	for (i=0; i<*Num_Current_IAC; i++)
		IAC_LAP[i] = __btoh24(cce->IAC_LAP[i].v);
	return 0;
}

int HCI_WriteCurrentIACLAP(int fd, __u8 Num_Current_IAC, __u32 *IAC_LAP)
{
	int				err, i;
	unsigned char			buf[HCI_MAX_CMD_SIZE];
	struct Write_Current_IAC_LAP	*cmd = (void*)buf;
	struct Command_Complete_Status	ccs;

	cmd->Num_Current_IAC = Num_Current_IAC;
	for (i=0; i<Num_Current_IAC; i++)
		cmd->IAC_LAP[i].v = __htob24(IAC_LAP[i]);
	
	err = hci_exec_cmd(fd, HCI_C_WRITE_CURRENT_IAC_LAP, &cmd, sizeof(cmd) + Num_Current_IAC*BD_LAP_SIZE,
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ReadPageScanPeriodMode(int fd, __u8 *Page_Scan_Period_Mode)
{
	int					err;
	struct Read_Page_Scan_Period_Mode_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PAGE_SCAN_PERIOD_MODE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Page_Scan_Period_Mode = cce.Page_Scan_Period_Mode;
	return 0;
}

int HCI_WritePageScanPeriodMode(int fd, __u8 Page_Scan_Period_Mode)
{
	int					err;
	struct Write_Page_Scan_Period_Mode	cmd;
	struct Command_Complete_Status		ccs;

	cmd.Page_Scan_Period_Mode = Page_Scan_Period_Mode;
	err = hci_exec_cmd(fd, HCI_C_WRITE_PAGE_SCAN_PERIOD_MODE, &cmd, sizeof(cmd),
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_ReadPageScanMode(int fd, __u8 *Page_Scan_Mode)
{
	int					err;
	struct Read_Page_Scan_Mode_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PAGE_SCAN_MODE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Page_Scan_Mode = cce.Page_Scan_Mode;
	return 0;
}

int HCI_WritePageScanMode(int fd, __u8 Page_Scan_Mode)
{
	int					err;
	struct Write_Page_Scan_Mode		cmd;
	struct Command_Complete_Status		ccs;

	cmd.Page_Scan_Mode = Page_Scan_Mode;
	err = hci_exec_cmd(fd, HCI_C_WRITE_PAGE_SCAN_MODE, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_WriteSCOFlowControlEnable(int fd, __u8 Flow_Control)
{
	int				err;
	struct Write_SCO_Flow_Control	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Flow_Control = Flow_Control;
	err = hci_exec_cmd(fd, HCI_C_WRITE_SCO_FLOW_CONTROL_ENABLE, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_WriteVoiceSetting(int fd, __u16 Voice_Setting)
{
	int				err;
	struct Write_Voice_Setting	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Voice_Setting = __htob16(Voice_Setting);
	err = hci_exec_cmd(fd, HCI_C_WRITE_VOICE_SETTING, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

/* Informational Commands */
int HCI_ReadCountryCode(int fd, int *Country_Code)
{
	int					err;
	struct Read_Country_Code_Event		cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_COUNTRY_CODE, 
			COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));
	if (err)
		return err;
	if (cce.Status)
		return cce.Status;
	*Country_Code = cce.Country_Code;
	return 0;
}

// Testing
int HCI_ReadLoopbackMode(int fd, __u8 *mode)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_READ_LOOPBACK_MODE, 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	*mode = ccs.Data[0];
	return ccs.Status;
}

int HCI_WriteLoopbackMode(int fd, __u8 mode)
{
	int				err;
	HCI_Command_Packet_Header	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Data[0] = mode;
	err = hci_exec_cmd(fd, HCI_C_WRITE_LOOPBACK_MODE, &cmd, sizeof(cmd) + 1,
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

int HCI_EnableDeviceUnderTestMode(int fd)
{
	int				err;
	struct Command_Complete_Status	ccs;

	err = hci_exec_cmd0(fd, HCI_C_ENABLE_DEVICE_UNDER_TEST_MODE, 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}


// Ericsson specific

int HCI_EricssonWritePCMSettings(int fd, __u8 Settings)
{
	int					err;
	struct Ericsson_Write_PCM_Settings	cmd;
	struct Command_Complete_Status		ccs;

	cmd.PCM_Settings = Settings;
	err = hci_exec_cmd(fd, HCI_C_ERICSSON_WRITE_PCM_SETTINGS, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}


int HCI_EricssonSetSCODataPath(int fd, __u8 Path)
{
	int					err;
	struct Ericsson_Set_SCO_Data_Path	cmd;
	struct Command_Complete_Status		ccs;

	cmd.SCO_Data_Path = Path;
	err = hci_exec_cmd(fd, HCI_C_ERICSSON_SET_SCO_DATA_PATH, &cmd, sizeof(cmd), 
			COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	if (err)
		return err;
	return ccs.Status;
}

/*
 * Affix specific command
 */
int HCI_WriteAudioSetting(int fd, int mode, __u16 setting)
{
	int 	err = 0;
	
	if ((mode & AFFIX_AUDIO_SYNC) && (mode & AFFIX_AUDIO_ASYNC)) {
		errno = EINVAL;
		return -1;
	}
	if (mode & AFFIX_AUDIO_ON) {
		if (mode & AFFIX_AUDIO_ASYNC) {
			/* enable SCO flow control */
			err = HCI_WriteSCOFlowControlEnable(fd, 0x01);
			if (err)
				return err;
		} else {
			/* disable SCO flow control */
			err = HCI_WriteSCOFlowControlEnable(fd, 0x00);
			if (err < 0)
				return err;
		}
		if (setting != 0xffff) {
			err = HCI_WriteVoiceSetting(fd, setting);
			if (err)
				return err;
		}
	}
	return hci_set_audio(fd, mode, setting);
}


#endif	/* __USER__ */

// Affix specific
int HCI_WriteSecurityMode(int fd, int Security_Mode)
{
	int	err;
	
	err = HCI_WriteAuthenticationEnable(fd, (Security_Mode & HCI_SECURITY_AUTH) != 0);
	if (err)
		return err;
	err = HCI_WriteEncryptionMode(fd, (Security_Mode & HCI_SECURITY_ENCRYPT) != 0);
	if (err)
		return err;
	return hci_set_secmode(fd, Security_Mode);
}


#ifdef CONFIG_AFFIX_BT_1_2

// 
// Link Control Commands
//

int HCI_CreateConnectionCancel(int fd, BD_ADDR* bda)
{
	int err;
	struct Create_Connection_Cancel	cmd;
	struct Command_Complete_Status	ccs;

	cmd.bda = *bda;
	err= hci_exec_cmd( fd, HCI_C_CREATE_CONNECTION_CANCEL, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	
	return err ? err : ccs.Status;
}

int HCI_RemoteNameRequestCancel(int fd, BD_ADDR* bda)
{
	int err;
	struct Remote_Name_Request_Cancel	cmd;
	struct Command_Complete_Status		ccs;

	cmd.bda = *bda;
	err = hci_exec_cmd( fd, HCI_C_REMOTE_NAME_REQUEST, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));

	return err ? err : ccs.Status;
}


int HCI_ReadLMPHandle(int fd, __u16 Connection_Handle, __u8* LMP_Handle)
{
	int err;
	struct Read_LMP_Handle		cmd;
	struct Read_LMP_Handle_Event	cce;

	cmd.Connection_Handle = Connection_Handle;

	err = hci_exec_cmd(fd, HCI_C_READ_LMP_HANDLE, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*LMP_Handle = cce.LMP_Handle;

	return err ? err : cce.Status;
}

int HCI_ReadRemoteExtendedFeatures(int fd, __u16 Connection_Handle, __u8 Page_Number, __u8* Maximum_Page_Number, __u8* Extended_LMP_Features)
{
	int err;
	struct Read_Remote_Extended_Features			cmd;
	struct Read_Remote_Extended_Features_Complete_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Page_Number = Page_Number;
	
	err = hci_exec_cmd(fd, HCI_C_READ_REMOTE_EXTENDED_FEATURES, &cmd, sizeof(cmd), READ_REMOTE_EXTENDED_FEATURES_COMPLETE_MASK, 0, &cce, sizeof(cce));
	
	if (!err){
		*Maximum_Page_Number = cce.Maximum_Page_Number;
		memcpy(Extended_LMP_Features,cce.Extended_LMP_Features,8);
		return cce.Status;
	}

	return err;
}


int HCI_SetupSynchronousConnectionCreate(int fd, __u16 Connection_Handle, SYNC_CON_REQ *param, SYNC_CON_RES *res)
// This function can be used to create synchronous connection or change some settings in an existing one.
{
	int err;
	unsigned char			buf[HCI_MAX_EVENT_SIZE];	
	struct Setup_Synchronous_Connection		cmd;
	struct Synchronous_Connection_Complete_Event  	*scce = (void*)buf;
	struct Synchronous_Connection_Changed_Event	*scche = (void*)buf;

	cmd.Connection_Handle = Connection_Handle;
	cmd.Transmit_Bandwidth = param->Transmit_Bandwidth;
	cmd.Receive_Bandwidth = param->Receive_Bandwidth;
	cmd.Max_Latency = param->Max_Latency;
	cmd.Voice_Setting = param->Voice_Setting;
	cmd.Retransmission_Effort = param->Retransmission_Effort;
	cmd.Packet_Type = param->Packet_Type;

	err = hci_exec_cmd1( fd, HCI_C_SETUP_SYNCHRONOUS_CONNECTION, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK | SYNCHRONOUS_CONNECTION_COMPLETE_MASK | SYNCHRONOUS_CONNECTION_CHANGED_MASK, 0 );
	if (err) // COMMAND STATUS ERROR (if any)
		return err;
		
	err = hci_recv_event(fd, buf,sizeof(buf), 30); 
	hci_event_mask(fd, 0);	/* remove listener */
	if (!err) {
			if (scce->hdr.EventCode == HCI_E_SYNCHRONOUS_CONNECTION_COMPLETE) {
				res->Connection_Handle = scce->Connection_Handle;
				memcpy(&res->bda,&scce->bda,6);
				res->Link_Type = scce->Link_Type;
				res->Transmission_Interval = scce->Transmission_Interval;
				res->Retransmission_window = scce->Retransmission_window;
				res->Rx_Packet_Length = scce->Rx_Packet_Length;
				res->Tx_Packet_Length = scce->Tx_Packet_Length;
				res->Air_Mode = scce->Air_Mode;
				return scce->Status;
			}
			else if (scche->hdr.EventCode == HCI_E_SYNCHRONOUS_CONNECTION_CHANGED) {
				res->Connection_Handle = scche->Connection_Handle;
				res->Transmission_Interval = scche->Transmission_Interval;
				res->Retransmission_window = scche->Retransmission_window;
				res->Rx_Packet_Length = scche->Rx_Packet_Length;
				res->Tx_Packet_Length = scche->Tx_Packet_Length;
				return scche->Status;
			}
			else {// Unexpected EVENT return an error.
				err = 0x1F; // Unspecified error !?
			}
	}
	return err;
}

int __HCI_SetupSynchronousConnectionCreate(int fd, __u16 Connection_Handle, SYNC_CON_REQ *param)
// This function can be used to create synchronous connection or change some settings in an existing one.
{
	int err;
	struct Setup_Synchronous_Connection		cmd;

	cmd.Connection_Handle = Connection_Handle;
	cmd.Transmit_Bandwidth = param->Transmit_Bandwidth;
	cmd.Receive_Bandwidth = param->Receive_Bandwidth;
	cmd.Max_Latency = param->Max_Latency;
	cmd.Voice_Setting = param->Voice_Setting;
	cmd.Retransmission_Effort = param->Retransmission_Effort;
	cmd.Packet_Type = param->Packet_Type;

	err = hci_exec_cmd1( fd, HCI_C_SETUP_SYNCHRONOUS_CONNECTION, &cmd, sizeof(cmd), 
			COMMAND_STATUS_MASK, 0 );
	return err;
		
}

int HCI_AcceptSynchronousConnectionRequest(int fd, ACCEPT_SYNC_CON_REQ* req, SYNC_CON_RES* res)
{
	int 						err;
	struct Accept_Synchronous_Connection_Request	cmd;
	struct Synchronous_Connection_Complete_Event  	scce;

	memcpy(&cmd.bda,&req->bda,6);
	cmd.Transmit_Bandwidth = req->Transmit_Bandwidth;
	cmd.Receive_Bandwidth = req->Receive_Bandwidth;
	cmd.Max_Latency = req->Max_Latency;
	cmd.Content_Format = req->Content_Format;
	cmd.Retransmission_Effort = req->Retransmission_Effort;
	cmd.Packet_Type = req->Packet_Type;
	
	err = hci_exec_cmd1( fd, HCI_C_ACCEPT_SYNCHRONOUS_CONNECTION_REQUEST, &cmd, sizeof(cmd), COMMAND_STATUS_MASK | SYNCHRONOUS_CONNECTION_COMPLETE_MASK,0);
	
	if (err)
		return err;

	err = hci_recv_event(fd, &scce, sizeof(scce), 30);
	hci_event_mask(fd, 0);
	if (!err) {
		res->Connection_Handle = scce.Connection_Handle;
		memcpy(&res->bda,&scce.bda,6);
		res->Link_Type = scce.Link_Type;
		res->Transmission_Interval = scce.Transmission_Interval;
		res->Retransmission_window = scce.Retransmission_window;
		res->Rx_Packet_Length = scce.Rx_Packet_Length;
		res->Tx_Packet_Length = scce.Tx_Packet_Length;
		res->Air_Mode = scce.Air_Mode;
		return scce.Status;
	}
	
	return err;
}

int __HCI_AcceptSynchronousConnectionRequest(int fd, ACCEPT_SYNC_CON_REQ* req)
{
	int 						err;
	struct Accept_Synchronous_Connection_Request	cmd;

	memcpy(&cmd.bda,&req->bda,6);
	cmd.Transmit_Bandwidth = req->Transmit_Bandwidth;
	cmd.Receive_Bandwidth = req->Receive_Bandwidth;
	cmd.Max_Latency = req->Max_Latency;
	cmd.Content_Format = req->Content_Format;
	cmd.Retransmission_Effort = req->Retransmission_Effort;
	cmd.Packet_Type = req->Packet_Type;
	
	err = hci_exec_cmd1( fd, HCI_C_ACCEPT_SYNCHRONOUS_CONNECTION_REQUEST, &cmd, sizeof(cmd), COMMAND_STATUS_MASK,0);
	
	return err;

}


int HCI_RejectSynchronousConnectionRequest(int fd, BD_ADDR* bda, __u8 Reason)
{
	int 		err;
	struct Reject_Synchronous_Connection_Request	cmd;
	struct Synchronous_Connection_Complete_Event  	scce;

	memcpy(&cmd.bda,bda,6);
	cmd.Reason = Reason;

	err = hci_exec_cmd1( fd, HCI_C_REJECT_SYNCHRONOUS_CONNECTION_REQUEST, &cmd, sizeof(cmd), COMMAND_STATUS_MASK | SYNCHRONOUS_CONNECTION_COMPLETE_MASK,0);

	if (err)
		return err;

	err = hci_recv_event(fd, &scce, sizeof(scce), 30); 
	hci_event_mask(fd, 0);	/* remove listener */
	
	return err ? err : scce.Status;
}

int __HCI_RejectSynchronousConnectionRequest(int fd, BD_ADDR* bda, __u8 Reason)
{
	int 		err;
	struct Reject_Synchronous_Connection_Request	cmd;

	memcpy(&cmd.bda,bda,6);
	cmd.Reason = Reason;

	err = hci_exec_cmd1( fd, HCI_C_REJECT_SYNCHRONOUS_CONNECTION_REQUEST, &cmd, sizeof(cmd), COMMAND_STATUS_MASK,0);

	return err;
}

//
// Link Policy Commands
// 

int HCI_ReadDefaultLinkPolicySettings(int fd, __u16* Default_Link_Policy_Settings)
{
	int err;
	struct Read_Default_Link_Policy_Settings_Event	cce;
	
	err = hci_exec_cmd0(fd, HCI_C_READ_DEFAULT_LINK_POLICY_SETTINGS, COMMAND_COMPLETE_MASK, 0,&cce, sizeof(cce));
	
	*Default_Link_Policy_Settings = __btoh16(cce.Default_Link_Policy_Settings);
	
	return err ? err : cce.Status;
}

int HCI_WriteDefaultLinkPolicySettings(int fd, __u16 Default_Link_Policy_Settings)
{
	int err;
	struct Write_Default_Link_Policy_Settings	cmd;
	struct Command_Complete_Status	 		ccs;
	
	cmd.Default_Link_Policy_Settings = __htob16(Default_Link_Policy_Settings);
	
	err = hci_exec_cmd(fd, HCI_C_READ_DEFAULT_LINK_POLICY_SETTINGS, &cmd, sizeof(cmd),COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	
	return err ? err : ccs.Status;
}

int HCI_Flow_Specification(int fd, __u16 Connection_Handle, HCI_FLOW* Requested_Flow, HCI_FLOW* Completed_Flow)
{
	int	err;
	struct Flow_Specification	cmd;
	struct Flow_Specification_Complete_Event	cce;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Flags =  Requested_Flow->Flags;
	cmd.Flow_direction =  Requested_Flow->Flow_direction;
	cmd.Service_Type =  Requested_Flow->Service_Type;
	cmd.Token_Rate =  __htob32(Requested_Flow->Token_Rate);
	cmd.Token_Bucket_Size =  __htob32(Requested_Flow->Token_Bucket_Size);
	cmd.Peak_Bandwidth =  __htob32(Requested_Flow->Peak_Bandwidth);
	cmd.Access_Latency =  __htob32(Requested_Flow->Access_Latency);

	// ISSUE HCI COMMAND
	err = hci_exec_cmd1( fd, HCI_C_FLOW_SPECIFICATION, &cmd, sizeof(cmd), COMMAND_STATUS_MASK | FLOW_SPECIFICATION_COMPLETE_MASK, 0);

	if (err) // COMMAND STATUS ERROR (if any)
		return err;
		
	while (!(err = hci_recv_event(fd, &cce, sizeof(cce), 30)) && (__btoh16(cce.Connection_Handle) != Connection_Handle) ) 
	{}// Wait for FLOW_SPECIFICATION EVENT
	
	hci_event_mask(fd, 0);	/* remove listener */
	if (!err) { // (__btoh16(cce->Connection_Handle) == Connection_Handle)
			// EVENT FOR THIS CONNECTION HANDLER, IT MUST BE FLOW_SPECIFICATION EVENT (OTHER EVENT MASKED EXCEPT COMMAND STATUS)
			Completed_Flow->Flags = cce.Flags;
			Completed_Flow->Flow_direction = cce.Flow_direction;
			Completed_Flow->Service_Type = cce.Service_Type;
			Completed_Flow->Token_Rate = cce.Token_Rate;
			Completed_Flow->Token_Bucket_Size = cce.Token_Bucket_Size;
			Completed_Flow->Peak_Bandwidth = cce.Peak_Bandwidth;
			Completed_Flow->Access_Latency = cce.Access_Latency;
			return cce.Status;
	}
	return err;
}

int __HCI_Flow_Specification(int fd, __u16 Connection_Handle, HCI_FLOW* Requested_Flow)
{	
	struct Flow_Specification 	cmd;

	cmd.Connection_Handle = __htob16(Connection_Handle);
	cmd.Flags =  Requested_Flow->Flags;
	cmd.Flow_direction =  Requested_Flow->Flow_direction;
	cmd.Service_Type =  Requested_Flow->Service_Type;
	cmd.Token_Rate =  __htob32(Requested_Flow->Token_Rate);
	cmd.Token_Bucket_Size =  __htob32(Requested_Flow->Token_Bucket_Size);
	cmd.Peak_Bandwidth =  __htob32(Requested_Flow->Peak_Bandwidth);
	cmd.Access_Latency =  __htob32(Requested_Flow->Access_Latency);

	return hci_exec_cmd1( fd, HCI_C_FLOW_SPECIFICATION, &cmd, sizeof(cmd), COMMAND_STATUS_MASK, 0);
}

// Controller & Baseban Commands
int HCI_SetAFHHostChannelClassification(int fd, __u8* AFH_Host_Channel_Classification)
{
	int err;
	struct Set_AFH_Host_Channel_Classification 	cmd;
	struct Command_Complete_Status		 	ccs;

	memcpy(cmd.AFH_Host_Channel_Classification,AFH_Host_Channel_Classification,10);

	err = hci_exec_cmd(fd, HCI_C_SET_AFH_HOST_CHANNEL_CLASSIFICATION, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));

	return err ? err : ccs.Status;
}

int HCI_ReadInquiryScanType(int fd, __u8* Inquiry_Scan_Type)
{
	int err;
	struct Read_Inquiry_Scan_Type_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_INQUIRY_SCAN_TYPE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Inquiry_Scan_Type = cce.Inquiry_Scan_Type;
	
	return err ? err : cce.Status;
}

int HCI_WriteInquiryScanType(int fd, __u8 Scan_Type)
{
	int err;
	struct Write_Inquiry_Scan_Type	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Scan_Type = Scan_Type;
	
	err = hci_exec_cmd(fd, HCI_C_WRITE_INQUIRY_SCAN_TYPE, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));

	return err ? err : ccs.Status;
}

int HCI_ReadInquiryMode(int fd, __u8* Inquiry_Mode)
{
	int err;
	struct Read_Inquiry_Mode_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_INQUIRY_MODE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Inquiry_Mode = cce.Inquiry_Mode;
	
	return err ? err : cce.Status;
}

int HCI_WriteInquiryMode(int fd, __u8 Inquiry_Mode)
{
	int err;
	struct Write_Inquiry_Mode	cmd;
	struct Command_Complete_Status	ccs;
	
	cmd.Inquiry_Mode = Inquiry_Mode;
	
	err = hci_exec_cmd(fd, HCI_C_WRITE_INQUIRY_MODE, &cmd, sizeof(cmd),COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));
	
	return err ? err : ccs.Status;
}

int HCI_ReadPageScanType(int fd, __u8* Page_Scan_Type)
{
	int err;
	struct Read_Page_Scan_Type_Event	cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_PAGE_SCAN_TYPE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Page_Scan_Type = cce.Page_Scan_Type;
	
	return err ? err : cce.Status;
}

int HCI_WritePageScanType(int fd, __u8 Page_Scan_Type)
{
	int err;
	struct Write_Page_Scan_Type	cmd;
	struct Command_Complete_Status	ccs;

	cmd.Page_Scan_Type = Page_Scan_Type;
	
	err = hci_exec_cmd(fd, HCI_C_WRITE_PAGE_SCAN_TYPE, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));

	return err ? err : ccs.Status;
}

int HCI_ReadAFHChannelAssessmentMode(int fd, __u8* AFH_Channel_Assessment_Mode)
{
	int err;
	struct Read_AFH_Channel_Assessment_Mode_Event cce;

	err = hci_exec_cmd0(fd, HCI_C_READ_AFH_CHANNEL_ASSESSMENT_MODE, COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*AFH_Channel_Assessment_Mode = cce.AFH_Channel_Assessment_Mode;
	
	return err ? err : cce.Status;
}

int HCI_WriteAFHChannelAssessmentMode(int fd, __u8 AFH_Channel_Assessment_Mode)
{
	int err;
	struct Write_AFH_Channel_Assessment_Mode	cmd;
	struct Command_Complete_Status			ccs;

	cmd.AFH_Channel_Assessment_Mode = AFH_Channel_Assessment_Mode;
	
	err = hci_exec_cmd(fd, HCI_C_WRITE_AFH_CHANNEL_ASSESSMENT_MODE, &cmd, sizeof(cmd),COMMAND_COMPLETE_MASK, 0, &ccs, sizeof(ccs));

	return err ? err : ccs.Status;
}

//Informational Parameters
int HCI_ReadLocalExtendedFeatures(int fd, __u8* Page_Number,__u8* Maximum_Page_Number,__u64* Extended_LMP_Features)
// Page_Number is an in/out parameter.
{
	int err;
        struct 	Read_Local_Extended_Features		cmd;
        struct 	Read_Local_Extended_Features_Event 	cce;

	cmd.Page_Number = *Page_Number;

	err = hci_exec_cmd(fd, HCI_C_READ_LOCAL_EXTENDED_FEATURES, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Page_Number = cce.Page_Number;
	*Maximum_Page_Number = cce.Maximum_Page_Number;
	*Extended_LMP_Features = cce.Extended_LMP_Features;

	return err ? err : cce.Status;
}


//Status Parameters

int HCI_ReadAFHChannelMap(int fd, __u16 Connection_Handle, __u8* AFH_Mode,__u8* AFH_Channel_Map)
{
	int err;
	struct Read_AFH_Channel_Map		cmd;
	struct Read_AFH_Channel_Map_Event	cce;

	cmd.Connection_Handle = Connection_Handle;
	
	err = hci_exec_cmd(fd, HCI_C_READ_AFH_CHANNEL_MAP, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	if (!err) {
		*AFH_Mode = cce.AFH_Mode;
		memcpy(AFH_Channel_Map,cce.AFH_Channel_Map,10);
		return cce.Status;
	}

	return err;
}

int HCI_ReadClock(int fd, __u16 Connection_Handle, __u8 Which_Clock, __u32* Clock, __u16* Accuracy)
{
	int err;
	struct Read_Clock		cmd;
	struct Read_Clock_Event	cce;

	cmd.Connection_Handle = Connection_Handle;
	cmd.Which_Clock = Which_Clock;

	err = hci_exec_cmd(fd, HCI_C_READ_CLOCK, &cmd, sizeof(cmd), COMMAND_COMPLETE_MASK, 0, &cce, sizeof(cce));

	*Clock = __btoh32(cce.Clock);
	*Accuracy = __btoh16(cce.Accuracy);

	return err ? err : cce.Status;
}

#endif // CONFIG_AFFIX_BT_1_2

#endif
