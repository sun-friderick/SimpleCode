/* 
   Affix - Bluetooth Protocol Stack for Linux
   Copyright (C) 2001 Nokia Corporation
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
   $Id: btutils.c,v 1.10 2004/05/26 10:24:21 kassatki Exp $

   utility functions for Affix

   Fixes: 
   		Dmitry Kasatkin
*/

#include <affix/config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include <affix/btcore.h>
#include <affix/utils.h>


/* speed stuff */
int get_speed(int size, struct timeval *tv_start, struct timeval *tv_end,
		long int *rsec, long int *rusec, double *speed)
{
	long int	sec, usec;

	sec = tv_end->tv_sec - tv_start->tv_sec;
	usec = (1000000 * sec) + tv_end->tv_usec - tv_start->tv_usec;
	*rsec = usec/1000000;
	*rusec = (usec - (*rsec * 1000000))/10000;
	*speed = (double)(size)/((double)(*rsec) + (double)(*rusec)/100);
	return 0;
}


int affix_dup2std(int fd)
{
	if (dup2(fd, 0) < 0 || dup2(fd, 1) < 0) {
		return -1;
	}
	if (fd != 0 && fd != 1)
		close(fd);
	return 0;
}

/* connect stdin with prog's stdout/stderr and stdout
 * with prog's stdin. */
void affix_open_pipes(char *prog)
{
	int from_cld[2] ;		/* from child process */
	int to_cld[2] ;		/* to child process */

	/* create pipes */
	if (pipe(from_cld) == -1) {
		BTERROR("pipe\n") ;
		exit(errno) ;
	}
	if (pipe(to_cld) == -1) {
		BTERROR("pipe\n") ;
		exit(errno) ;
	}

	/* for child process */
	switch (fork()) {
		case 0:			/* this is the child process */
			/* connect stdin to pipe */
			close(0) ;
			close(to_cld[1]) ;
			dup2(to_cld[0], 0) ;
			close(to_cld[0]) ;
			/* connect stdout to pipe */
			close(1) ;
			close(from_cld[0]) ;
			dup2(from_cld[1], 1) ;
			/* connect stderr to pipe */
			close(2) ;
			dup2(from_cld[1], 2) ;
			close(from_cld[1]) ;
			/* call program via sh */
			execl("/bin/sh", "sh", "-c", prog, NULL) ;
			BTERROR("exec /bin/sh\n") ;
			/* terminate parent silently */
			kill(getppid(), SIGUSR1) ;
			exit(255) ;
		case -1:
			BTERROR("fork\n") ;	/* fork failed */
			exit(errno) ;
		default:			/* parent process */
			/* connect stderr to pipe */
			close(0) ;
			close(from_cld[1]) ;
			dup2(from_cld[0], 0) ;
			close(from_cld[0]) ;
			/* connect stderr to pipe */
			close(1) ;
			close(to_cld[0]) ;
			dup2(to_cld[1], 1) ;
			close(to_cld[1]) ;
	}
}

/* remove zombie child processes */
void affix_wait_for_children(void)
{
	int wret, status ;

	while ((wret = waitpid(-1, &status, WNOHANG)) > 0) ;
}

/* IO */

int active_socket = -1;
int readonlyflag = 0;	// read only socket
int writeonlyflag = 0;	// write only socket
int quitflag = 0;
int crlfflag = 0;

/* expand LF characters to CRLF and adjust *sizep */
void affix_add_crs(char *from, char *to, int *sizep)
{
	int countdown ;		/* counter */

	countdown = *sizep ;
	while (countdown) {
		if (*from == '\n') {
			*to++ = '\r' ;
			(*sizep)++ ;
		}
		*to++ = *from++ ;
		countdown-- ;
	}
}

/* strip CR characters from buffer and adjust *sizep */
void affix_strip_crs(char *from, char *to, int *sizep)
{

	int countdown ;		/* counter */

	countdown = *sizep ;
	while (countdown) {
		if (*from == '\r') {
			from++ ;
			(*sizep)-- ;
		} else {
			*to++ = *from++ ;
		}
		countdown-- ;
	}
}

