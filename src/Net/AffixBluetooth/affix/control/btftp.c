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
   $Id: btftp.c,v 1.12 2004/07/19 13:01:44 kassatki Exp $

   btclt - driver control program

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
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <setjmp.h>

#include <stdint.h>
#include <ctype.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <openobex/obex.h>

#include <affix/bluetooth.h>
#include <affix/btcore.h>
#include <affix/obex.h>
#include <affix/utils.h>


static sigjmp_buf	sigj;	// for command line mode
static char		*command;

extern struct command cmds[];


union {
	struct sockaddr_affix	bt;
	struct sockaddr_in	in;
} saddr;

static obexclt_t	*handle = NULL;
static obex_target_t	*target = NULL;
static obex_target_t	browse = {16, "\xF9\xEC\x7B\xC4\x95\x3C\x11\xD2\x98\x4E\x52\x54\x00\xDC\x9E\x09"};

static int		do_push = 0;

int print_speed(char *format, char *name, struct timeval *tv_start)
{
	struct timeval	tv_end;
	long int	sec, rsec;
	long int	usec, rusec;
	long int	size;
	double		speed;

	size = get_filesize(name);
	gettimeofday(&tv_end, NULL);
	sec = tv_end.tv_sec - tv_start->tv_sec;
	usec = (1000000 * sec) + tv_end.tv_usec - tv_start->tv_usec;
	rsec = usec/1000000;
	rusec = (usec - (rsec * 1000000))/10000;
	speed = (double)(size)/((double)(rsec) + (double)(rusec)/100);

	printf(format, size, rsec, rusec, speed);

	return 0;
}

static void set_perm(char *perm, char *str)
{
	int	c;
	
	//printf("In set_perm\n");
	for (;*str; str++) {
		c = tolower(*str);
		//printf("char: %c\n", c);
		switch (c) {
			case 'r':
				perm[1] = 'r';
				break;
			case 'w':
				perm[2] = 'w';
				break;
			case 'd':
				perm[3] = 'd';
			case 'x':
				perm [4] = 'x';
				break;
		}
	}
}

#define PERM_SIZE	6
void print_folder(const char *buf)
{
	char	*next = (char*)buf, *elem, *attrs, *attr, *value;
	char	*size = NULL, *name = NULL;
	int	count = 0;
	char	str[80], perm[PERM_SIZE];

	//printf("buf: %s\n", buf); return;
	while ((elem = xml_element(&next, &attrs))) {
		if (strcmp(elem, "folder-listing") == 0)
			break;
	}

	while ((elem = xml_element(&next, &attrs))) {
		if (strcmp(elem, "/folder-listing") == 0)
			break;
		size = NULL, name = NULL;
		count = 0;
		//printf("element: %s\n", elem);
		//printf("attr left: %s\n", attrs);
		memset(perm, '-', PERM_SIZE);
		perm[PERM_SIZE-1] = '\0';
		if (strcmp(elem, "folder") == 0)
			perm[0] = 'd';
		else if (strcmp(elem, "file") == 0)
			;
		else if (strcmp(elem, "parent-folder") == 0) {
			perm[0] = 'd';
			name = "..";
		} else {
		}

		// get attributes
		while ((attr = xml_attribute(&attrs, &value))) {
			//printf("attr: %s, value: %s\n", attr, value);
			if (strcmp(attr, "user-perm") == 0)
				set_perm(perm, value);
			else if ( name == NULL && strcmp(attr, "name") == 0 )
				name = value;
			else if ( size == NULL && strcmp(attr, "size") == 0 )
				size = value;
		}
		count += sprintf(str+count, "%s\t\t%s\t\t%s", perm,
				size?size:"0", name?name:"<no name>");
		printf("%s\n", str);
	}
}

