/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
   Authors:
   		Christian Plog <plog@informatik.uni-bonn.de>
	emulation part:
		Imre Deak <imre.deak@nokia.com>
   		Dmitry Kasatkin <dmitry.kasatkin@nokia.com>

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
   $Id: btmodem.c,v 1.1 2004/02/26 11:38:35 kassatki Exp $

   Modem multiplexer/emulator

   Fixes:
   		Dmitry Kasatkin		:

*/		

#include <syslog.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <affix/btcore.h>

/* The size of the read buffer: */
#define BUFFER_SIZE 1024

/* Global variables, are used in subroutines: */
char *mddev;		/* name of the serial device for the modem*/
char *bty;		/* name of the serial device emulation on the bluetoothconnection */
char *mdbuf;		/* buffer for data read from the modem but not yet delivered to the bluetooth device */
char *btbuf;		/* buffer for data read from the bluetooth device but not yet delivered to the modem */
int   btfd;		/* the file handle of the bluetooth serial device */
int   mdfd;		/* the file handle of the serial device for the modem */
int   verbose;		/* does the user want comments? */
int   logging;		/* does the user want logs? */
char *logfile;		/* the name of the logfile */
FILE *logfp;		/* file handle for the logfile */
char *logtext;		/* used for construction of the strings to log */
int   silent;		/* whether to be silent on stdout or not */
long  btrxcount;	/* bytes received over bluetooth */
long  bttxcount;	/* bytes send over bluetooth */
long  mdrxcount;	/* bytes received over the modem line */
long  mdtxcount;	/* bytes send over the modem line */
int   crtscts;		/* use RTS/CTS instead of XON/XOFF */

int val;

#define TIMEOUT 30000 

// reads from device, one character at a time with a fixed timeout
int read_line_nonblk(int fd, char *buf, int max_size)
{
        int     i;
        fd_set  fset_rd;
        struct  timeval tv;
        int     res;
 
        if (max_size < 1)
                return -1;
        for (i = 0; i < max_size - 1; i++) {
                FD_ZERO(&fset_rd);
                FD_SET(fd, &fset_rd);
                tv.tv_sec = 0;
                tv.tv_usec = TIMEOUT * 1000;
                if (select(fd + 1, &fset_rd, NULL, NULL, &tv) <= 0)
                        return i;
                if ((res = read(fd, &buf[i], 1)) <= 0)
                        return -1;
                buf[i + 1] = '\0';
		if (buf[i] == '\n' || buf[i] == '\r')
                        return i + 1;
        }
        return -1;
}

void nsleep(int sec, int nsec)
{
	struct timespec req;

	req.tv_sec = sec;
	req.tv_nsec = nsec;	/* 1msec */
	nanosleep(&req, NULL);
}

int write_to_modem(int fd, char *buf, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (write(fd, buf++, 1) != 1)
			return -1;
		nsleep(0, 500000);
	}
	return 0;
}

#define CARRIER_STR "\r\nCARRIER\r\n"
#define CONNECT_STR "\r\nCONNECT 9600\r\n"
#define OK_STR "\r\nOK\r\n"

int emulate(void)
{
	static char 	buf[512];
	struct termios	term;

	openlog(NULL/*"btmodem"*/, 0, LOG_USER);
	if (tcgetattr(1, &term) != 0) {
		syslog(LOG_ERR, "tcgetattr failed.\n");
		goto failed;
	}
	cfmakeraw(&term);
	term.c_cflag |= CRTSCTS;
	if (tcsetattr(1, TCSANOW, &term) != 0) {
		syslog(LOG_ERR, "tcsetattr failed.\n");
		goto failed;
	}
	for (;;) {
		int i;
		int res = read_line_nonblk(0, buf, sizeof(buf));
		if (res <= 0) {
			syslog(LOG_ERR, "input failed.\n");
			goto failed;
		}
		syslog(LOG_DEBUG, "Got %s\n", buf);
		for (i = 0; i < (int)strlen(buf); i++)
			buf[i] = toupper(buf[i]);
		if (strncmp(buf, "AT", 2) == 0) {
			if (strncmp(buf, "ATD", 3) == 0 || strstr(buf, "DT") || strstr(buf, "DP")) {
				nsleep(1, 0);
				syslog(LOG_DEBUG, "Send %s %s", CARRIER_STR, CONNECT_STR);
				if (write_to_modem(1, CARRIER_STR, sizeof(CARRIER_STR) - 1) != 0 ||
				    write_to_modem(1, CONNECT_STR, sizeof(CONNECT_STR) - 1) != 0) {
					syslog(LOG_ERR, "write to modem failed.\n");
					goto failed;
				}
#if 0
				syslog(LOG_DEBUG, "Set CD\n");
				val = TIOCM_CD;
				if (ioctl(1, TIOCMBIS, &val) != 0) {
					syslog(LOG_ERR, "failed to set CD. errno=%d\n", errno);
					goto failed;
				}
#endif
				break;
			} else {
				syslog(LOG_DEBUG, "Send OK\\r");
				if (write_to_modem(1, OK_STR, sizeof(OK_STR) - 1) != 0) {
					syslog(LOG_ERR, "write to modem failed.\n");
					goto failed;
				}
			}
		} else if (strncmp(buf, "CLIENT", 6) == 0) {
			syslog(LOG_DEBUG, "Send CLIENTSERVER\n");
			write(1, "CLIENTSERVER", 12);
			break;
		} else {
			syslog(LOG_DEBUG, "Send ERROR\n");
			write(1, "\r\nERROR\r\n", 9);
		}
	}
	closelog();
	return 0;
failed:
	closelog();
	return -1;
}

