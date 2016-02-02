/*************************************************************************************************
*
*×÷Õß:³Â±¦Ç¿2011-05-01
*
*************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mntent.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>

#include "libzebra.h"
#include "disk_js_api.h"
#include "disk_info.h"
//#include "cpvr/yx_dbg.h"
#define min(a,b)   (a) > (b) ? (b):(a)

#define OUTPUT_LEVEL LOG_ERROR//LOG_NONE //LOG_ERROR


#define usb_dir "/mnt/usb"