int obex_setaddress(void)
{
	int		err;

	if (do_push)
		target = NULL;
	else
		target = &browse;

	if (linkmode == PF_AFFIX) {
		struct sockaddr_affix	*sa = &saddr.bt;

		if (!__argv[optind])
			return -1;

		sa->family = PF_AFFIX;
		sa->devnum = HCIDEV_ANY;
		err = btdev_get_bda(&sa->bda, __argv[optind++]);
		if (err) {
			return -1;
		}

		if (__argv[optind] && sscanf(__argv[optind], "%hd", &sa->port) > 0)
			optind++;
		else {
			err = sdp_find_port_by_id(sa, do_push ? SDP_UUID_OBEX_PUSH : SDP_UUID_OBEX_FTP);
			if (err < 0)
				return err;
			sa->port = err;
		}

	} else if (linkmode == PF_INET) {
		if (__argv[optind] == NULL) {
			return -1;
			err = inet_aton("127.0.0.1", &saddr.in.sin_addr);
		} else {
			err =  inet_aton(__argv[optind], &saddr.in.sin_addr);
			if (err == 0) {
				struct hostent	*he;
				he = gethostbyname(__argv[optind]);
				if (he == NULL)
					return -1;
				saddr.in.sin_addr.s_addr = *(uint32_t*)he->h_addr;
			}
			optind++;
		}
		saddr.in.sin_family = PF_INET;
	} else {
	}

	return 0;
}

int _cmd_open(struct command *cmd)
{
	int	err;

	err = obex_setaddress();
	if (err) {
		printf("Address error\n");
		return -1;
	}

	handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
	if (handle == NULL) {
		fprintf(stderr, "Unable to connect: %s\n", obex_error(err));
		return err;
	}
	printf("Connected.\n");
	return 0;
}

int _cmd_close(struct command *cmd)
{
	if (handle) {
		obex_disconnect(handle);
		handle = NULL;
	}

	return 0;
}

int _cmd_ls(struct command *cmd)
{
	int		err;
	char		*buf;
	obex_file_t	*file;

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}

	file = obex_create_file(NULL);
	if (!file)
		return -1;

	err = obex_browse(handle, file->name, __argv[optind]);
	if (err) {
		fprintf(stderr, "Browsing error: %s\n", obex_error(err));
	} else {
		//printf("%s\n", buf);
		buf = obex_map_file(file);
		if (!buf) {
			fprintf(stderr, "%s\n", obex_error(-1));
			obex_destroy_file(file, 1);
			return -1;
		}
		print_folder(buf);
		printf("Command complete.\n");
	}
	obex_destroy_file(file, 1);
	return 0;
}

int _cmd_get(struct command *cmd)
{
	int		err;
	char		*remote, *local;
	struct timeval	tv_start;

	gettimeofday(&tv_start, NULL);

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}
	if (__argv[optind] == NULL) {
		printf("No file name.\n");
		return -1;
	}
	remote = __argv[optind++];
	local = __argv[optind++];		/* may be NULL */
	printf("Transfer started...\n");
	err = obex_get_file(handle, local, remote);
	if (err) {
		fprintf(stderr, "File transfer error: %s\n", obex_error(err));
	} else {
		printf("Transfer complete.\n");
		print_speed("%ld bytes received in %ld.%ld secs (%.2f B/s)\n", local, &tv_start);
	}
	return 0;
}

int _cmd_put(struct command *cmd)
{
	int		err;
	char		*local, *remote;
	struct timeval	tv_start;

	gettimeofday(&tv_start, NULL);

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}
	if (__argv[optind] == NULL) {
		printf("No file name.\n");
		return -1;
	}
	local = __argv[optind++];
	remote = __argv[optind++];	/* may be NULL */
	printf("Transfer started...\n");
	err = obex_put_file(handle, local, remote);
	if (err) {
		fprintf(stderr, "File transfer error: %s\n", obex_error(err));
	} else {
		printf("Transfer complete.\n");
		print_speed("%ld bytes sent in %ld.%ld secs (%.2f B/s)\n", local, &tv_start);
	}
	return 0;
}

int _cmd_push(struct command *cmd)
{
	int		err;
	char		*local, *remote;
	struct timeval	tv_start;

	gettimeofday(&tv_start, NULL);

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}

	if (__argv[optind] == NULL) {
		printf("No file name.\n");
		return -1;
	}
	local = __argv[optind++];
	remote = __argv[optind++];
	printf("Transfer started...\n");
	err = obex_put_file(handle, local, remote);
	if (err) {
		fprintf(stderr, "Object pushing error: %s\n", obex_error(err));
	} else {
		printf("Transfer complete.\n");
		print_speed("%ld bytes sent in %ld.%ld secs (%.2f B/s)\n", local, &tv_start);
	}
	return 0;
}