/* write the buffer; in successive pieces, if necessary. */
int affix_do_write(char *buffer, int size, int to)
{
	char buffer2[2 * BUFSIZ] ;	/* expanding lf's to crlf's can
					 * make the block twice as big at most */
	int written ;

	if (crlfflag) {
		if (to == active_socket) {
			affix_add_crs(buffer, buffer2, &size) ;
		} else {
			affix_strip_crs(buffer, buffer2, &size) ;
		}
		buffer = buffer2 ;
	}
	while (size > 0) {
		// written = write(to, buffer2, size) ;
		written = write(to, buffer, size) ;
		if (written == -1) {
			/* this should not happen */
			perror("write") ;
			fprintf(stderr, "%s: error writing to %s\n",
					progname,
					to == active_socket ? "socket" : "stdout") ;
			return -1 ;
		}
		size -= written ;
		buffer += written ;
	}
	return 0;
}

/* read from from, write to to. select(2) has returned, so input
 * must be available. */
int affix_do_read_write(int from, int to)
{
	int size ;
	char input_buffer[BUFSIZ] ;

	if ((size = read(from, input_buffer, BUFSIZ)) == -1) {
		perror("read\n");
		return -1 ;
	}
	if (size == 0) {		/* end-of-file condition */
		if (from == active_socket) {
			/* if it was the socket, the connection is closed */
			if (verboseflag) {
				fprintf(stderr, "connection closed by peer\n") ;
			}
			return -1 ;
		} else {
			if (quitflag) {
				/* we close connection later */
				if (verboseflag) {
					fprintf(stderr, "connection closed\n") ;
				}
				return -1 ;
			} else if (verboseflag) {
				fprintf(stderr, "end of input on stdin\n") ;
			}
			readonlyflag = 1 ;
			return 0;
		}
	}
	return affix_do_write(input_buffer, size, to) ;

}

/* all IO to and from the socket is handled here. The main part is
 * a loop around select(2). */
void affix_do_io(void)
{
	fd_set readfds;
	int fdset_width = 1;
	int selret ;

	if (active_socket > 0)
		fdset_width = active_socket + 1;
	while (1) {			/* this loop is exited sideways */
		/* set up file descriptor set for select(2) */
		FD_ZERO(&readfds) ;
		if (!readonlyflag) {
			FD_SET(0, &readfds) ;
		}
		if (!writeonlyflag) {
			FD_SET(active_socket, &readfds) ;
		}

		do {
			/* wait until input is available */
			selret = select(fdset_width, &readfds, NULL, NULL, NULL) ;
			/* EINTR happens when the process is stopped */
			if (selret < 0 && errno != EINTR) {
				perror("select\n") ;
				exit(1) ;
			}
		} while (selret <= 0) ;

		/* do the appropriate read and write */
		if (FD_ISSET(active_socket, &readfds)) {
			if (affix_do_read_write(active_socket, 1) < 0) {
				break ;
			}
		} else {
			if (affix_do_read_write(0, active_socket) < 0) {
				break ;
			}
		}
	}
}

int sdp_find_port_by_name(struct sockaddr_affix *saddr, char *svc)
{
	int	svc_id, port;

	if (sscanf(svc, "%d", &port) > 0)
		return port;	// numeric port given
	if (bda_zero(&saddr->bda))
		return -1;
#if defined(CONFIG_AFFIX_SDP)
	if (!str2val(sdp_service_map, svc, &svc_id)) {
		fprintf(stderr, "unknown service: %s\n", svc);
		return -1;
	}
	return sdp_find_port(saddr, svc_id);
#else
	return -1;
#endif
}

int sdp_find_port_by_id(struct sockaddr_affix *sa, uint16_t svc_id)
{
#if defined(CONFIG_AFFIX_SDP)
	return sdp_find_port(sa, svc_id);
#else
	return -1;
#endif
}