/* Writes the string to the log facilities - always expects complete lines! */
void logit(char *logtext)
{
	if (silent != 1) {
		printf(logtext);
		printf("\n");
		fflush(stdout);
	}
	if (logging) {
		fprintf(logfp, "btmodem [%d]: ", getpid());
		fprintf(logfp, logtext);
		fprintf(logfp, "\n");
		fflush(logfp);
	}
}

/* Frees all used ressources: */
void free_ressources(void)
{
	if (verbose)
		logit("Freeing buffer for modem data");
	if (mdbuf)
		free(mdbuf);
	if (verbose)
		logit("Freeing buffer for bluetooth data");
	if (btbuf)
		free(btbuf);
	if (verbose)
		logit("Closeing modem device");
	if (mdfd)
		close(mdfd);
	if (verbose)
		logit("Closing bluetooth serial device");
	if (btfd)
		close(btfd);
	if (verbose)
		logit("Freeing buffer for devicename of modem");
	if (mddev)
		free(mddev);
	if (verbose)
		logit("Freeing buffer for devicename of bluetooth serial device");
	if (bty)
		free(bty);
	if (verbose)
		logit("Freeing buffer for name of log file");
	if (logfile)
		free(logfile);
	if (logtext)
		free(logtext);
	if (verbose)
		logit("Closing log file");
	if (logfp)
		fclose(logfp);
	fflush(stdout);
}

/* Handles breaks: */
void handle_interruption(int sig)
{
	if (sig == 2)
		printf("\nInterrupted\n");
	else
		printf("\nAborted!\n");
	fflush(stdout);
	free_ressources();
	exit(0);
}

/* Shows all the parameters: */
void usage(void)
{
	printf("Syntax: btmodem [-e] [-b bluetooth] [-h] [-l [logfile]] [-m modem] [-q] [-v [-v]] [-x [count]]\n");
	printf("        -e              Modem emulation (use stdin/stdout). -m is not used\n");
	printf("        -b client       The device name of the client serial connection,\n");
	printf("                        usually over a bluetooth connection.\n");
	printf("                        Default: /dev/bty0\n");
	printf("        -h              Use hardware flow control (RTS/CTS) instead of the\n");
	printf("                        default XON/XOFF\n");
	printf("        -l [logfile]    Log in a file. The default name for the logfile is\n");
	printf("                        /var/log/btmodem\n");
	printf("        -m modem        The device name of the serial port on which the modem\n");
	printf("                        is connected. The defualt is: /dev/ttyS0\n");
	printf("        -q              Quiet mode - do not show any messages on stdout.\n");
	printf("                        This has no influence on the information written in\n");
	printf("                        the logfile\n");
	printf("        -v              Verbose mode: Show a lot of information as output on\n");
	printf("                        stdout and in the logfile if logging is activated\n");
	printf("                        REMARK: usually only important status messages are\n");
	printf("                                shown.\n");
	printf("                        WARNING: if both -q and -v are set, -q takes\n");
	printf("                                 precedence!\n");
	printf("        -v -v           Higher verbosity: Show Even more information,\n");
	printf("                        including all data transmitted in verbatim form\n");
	printf("        -x [count]      Show the data as hexdump (always, regardless of\n");
	printf("                        verbosity. If the quiet mode is set, don't show the\n");
	printf("                        hexdump on the screen). The hexdump is written to the\n");
	printf("                        logfile. If -v -v and -x is set, the data will be\n");
	printf("                        shown twice, once verbatim and once as hexdump. count\n");
	printf("                        is the number of bytes shown in each line.\n");
	printf("                        Default: 16\n");
}