int _cmd_rm(struct command *cmd)
{
	int	err;
	char	*name;

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}
	if (__argv[optind] == NULL) {
		printf("No file name.\n");
		return -1;
	}
	name = __argv[optind];

	err = obex_remove(handle, name);
	if (err) {
		fprintf(stderr, "File removal error: %s\n", obex_error(err));
	} else
		printf("Command complete.\n");

	return 0;
}

int _cmd_cd(struct command *cmd)
{
	int	err;
	char	*name;

	if (!handle) {
		printf("Not connected.\n");
		return -1;
	}
	name = __argv[optind];
	err = obex_setpath(handle, name);
	if (err) {
		fprintf(stderr, "cd error: %s\n", obex_error(err));
	} else
		printf("Command complete.\n");

	return 0;
}

int _cmd_mkdir(struct command *cmd)
{
	int	err;
	char	*name;

	if (!handle) {
		printf("Not connected.\n");
		return 0;
	}
	name = __argv[optind];
	err = obex_mkdir(handle, name);
	if (err) {
		fprintf(stderr, "mkdir error: %s\n", obex_error(err));
	} else
		printf("Command complete.\n");

	return 0;
}


// primary commands
//
int cmd_open(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		printf("Command available only in FTP mode.\n");
		return -1;
	}

	err = _cmd_open(cmd);

	return err;
}

int cmd_close(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		printf("Command available only in FTP mode.\n");
		return -1;
	}

	err = _cmd_close(cmd);

	return err;
}
	
int cmd_ls(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		err = obex_setaddress();
		if (err) {
			printf("Address error\n");
			return 1;
		}

		handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
		if (handle == NULL) {
			fprintf(stderr, "Connection failed: %s\n", obex_error(err));
			return err;
		}
	}

	err = _cmd_ls(cmd);
	
	if (!promptmode) {
		obex_disconnect(handle);
	}

	return err;
}

int cmd_put(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		err = obex_setaddress();
		if (err) {
			printf("Address error\n");
			return 1;
		}

		if (__argv[optind] == NULL) {
			printf("No file name.\n");
			return 1;
		}

		handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
		if (handle == NULL) {
			fprintf(stderr, "Connection failed: %s\n", obex_error(err));
			return err;
		}
	}

	err = _cmd_put(cmd);

	if (!promptmode) {
		obex_disconnect(handle);
	}

	return err;
}

int cmd_get(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		err = obex_setaddress();
		if (err) {
			printf("Address error\n");
			return 1;
		}

		if (__argv[optind] == NULL) {
			printf("No file name\n");
			return 1;
		}

		handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
		if (handle == NULL) {
			fprintf(stderr, "Connection failed: %s\n", obex_error(err));
			return err;
		}
	}
	
	err = _cmd_get(cmd);

	if (!promptmode) {
		obex_disconnect(handle);
	}

	return err;
}

int cmd_push(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		do_push = 1;
		err = obex_setaddress();
		if (err) {
			printf("Address error\n");
			return -1;
		}
		do_push = 0;

		if (__argv[optind] == NULL) {
			printf("No file name\n");
			return 1;
		}

		handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
		if (handle == NULL) {
			fprintf(stderr, "Connection failed: %s\n", obex_error(err));
			return err;
		}
	}

	err = _cmd_push(cmd);

	if (!promptmode) {
		obex_disconnect(handle);
	}

	return err;
}

int cmd_rm(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		err = obex_setaddress();
		if (err) {
			printf("Address error\n");
			return 1;
		}

		if (__argv[optind] == NULL) {
			printf("No file name\n");
			return 1;
		}

		handle = obex_connect((struct sockaddr_affix*)&saddr, target, &err);
		if (handle == NULL) {
			fprintf(stderr, "Connection failed: %s\n", obex_error(err));
			return err;
		}
	}
	
	err = _cmd_rm(cmd);

	if (!promptmode) {
		obex_disconnect(handle);
	}

	return err;
}

int cmd_cd(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		printf("Command available only in FTP mode.\n");
		return -1;
	}

	err = _cmd_cd(cmd);

	return err;
}

