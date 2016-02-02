
#include <sys/sysinfo.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int GetTickCount(void)
{
        struct  sysinfo         si;
        sysinfo(&si);
        return (int)si.uptime;
}

static unsigned long GetFreeDiskBytes(const char * path)
{
        struct statfs   buf;
        if(statfs(path, &buf) != 0)
        {
                perror("statfs");
                return 0;
        }
        return (unsigned long)buf.f_bfree * 4 * 1024;
}

unsigned long GetFreeVarSize(void)
{
        return GetFreeDiskBytes("/var");
}

int GetFreeMemSize(void)
{
        int     fd;
        fd = open("/proc/meminfo", O_RDONLY);
        if(fd < 0)
        {
                perror("open");
                return 0;
        }

        char    buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        read(fd, buffer, 1023);
        close(fd);
        char *  p = strstr(buffer, "MemFree:");
        if(p == NULL)
        {
                printf("no MemFree\n");
                return 0;
        }
        p += strlen("MemFree:");
        int     size = atoi(p);
        return size * 1024;
}

int IsFileExists(const char * filename)
{
    if (filename == NULL)
        return 0;
    if (access(filename, F_OK) == 0)
        return 1;
    return 0;
}

long GetFileSizeBytes(const char * filename)
{
    struct stat statbuff;
    if (!filename)
        return 0;

    if (stat(filename, &statbuff) == 0)
        return statbuff.st_size;
    return 0;
}

