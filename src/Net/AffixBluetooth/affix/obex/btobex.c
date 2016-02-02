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
   $Id: btobex.c,v 1.28 2004/03/24 19:00:24 kassatki Exp $

   OBEX server application for Affix

*/

#include <affix/config.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <getopt.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>

//#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <stdint.h>

#include <openobex/obex.h>

#include <affix/btcore.h>
#include <affix/obex.h>

char	*obex_root_path = NULL;

char *file_base_name(char *path)
{
	char *pos = strrchr(path, '/');
	return pos ? pos + 1 : path;
}

int change_dir(char *new_dir, int mode)
{
	char	new_path[PATH_MAX + 1], root_path[PATH_MAX + 1];
	char	last_comp[NAME_MAX + 1] = {0, };
	int	create_dir = 0;
	char	*p;

	if (new_dir == NULL || new_dir[0] == '\0')
		return -1;

	DBPRT("Changing directory to: [%s]\n", new_dir);

	if (realpath(new_dir, new_path) == NULL) {
		if ((mode & CHDIR_CREATE_IF_NOT_EXIST) && errno == ENOENT) {
			/*
			 * in case we want to create a non existent directory
			 * we should check the validity of the path without the last component
			 * so save the last component and strip it off
			 * don't strip the last component if it's ".." because this should be
			 * taken into account when checking against obex_root_path..
			 */
			char	buf[PATH_MAX + 1];
			strcpy(buf, new_dir);
			if ((p = strrchr(buf, '/')) == NULL)
				p = buf;
			else
				p++;
			if (strcmp(p, "..") != 0) {
				strcpy(last_comp, p);
				if (p == buf)
					strcpy(buf, "./");
				else
					*p = '\0';
			}
			if (realpath(buf, new_path)) {
				strcat(new_path, "/");
				strcat(new_path, last_comp);
			} else {
				if (!(mode & CHDIR_CREATE_RECURSIVE))
					return -1;
				strcpy(new_path, new_dir);
			}
			create_dir = 1;
		} else
			return -1;
	}
	
	DBPRT("Creating: %s, create: %s\n", new_path, create_dir?"yes":"no");
	if (!realpath(obex_root_path, root_path) || strncmp(new_path, root_path, strlen(root_path)) != 0)
		return -1;

	if (mode & CHDIR_CHECK_ONLY)
		return 0;
			
	if (create_dir)	{
		int	err;
		if (mode & CHDIR_CREATE_RECURSIVE)
			err = rmkdir(new_path, 0700);
		else
			err = mkdir(new_path, 0700);
		if (err && errno != EEXIST)
			return -1;
	}
	if (chdir(new_path) != 0)
		return -1;
		
	return 0;
}


int btobex_put(obexsrv_t *srv, char *file, char *name, char *type, int flags)
{
	char	*p;
	int	fd;

	// if path contains folders check if it is valid
	if ((p = strrchr(name, '/')) != NULL) {
		char	dir[PATH_MAX + 1];
		
		strncpy(dir, name, p - name);
		dir[p - name] = '\0';
		if (change_dir(dir, CHDIR_CHECK_ONLY) != 0) {
			BTERROR(" Invalid path: %s", dir);
			return -1;
		}
	}
	if (flags & 0x02) {// delete
		unlink(name);
	} else if (flags & 0x01) {// create empty object
		fd = open(name, O_CREAT, 0600);
		close(fd);
	} else {
		char	cmd[PATH_MAX];
		sprintf(cmd, "/bin/mv \"%s\" \"%s\"", file, name);
		fd = system(cmd);
		if (fd) {
			BTERROR("failed: system(\"%s\") = %d\n", cmd, fd);
		}
	}
	return 0;
}