int cmd_mkdir(struct command *cmd)
{
	int	err;

	if (!promptmode) {
		printf("Command available only in FTP mode.\n");
		return -1;
	}

	err = _cmd_mkdir(cmd);

	return err;
}


/* Command line mode */
static inline int white_space(char c)
{
	return (c == ' ' || c == '\t');
}

static inline int end_line(char c)
{
	return (c == '\n' || c == '\0');
}

static inline char *find_non_white(char *str)
{
	while (white_space(*str))
		str++;
	return (!end_line(*str)?str:NULL);
}

static inline char *find_white(char *str)
{
	while (!end_line(*str) && !white_space(*str))
		str++;
	return str;
}

static inline char *find_char(char *str, char c)
{
	while (*str && *str != c)
		str++;
	return (*str?str:NULL);
}

char *get_string(char **str)
{
	char	*first, *next;
	int	quotes = 0;

	if (*str == NULL)
		return NULL;
	first = find_non_white(*str);
	if (first == NULL)
		return NULL;

	if (*first == '"') {
		quotes = 1;
		first++;
	}
	next = first;
	if (quotes) {
		next = find_char(next, '"');
		if (!next) {
			fprintf(stderr, "parse error\n");
			return NULL;
		}
		*next = '\0';
		next++;
	} else {
		next = find_white(next);
		if (*next) {
			*next = '\0';
			next++;
		}
	}

	if (*next)
		next = find_non_white(next);

	*str = next;
	return first;
}


char **parse_command_line(char *str)
{
	static char	*argv[32];	// for 32 arguments
	char	*arg;
	int	i;

	argv[0] = __argv[0];
	for (i = 1; (arg = get_string(&str)) ; i++) {
		argv[i] = arg;
		//printf("__argv[%d]: %s\n", i, arg);
	}
	argv[i] = NULL;
	__argc = i;
	if (i > 1) {
		__argv = argv;
		optind = 2;
		return __argv + 1;
	}
	return NULL;
}

static void signal_handler(int sig)
{
	//BTINFO("Sig handler : %d", sig);
	if (sig == SIGINT) {
		printf("\n");
		siglongjmp(sigj, 0);
	}
	printf("Terminating on signal (%d) ...\n", sig);
	exit(0);
}

#if 0
	len = getline(&lineread, &size, stdin);
	if (len == -1 || !lineread) {
err:
		INTON;
		return NULL;
	}
#endif		

int run_prompt(char *prompt, struct command *cmds)
{
#ifndef HAVE_READLINE_READLINE_H
	size_t	size;
	ssize_t	len;
#endif
	char	*str = NULL, **cmd;
	int	err;

	promptmode = 1;

	/* install signal handlers to unregister services from SDP */
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, signal_handler);		// CTRL-C
	signal(SIGTERM, signal_handler);

	for (;;) {
		sigsetjmp(sigj, 1);
		free(command); command = NULL;
		free(str); str = NULL;
#ifdef HAVE_READLINE_READLINE_H
		str = readline(prompt);
		if (!str)
			break;
		if (str[0])
			add_history(str);
#else
		printf(prompt);
		// get a line
		len = getline(&str, &size, stdin);
		if (len == -1 || !str)
			break;
#endif
		command = strdup(str);
		//printf("got: %s, len: %d\n", str, strlen(str));
		if (str[0] == '!') {
			if (str[1] == '\0')
				err = system("/bin/sh -l");
			else
				err = system(str+1);
			continue;
		}
		cmd = parse_command_line(str);
		if (cmd == NULL)
			continue;
		if (strcasecmp(*cmd, "quit") == 0) {
			printf("Bye!!!\n");
			break;
		} else if (strcasecmp(*cmd, "mode") == 0) {
			if (__argv[optind]) {
				if (strcasecmp(__argv[optind], "tcp") == 0)
					linkmode = PF_INET;
				else
					linkmode = PF_AFFIX;
			}
			printf("Mode: %s\n", linkmode == PF_INET ? "TCP" : "Bluetooth");
		} else if (strcasecmp(*cmd, "?") == 0) {
			print_all_usage(cmds);
		} else {
			err = call_command(cmds, *cmd);
		}
	}
	free(str);
	return 0;
}