/* Setup for the serial ports: */
void modem_setup(int desc)
{
	struct termios settings;
	int result;

	result = tcgetattr(desc, &settings);
	if (result < 0) {
		perror ("error in tcgetattr");
		return;
	}

	cfmakeraw(&settings);
	if (result < 0) {
		perror ("error in cfmakeraw");
		return;
	}

	settings.c_cflag &= ~(PARENB | CSIZE | CSTOPB);
	settings.c_cflag |= (CS8);
	if (crtscts) {
		settings.c_cflag |= (CRTSCTS);
		settings.c_iflag &= ~(IXON | IXOFF);
		logit("Using hardware flow control");
	} else {
		settings.c_cflag &= ~(CRTSCTS);
		settings.c_iflag |= (IXON | IXOFF);
		logit("Using software flow control");
	}
	settings.c_lflag &= ~(ECHO | ICANON);
	cfsetspeed(&settings, B115200);
	result = tcsetattr (desc, TCSANOW, &settings);
	if (result < 0) {
		perror ("error in tcgetattr");
		return;
	}
}

/* Makes an hexdump out of buffer and prints it with loggit   */
/* each line will contain linewidth bytes with two signs each */
void print_hexdump(unsigned char *buffer, int bufsize, int linewidth, int modemin)
{
	char *helpbuffer;
	int   i, pos;

	if (!linewidth)
		return;

	helpbuffer = (char *)malloc(linewidth * 3 + 1);
	memset(helpbuffer, 0, linewidth * 3 + 1);

	pos = 0;
	if (modemin)
		helpbuffer[0] = '>';
	else
		helpbuffer[0] = '<';
	for (i = 0; i < bufsize; i++) {
		pos++;
		snprintf(helpbuffer + strlen(helpbuffer), linewidth * 3 + 1 - strlen(helpbuffer), "%.2X ", buffer[i]);
		/* If line is long enough, then print the line and start a new one: */
		if (pos == linewidth) {
			pos = 0;
			logit(helpbuffer);
			memset(helpbuffer, 0, linewidth * 3 + 1);
			if (modemin)
				helpbuffer[0] = '>';
			else
				helpbuffer[0] = '<';
		}
	}
	/* If at least one sign is in the buffer, then print it: */
	if (pos > 0)
		logit(helpbuffer);

	free(helpbuffer);
}

