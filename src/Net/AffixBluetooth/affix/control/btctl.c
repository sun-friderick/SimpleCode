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
   $Id: btctl.c,v 1.190 2004/05/25 11:51:34 kassatki Exp $

   btctl - driver control program

	Fixes:	Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
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
#include <affix/utils.h>

#include "btctl.h"

int		nameset;
int		showall = 0;
int		uart_rate = -1;
char		*uart_map = "/etc/affix/device.map";


#if defined(CONFIG_AFFIX_BT_1_2)

int cmd_create_connection_cancel(struct command *cmd)
{
	int	err, fd;
	BD_ADDR	bda;

	if (__argv[optind] == NULL) {
		printf("Bluetooth address missed\n");
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	err = HCI_CreateConnectionCancel(fd, &bda);
	hci_close(fd);
	
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Create Connection cancelled on BT address: %s\n", bda2str(&bda));
	pause();
	
	return 0;
}

int cmd_remote_name_cancel (struct command *cmd)
{
	int		err, fd;
	BD_ADDR		bda;

	if (__argv[optind] == NULL) {
		printf("Bluetooth address missed\n");
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return fd;
	}

	err = HCI_RemoteNameRequestCancel(fd, &bda);
	hci_close(fd);	
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Remote name request CANCELED !\n");
	return 0;
}

int cmd_lmp_handle(struct command *cmd)
{
	int err,fd;
	int handle;
	__u8	lmp_handle = 0;
	BD_ADDR	bda;
	
	if (__argv[optind] == NULL) {
		print_usage(cmd);
		return -1;
	}
	
	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}

	err = HCI_ReadLMPHandle(fd, handle, &lmp_handle);
	hci_close(fd);


	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	
	printf("LMP handle for %s connection: %x",bda2str(&bda), lmp_handle);

	return 0;

}

int cmd_remote_extended_features(struct command *cmd)
{
	int		err,fd,handle;
	__u8		maximum_page_number = 0,page_number = 0;
	__u8		extended_lmp_features[8];
	BD_ADDR		bda;

	if (__argv[optind] == NULL) {
		print_usage(cmd);
		return -1;
	}
	
	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	if (__argv[optind+1] != NULL) {
		page_number = atoi(__argv[optind+1]);
	}
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}

	err = HCI_ReadRemoteExtendedFeatures(fd, handle, page_number,&maximum_page_number,extended_lmp_features);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Extended Features for %s :\n Maximum Page Number : %x\n LMP Features byte 0: %x, byte 1: %x, byte 2: %x, byte 3: %x, byte 4: %x, byte 5 : %x, byte 6: %x, byte 7: %x \n",bda2str(&bda),maximum_page_number,extended_lmp_features[0],extended_lmp_features[1],extended_lmp_features[2],extended_lmp_features[3],extended_lmp_features[4],extended_lmp_features[5],extended_lmp_features[6],extended_lmp_features[7]);
	
	hci_close(fd);
	return 0;
}

// Link Policy Commands