int cmd_client(struct command *cmd)
{
	char	__cmd[256];
	char	*ch;

	strcpy(__cmd, progname);
	ch = strrchr(__cmd, '/');
	if (ch) 
		*(ch + 1) = '\0';
	else
		__cmd[0] = '\0';
	strcat(__cmd, "btclt ");
	strcat(__cmd, command);
	return system(__cmd);
}

int cmd_help(struct command *cmd)
{
	if (!__argv[optind]) {
		print_usage(cmd);
		return 0;
	}
	return print_command_usage(cmds, __argv[optind]);
}

void usage(void)
{
	printf("Usage: btftp [-i <name>] [<command> [parameters..]>\n");

	print_all_usage(cmds);

	printf("\n"
		"Notes:\n"
		"\t1. *address* argument can be replaced by cache entry number (btctl list)\n"
		"\t2. *--nosdp* option required to manually define server channel\n"
		"\t3. use *search* command if *browse* returns nothing\n"
		"\t4. argument *channel* is not used in SDP mode\n"
		"\t5. arguments *address* and *channel* are not used with\n"
		"\t   commands: *ls*, *get*, *put*, *rm* in FTP (prompt) mode\n"
		"\n"
		);
}

int cmd_prompt(struct command *_cmd)
{
	int	err;

	printf("Affix version: %s\n", affix_version);
	printf("Welcome to btftp (OBEX) tool. Type ? for help.\n");
	printf("Mode: %s\n", linkmode == PF_INET ? "TCP" : "Bluetooth");

	err = run_prompt("ftp> ", cmds);

	printf("Terminating ...\n");
	return err;
}


struct command cmds[] = {
	{0, 0, 0, 0, "---->>>> General commands <<<<----\n"},
	{"help", cmd_help, 0, "<command name>"},
	{"list", cmd_client, 0, "", "shows know/found devices\n"},
	{"flush", cmd_client, 1, "", "removes all know devices from the cache\n"},
	{"inquiry", cmd_client, 2, "[length]", "search for the device\n" },
	{"discovery", cmd_client, 3, "[length]","search for the devices and resolve their names\n"},
	{"browse", cmd_client, 4, "<address>", "browse for services on remote device\n"},
	{"search", cmd_client, 5, "<address>",
		"search for services on remote device\n"
		"notes: used if browse does not find anything, because\n"
		"some devices does not suport browseing\n"
	},
	/* OBEX */
	{0, 0, 0, 0, "---->>>> OBEX commands <<<<----\n"},
	{"ftp", cmd_prompt, 0, "", "enter interactive mode\n"
	},
	{"open", cmd_open, 0, "<address> [<channel>]",
		"open connection to obex ftp server\n"
	},
	{"close", cmd_close, 0, "", "close connection to obex ftp server\n"
	},
	{"ls", cmd_ls, 0, "[<address> [<channel>]]"
	},
	{"put", cmd_put, 0, "[<address> [<channel>]] <local-file> [remote-file]"
	},
	{"get", cmd_get, 0, "[<address> [<channel>]] <remote-file> [local-file]"
	},
	{"push", cmd_push, 0, "<address> [<channel>] <local-file> [remote-file]"
	},
	{"rm", cmd_rm, 0, "[<address> [<channel>]] <remote-file>"
	},
	{"cd", cmd_cd, 0, "<dir name>"
	},
	{"mkdir", cmd_mkdir, 0, "<dir name>"
	},
	{0, 0, 0, NULL}
};

//int cmdnum = sizeof(cmds)/sizeof(cmds[0]);

void do_exit(void)
{
	if (promptmode)
		cmd_close(0);
}

int main(int argc, char **argv)
{
	int	c, lind, i, err = 0;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"tcp", 0, 0, 't'},
		{"nosdp", 0, 0, 's'},
		{"version", 0, 0, 'V'},
		{0, 0, 0, 0}
	};
	
	if (affix_init(argc, argv, LOG_USER)) {
		fprintf(stderr, "Affix initialization failed\n");
		return 1;
	}

	atexit(do_exit);

	for (;;) {
		c = getopt_long(argc, __argv, "+htvsVi:", opts, &lind);
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
	}
	if (__argv[optind] == NULL) {
		err = cmd_prompt(NULL);
		goto exit;
	}
	err = call_command(cmds, __argv[optind++]);
exit:
	return err;
}

