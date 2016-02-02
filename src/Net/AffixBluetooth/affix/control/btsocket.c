/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2004 Nokia Corporation
   Original Author: Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

   Partly based on socket.c
	Copyright (C) 1992 by Juergen Nickelsen <nickel@cs.tu-berlin.de>
	Please read the file COPYRIGHT for further details.
   
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
   $Id: btsocket.c,v 1.16 2004/05/26 10:24:21 kassatki Exp $

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

int		nameset;
int		proto = BTPROTO_RFCOMM;
BD_ADDR		bda;


/* global variables */
int forkflag = 0 ;		/* server forks on connection */
int loopflag = 0 ;		/* loop server */
int serverflag = 0 ;		/* create server socket */
int backgflag = 0 ;		/* put yourself in background */
char *pipe_program = NULL ;	/* program to execute in two-way pipe */
int WaitForever = 0;            /* wait forever for socket on connection refused */
int btyflag = 0;

void server(struct sockaddr_affix *sa);
void client(struct sockaddr_affix *sa);
void handle_server_connection(void);

/* perror with progname */
void perror2(char *s)
{
    fprintf(stderr, "%s: ", progname) ;
    perror(s) ;
}

void usage(void)
{
	printf("Usage: btsocket [--l2cap] [--tty] [ -2hbcfqrvw ] [-i <name>] [ -p command ] bda <port | service>\n");
	printf("       btsocket [--l2cap] [--tty] [ -2hbcfqrvw ] [-i <name>] [ -p command ] -s [ -l ] port\n");

	printf("\n"
		"Notes:\n"
		"\t1. *address* argument can be replaced by cache entry number (btclt list)\n"
		"\n"
		);
}

void do_exit(void)
{
}

int main(int argc, char **argv)
{
	int	c, i, lind, err;
	struct sockaddr_affix	saddr;
	int port ;			/* port number for socket */

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"l2cap", 0, 0, '2'},
		{"tty", 0, 0, 't'},
		{"nosdp", 0, 0, 's'},
		{"version", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	
	if (affix_init(argc, argv, LOG_USER)) {
		fprintf(stderr, "Affix initialization failed\n");
		return 1;
	}

	atexit(do_exit);

	/* parse options */
	while ((c = getopt_long(argc, argv, "2thbcflp:qrsvWw?", opts, &lind)) != -1) {
		switch (c) {
			case 't':
				btyflag = 1;
				break;
			case '2':
				proto = BTPROTO_L2CAP;
				break;
			case 'h':
				usage();
				return 0;
				break;
			case 'f':
				forkflag = 1 ;
				break ;
			case 'c':
				crlfflag = 1 ;
				break ;
			case 'w':
				writeonlyflag = 1 ;
				break ;
			case 'p':
				pipe_program = argv[optind - 1] ;
				break ;
			case 'q':
				quitflag = 1 ;
				break ;
			case 'r':
				readonlyflag = 1 ;
				break ;
			case 's':
				serverflag = 1 ;
				break ;
			case 'v':
				verboseflag = 1 ;
				break ;
			case 'l':
				loopflag = 1 ;
				break ;
			case 'b':
				backgflag = 1 ;
				break ;
			case 'W':
				WaitForever = 1 ;
			default:
				printf("Unknown option: %c\n", optopt);
				usage();
				exit(15);
		}
	}
	if (__argv[optind] && sscanf(__argv[optind], "bt%d", &i) > 0) {
		/* interface name */
		sprintf(btdev, "bt%d", i);
		optind++;
		nameset = 1;
	}
	if (argc - optind + serverflag != 2) { /* number of args ok? */
		usage() ;
		exit(15) ;
	}

	/* check some option combinations */
#define senseless(s1, s2) \
	fprintf(stderr, "It does not make sense to set %s and %s.\n", (s1), (s2))

		if (writeonlyflag && readonlyflag) {
			senseless("-r", "-w") ;
			exit(15) ;
		}
	if (loopflag && !serverflag) {
		senseless("-l", "not -s") ;
		exit(15) ;
	}
	if (backgflag && !serverflag) {
		senseless("-b", "not -s") ;
		exit(15) ;
	}
	if (forkflag && !serverflag) {
		senseless("-f", "not -s") ;
	}
	if (btyflag && proto == BTPROTO_L2CAP) {
		senseless("--l2cap", "--tty") ;
		exit(15);
	}

	/* set up signal handling */
	//init_signals() ;

	saddr.family = PF_AFFIX;
	saddr.devnum = hci_devnum(btdev);//HCIDEV_ANY;
	saddr.bda = BDADDR_ANY;

	/* get port number */
	if (!serverflag) {
		// get bda
		err = btdev_get_bda(&saddr.bda, argv[optind++]);
		if (err) {
			fprintf(stderr, "Incorrect address given\n");
			return 1;
		}
	} 

	/* get port number */
	port = sdp_find_port_by_name(&saddr, argv[optind]) ;
	if (port < 0) {
		fprintf(stderr, "%s: unknown service\n", progname) ;
		exit(5) ;
	}
	saddr.port = port;

	/* and go */
	if (serverflag) {
		if (backgflag) {
			affix_background() ;
		}
		server(&saddr) ;
	} else {
		client(&saddr) ;
	}	       
	return 0;
}

