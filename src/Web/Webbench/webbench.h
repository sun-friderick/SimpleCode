/*
 * (C) Radim Kolar 1997-2004
 * This is free software, see GNU Public License version 2 for
 * details.
 *
 * Simple forking WWW Server benchmark:
 *
 * Usage:
 *   webbench --help
 *
 * Return codes:
 *    0 - sucess
 *    1 - benchmark failed (server is not on-line)
 *    2 - bad param
 *    3 - internal error, fork failed
 * 
 */ 
#ifndef __WEBBENCH_H__
#define __WEBBENCH_H__

#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

#include "socket.h"


int webbench(int argc, char *argv[]);

void build_request(const char *url);

/* vraci system rc error kod */
static int bench(void);

void benchcore(const char *host,const int port,const char *req);



#endif //__WEBBENCH_H__