int btobex_get_listing(obexsrv_t *srv, char *name)
{
	static char header[] = 
		"<?xml version=\"1.0\"?>\n"
		"<!DOCTYPE folder-listing SYSTEM \"obex-folder-listing.dtd\">\n"
		"<folder-listing version=\"1.0\">\n";
	static char trailer[] =
		"</folder-listing>\r";

	DIR		*dir;
	char		old_dir[256 + PATH_MAX];
	int		pdir_writeable;
	struct stat	f_stat;
	obex_file_t	*file;
	
	if (getcwd(old_dir, sizeof(old_dir)) == NULL)
		return -1;
	DBPRT("old_dir [%s], new: %s\n", old_dir, name);
	file = obex_create_file(NULL);
	if (!file) {
		DBPRT("unable to create file\n");
		return -1;
	}
	if (name && name[0] != '\0') {
		if (change_dir(name, 0) != 0)
			goto error;
	}

	dir = opendir(".");
	if (dir == NULL)
		goto error;

	dprintf(file->fd, "%s", header);
	
	if (stat(".", &f_stat) != 0)
		goto error;

	pdir_writeable = (geteuid() == f_stat.st_uid && (f_stat.st_mode & S_IWUSR)) ||
		     (getegid() == f_stat.st_gid && (f_stat.st_mode & S_IWGRP)) ||
		     (f_stat.st_mode & S_IWOTH);
		     
	for (;;) {
		struct dirent	*dent;
		struct tm	*created, *modified, *accessed;
		char		user_perm[4];
		
		dent = readdir(dir);
		if (dent == NULL)
			break;

		if (stat(dent->d_name, &f_stat) != 0)
			goto error;
			
		created = localtime(&f_stat.st_ctime);
		modified = localtime(&f_stat.st_mtime);
		accessed = localtime(&f_stat.st_atime);
		
		user_perm[0] = '\0';
		if ((geteuid() == f_stat.st_uid && f_stat.st_mode & S_IRUSR) ||
		    (getegid() == f_stat.st_gid && f_stat.st_mode & S_IRGRP) ||
		    (f_stat.st_mode & S_IROTH))
			strcat(user_perm, "R");
			
		if ((geteuid() == f_stat.st_uid && f_stat.st_mode & S_IWUSR) ||
		    (getegid() == f_stat.st_gid && f_stat.st_mode & S_IWGRP) ||
		    (f_stat.st_mode & S_IWOTH))
			strcat(user_perm, "W");
			
		if (pdir_writeable)
			strcat(user_perm, "D");
		
		if (S_ISDIR(f_stat.st_mode) &&
		    (strcmp(dent->d_name, ".") == 0 ||
		     strcmp(dent->d_name, "..") == 0))
			continue;
		dprintf(file->fd, "<%s name=\"%s\" size=\"%lld\" user-perm=\"%s\" "
						"created=\"%04d%02d%02dT%02d%02d%02dZ\" "
						"modified=\"%04d%02d%02dT%02d%02d%02dZ\" "
						"accessed=\"%04d%02d%02dT%02d%02d%02dZ\"/>\n",
				(S_ISDIR(f_stat.st_mode) ? "folder" : "file"),
				file_base_name(dent->d_name),
				(long long unsigned)f_stat.st_size,
				user_perm,
				created->tm_year + 1900, created->tm_mon + 1, created->tm_mday,
					created->tm_hour, created->tm_min, created->tm_sec,
				modified->tm_year + 1900, modified->tm_mon + 1, modified->tm_mday,
					modified->tm_hour, modified->tm_min, modified->tm_sec,
				accessed->tm_year + 1900, accessed->tm_mon + 1, accessed->tm_mday,
					accessed->tm_hour, accessed->tm_min, accessed->tm_sec
				);
	}
	dprintf(file->fd, "%s", trailer);

	DBPRT("folder listing written\n");
	//obex_close_file(file);	// not really necessary
	obexsrv_set_file(srv, file->name, 1);
	obex_destroy_file(file, 0);
	chdir(old_dir);
	return 0;
error:
	obex_destroy_file(file, 1);
	chdir(old_dir);
	return -1;
}	