int cmd_link_policy_settings (struct command *cmd)
{
	int err,fd;
	__u16 Default_Link_Policy_Settings;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	if (cmd->cmd) {
		if (__argv[optind] == NULL){
			print_usage(cmd);
			return -1;
		}
	
		Default_Link_Policy_Settings = atoi(__argv[optind]);
		err = HCI_WriteDefaultLinkPolicySettings(fd,Default_Link_Policy_Settings);
	}
	else
	{
		err = HCI_ReadDefaultLinkPolicySettings(fd,&Default_Link_Policy_Settings);
	}
	hci_close(fd);
	if (err)
	{
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	printf("Writting Default Link Policy Settings (%x) DONE.\n",Default_Link_Policy_Settings);
	return 0;
}

int cmd_flow_specification(struct command *cmd)
{
	int fd,err,handle;
	BD_ADDR bda;
	HCI_FLOW Requested_Flow, Completed_Flow;

	if (__argc < 7) {
		print_usage(cmd);
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	Requested_Flow.Flags = 0; // Reserved
	if ((Requested_Flow.Flow_direction = atoi(__argv[optind+1])) > 1) {
		printf("Wrong Flow direction. Valid values: 0  -> Outgoing Flow, 1  -> Incoming Flow.\n");
		return 1;
	}
	
	if ((Requested_Flow.Service_Type = atoi(__argv[optind+2])) > 2) {
		printf ("Wrong Service Type: Valid values: 0 -> No traffic, 1 -> Best Effort, 2 -> Guaranteed\n");
		return 1;
	}
	Requested_Flow.Token_Rate = atoi(__argv[optind+3]);
	Requested_Flow.Token_Bucket_Size = atoi(__argv[optind+4]);
	Requested_Flow.Peak_Bandwidth = atoi(__argv[optind+5]);
	Requested_Flow.Access_Latency = atoi(__argv[optind+6]);
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}
	
	err = HCI_Flow_Specification( fd, handle, &Requested_Flow, &Completed_Flow);
	hci_close(fd);
	if (err)
	{
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Flow specification: ->Flow_direction: %x\n ->Service_Type: %x\n ->Token_Rate: %x\n ->Token_Bucket_Size: %x\n ->Peak_Bandwidth: %x\n Access_Latency: %x\n",					Completed_Flow.Flow_direction,																		   Completed_Flow.Service_Type, 																	      Completed_Flow.Token_Rate,																		 Completed_Flow.Token_Bucket_Size, 
				     Completed_Flow.Peak_Bandwidth,
				     Completed_Flow.Access_Latency);
	return 0;
}

int cmd_afh_host_channel(struct command *cmd)
{
	int fd,err,i;
	__u32	value;
	__u8	channels[12];


	for (i=0;i<3;i++)
	{
		if (sscanf(__argv[optind+i],"%x",&value)){
			print_usage(cmd);
			return -1;
		}
		channels[i*4] = value;
		channels[i*4 + 1] = value>>8;
		channels[i*4 + 2] = value>>16;
		channels[i*4 + 3] = value>>24;
	}
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	err = HCI_SetAFHHostChannelClassification(fd,channels);
	hci_close(fd);
	if (err)
	{
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Done.\n");
	return 0;
}

int cmd_inquiry_scan_type(struct command *cmd)
{
	int 	err,fd;
	__u8 	inquiry_scan_type = 0;
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	if (cmd->cmd) { // If Write Inwuiry scan command then read value from command line.
		if ((__argv[optind]==NULL) || ((inquiry_scan_type = atoi(__argv[optind])) > 1))
		{
			print_usage(cmd);
			return -1;
		}
		err = HCI_WriteInquiryScanType(fd,inquiry_scan_type);
	}
	else
	{
		err = HCI_ReadInquiryScanType(fd,&inquiry_scan_type);
	}
	
	hci_close(fd);
	if (err)
	{
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Inquiry Scan Type: %x\n",inquiry_scan_type);
	return 0;
}

int cmd_inquiry_mode(struct command *cmd)
{
	int	err,fd;
	__u8 	inquiry_mode = 0;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	if (cmd->cmd) { // If write inquiry mode the read value from command line.
		if ((__argv[optind]==NULL) || ((inquiry_mode = atoi(__argv[optind])) > 1))
		{
			print_usage(cmd);
			return -1;
		}
		err = HCI_WriteInquiryMode(fd,inquiry_mode);
	}
	else
	{
		err = HCI_ReadInquiryMode(fd,&inquiry_mode);
	}
	
	hci_close(fd);
	if (err)
	{
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Inquiry Mode: %x\n",inquiry_mode);
	return 0;
	
}

int cmd_page_scan_type(struct command *cmd)
{
	int 	err,fd;
	__u8	page_scan_type = 0;
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	if (cmd->cmd) {
		if ((__argv[optind]==NULL) || ((page_scan_type = atoi(__argv[optind])) > 1))
		{
			print_usage(cmd);
			return -1;
		}
		err = HCI_WritePageScanType(fd,page_scan_type);
	}
	else
	{
		err = HCI_ReadPageScanType(fd,&page_scan_type);
	}
	
	hci_close(fd);
	if (err)
	{
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Page Scan Type: %x\n",page_scan_type);
	return 0;
}

int cmd_afh_cam(struct command *cmd)
{
	int 	err,fd;
	__u8	AFH_Channel_Assessment_Mode = 0;
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	if (cmd->cmd) { 
		if ((__argv[optind]==NULL) || ((AFH_Channel_Assessment_Mode = atoi(__argv[optind])) > 1))
		{
			print_usage(cmd);
			return -1;
		}
		err = HCI_WriteAFHChannelAssessmentMode(fd,AFH_Channel_Assessment_Mode);
	}
	else
	{
		err = HCI_ReadAFHChannelAssessmentMode(fd,&AFH_Channel_Assessment_Mode);
	}
	hci_close(fd);
	if (err)
	{
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("AFH Channel Assessment Mode: %x\n",AFH_Channel_Assessment_Mode);
	return 0;
}

int cmd_local_extended_features(struct command *cmd)
{
	int err,fd;
	__u8	page_number,maximum_page_number;
	__u64	extended_lmp_features;

	if (__argv[optind]==NULL) {
		print_usage(cmd);
		return -1;
	}

	page_number = atoi(__argv[optind]);
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	err = HCI_ReadLocalExtendedFeatures(fd, &page_number, &maximum_page_number, &extended_lmp_features);

	hci_close(fd);
	if (err){
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Read Local Extended Features:\n ->Page Number: %x\n ->Maximum Page Number: %x\n Extended_LMP_Features: %llx\n",page_number,maximum_page_number,extended_lmp_features);
	return 0;

}

int cmd_read_clock(struct command *cmd)
{
	int 	err,fd,handle = 0;
	__u8	which_clock = 0;
	__u16	accuracy;
	__u32	clock;
	BD_ADDR bda;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	if (__argv[optind] != NULL) {
		err = btdev_get_bda(&bda, __argv[optind]);
		if (err) {
			printf("Wrong address format\n");
			return 1;
		}

		if (__argv[optind+1] == NULL) {
			print_usage(cmd);
			return -1;
		}
		which_clock = atoi(__argv[optind+1]);
	
		handle = hci_get_conn(fd, &bda);
		if (handle < 0) {
			printf("Connection not found\n");
			return 1;
		}
	}
	err = HCI_ReadClock(fd, handle, which_clock, &clock, &accuracy);
	hci_close(fd);
	if (err){
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("Clock: %x, Accuracy: %x\n", clock, accuracy);
	return 0;
}

int cmd_afh_channel_map(struct command *cmd)
{
	int 	err,fd,handle;
	__u8	AFH_Mode;
	__u8	AFH_Channel_Map[8];
	BD_ADDR	bda;
	
	if (__argv[optind] == NULL) {
		print_usage(cmd);
		return -1;
	}
	
	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	
	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}
	
	err = HCI_ReadAFHChannelMap(fd, handle, &AFH_Mode, AFH_Channel_Map);
	hci_close(fd);
	if (err){
			fprintf(stderr, "%s\n", hci_error(err));
			exit(1);
	}
	printf("AFH Mode: %x\nAFH_Channel_Map [Channels]:\n [0-7]:%x\n [8-15]:%x\n [16-23]:%x\n [24-31]:%x\n [32-39]:%x\n [40-47]:%x\n [48-55]:%x\n [56-63]:%x\n [64-71]:%x\n [72-78]:%x\n",AFH_Mode,AFH_Channel_Map[0],AFH_Channel_Map[1],AFH_Channel_Map[2],AFH_Channel_Map[3],AFH_Channel_Map[4],AFH_Channel_Map[5],AFH_Channel_Map[6],AFH_Channel_Map[7],AFH_Channel_Map[8],AFH_Channel_Map[9]);
	return 0;
}

#endif

int cmd_bdaddr(struct command *cmd)
{
	int	err, fd;
	BD_ADDR	bda;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	err = HCI_ReadBDAddr(fd, &bda);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Local Address: %s\n", bda2str(&bda));
	pause();

	hci_close(fd);
	return 0;
}

int cmd_name(struct command *cmd)
{
	int	err, fd;
	char	name[248];

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	if (__argv[optind] == NULL) {
		// read name
		err = HCI_ReadLocalName(fd, name);
	} else {
		strncpy(name, __argv[optind], 248);
		err = HCI_ChangeLocalName(fd, name);
	}
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	if (__argv[optind] == NULL) {
		// read name
		printf("Name: %s\n", name);
	}

	hci_close(fd);
	return 0;
}

int cmd_scan(struct command *cmd)
{
	int	err, fd, flag;
	__u8	mode = HCI_SCAN_OFF;
	char	*param;

	if (!__argv[optind]) {
		printf("scan mode missed\n");
		return -1;
	}
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	while (__argv[optind]) {
		param = __argv[optind];
		if (param[0] == '+') flag = 1, param++;
		else if (param[0] == '-') flag = 0, param++;
		else flag = 1;

		if (strcmp(param, "disc") == 0)
			flag ? (mode |= HCI_SCAN_INQUIRY) : (mode &= ~HCI_SCAN_INQUIRY);
		else if (strcmp(param, "conn") == 0)
			flag ? (mode |= HCI_SCAN_PAGE) : (mode &= ~HCI_SCAN_PAGE);

		optind++;
	}
	err = HCI_WriteScanEnable(fd, mode);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

int cmd_pincode(struct command *cmd)
{
	int		err;
	BD_ADDR		bda;
	int		Length;
	__u8		Code[16];

	if (cmd->cmd == 1 && (__argv[optind] == NULL || __argv[optind+1] == NULL)) {
		printf("Parameters missed\n");
		return -1;
	}

	if (__argv[optind]) {
		if (strcasecmp(__argv[optind], "default") == 0)
			memset(&bda, 0, 6);
		else {
			err = btdev_get_bda(&bda, __argv[optind]);
			if (err) {
				printf("Incorrect address given\n");
				return 1;
			}
		}
		if (cmd->cmd == 1) {
			Length = strlen(__argv[optind+1]);
			strncpy((char*)Code, __argv[optind+1], 16);
			err = hci_add_pin(&bda, Length, Code);
		} else
			err = hci_remove_pin(&bda);
	} else
		err = hci_remove_pin(NULL);

	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	return err;
}

int cmd_rmkey(struct command *cmd)
{
	char		*peer =__argv[optind];
	int		err, i;
	BD_ADDR		bda;
	btdev_struct	*btdev;

	btdev_cache_reload();
	if (peer) {
		err = btdev_get_bda(&bda, peer);
		if (err) {
			fprintf(stderr, "Incorrect address given\n");
			return 1;
		}
		err = hci_remove_key(&bda);
		btdev = btdev_cache_lookup(&bda);
		if (btdev)
			btdev->flags &= ~BTDEV_KEY;
	} else {
		err = hci_remove_key(NULL);
		/* remove all keys */
		for (i = 0; (btdev = s_list_nth_data(devcache.head, i)); i++)
			btdev->flags &= ~BTDEV_KEY;
	}
	btdev_cache_save();
	return err;
}

int cmd_security(struct command *cmd)
{
	int	err, fd, sec_flags;

	if (!__argv[optind]) {
		print_usage(cmd);
		return 1;
	}
	if (!str2mask(sec_mode_map, argv2str(__argv + optind), &sec_flags)) {
		printf("format error\n");
		return -1;
	}
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	err = HCI_WriteSecurityMode(fd, sec_flags);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	close(fd);
	return 0;
}



int cmd_remotename(struct command *cmd)
{
	int		err, fd;
	BD_ADDR		bda;
	INQUIRY_ITEM	dev;
	char		Name[248];

	if (__argv[optind] == NULL) {
		printf("Bluetooth address missed\n");
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return fd;
	}

	dev.bda = bda;
	dev.PS_Repetition_Mode = 0x00;
	dev.PS_Mode = 0x00;
	dev.Clock_Offset = 0x00;
	err = HCI_RemoteNameRequest(fd, &dev, Name);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	printf("Remote name: %s\n", Name);
	return 0;
}

int cmd_role(struct command *cmd)
{
	int	err, fd, role_flags = 0;
	
	if (__argv[optind] == NULL) {
		print_usage(cmd);
		return -1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	if (strcmp(__argv[optind++], "deny") == 0)
		role_flags |= HCI_ROLE_DENY_SWITCH;

	if (strcmp(__argv[optind++], "master") == 0)
		role_flags |= HCI_ROLE_BECOME_MASTER;

	err = hci_set_role(fd, role_flags);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

int cmd_class(struct command *cmd)
{
	int		err, fd;
	__u32		cod;
	__u32		value;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	if (__argv[optind] == NULL) {
		// read name
		err = HCI_ReadClassOfDevice(fd, &cod);
		if (!err) {
			char	buf[80];
			parse_cod(buf, cod);
			printf("Class: 0x%06X, %s\n", cod, buf);
		}
	} else {
		if (strstr(__argv[optind], "0x")) {
			err = sscanf(__argv[optind], "%x ", &value);
			if (err > 0)
				cod = value;
		} else {
			// parse command line
			err = str2cod(argv2str(__argv + optind), &cod);
			if (err) {
				printf("bad arguments\n");
				return -1;
			}
		}
		err = HCI_WriteClassOfDevice(fd, cod);
	}
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

int cmd_auth(struct command *cmd)
{
	int		err, fd;
	int		handle;
	BD_ADDR		bda;


	if (__argv[optind] == NULL) {
		print_usage(cmd);
		return -1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind]);
	if (err) {
		printf("Wrong address format\n");
		return 1;
	}
	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}
	err = HCI_AuthenticationRequested(fd, handle);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

/*
 * L2CAP
 */

void print_data(__u8 *p, int len)
{
	int	i,j;
	char 	buf[81];

	if (len > 100)
		return;
	for (i = 0; i < len; i++) {
		if ( ((i % 16) == 0) && i > 0)
			printf("%s\n", buf);
		j = (i % 16) * 3;
		sprintf( &(buf[j]), "%02x ", p[i]);
	}
	if (len)
		printf("%s\n", buf);
}


int cmd_ping(struct command *cmd)
{
	int		err, fd, i;
	BD_ADDR		bda;
	struct sockaddr_affix	saddr;
	int		size;
	__u8		*data;

	if (__argv[optind] == NULL || __argv[optind+1] == NULL) {
		printf("parameters missing\n");
		print_usage(cmd);
		return 1;
	}

	err = btdev_get_bda(&bda, __argv[optind++]);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}
	printf("Connecting to host %s ...\n", bda2str(&bda));

	size = atoi(__argv[optind++]);

	fd = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (fd < 0) {
		printf("Unable to create RFCOMM socket: %d\n", PF_AFFIX);
		return 1;
	}

	saddr.bda = bda;
	saddr.port = 0;
	saddr.devnum = HCIDEV_ANY;
	err = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
	if (err < 0) {
		printf("Unable to connect to remote side\n");
		close(fd);
		return 1;
	}

	data = (__u8*)malloc(size);
	if( data == NULL )
		return 0;

	for(i = 0; i < size; i++)
		data[i] = i;

	printf("Sending %d bytes...\n", size);
	print_data(data, size);
	if (cmd->cmd == 0) {
		err = l2cap_ping(fd, data, size);
		if (err < 0) {
			printf("ping error\n");
			exit(1);
		}
		printf("Received %d bytes back\n", err);
		print_data(data, err);
	} else if (cmd->cmd == 1) {
		struct timeval	tv_start, tv_end;
		long int	sec, rsec;
		long int	usec, rusec;
		double		speed;

		gettimeofday(&tv_start, NULL);
		err = l2cap_singleping(fd, data, size);
		if (err < 0) {
			printf("ping error\n");
			exit(1);
		}
		printf("flushing...\n");
		err = l2cap_flush(fd);
		if (err < 0) {
			printf("flush error\n");
			exit(1);
		}
		gettimeofday(&tv_end, NULL);			

		sec = tv_end.tv_sec - tv_start.tv_sec;
		usec = (1000000 * sec) + tv_end.tv_usec - tv_start.tv_usec;
		rsec = usec/1000000;
		rusec = (usec - (rsec * 1000000))/10000;
		speed = (double)(size)/((double)(rsec) + (double)(rusec)/100);

		printf("%d bytes sent in %ld.%ld secs (%.2f B/s)\n", size, rsec, rusec, speed);
	}

	close(fd);
	return 0;
}

void sig_handler(int sig)
{
}

int cmd_periodic_inquiry(struct command *cmd)
{
	int		fd;
	unsigned char	buf[HCI_MAX_EVENT_SIZE];
	unsigned int	max_period_len, min_period_len, length;
	int		err;
	__u8		num;
	struct Inquiry_Result_Event	*ir = (void*)buf;
	struct Inquiry_Complete_Event	*ic = (void*)buf;

	/* Default values */
	length = 3;
	min_period_len = 4;
	max_period_len = 5;

	if (cmd->cmd == 0) {
		if (__argv[optind]) {
			sscanf(__argv[optind++], "%d", &max_period_len);
			if(__argv[optind]) {
					sscanf(__argv[optind++], "%d", &min_period_len);
					if(__argv[optind]) {
						sscanf(__argv[optind++], "%d", &length);
					}
			}
		}
	}

	num=0; /* Unlimited */
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	switch (cmd->cmd) {
		case 1:
			{
				err = HCI_ExitPeriodicInquiryMode(fd);
				if (err) {
					fprintf(stderr, "%s\n", hci_error(err));
					exit(1);
				}
				return 0;
			}
	}

	err = HCI_PeriodicInquiryMode(fd, max_period_len, min_period_len, length, num);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}

	hci_event_mask(fd, COMMAND_STATUS_MASK|INQUIRY_RESULT_MASK|INQUIRY_COMPLETE_MASK);
	
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	do {
		err = hci_recv_event(fd, buf, sizeof(buf), 20);
		if (err < 0) {
			//fprintf(stderr, "%s\n", hci_error(err));
			fprintf(stderr, "Exit from Periodic Inquire Mode ...\n");
			HCI_ExitPeriodicInquiryMode(fd);
			exit(1);
		}
		
		if (ir->hdr.EventCode == HCI_E_INQUIRY_RESULT) {
			int	i;
			for (i = 0; i < ir->Num_Responses; i++) {
				printf("Found bda: %s\n", bda2str(&ir->Results[i].bda)); 
				
			}
		}
	} while (1);//ic->hdr.EventCode != HCI_E_INQUIRY_COMPLETE);
	return ic->Status;
}

int cmd_link_info(struct command *cmd)
{
	int		err, fd, interval;
	BD_ADDR		bda;
	__u8		lq;
	__s8		RSSI;
	__s8		cpl;
	int		handle;

	if (__argv[optind] == NULL) {
		printf("parameters missing\n");
		print_usage(cmd);
		return 1;
	}

	err = btdev_get_bda(&bda, __argv[optind++]);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}

	interval = 0; /* No loop */

	if (cmd->cmd == 3) {
		if (__argv[optind] != NULL) {
			sscanf(__argv[optind], "%d", &interval);
			if (interval <= 0 || interval > 100) {
				printf("Interval value 1 - 100\n");
				exit(1);
			}
		}
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	handle = hci_get_conn(fd, &bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}

	switch (cmd->cmd) {
		case 0: /* lq */
			{
				err = HCI_GetLinkQuality(fd, handle, &lq);
				if (err) {
					fprintf(stderr, "%s\n", hci_error(err));
					exit(1);
				}
				printf("Link quality: %d\n", lq);
				break;
			}
		case 1: /* RSSI  dBm = 10*log(P/Pref) Pref = 1 milliwatt */
			{
				err = HCI_ReadRSSI(fd, handle, &RSSI);
				if (err) {
					fprintf(stderr, "%s\n", hci_error(err));
					exit(1);
				}
				printf("RSSI: %d\n", RSSI);
				break;
			}

		case 2: /* Transmit Power Level */
			{
				err = HCI_ReadTransmitPowerLevel(fd, handle, 0, &cpl);
				/* 0 means current transmit power level */
				if (err) {
					fprintf(stderr, "%s\n", hci_error(err));
					exit(1);
				}
				printf("Current Transmit Power Level: %d\n", cpl);
				break;
			}


		case 3: /* All HCI info parameters */
			{
				do {
					err = HCI_ReadRSSI(fd, handle, &RSSI);
					if (err) {
						fprintf(stderr, "%s\n", hci_error(err));
						exit(1);
					}

					err = HCI_GetLinkQuality(fd, handle, &lq);
					if (err) {
						fprintf(stderr, "%s\n", hci_error(err));
						exit(1);
					}
					
					err = HCI_ReadTransmitPowerLevel(fd, handle, 0, &cpl);
					/* 0 means current transmit power level */
					if (err) {
						fprintf(stderr, "%s\n", hci_error(err));
						exit(1);
					}

					printf("RSSI: %d, LQ: %d, CTPL: %d\n", RSSI, lq, cpl);
					sleep(interval);
				} while (interval);
				break;
			}


	}

	//pause();

	hci_close(fd);
	return 0;
}

int cmd_switch(struct command *cmd)
{
	int	err, fd;
	int	role;
	BD_ADDR	bda;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	err = btdev_get_bda(&bda, __argv[optind++]);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}

	if (cmd->cmd == 1) {
		/* policy */
		int	handle;
		int	policy;
		
		handle = hci_get_conn(fd, &bda);
		if (handle < 0) {
			printf("Connection not found\n");
			return 1;
		}
		if (__argv[optind]) {
			err = sscanf(__argv[optind++], "%x", &policy);
			if (err <= 0)
				policy = 0x0001;
		} else
			policy = 0x0001;

		err = HCI_WriteLinkPolicySettings(fd, handle, policy);
	} else {
		role = atoi(__argv[optind++]);
		err = HCI_SwitchRole(fd, &bda, role);
	}

	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}


int cmd_pkttype(struct command *cmd)
{
	int	err, value, fd;
	
	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	if (__argv[optind] == NULL)
		value = 0; //set default
	else {
		if (__argv[optind][0] == '0')
			err = sscanf(__argv[optind], "%x", &value);
		else {
			str2mask(pkt_type_map, argv2str(__argv + optind), &value);
		}
	}

	err = hci_set_pkttype(fd, value);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;

}

int cmd_control(struct command *cmd)
{
	int	err = 0, fd;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	switch (cmd->cmd) {
		case 0:	/* page_to */
			{
				__u16	timeo;

				if (__argv[optind] == NULL) {
					// read name
					err = HCI_ReadPageTimeout(fd, &timeo);
				} else {
					sscanf(__argv[optind], "%hu", &timeo);
					printf("Timeo: %u\n", timeo);
					err = HCI_WritePageTimeout(fd, timeo);
				}
				if (!err && __argv[optind] == NULL) {
					// read name
					printf("Timeo: %u\n", timeo);
				}
			}
			break;

		case 1:	/* hdisc */
			{
				struct sockaddr_affix	sa;;

				if (__argv[optind] == NULL) {
					printf("Bluetooth address missed\n");
					return -1;
				}
	
				if (strstr(__argv[optind], "0x")) {
					err = sscanf(__argv[optind++], "%hx", &sa.port);
					if (err <= 0) {
						printf("format error\n");
						return 1;
					}
				} else {
					err = btdev_get_bda(&sa.bda, __argv[optind++]);
					if (err) {
						printf("Incorrect address given\n");
						return 1;
					}
				}
				printf("Disconnecting %s ...\n", bda2str(&sa.bda));
				err = hci_disconnect(&sa);
			}
			break;

		case 2:	/* hconnect */
			{
				int			err, sock;
				struct sockaddr_affix	sa;
	
				if (__argv[optind] == NULL) {
					printf("Bluetooth address missed\n");
					return -1;
				}
		
				err = btdev_get_bda(&sa.bda, __argv[optind++]);
				if (err) {
					printf("Incorrect address given\n");
					return -1;
				}

				sock = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_HCIACL);
				if (sock < 0) {
					printf("unable to create affix socket\n");
					return -1;
				}
				sa.family = PF_AFFIX;
				sa.devnum = HCIDEV_ANY;
	
				err = connect(sock, (struct sockaddr*)&sa, sizeof(sa));
				if (err) {
					printf("unable to create connection\n");
					return -1;
				}

				return 0;

				/*
				INQUIRY_ITEM	dev;
				
				err = btdev_get_bda(&dev.bda, __argv[optind++]);
				if (err) {
					printf("Incorrect address given\n");
					return 1;
				}
				dev.PS_Repetition_Mode = 0x00;
				dev.PS_Mode = 0x00;
				dev.Clock_Offset = 0x00;
				err = __HCI_CreateConnection(fd, &dev, 
						HCI_PT_DM1|HCI_PT_DH1|HCI_PT_DM3|HCI_PT_DH3|HCI_PT_DM5|HCI_PT_DH5, 0); */
			}
			break;
		case 3:	/* hpkt_type */
			{
				int	handle, type;
				BD_ADDR	bda;

				if (strstr(__argv[optind], "0x")) {
					err = sscanf(__argv[optind++], "%x", &handle);
					if (err <= 0) {
						printf("format error\n");
						return 1;
					}
				} else {
					err = btdev_get_bda(&bda, __argv[optind++]);
					if (err) {
						printf("Incorrect address given\n");
						return 1;
					}
					handle = hci_get_conn(fd, &bda);
					if (handle < 0) {
						printf("Connection not found\n");
						return 1;
					}
				}
				if (__argv[optind]) {
					err = sscanf(__argv[optind++], "%x", &type);
					if (err <= 0)
						type = HCI_PT_DM1|HCI_PT_DH1|HCI_PT_DM3|HCI_PT_DH3|HCI_PT_DM5|HCI_PT_DH5;
				} else
					type = HCI_PT_DM1|HCI_PT_DH1|HCI_PT_DM3|HCI_PT_DH3|HCI_PT_DM5|HCI_PT_DH5;
					
				printf("Changing pkt_type, handle %#hx ...\n", handle);
				err = HCI_ChangeConnectionPacketType(fd, handle, type);
			}
		case 4:	/* read broadcast retransmissions */
			{
				__u8 Num;

				err = HCI_Read_Num_Broadcast_Retransmissions(fd,&Num);
				printf("\nRetransmissions : %d\n", Num);
			}
			break;
		case 5:	/* write broadcast retransmissions */
			{
				int num = 0;

				if (strstr(__argv[optind], "0x")) {
					err = sscanf(__argv[optind++], "%x", &num);
					if (err <= 0) {
						printf("format error\n");
						return 1;
					}
				}
				err = HCI_Write_Num_Broadcast_Retransmissions(fd,num);
			}
			break;
		default:
			break;
	}
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

int cmd_pair(struct command *cmd)
{
	int			err, fd, sock;
	struct sockaddr_affix	sa;
	int			handle;

	err = btdev_get_bda(&sa.bda, __argv[optind++]);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	sock = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_HCIACL);
	if (sock < 0) {
		printf("unable to create affix socket\n");
		return -1;
	}
	sa.family = PF_AFFIX;
	sa.devnum = HCIDEV_ANY;
	
	err = connect(sock, (struct sockaddr*)&sa, sizeof(sa));
	if (err) {
		printf("unable to create connection\n");
		return -1;
	}

	handle = hci_get_conn(fd, &sa.bda);
	if (handle < 0) {
		printf("Connection not found\n");
		return 1;
	}
	hci_remove_key(&sa.bda);
	err = HCI_AuthenticationRequested(fd, handle);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	printf("Pairing completed: %s\n", err?"error":"successful");
	close(sock);
	hci_close(fd);

	return 0;
}


int cmd_reset(struct command *cmd)
{
	int	fd, err;

	fd = hci_open(btdev);
	if (fd < 0) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}
	err = HCI_Reset(fd);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}


int cmd_debug(struct command *cmd)
{
	int	err, flag, i, addmod;
	__u32	dbmask;
	char	*param;

	err = hci_get_dbmask(&dbmask);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	addmod = dbmask;
	while (__argv[optind]) {
		param = __argv[optind];
		if (param[0] == '+') flag = 1, param++;
		else if (param[0] == '-') flag = 0, param++;
		else flag = 1;

		for (i = 0; debug_flags_map[i].match; i++) {
			if (strcasecmp(param, debug_flags_map[i].match) == 0)
				break;
		}
		if (!debug_flags_map[i].match) {
			fprintf(stderr, "invalid argument: %s\n", param);
			return 1;
		}
		flag?(dbmask|=debug_flags_map[i].value):(dbmask&=~debug_flags_map[i].value);
		if (!addmod && flag == 1 && (dbmask & DBALLMOD)) {
			dbmask |= DBALLDETAIL;
			addmod = 1;
		}
		optind++;
	}
	err = hci_set_dbmask(dbmask);
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	return 0;
}

int print_devinfo(char *dev)
{
	int		err = 0, fd;
	char		name[248], buf[80];
	__u32		cod;
	__u8		val;
	__u8		hci_version, lmp_version;
	__u16		hci_revision, manf_name, lmp_subversion;
	__u64		lmp_features;
	__u16		acl_len, acl_num, sco_num;
	__u8		sco_len;
	struct hci_dev_attr	da;


	err = hci_get_attr(dev, &da);
	if (err < 0) {
		printf("Unable to get attr: %s\n", dev);
		return err;
	}

	printf("%s\t%s\n", da.name, bda2str(&da.bda));
	if (!verboseflag) {
		printf("\tFlags: %s %s %s\n", 
				(da.flags & HCI_FLAGS_UP)?"UP":"DOWN", 
				(da.flags & HCI_FLAGS_SCAN_INQUIRY)?"DISC":"\b",
				(da.flags & HCI_FLAGS_SCAN_PAGE)?"CONN":"\b");
		printf("\tRX: acl:%lu sco:%lu event:%lu bytes:%lu errors:%lu dropped:%lu\n",
				da.stats.rx_acl, da.stats.rx_sco, da.stats.rx_event, da.stats.rx_bytes,
				da.stats.rx_errors, da.stats.rx_dropped);
		printf("\tTX: acl:%lu sco:%lu cmd:%lu bytes:%lu errors:%lu dropped:%lu\n",
				da.stats.tx_acl, da.stats.tx_sco, da.stats.tx_cmd, da.stats.tx_bytes, 
				da.stats.tx_errors, da.stats.tx_dropped);
		mask2str(sec_mode_map, buf, da.flags & ~(HCI_SECURITY_AUTH|HCI_SECURITY_ENCRYPT));
		printf("\tSecurity: %s [%cauth, %cencrypt]\n", buf, 
				(da.flags & HCI_SECURITY_AUTH)?'+':'-',
				(da.flags & HCI_SECURITY_ENCRYPT)?'+':'-'); 
		mask2str(pkt_type_map, buf, da.pkt_type);
		printf("\tPackets: %s\n", buf);

		printf("\tRole: %s, ", (da.flags & HCI_ROLE_DENY_SWITCH)?"deny switch":"allow switch");
		printf("%s\n", (da.flags & HCI_ROLE_BECOME_MASTER)?"become master":"remain slave");
		printf("\n");
		return 0;
	}

	if (!(da.flags & HCI_FLAGS_UP)) {
		printf("\tDevice is down\n\n");
		return 0;
	}

	fd = hci_open(dev);
	if (fd < 0) {
		printf("Unable to open: %s\n\n", dev);
		return 0;
	}

	err = HCI_ReadLocalName(fd, name);
	if (err)
		goto exit;
	printf("\tName: \"%s\"\n", name);

	err = HCI_ReadClassOfDevice(fd, &cod);
	if (err)
		goto exit;
	parse_cod(buf, cod); 
	printf("\tClass: 0x%06X, %s\n", cod, buf);

	err = HCI_ReadScanEnable(fd, &val);
	if (err)
		goto exit;
	printf("\tScan Mode: %s, %s\n", (val&0x01)?"discoverable":"non-discoverable",
			(val&0x02)?"connectable":"non-connectable");

	mask2str(sec_mode_map, buf, da.flags & ~(HCI_SECURITY_AUTH|HCI_SECURITY_ENCRYPT));
	printf("\tSecurity: %s [%cauth, %cencrypt]\n", buf, 
			(da.flags & HCI_SECURITY_AUTH)?'+':'-',
			(da.flags & HCI_SECURITY_ENCRYPT)?'+':'-'); 
	
	mask2str(pkt_type_map, buf, da.pkt_type);
	printf("\tPackets: %s\n", buf);

	printf("\tRole: %s, ", (da.flags & HCI_ROLE_DENY_SWITCH)?"deny switch":"allow switch");
	printf("%s\n", (da.flags & HCI_ROLE_BECOME_MASTER)?"become master":"remain slave");

	err = HCI_ReadLocalVersionInformation(fd, &hci_version, &hci_revision, &lmp_version,
			&manf_name, &lmp_subversion);
	if (err)
		goto exit;
	printf("\tBaseband:\n\t\tManufacture: %s, id: %d\n" 
			"\t\tHCI Version: %d, HCI Revision: %d\n"
			"\t\tLMP Version: %d, LMP Subversion: %d\n"
			"\t\tFeatures: 1.%d compliant\n", 
			(manf_name<NUM_LMPCOMPID)?lmp_compid[manf_name]:"Unknown", manf_name,
			hci_version, hci_revision, lmp_version, lmp_subversion, lmp_version);

	// Read buffer information
	err = HCI_ReadBufferSize(fd, &acl_len, &sco_len, &acl_num, &sco_num);
	if (err)
		goto exit;
	printf("\tBuffers:\n");
	printf("\t\tACL: %d x %d bytes\n", acl_num, acl_len);
	printf("\t\tSCO: %d x %d bytes\n", sco_num, sco_len);

	// Read Supported Features
	err = HCI_ReadLocalSupportedFeatures(fd, &lmp_features);
	if (err)
		goto exit;
	
	printf("\tSuported features:");
	
	mask2str_comma(packets_features_map, buf, lmp_features);
	printf("\n\t\tPacket types: %s", (lmp_features&HCI_LF_PACKETS)?buf:"none");

	mask2str_comma(radio_features_map, buf, lmp_features);
	printf("\n\t\tRadio features: %s", (lmp_features&HCI_LF_RADIO)?buf:"none");

	mask2str_comma(policy_features_map, buf, lmp_features);
	printf("\n\t\tPolicy: %s", (lmp_features&HCI_LF_POLICY)?buf:"not supported");

	printf("\n\t\tEncryption: %s", (lmp_features&HCI_LF_ENCRYPTION)?"supported":"not supported");

	mask2str_comma(timing_features_map, buf, lmp_features);
	printf("\n\t\tClock modes: %s", (lmp_features&HCI_LF_TIMING)?buf:"none");

	mask2str_comma(audio_features_map, buf, lmp_features);
	printf("\n\t\tAudio: %s", (lmp_features&HCI_LF_AUDIO)?buf:"not supported");
	
	printf("\n\t\tPower Control: %s", (lmp_features&HCI_LF_POWER_CONTROL)?"supported":"not supported");
exit:
	printf("\n\n");
	close(fd);
	if (err){
		fprintf(stderr, "%s\n", hci_error(err));
		return 1;
	}
	return 0;
}

int devinfo(void)
{
	int		err = 0;
	int		i, num;
	struct hci_dev_attr	da;
	int		devs[HCI_MAX_DEVS];

	if (nameset)
		return print_devinfo(btdev);

	num = hci_get_devs(devs);
	if (num < 0) {
		fprintf(stderr, "%s\n", hci_error(num));
		exit(1);
		return num;
	}
	if (num == 0) {
		printf("No Bluetooth Adapters found\n");
		return 0;
	}
	for (i = 0; i < num; i++) {
		err = hci_get_attr_id(devs[i], &da);
		if (err < 0) {
			printf("Unable to get attr: %d\n", devs[i]);
			continue;
		}
		err = print_devinfo(da.name);
		if (err)
			return err;
	}
	return 0;
}

int cmd_test(struct command *cmd)
{
#if 0
	{
		struct Write_Current_IAC_LAP	cmd;
		struct Inquiry			event;
		INQUIRY_ITEM			Item;
		printf("sizeof(lap) = %d, sizeof(cmd) = %d, sizeof(event) = %d, sizeof(Item) = %d\n",
				sizeof(cmd.IAC_LAP[0]), sizeof(cmd), sizeof(event), sizeof(Item));
		printf("bda: %p, cod: %p\n", &Item.bda, &Item.bda+1);
		return 0;
	
	}
#endif
	return 0;
}

int cmd_disabled(int cmd)
{
	printf("\n-->> Command disabled at compilation time. Enable certain configuration options. <<--\n\n");
	return 0;
}

struct command cmds[] = {
	{0, 0, 0, 0, "---->>>> General commands <<<<----\n"},
	{"help", cmd_help, 0, "<command name>"},
	{"debug", cmd_debug, 0, "[+|-][<module>|<detail>|all]",
		"\tmodule:\thcicore|afhci|hcisched|hcimgr|hcilib|hci|\n"
		"\t\tpl2cap|afl2cap|l2cap|\n"
		"\t\tprfcomm|afrfcomm|bty|rfcomm|\n"
		"\t\tpan|\n"
		"\t\tdrv|\n"
		"\t\tallmod\n"
		"\tdetail: ctrl|dump|chardump|parse|fname|func|alldetail\n"
	},
	{"initdev", cmd_initdev, CMD_INITDEV, "<type> [params...]",
	},
	{"up", cmd_initdev, CMD_UPDEV, "", "brings interface up\n"},
	{"down", cmd_initdev, CMD_DOWNDEV, "", "brings interface down\n"},
	{"capture", cmd_capture, 0, "<file> | -", 
		"capture traffic on a HCI interface\n"
		"examples: btctl capture mylog.log\n"
		"          btctl capture - | ethereal -i - -k\n"
	},
	/* security */
	{0, 0, 0, 0, "---->>>> Security commands <<<<----\n"},
	{"security", cmd_security, 0, "<mode>",
		"mode: <open|link|service> [auth] [encrypt]\n",
	},
	{"addpin", cmd_pincode, 1, "[<address>|default] <pin>",
		"add pin code to the driver (used when no btsrv or AFE run)\n"},
		
	{"rmpin", cmd_pincode, 0, "[<address>|default]",
		"remove pin code from the driver\n"},
		
	{"unpair", cmd_rmkey, 0, "[<address>]", "remove pairing with remote device (remove link key)\n",
	},
	{"pair", cmd_pair, 0, "<address>", "start paring with remote device\n"},
	/* HCI */
	{0, 0, 0, 0, "---->>>> HCI commands <<<<----\n"},
	{"bdaddr", cmd_bdaddr, 1, "", "read device address\n"
	},
	{"name", cmd_name, 1, "[<name>]",
		"get or set device name\n"
	},
	{"scan", cmd_scan, 1, "[+|-][disc|conn]"
	},
	{"remotename", cmd_remotename, 1, "<bda>", "resolve remote device name\n"
	},
	{"role", cmd_role, 1, "<allow|deny> <master|slave>"
	},
	{"class", cmd_class, 1, "[<class> | <major> <minor> <service1> ... <servicen>]",
		"\tmajor:\tcomputer|phone|audio\n"
		"\tminor:\tdesktop|laptop|hand|server|lap\n"
		"\tservice:\tnetworking,information,rendring,capturing,audio,transfer,telephony\n"
	},
	{"auth", cmd_auth, 0, "<address>",
		"authenticate remote device\n"
	},
	{"pkt_type", cmd_pkttype, 0, "[<0xXXX> | <mnemonic>]",
		"\tmnemonic: DM1 DH1 DM3 DH3 DM5 DH5 HV1 HV2 HV3>]\n"
	},
	{"page_to", cmd_control, 0, "[to]",
		"get/set page timeout\n"
	},
	{"hdisc", cmd_control, 1, "<handle | address>", "disconnect ACL/SCO link\n"
	},
	{"hconnect", cmd_control, 2, "<address>", "create ACL connection\n"
	},
	{"hpkt", cmd_control, 3, "<address> [pkt_type]", "change pkt_type\n"
	},
	{"readbroad", cmd_control, 4, "", "Read Broadcast Retransmissions\n"
	},
	{"writebroad", cmd_control, 5, "", "Write Broadcast Retransmissions\n"
	},
	{"reset", cmd_reset, 0, "", "reset device\n"
	},
	{"ping", cmd_ping, 0, "<bda> <size>",
		"Send ping packet to remote device\n"
	},
	{"lq", cmd_link_info, 0, "<bda>",
		"Show link quality\n"
	},
	{"rssi", cmd_link_info, 1, "<bda>",
		"Show Received Signal Strength Indication (RSSI)\n"
	},
	{"cpl", cmd_link_info, 2, "<bda>",
		"Show Current Transmit Power Level (CTPL)\n"
	},
	{"linkinfo", cmd_link_info, 3, "<bda> [<interval>]",
		"Show HCI debug dump\n"
	},
	{"pinq_start", cmd_periodic_inquiry, 0, 
		"<length> [<max_period_len> <min_period_len> <len>]",
		"Start periodic inquiry mode\n"
	},
	{"pinq_stop", cmd_periodic_inquiry, 1, "<length>",
		"Stop periodic inquiry mode start\n"
	},
	{"speed", cmd_ping, 1,
	},
	/* UART */
#if defined(CONFIG_AFFIX_UART)
	{0, 0, 0, 0, "---->>>> UART commands <<<<----\n"},
	{"init_uart", cmd_uart, CMD_INIT_UART, "<name> <vendor> [speed] [flags]",
		"open UART interface\n"
		"\tvendor: any|csr|ericsson|...]\n"
	},
	{"open_uart", cmd_uart, CMD_OPEN_UART, "<name> <vendor> [speed] [flags]",
		"open UART interface\n"
		"\tvendor: any|csr|ericsson|...]\n"
	},
	{"close_uart", cmd_uart, CMD_CLOSE_UART, "<name>"
	},
#else
#endif
	/* AUDIO */
#if defined(CONFIG_AFFIX_AUDIO)
	{0, 0, 0, 0, "---->>>> AUDIO commands <<<<----\n"},
	{"path", cmd_audio, 0, "0x00 | 0x01", "set Ericsson SCO data path"
	},
	{"audio", cmd_audio, 1, "on|off [sync | async] [setting <SCO setting>]",
		"enable/disable audio (SCO) support\n"
	},
	{"scoflow", cmd_audio, 2,
	},
	{"addsco", cmd_addsco, 0,
	},
	{"play", cmd_play, 0, "<bda> [<file> | -]",
		"send an audio stream from a file or stdin to remote device\n"
	},
	{"record", cmd_record, 0, "<bda> [<file> | -]",
		"store an audio stream in a file or stdout received from remote device\n"
	},
#endif
	{0, 0, 0, 0, "---->>>> Testing commands <<<<----\n"},
	{"test", cmd_test, 0, },
	{"switch", cmd_switch, 0, },
	{"policy", cmd_switch, 1, },
	/* BLUETOOTH 1.2 */
#if defined(CONFIG_AFFIX_BT_1_2)
	{0, 0, 0, 0, "---->>>> Bluetooth 1.2 commands <<<<----\n"},
	{"concancel", cmd_create_connection_cancel, 0, "<address>", "Cancels a create connection request\n"},
	{"remotenamecancel", cmd_remote_name_cancel, 0, "<address>", " Cancel a Remote Name request\n" },
	{"lmphandle",cmd_lmp_handle, 0, "<bda>", "Gets the LMP Handle from a current connection\n"}, 
	{"ref", cmd_remote_extended_features, 0,"<bda> [page number]"," Read the remote extended features from a current connection\n"},
	{"read_dlps",cmd_link_policy_settings,0,"", "Gives the default link policy settings of the device.\n"},
	{"write_dlps",cmd_link_policy_settings,1,"<link_policy_settings>", "Sets the default link policy settings of the device.\n"},
	{"flow",cmd_flow_specification,0,"<bda> <Flow direction> <Service Type> <Token Rate> <Token Bucket Size> <Peak Bandwidth> <Access Latency>","Sets the flow control parameters for the connection with <bda>\n"},
	{"setafh",cmd_afh_host_channel,0,"<channels 0-31> <channels 32-63> <channels 64-79>","Sets the AFH host channels classification.\n"},
	{"read_ist",cmd_inquiry_scan_type,0,"", "Gives the current inquiry scan type of the device.(0x00 Mandatory: Standard Scan, 0x01 Optional:Interlaced Scan)\n"},
	{"write_ist",cmd_inquiry_scan_type,1,"0x00|0x01", "Sets the current inquiry scan type of the device. (0x00 Mandatory: Standard Scan, 0x01 Optional:Interlaced Scan)\n"},
	{"read_inquiry_mode",cmd_inquiry_mode,0,"", "Returns the current inquiry mode of the device.(0x00:Standard Inquiry Result Event, 0x01:Inquiry Result format with RSSI)\n"},
	{"write_inquiry_mode",cmd_inquiry_mode,1,"0x00|0x01", "Sets the current inquiry scan type of the device. (0x00 Mandatory: Standard Scan, 0x01 Optional:Interlaced Scan)\n"},
	{"read_pst",cmd_page_scan_type,0,"", "Returns the current page scan type of the device.(0x00 Mandatory: Standard Scan, 0x01 Optional:Interlaced Scan)\n"},
	{"write_pst",cmd_page_scan_type,1,"0x00|0x01", "Sets the current page scan type of the device.(0x00 Mandatory: Standard Scan, 0x01 Optional:Interlaced Scan)\n"},
	{"read_afh_cam",cmd_afh_cam,0,"", "Returns AFH Channel Assessment Mode value of the device.(0x00 Disable, 0x01 Enable)\n"},
	{"write_afh_cam",cmd_afh_cam,1,"0x00|0x01", "Sets AFH Channel Assessment Mode of the device.(0x00 Disable, 0x01 Enable)\n"},
	{"lef",cmd_local_extended_features,0,"<page number>", "Reads the extended features from the local device.\n"},
	{"clock",cmd_read_clock,0,"[<bda> <which clock>]","Reads the estimate value of the Bluetooth clock.\n"},
	{"afhchannelmap",cmd_afh_channel_map,0,"<bda>","Returns the values for AFH_Mode (0:Disable,1:Enable) and AFH_Channel_Map (if AFH_Mode=1).\n"},
#endif
	{0, 0, 0, NULL}
};

int cmd_help(struct command *cmd)
{
	if (!__argv[optind]) {
		print_usage(cmd);
		exit(0);
	}

	return print_command_usage(cmds, __argv[optind]);
}

void usage(void)
{
	printf("Usage: btctl [-i <name>] [-a] <command> [parameters..]>\n");

	print_all_usage(cmds);

	printf("\n"
		"Notes:\n"
		"\t1. *address* argument can be replaced by cache entry number (btclt list)\n"
		"\n"
		);
}

int main(int argc, char **argv)
{
	int	c, lind, i, err = 0;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"tcp", 0, 0, 't'},
		{"speed", 1, 0, 'r'},
		{"version", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	
	if (affix_init(argc, argv, LOG_USER)) {
		fprintf(stderr, "Affix initialization failed\n");
		return 1;
	}

	for (;;) {
		c = getopt_long(argc, __argv, "+htrvsVam:i:r:", opts, &lind);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage();
				return 0;
				break;
			case 'v':
				verboseflag = 1;
				break;
			case 'i':
				strncpy(btdev, optarg, IFNAMSIZ);
				break;
			case 't':
				linkmode = PF_INET;
				break;
			case 'a':
				showall = 1;
				break;
			case 'r':
				uart_rate = atoi(optarg);
				break;
			case 'm':
				uart_map = optarg;
				break;
			case 'V':
				printf("Affix version: %s\n", affix_version);
				return 0;
				break;
			case ':':
				printf("Missing parameters for option: %c\n", optopt);
				return 1;
				break;
			case '?':
				printf("Unknown option: %c\n", optopt);
				return 1;
				break;
		}
	}
	if (__argv[optind] && sscanf(__argv[optind], "bt%d", &i) > 0) {
		/* interface name */
		sprintf(btdev, "bt%d", i);
		optind++;
		nameset = 1;
	}
	if (__argv[optind] == NULL) {
		err = devinfo();
		goto exit;
	}
	err = call_command(cmds, __argv[optind++]);
exit:
	return err;
}

