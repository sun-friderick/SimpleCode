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
   $Id: btctl-audio.c,v 1.38 2004/03/02 15:59:39 kassatki Exp $

   Utility functions to read and parse the BT server config file. 

   Fixes:
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>
*/

#include <affix/config.h>

#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/soundcard.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <fcntl.h>
#include <ctype.h>

#include <affix/btcore.h>

#include "btctl.h"

/*
 * Audio stuff
 */

int cmd_audio(struct command *cmd)
{
	int	fd, value, err = 0;

	__argv = &__argv[optind];

	fd = hci_open(btdev);
	if (fd == -1) {
		printf("Unable to open device %s: %s\n", btdev, strerror(errno));
		return -1;
	}

	switch (cmd->cmd) {
		case 0:
			if (!*__argv)
				value = 0x01; //set default
			else {
				err = sscanf(*__argv, "%d", &value);
				if (err <= 0) {
					printf("input parameter error, using default value...\n");
					value = 0x01;
				}
			}
			err = HCI_EricssonSetSCODataPath(fd, value);
			break;
		case 1:
			{
				__u16	setting = 0xffff;	// default
				int	mode = 0;

				if (!*__argv) {
					err = HCI_ReadVoiceSetting(fd, &setting);
					if (!err)
						printf("Voice setting: %#03hx\n", setting);
					break;
				}
				for (;*__argv; __argv++) {
					if (**__argv == '0') {
						if (sscanf(*__argv, "%x", &err) <= 0) {
							fprintf(stderr, "voice setting invalid\n");
							return 1;
						}
						setting = err;
						printf("setting: %#03hx\n", setting);
					} else if (strcmp(*__argv, "on") == 0)
						mode |= AFFIX_AUDIO_ON;
					else if (strcmp(*__argv, "off") == 0)
						mode &= ~AFFIX_AUDIO_ON;
					else if (strcmp(*__argv, "async") == 0)
						mode |= AFFIX_AUDIO_ASYNC;
					else if (strcmp(*__argv, "sync") == 0)
						mode |= AFFIX_AUDIO_SYNC;
					else if (strncmp(*__argv, "alt", 3) == 0) {
						if (isdigit((*__argv)[3]))
							mode = AFFIX_AUDIO_SETALT(mode, (*__argv)[3] - 0x30);
						else
							mode = AFFIX_AUDIO_SETALT(mode, 0);
					} else {
						printf("unknown option: %s\n", *__argv);
						return 1;
					}
				}
				err = HCI_WriteAudioSetting(fd, mode, setting);
			}
			break;
		case 2:
			{
				__u8	flow;

				if (!*__argv) {
					err = HCI_ReadSCOFlowControlEnable(fd, &flow);
					if (err == 0)
						printf("SCO Flow Control: %d\n", flow);
					break;
				}
				err = sscanf(*__argv, "%d", &value);
				if (err <= 0) {
					value = 0;
					printf("input parameter error, using default value... %d\n", value);
				}
				err = HCI_WriteSCOFlowControlEnable(fd, value);
			}
			break;
	}
	if (err) {
		fprintf(stderr, "%s\n", hci_error(err));
		exit(1);
	}
	hci_close(fd);
	return 0;
}

int audio_connect(void)
{
	struct sockaddr_affix	saddr;
	BD_ADDR		bda;
	int		fd, err, server = 0;
	char		*addr;
	socklen_t	len;

	if (__argv[optind] == NULL) {
		printf("parameters missing\n");
		usage();
		return 1;
	}
	addr = __argv[optind];
	if (__argv[optind][0] == '-') {
		server = 1;
		addr++;
	}

	err = btdev_get_bda(&bda, addr);
	if (err) {
		printf("Incorrect address given\n");
		return 1;
	}
	printf("Connecting to host %s ...\n", addr);

	optind++;

	fd = socket(PF_AFFIX, SOCK_SEQPACKET, BTPROTO_HCISCO);
	if (fd < 0) {
		printf("Unable to create SCO socket: %d\n", PF_AFFIX);
		return fd;
	}

	saddr.devnum = HCIDEV_ANY;
	if (server) {
		int	cfd;
		
		saddr.bda = BDADDR_ANY;
		err = bind(fd, (struct sockaddr*)&saddr, sizeof(saddr));
		if (err) {
			close(fd);
			return err;
		}
		
		err = listen(fd, 5);
		if (err) {
			close(fd);
			return err;
		}
		cfd = accept(fd, (struct sockaddr*)&saddr, &len);
		close(fd);
		if (cfd < 0) {
			printf("Unable to accept remote side\n");
			return cfd;
		}
		fd = cfd;
		
	} else {
		saddr.bda = bda;
		err = connect(fd, (struct sockaddr*)&saddr, sizeof(saddr));
		if (err < 0) {
			printf("Unable to connect to remote side\n");
			return err;
		}
	}
	printf("Connected.\n");
	return fd;
}

int cmd_play(struct command *cmd)
{
	int	rfd, wfd;
	char	buf[256];


	wfd = audio_connect();
	if (wfd < 0)
		return wfd;

	if (!__argv[optind] || __argv[optind][0] == '-')
		rfd = 0;/* stdin */
	else {
		rfd = open(__argv[optind], O_RDONLY);
		if (rfd < 0) {
			printf("Unable to open a file %s\n", __argv[optind]);
			return rfd;
		}
	}
	printf("Playing...\n");
	for (;;) {
		int		wrote, len;
		char const	*ptr = buf;

		len = read(rfd, buf, 256);
		if (len <= 0)
			break;
		while (len) {
			wrote = write(wfd, ptr, len);
			//printf("wrote: %d\n", wrote);
			if (wrote == -1) {
				if (errno == EINTR)
					continue;
				else {
					perror("write() error");
					return -1;
				}
			}
			ptr += wrote;
			len -= wrote;
		}
	}
	pause();
	close(wfd);
	close(rfd);
	return 0;
}

int cmd_record(struct command *cmd)
{
	int	rfd, count, wfd;
	char	buf[256];

	rfd = audio_connect();
	if (rfd < 0)
		return rfd;

	if (!__argv[optind] || __argv[optind][0] == '-')
		wfd = 1;/* stdout */
	else {
		wfd = open(__argv[optind], O_RDWR|O_CREAT|O_TRUNC, 0644);
		if (wfd < 0) {
			printf("Unable to open a file %s\n", __argv[optind]);
			return wfd;
		}
	}
	for (;;) {
		count = recv(rfd, buf, 256, 0);
		if (count == 0 || count == -1)
			break;
		//printf("writing %d bytes...\n", count);
		write(wfd, buf, count);
	}
	close(wfd);
	close(rfd);
	return 0;
}


int cmd_addsco(struct command *cmd)
{
	int	fd;

	fd = audio_connect();
	if (fd < 0)
		return -1;
	pause();
	close(fd);
	return 0;
}