int sock2bty(void)
{
	int	line;
	char	dev[32];

	line = rfcomm_open_tty(active_socket, RFCOMM_BTY_ANY);
	close(active_socket);
	if (line < 0) {
		perror2("Unable to bind a socket to virtual port device");
		exit(errno);
	}
	sprintf(dev, "/dev/bty%d", line);
	active_socket = open(dev, O_RDWR);
	if (active_socket < 0) {
		fprintf(stderr, "Unable to open port device %s", dev);
		exit(errno);
	}
	return active_socket;
}

int create_server_socket(struct sockaddr_affix *sa, int queue_length)
{
	int s;

	if ((s = socket(PF_AFFIX, SOCK_STREAM, proto)) < 0) {
		return -1 ;
	}
	if (bind(s, (struct sockaddr *)sa, sizeof(*sa)) < 0) {
		return -1 ;
	}
	if (listen(s, queue_length) < 0) {
		return -1 ;
	}
	return s ;
}


void server(struct sockaddr_affix *sa)
{
	int socket_handle, alen ;

	/* allocate server socket */
	socket_handle = create_server_socket(sa, 1) ;
	if (socket_handle < 0) {
		perror2("server socket") ;
		exit(1) ;
	}
	if (verboseflag) {
		fprintf(stderr, "listening on port %d\n", sa->port) ;
	}

	/* server loop */
	do {
		struct sockaddr_affix sa ;

		alen = sizeof(sa) ;

		/* accept a connection */
		if ((active_socket = accept(socket_handle, (struct sockaddr*) &sa, &alen)) == -1) {
			perror2("accept") ;
		} else {
			if (verboseflag) {
				/* if verbose, get name of peer and give message */
			}
			if (forkflag) {
				switch (fork()) {
					case 0:
						handle_server_connection() ;
						exit(0) ;
					case -1:
						perror2("fork") ;
						break ;
					default:
						close(active_socket) ;
						affix_wait_for_children() ;
				}
			} else {
				handle_server_connection() ;
			}
		}
	} while (loopflag) ;
}


void handle_server_connection()
{
	/* open pipes to program, if requested */
	if (btyflag)
		sock2bty();

	if (pipe_program) {
#if 0
		affix_open_pipes(pipe_program);
#else
		if (affix_dup2std(active_socket) < 0) {
			perror2("Unable to duplicate socket descriptor to stdin/stdout");
			exit(errno);
		}
		execl("/bin/sh", "sh", "-c", pipe_program, NULL) ;
#endif
	}
	/* enter IO loop */
	affix_do_io() ;
	/* connection is closed now */
	close(active_socket) ;
	if (pipe_program) {
		/* remove zombies */
		affix_wait_for_children() ;
	}
}

int create_client_socket(struct sockaddr_affix *sa)
{
	int s ;

	if ((s = socket(PF_AFFIX, SOCK_STREAM, proto)) < 0) { /* get socket */
		return -1 ;
	}
	if (connect(s, (struct sockaddr*)sa, sizeof(*sa)) < 0) {                  /* connect */
		close(s) ;
		return -1 ;
	}
	return s ;
}

void client(struct sockaddr_affix *sa)
{
	/* get connection */
	while (1) {
		active_socket = create_client_socket(sa) ;
		if (active_socket >= 0 || !WaitForever || errno != ECONNREFUSED)
			break;
		sleep( 60 );
	}
	if (active_socket == -1) {
		perror2("client socket") ;
		exit(errno) ;
	} 
	if (verboseflag) {
		fprintf(stderr, "connected to %s port %d\n", bda2str(&sa->bda), sa->port) ;
	}
	
	if (btyflag)
		sock2bty();

	if (pipe_program) {
#if 0
		affix_open_pipes(pipe_program);
#else
		if (affix_dup2std(active_socket) < 0) {
			perror2("Unable to duplicate socket descriptor to stdin/stdout");
			exit(errno);
		}
		execl("/bin/sh", "sh", "-c", pipe_program, NULL) ;
		exit(errno);
#endif
	}
	/* enter IO loop for stdin/stdout */
	affix_do_io() ;
	/* connection is closed now */
	close(active_socket) ;
}