int main(int argc, char **argv)
{
	int    param;                   /* Command line option to parse */
	int    endprog;                 /* whether to abort the main loop */
	int    mddata;               /* count of bytes read and buffered from the modem */
	int    btdata;           /* count of bytes read and buffered from the bluetooth device */
	int    mdrxsize;           /* count of bytes now read from the modem */
	int    btrxsize;       /* count of bytes now read from the bluetooth device */
	int    mdtxsize;          /* count of bytes writen to the modem */
	int    bttxsize;      /* count of bytes writen to the bluetooth device */
	int    mdsig;     /* the new signal stats on the modem lines */
	int    oldmdsig;		/* the old signal stats on the modem lines */
	int    btsig; /* the new signal stats on the bluetooth lines */
	int    oldbtsig;		/* the old signal stats on the bluetooth lines */
	int    setmdsig;         /* which signals to set on the modem lines */
	int    setbtsig;     /* which signals to set on the bluetooth lines */
	int    unsetmdsig;       /* which signals to unset on the modem lines */
	int    unsetbtsig;   /* which signals to unset on the bluetooth lines */
	int    maxhandle;               /* the maximum file handle (neccessary for select) */
	int    hexdumplinewidth;        /* number of chars in each line of an hexdump */
	fd_set readfds;
	fd_set writefds;
	struct timeval tv;
	int	emu = 0;

	openlog(NULL/*argv[0]*/, 0, LOG_DAEMON);
	BTINFO("%s %s started.", argv[0], affix_version);

	if (argc == 1) {
		usage();
		return 0;
	}

	/* Initialize all variables: */
	mddev = NULL;
	bty = NULL;
	mdfd = 0;
	btfd = 0;
	endprog = 0;
	mdbuf = NULL;
	btbuf = NULL;
	verbose = 0;
	oldmdsig = 0;
	oldbtsig = 0;
	logging = 0;
	logfile = NULL;
	logfp = 0;
	logtext = 0;
	silent = 0;
	crtscts = 0;
	hexdumplinewidth = 0;
	mdtxcount = mdrxcount = btrxcount = bttxcount = 0;

	/* Read the command line parameters: */
	while ((param = getopt(argc, argv, "eb:hl::m:qvx::?")) != -1) {
		switch (param) {
			case 'e':
				emu = 1;
				break;
			case 'b': /* Get the string with the bluetooth bty: */
				bty = (char *)malloc(strlen(optarg)+1);
				memset(bty, 0, strlen(optarg)+1);
				memcpy(bty, optarg, strlen(optarg));
				break;
			case 'h':
				crtscts = 1;
				break;
			case 'l':
				logging = 1;
				if (optarg != 0) {
					logfile = (char *)malloc(strlen(optarg)+1);
					memset(logfile, 0, strlen(optarg)+1);
					memcpy(logfile, optarg, strlen(optarg));
				} else {
					logfile = (char *)malloc(19);
					memset(logfile, 0, 19);
					memcpy(logfile, "/var/log/btmodem", 18);
				}
				break;
			case 'm': /* Get the string with the modem tty: */
				mddev = (char *)malloc(strlen(optarg)+1);
				memset(mddev, 0, strlen(optarg)+1);
				memcpy(mddev, optarg, strlen(optarg));
				break;
			case 'q':
				silent = 1;
				break;
			case 'v':
				verbose++;
				break;
			case 'x':
				hexdumplinewidth = 16;
				if (optarg != 0) {
					hexdumplinewidth = atoi(optarg);
				}
				break;
			case '?':
			default:
				free_ressources();
				usage();
				exit(1);
		}
	}

	if (emu)
		return emulate();

	logtext = (char *)malloc(BUFFER_SIZE);
	memset(logtext, 0, BUFFER_SIZE);

	/* Open the logfile: */
	if (logging) {
		logfp = fopen(logfile, "a");
		if (logfp == NULL) {
			perror("Unable to open logfile");
			handle_interruption(0);
		}
	}

	/* Set the devicenames to default values, if not yet initialized: */
	if (bty == NULL) {
		bty = (char *)malloc(10);
		memset(bty, 0, 10);
		memcpy(bty, "/dev/bty0", 9);
	}
	if (mddev == NULL) {
		mddev = (char *)malloc(11);
		memset(mddev, 0, 11);
		memcpy(mddev, "/dev/ttyS0", 10);
	}

	snprintf(logtext, BUFFER_SIZE, "Connecting bluetooth serial connection on device %s with modem on device %s",
			bty, mddev);
	logit(logtext);
	memset(logtext, 0, BUFFER_SIZE);

	if (verbose)
		logit("Installing signal handler for interruption");
	signal(SIGINT, &handle_interruption);
	fflush(stdout);

	/* Open the devices */
	btfd = open(bty, O_RDWR);
	mdfd = open(mddev, O_RDWR);
	if (btfd == -1) {
		perror("Unable to open bluetooth serial device");
		handle_interruption(0);
	}
	if (mdfd == -1) {
		perror("Unable to open modem device");
		handle_interruption(0);
	}
	/* Initialize the serial connection to the modem: */
	modem_setup(mdfd);
	modem_setup(btfd);

	snprintf(logtext, BUFFER_SIZE, "Opened devices %s and %s successfully", bty, mddev);
	logit(logtext);
	memset(logtext, 0, BUFFER_SIZE);

	/* Make them nonblocking */

	val = fcntl(mdfd, F_GETFL, 0);
	fcntl(mdfd, F_SETFL, val | O_NONBLOCK);

	val = fcntl(btfd, F_GETFL, 0);
	fcntl(btfd, F_SETFL, val | O_NONBLOCK);


	/* Get the maximal file descriptor for select: */
	if (mdfd > btfd)
		maxhandle = mdfd;
	else
		maxhandle = btfd;

	/* Initialize the buffers: */
	mdbuf = (char *)malloc(BUFFER_SIZE);
	btbuf = (char *)malloc(BUFFER_SIZE);
	memset(mdbuf, 0, BUFFER_SIZE);
	memset(btbuf, 0, BUFFER_SIZE);
	btdata = 0;
	mddata = 0;
	mdrxsize = 0;
	btrxsize = 0;
	if (verbose > 1)
		logit("Buffers for data created and initialized");

	/* Initialize the bluetooth connection with the modem stat */
	ioctl(mdfd, TIOCMGET, &oldmdsig);
	/* Check DSR stat: */
	snprintf(logtext, BUFFER_SIZE, "Initializing lines on bluetooth connection: DSR: ");
	if ((oldmdsig & TIOCM_DSR) == TIOCM_DSR)	{
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "1, ");
		setbtsig |= TIOCM_DTR;
	} else {
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "0, ");
		unsetbtsig |= TIOCM_DTR;
	}
	/* Check RI stat: */ 
	snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "RI: ");
	if ((oldmdsig & TIOCM_RI) == TIOCM_RI) {
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "1, ");
		setbtsig |= TIOCM_RI;
	} else {
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "0, ");
		unsetbtsig |= TIOCM_RI;
	}
	/* Check CAR stat: */ 
	snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "CAR: ");
	if ((oldmdsig & TIOCM_CAR) == TIOCM_CAR) {
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "1");
		setbtsig |= TIOCM_CAR;
	} else {
		snprintf(logtext+strlen(logtext), BUFFER_SIZE - strlen(logtext), "0");
		unsetbtsig |= TIOCM_CAR;
	}
	if (verbose)
		logit(logtext);
	memset(logtext, 0, BUFFER_SIZE);
	ioctl(btfd, TIOCMBIS, &setbtsig);
	ioctl(btfd, TIOCMBIC, &unsetbtsig);

	ioctl(btfd, TIOCMGET, &oldbtsig);

	/* 
	 * The main loop of the program.
	 * Runs until interupted
	 */

	while (endprog == 0) {

		/* Read the signals on the devices: */
		ioctl(mdfd, TIOCMGET, &mdsig);
		ioctl(btfd, TIOCMGET, &btsig);

		/* If the signals differ from the old ones,
		   check if some signals must be set on the other side: */
		if (btsig != oldbtsig) {
			snprintf(logtext, BUFFER_SIZE, "Line change on bluetooth side: ");
			setmdsig = 0;
			unsetmdsig = 0;
			/* Check if stat of DSR has changed: */
			if ((btsig & TIOCM_DSR) != (oldbtsig & TIOCM_DSR))	{
				snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "DTR -> ");
				if ((btsig & TIOCM_DSR) == TIOCM_DSR) {
					setmdsig |= TIOCM_DTR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "1 ");
				} else {
					unsetmdsig |= TIOCM_DTR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "0 ");
				}
			}
			if (verbose)
				logit(logtext);
			memset(logtext, 0, BUFFER_SIZE);
			ioctl(mdfd, TIOCMBIS, &setmdsig);
			ioctl(mdfd, TIOCMBIC, &unsetmdsig);
			oldbtsig = btsig;
		}
		if (mdsig != oldmdsig) {
			snprintf(logtext, BUFFER_SIZE, "Line change on modemside: ");
			setbtsig = 0;
			unsetbtsig = 0;
			/* Check if DSR has changed: */
			if ((mdsig & TIOCM_DSR) != (oldmdsig & TIOCM_DSR))	{
				snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "DSR -> ");
				if ((mdsig & TIOCM_DSR) == TIOCM_DSR) {
					setbtsig |= TIOCM_DTR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "1 ");
				} else {
					unsetbtsig |= TIOCM_DTR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "0 ");
				}
			}
			/* Check if RI has changed: */ 
			if ((mdsig & TIOCM_RI) != (oldmdsig & TIOCM_RI)) {
				snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "RI -> ");
				if ((mdsig & TIOCM_RI) == TIOCM_RI) {
					setbtsig |= TIOCM_RI;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "1 ");
				} else {
					unsetbtsig |= TIOCM_RI;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "0 ");
				}
			}
			/* Check if CAR has changed: */ 
			if ((mdsig & TIOCM_CAR) != (oldmdsig & TIOCM_CAR)) {
				snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "CAR -> ");
				if ((mdsig & TIOCM_CAR) == TIOCM_CAR) {
					setbtsig |= TIOCM_CAR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "1 ");
				} else {
					unsetbtsig |= TIOCM_CAR;
					snprintf(logtext + strlen(logtext), BUFFER_SIZE - strlen(logtext), "0 ");
				}
			}
			if (verbose)
				logit(logtext);
			memset(logtext, 0, BUFFER_SIZE);
			ioctl(btfd, TIOCMBIS, &setbtsig);
			ioctl(btfd, TIOCMBIC, &unsetbtsig);
			oldmdsig = mdsig;
		}

		/* Handle the data on the serial devices: */
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		/* Check only for data if we can read something: */
		if (btdata < BUFFER_SIZE)
			FD_SET(btfd, &readfds);
		if (mddata < BUFFER_SIZE)
			FD_SET(mdfd, &readfds);

		/* Check if we can write data - as long as we have some: */
		if (mddata > 0)
			FD_SET(btfd, &writefds);
		if (btdata > 0)
			FD_SET(mdfd, &writefds);

		/* Now check the file handles: */
		tv.tv_sec = 0;
		tv.tv_usec = 10000;

		select(maxhandle+1, &readfds, &writefds, NULL, &tv);

		/* First write data, if possible: */
		/* to the modem: */
		if (FD_ISSET(mdfd, &writefds)) {
			/* Try to write, ignore error: */
			if ((mdtxsize = write(mdfd, btbuf, btdata)) == -1) {
				/* NO ERRORHANDLING! */
			} else {
				/* If verbosity is high enough, show the data: */
				if (verbose > 1) {
					snprintf(logtext, BUFFER_SIZE, "Data written to modem");
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
					memcpy(logtext, btbuf, btdata);
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
				}
				/* If asked for, print an hexdump of the data: */
				print_hexdump(btbuf, btdata, hexdumplinewidth, 1);
				/* If only partial written, copy the rest to the beginning: */
				if (mdtxsize != btdata) {
					memmove(btbuf, btbuf + mdtxsize, btdata - mdtxsize);
				}
				/* Decrease the byte count for the buffer: */
				btdata -= mdtxsize;
				mdtxcount += mdtxsize;
			}
		}
		/* to the bluetooth device: */
		if (FD_ISSET(btfd, &writefds)) {
			/* Try to write, ignore error: */
			if ((bttxsize = write(btfd, mdbuf, mddata)) == -1) {
				/* NO ERRORHANDLING! */
			} else {
				/* If verbosity is high enough, show the data: */
				if (verbose > 1) {
					snprintf(logtext, BUFFER_SIZE, "Data written to bluetooth");
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
					memcpy(logtext, mdbuf, mddata);
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
				}
				/* If asked for, print an hexdump of the data: */
				print_hexdump(mdbuf, mddata, hexdumplinewidth, 0);
				/* If only partial written, copy the rest to the beginning: */
				if (bttxsize != mddata) {
					memmove(mdbuf, mdbuf + bttxsize, mddata - bttxsize);
				}
				/* Decrease the byte count for the buffer: */
				mddata -= bttxsize;
				bttxcount += bttxsize;
			}
		}
		/* Now read the data - if there is data and there is space in the buffers: */
		/* from the modem: */
		if (FD_ISSET(mdfd, &readfds)) {
			/* Try to read the data, ignore error: */
			if ((mdrxsize = read(mdfd, mdbuf + mddata, BUFFER_SIZE - mddata)) == -1) {
				/* NO ERRORHANDLING! */
			} else {
				if (verbose > 1) {
					snprintf(logtext, BUFFER_SIZE, "Data read from modem");
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
				}
				mddata += mdrxsize;
				mdrxcount += mdrxsize;
			}
		}
		/* from the bluetooth device: */
		if (FD_ISSET(btfd, &readfds)) {
			/* Try to read the data, ignore error: */
			if ((btrxsize = read(btfd, btbuf + btdata, BUFFER_SIZE - btdata)) == -1) {
				/* NO ERRORHANDLING! */
			} else {
				if (verbose > 1) {
					snprintf(logtext, BUFFER_SIZE, "Data read from bluetooth");
					logit(logtext);
					memset(logtext, 0, BUFFER_SIZE);
				} else if (btrxsize == 0) {
					/* Check for null packets to close the socket: */
					endprog = 1;
				}
				btdata += btrxsize;
				btrxcount += btrxsize;
			}
		}
	}

	logit("Connection closed");
	snprintf(logtext, BUFFER_SIZE, "Modem stats: %ld bytes received, %ld bytes sent", 
			mdrxcount, mdtxcount);
	logit(logtext);
	memset(logtext, 0, BUFFER_SIZE);
	snprintf(logtext, BUFFER_SIZE, "Bluetooth stats: %ld bytes received, %ld bytes sent", 
			btrxcount, bttxcount);
	logit(logtext);
	memset(logtext, 0, BUFFER_SIZE);

	free_ressources();
	return 0;
}