int btobex_get_file(obexsrv_t *srv, char *name)
{
	char	*p;
	
	DBPRT("Got a request for %s", name);
	
	if ((p = strrchr(name, '/')) != NULL) {
		char	dir[PATH_MAX + 1];
		
		strncpy(dir, name, p - name);
		dir[p - name] = '\0';
		if (change_dir(dir, CHDIR_CHECK_ONLY) != 0) {
			BTERROR(" Invalid path.");
			return -1;
		}
	}
	obexsrv_set_file(srv, name, 0);
	return 0;
}

int btobex_get(obexsrv_t *srv, char *name, char *type)
{
	
	if (type && strcasecmp(type, "x-obex/folder-listing") == 0) {
		/* get folder listing */
		return btobex_get_listing(srv, name);
	} else {
		return btobex_get_file(srv, name);
	}
	return -1;
}

int btobex_setpath(obexsrv_t *srv, char *name, int flags)
{
	DBPRT("");
	
	if (!name) {
		if (flags & 0x80) {
			if (change_dir(obex_root_path, 0) != 0)
				goto error;
		} else {
			if ((flags & 0x03) == 0x03 /*& 1*/) {	// backup a level before applying the name
				if (change_dir("..", 0) != 0)
					goto error;
			}
		}
	} else {
		if (change_dir(name, (flags&0x02)? 0 : CHDIR_CREATE_IF_NOT_EXIST) != 0)
			goto error;
	}	
	return 0;
error:
	return -1;
}

int btobex_connect(obexsrv_t *srv, obex_target_t *target)
{
	return 1;
}

void btobex_disconnect(obexsrv_t *srv)
{
}

void usage(void)
{
	fprintf(stderr,
		"btobex -d <obex_root_dir> --debug\n"
		"\tobex_root_dir: non-default obex root directory\n"
		);
	BTERROR("Invalid options");
}

//
//
//
int main (int argc, char *argv[])
{
	obexsrv_t	srv;
	int		c, lind;

	struct option	opts[] = {
		{"help", 0, 0, 'h'},
		{"rootdir", 1, 0, 'd'},
		{0, 0, 0, 0}
	};

	openlog(NULL/*argv[0]*/, 0, LOG_DAEMON);
	BTINFO("%s %s started.", argv[0], affix_version);
	
	affix_logmask = BTDEBUG_MODULE;

	for (;;) {
		c = getopt_long(argc, argv, "+hvd:", opts, &lind);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				usage();
				return 0;
				break;
			case 'd':
				obex_root_path = strdup(optarg);
				break;
			case 'v':
				verboseflag = 1;
				break;
			case ':':
				BTERROR("Missing parameters for option: %c\n", optopt);
				return 1;
				break;
			case '?':
				BTERROR("Unknown option: %c\n", optopt);
				return 1;
				break;
		}
	}

	if (!obex_root_path) {
		if (getuid() == 0) {
			/* root */
			obex_root_path = strdup("/var/spool/affix/Inbox");
		} else {
			/* set to home dir */
			char	buf[80];
			sprintf(buf, "%s/Inbox", getenv("HOME"));
			obex_root_path = strdup(buf);
		}
	}
	if (rmkdir(obex_root_path, 0700) != 0 || chdir(obex_root_path) != 0) {
		BTERROR("failed to set the obex_root directory.");
		exit(0);
	}

	/* setup obex object */
	memset(&srv, 0, sizeof(obexsrv_t));
	srv.connect = btobex_connect;
	srv.setpath = btobex_setpath;
	srv.put = btobex_put;
	srv.get = btobex_get;
	srv.disconnect = btobex_disconnect;

	DBPRT("started\n");
	/* start server in a loop */
	obexsrv_run(&srv, 0, 1);

	free(obex_root_path);
	return 0;
}

