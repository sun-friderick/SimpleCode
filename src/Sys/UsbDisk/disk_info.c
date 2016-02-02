#include <stdio.h>
#include <string.h>

#include "disk_info.h"

#include "Assertions.h"

#include "libzebra.h"

extern int refreshDiskInfo();
extern int getPvrDisk(char *diskName, int diskNameLen, char *partitionName, int partitionNameLen);
extern int getPartitionInfo(const char *diskName,
                            int partitionIndex,
                            char *pName, int nameLen,
                            char *pMountPath, int mountPathLen,
                            long *totalSize_M,
                            long *freeSize_M,
                            char *fileSystype, int fsTypeBufLen,
                            int *storateFlag);
extern int getPartitionCount(const char *diskName);
extern int autoSetPvrTag();

int pvr_partition_info_get(struct pvr_partition *p_pvr_partition)
{
    LogUserOperDebug("pvr_partition_info_get pvr_partition_info_get\n");
    if(!p_pvr_partition) {
        LogUserOperError("pvr_partition_info_get p_pvr_partition null\n");
        return -1;
    }
    char disk[256] = {0};
    char partition[256] = {0};
    if(getPvrDisk(disk, 256, partition, 256)) { //no pvr disk
        LogUserOperError("no pvr disk\n");
        return -1;
    }
    int partitionCount = getPartitionCount(disk);
    if(partitionCount <= 0) {
        LogUserOperError("pvr_partition_info_get partitionCount<=0\n");
        return -1;
    }
    int i = 0;
    for(i = 0; i < partitionCount; i++) {
        char partionName2[256] = {0};
        char pMountPath[256] = {0};
        long totalSize_M = 0L;
        long freeSize_M = 0L;
        char fileSystype[128] = {0};
        int storateFlag = 0;
        getPartitionInfo(disk, i, partionName2, 256, pMountPath, 256, &totalSize_M, &freeSize_M,
                         fileSystype, 128, &storateFlag);
        if(!strcmp(partition, partionName2)) {
            memcpy(p_pvr_partition->disk_name, disk, strlen(disk));
            p_pvr_partition->disk_size = totalSize_M;
            strcpy(p_pvr_partition->partition_name, partition);
            strcpy(p_pvr_partition->mounted_dir, pMountPath);
            p_pvr_partition->partition_size = totalSize_M;
            p_pvr_partition->partition_free_size = freeSize_M;
            break;
        }

    }
    return 0;
}

int storage_in_out_event(int msg_type, int disk_index)
{

    if(msg_type == YX_EVENT_HOTPLUG_ADD) {
        sleep(2);//very important, zm add
        refreshDiskInfo();
    } else if(msg_type == YX_EVENT_HOTPLUG_REMOVE) {
        refreshDiskInfo();
    } else {
        LogUserOperError("unknown disk msg\n");
        return -1;
    }
    //autoSetPvrTag();
    return 0;
}

