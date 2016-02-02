
#include "DiskManager.h"

using namespace Hippo;

extern "C" {

#include <stdio.h>
#include <string.h>
#include "Assertions.h"
#include "Hippo_api.h"
#include "json/json.h"
#include "json/json_object.h"
#include "json/json_public.h"

extern int yos_systemcall_runSystemCMD(char *buf, int *ret);

static DiskManager * g_DiskManager = NULL;

int diskMangerInit()
{
   if (!g_DiskManager) {
        g_DiskManager = new  DiskManager();
    }
    return 0;
}

int getDiskCount()
{   
    diskMangerInit();
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }
    return g_DiskManager->getDiskCount();
}

int refreshDiskInfo()
{   
    diskMangerInit();
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }
    return g_DiskManager->refresh();
}

int getDiskInfo(int pIndex, char *name, int nameLen, long *size, int *position, int *storageFlag)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }
    if ((!position) || (!size) || (!storageFlag)) {
        LogUserOperError("position or size error\n");
        return -1;          
    }
    if ((pIndex < 0) || (pIndex >= g_DiskManager->getDiskCount())) {
        LogUserOperError("getDiskInfo index error\n");
        return -1;    
    }
    if (name) {
        snprintf(name, nameLen, "%s", g_DiskManager->getDiskDevice(pIndex)->getName().c_str());
        *position = g_DiskManager->getDiskDevice(pIndex)->getPosition();
        *size = g_DiskManager->getDiskDevice(pIndex)->getSize();
        *storageFlag = 0;//first set 0
        if (g_DiskManager->getPvrTag(g_DiskManager->getDiskDevice(pIndex)->getName(), "null") == 1) {
              *storageFlag = 1;  
        }
        return 0;
    }
    return -1;
}

int getPartitionCount(const char *diskName)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }
    if (diskName) {
        std::string name = diskName;
        DiskDev *diskDev = g_DiskManager->getDiskDevice(name);
        if (diskDev)
            return diskDev->getPartitionCount();
        else
            LogUserOperError("getPartitionCount diskDev is NULL\n");
    }
    LogUserOperError("getPartitionCount diskName is null \n");
    return 0;
  
}

int getPartitionInfo(const char *diskName, 
                                int partitionIndex, 
                                char *pName, int nameLen, 
                                char *pMountPath, int mountPathLen,
                                long *totalSize_M, 
                                long *freeSize_M, 
                                char *fileSystype, int fsTypeBufLen, 
                                int *storateFlag)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }    
   if ((!pName) || (!totalSize_M) || (!freeSize_M) || (!fileSystype) || (!storateFlag) || (!pMountPath)) {
        LogUserOperError("parameter error\n");
        return -1;
    }    
    std::string name =  diskName;
    DiskPartition *partition = g_DiskManager->getDiskDevice(name)->getPartition(partitionIndex);
    if (partition) {
        snprintf(pName, nameLen, "%s", partition->getName().c_str());
        snprintf(pMountPath, mountPathLen, "%s", partition->getMountPoint().c_str());
        *totalSize_M = (long)(partition->getTotalSize()/1024);
        *freeSize_M = (long)(partition->getFreeSize()/1024);
        long fsType = partition->getFsType();
        *storateFlag = 0;
        if(g_DiskManager->getPvrTag(name, partition->getName()) == 1) {
           *storateFlag = 1;     
        }
        LogUserOperDebug("filesystem type [%ld]\n", fsType);
        if(fsType == 0xEF53 )  
            snprintf(fileSystype, fsTypeBufLen, "EXT3");
        else if(fsType ==  0xEF52)
            snprintf(fileSystype, fsTypeBufLen, "EXT2");
        else if(fsType == 0x4d44)
            snprintf(fileSystype, fsTypeBufLen, "FAT32");
        else if(fsType == 0x4d43)
            snprintf(fileSystype, fsTypeBufLen, "FAT");                    
        else if(fsType == 0x5346544e)
            snprintf(fileSystype, fsTypeBufLen, "NTFS");
        else if(fsType == 0x58465342)
            snprintf(fileSystype, fsTypeBufLen, "XFS");
        else if(fsType == 0x4244)
            snprintf(fileSystype, fsTypeBufLen, "HFS");
        else if(fsType == 0xF995E849)
            snprintf(fileSystype, fsTypeBufLen, "HFS+");
        else  
            snprintf(fileSystype, fsTypeBufLen, "UNKOWN");
        LogUserOperDebug("get disk [%s] partition [%d], name [%s], pMountPath[%s] Tsize[%d], Fsize[%d], fs[%s],storFlag[%d]\n", 
                    diskName, partitionIndex, pName, pMountPath, *totalSize_M, *freeSize_M, fileSystype, *storateFlag);
        return 0;                    
    }
    
    LogUserOperError("getPartition error\n");
    return -1;
}

int setPvrTagInfoInDisk(const char *pDisk, const char *pPartition)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }   
    if ((!pDisk) || (!pPartition)) {
        LogUserOperError("setPvrTagInfoInDisk error\n");
        return -1;    
    }
    std::string disk = pDisk;
    std::string partition = pPartition;
    return g_DiskManager->setPvrTag(disk, partition);    
}

int getPvrDisk(char *diskName, int diskNameLen, char *partitionName, int partitionNameLen)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    }    
   if ((!diskName) || (!partitionName)) {
        LogUserOperError("getPvrDisk error\n");
        return -1;
    }
    if (g_DiskManager->getPvrDiskFlag()) {
        snprintf(diskName, diskNameLen, "%s", g_DiskManager->getPvrDiskName().c_str());
        snprintf(partitionName, partitionNameLen, "%s", g_DiskManager->getPvrPartitionName().c_str());
        return 0;
    }
    return -1;
}

int umountDiskByName(const char * pName)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    } 
    if (!pName) {
        LogUserOperError("umountDiskByName pName is null\n");
        return -1;
    }
    std::string name = pName;
    g_DiskManager->umountDisk(name);
    return 0;
}

int formatPartition(const char *dev, const char * partition, int type)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    } 
    if (!dev ) {
        LogUserOperError("formatDiskPartition dev is null\n");
        return -1;
    }
    //type:   //1 fat 2 xfs 3 ext3
    std::string disk = dev;
    if (partition) {
        std::string partit = partition;
        g_DiskManager->formatPartition( disk, partit, type);
    } else {//merge partition 
        g_DiskManager->formatPartition( disk, "null", type);
    }

    return 0;
}

int getPartitionFormatProcess()
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    } 
    return g_DiskManager->getFormatProgress();
}

int checkPartition(const char *dev, const char * partition)
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    } 
    if (!dev ) {
        LogUserOperError("checkPartition pName is null\n");
        return -1;
    }
    LogUserOperDebug("check partition#########\n");
    std::string disk = dev;
    if (partition) {
        std::string parti = partition;
        g_DiskManager->checkPartition(disk, parti);
    } else {//merge partition 
        g_DiskManager->checkPartition(disk, "null");
    }
    return 0;
}

int getPartitionCheckProcess()
{
    if(!g_DiskManager) {
        LogUserOperError("error g_DiskManager is null \n");
        return -1;    
    } 
    return g_DiskManager->getPartitionCheckProgress();
}

static int getAllDiskNumberInfo(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    int diskCount = getDiskCount();

    LogUserOperDebug("getAllDiskNumberInfo getAllDiskNumberInfo\n");
    if(!diskCount) {
        snprintf(aFieldValue, 1024, "{\"HDD_count\":0}");
        return 0;
    }
    int i = 0;
    json_object *diskArray = json_object_new_array();
    for(i = 0; i < diskCount; i++) {
        char diskName[128] = {0};
        int position = 0;
        char connectPort[32] = {0};
        long diskSize = 0;
        int storateFlag = 0;
        if(getDiskInfo(i, diskName, 128, &diskSize, &position, &storateFlag)) {
            LogUserOperError("getDiskInfo error\n");
            return 0;
        }
        if(0 == position)
            snprintf(connectPort, 32, "SATA");
        else if(1 == position)
            snprintf(connectPort, 32, "USB1");
        else if(2 == position)
            snprintf(connectPort, 32, "USB2");
        else
            snprintf(connectPort, 32, "USB1");
        int partitionCount = getPartitionCount(diskName);
        char tmpBuf[128] = "";

        json_object *diskInfo = json_object_new_object();
        json_object_object_add(diskInfo, "deviceName", json_object_new_string(diskName));
        json_object_object_add(diskInfo, "partitionCount", json_object_new_int(partitionCount));
        snprintf(tmpBuf, 128, "%d", diskSize / 1024);
        json_object_object_add(diskInfo, "totalSpace", json_object_new_string(tmpBuf));
        json_object_object_add(diskInfo, "usedForStorage", json_object_new_int(storateFlag));
        if(storateFlag) {
            int tstvSpace = 2000;
            snprintf(tmpBuf, 128, "%d", tstvSpace);
            json_object_object_add(diskInfo, "reserverSpace", json_object_new_string(tmpBuf));
        }
        json_object_object_add(diskInfo, "connectedPort", json_object_new_string(connectPort));
        json_object_array_add(diskArray, diskInfo);
    }
    char *jsonstr_disk_info = (char *)json_object_to_json_string(diskArray);
    snprintf(aFieldValue, 4096, "{\"HDD_count\":%d,\"devices\":%s}", diskCount, jsonstr_disk_info);
    LogUserOperDebug("aFieldValue[%s]\n", aFieldValue);
    json_object_put(diskArray);
    return 0;
}

static int getDiskDetailInfo(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperDebug("getDiskDetailInfo [%s]\n", param_json);
    json_object *json_info = json_tokener_parse(param_json);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "deviceName"));

    LogUserOperDebug("getDiskDetailInfo [%s]\n", param_json);
    if(!diskName) {
        LogUserOperError("json_object_get_string deviceName error\n");
        return -1;
    }

    int partitionCount = getPartitionCount(diskName);
    if(0 == partitionCount) {
        snprintf(aFieldValue, 4096, "{\"partitionCount\":0,\"partitions\":[]}");
        return 0;
    }
    json_object *diskArray = json_object_new_array();
    int partitionIndex = 0;
    for(partitionIndex = 0; partitionIndex < partitionCount; partitionIndex++) {
        char partitionName[128] = {0};
        char mountPath[256] = {0};
        long totalSize_M = 0;
        long freeSize_M = 0;
        char fileSysType[32] = {0};
        int storateFlag = 0;
        char tmpBuf[128] = "";
        if(getPartitionInfo(diskName, partitionIndex,
                            partitionName, 128,
                            mountPath, 256,
                            &totalSize_M,
                            &freeSize_M,
                            fileSysType, 32,
                            &storateFlag)) {
            LogUserOperError("getPartitionCount error\n");
            return 0;
        }
        if(!strlen(mountPath))
            continue;
        json_object *diskInfo = json_object_new_object();
        json_object_object_add(diskInfo, "partitionName", json_object_new_string(partitionName));
        json_object_object_add(diskInfo, "mountPath", json_object_new_string(mountPath));
        json_object_object_add(diskInfo, "filesystemType", json_object_new_string(fileSysType));
        snprintf(tmpBuf, 128, "%d", freeSize_M / 1024);
        json_object_object_add(diskInfo, "freeSpace", json_object_new_string(tmpBuf));
        snprintf(tmpBuf, 128, "%d", totalSize_M / 1024);
        json_object_object_add(diskInfo, "totalSpace", json_object_new_string(tmpBuf));
        json_object_object_add(diskInfo, "usedForStorage", json_object_new_int(storateFlag));
        json_object_array_add(diskArray, diskInfo);
    }
    char *jsonstr_disk_info = (char *)json_object_to_json_string(diskArray);
    snprintf(aFieldValue, 4096, "{\"partitionCount\":%d,\"partitions\":%s}", partitionCount, jsonstr_disk_info);
    LogUserOperDebug("aFieldValue[%s]\n", aFieldValue);
    json_object_put(diskArray);
    json_object_put(json_info);
    return 0;
}

static int setPVRPartition(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperDebug("setPVRPartition [%s]\n", aFieldValue);
    json_object *json_info = json_tokener_parse(aFieldValue);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "device"));
    char *partition = (char*)json_object_get_string(json_object_object_get(json_info, "partition"));

    if(diskName && partition) {
#ifdef INCLUDE_cPVR
        cpvr_module_disable();
#endif
        setPvrTagInfoInDisk(diskName, partition);
#ifdef INCLUDE_cPVR
        cpvr_module_enable();
#endif     
    } else {
        LogUserOperError("setPVRPartition error\n");
    }
    json_object_put(json_info);
    return 0;
}

static int initializeDisk(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperError("initializeDisk not support\n");
    return 0;
}

static int getInitializeProgress(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperError("getInitializeProgress not support\n");
    return 0;
}

static int formatDiskPartition(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    json_object *json_info = json_tokener_parse(aFieldValue);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "deviceName"));
    char *partition = (char*)json_object_get_string(json_object_object_get(json_info, "partition"));
    char *fsType = (char*)json_object_get_string(json_object_object_get(json_info, "FilesystemType"));
    char *mode = (char*)json_object_get_string(json_object_object_get(json_info, "formatModel"));
    int fileSys = 3; //type:   //1 fat 2 xfs 3 ext3

    LogUserOperDebug("formatDiskPartition [%s]\n", aFieldValue);
    formatPartition(diskName, partition, fileSys);
    json_object_put(json_info);
    return 0;
}

static int getFormatProgress(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    json_object *json_info = json_tokener_parse(param_json);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "device"));
    char *partition = (char*)json_object_get_string(json_object_object_get(json_info, "partition"));

    LogUserOperDebug("getFormatProgress [%s]\n", param_json);
    if(!diskName) {
        LogUserOperError("getFormatProgress diskName is null\n");
        return -1;
    }
    json_object *diskInfo = json_object_new_object();
    
    char progress[64] = {0};
    snprintf(progress, 64, "%d", getPartitionFormatProcess());
    json_object_object_add(diskInfo, "formatProgress", json_object_new_string(progress));
    json_object_object_add(diskInfo, "device", json_object_new_string(diskName));
    
    char *jsonstr_disk_info = (char *)json_object_to_json_string(diskInfo);
    snprintf(aFieldValue, 1024, "%s", jsonstr_disk_info);
    LogUserOperDebug("getFormatProgress [%s]\n", aFieldValue);

    json_object_put(diskInfo);
    json_object_put(json_info);
    return 0;
}

static int restoreFilesystem(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    json_object *json_info = json_tokener_parse(aFieldValue);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "device"));
    char *partition = (char*)json_object_get_string(json_object_object_get(json_info, "partition"));

    LogUserOperDebug("restoreFilesystem [%s]\n", aFieldValue);
    checkPartition(diskName, partition);
    json_object_put(json_info);
    return 0;
}

static int getRestoreProgress(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperDebug("getRestoreProgress [%s]\n", param_json);

    json_object *json_info = json_tokener_parse(param_json);
    char *diskName = (char*)json_object_get_string(json_object_object_get(json_info, "device"));

    if(!diskName) {
        LogUserOperError("getRestoreProgress diskName is null\n");
        return -1;
    }
    json_object *diskInfo = json_object_new_object();
    char progress[64] = {0};
    snprintf(progress, 64, "%d", getPartitionCheckProcess());
    json_object_object_add(diskInfo, "RestoreProgress", json_object_new_string(progress));
    json_object_object_add(diskInfo, "device", json_object_new_string(diskName));
    char *jsonstr_disk_info = (char *)json_object_to_json_string(diskInfo);
    snprintf(aFieldValue, 1024, "%s", jsonstr_disk_info);

    json_object_put(diskInfo);
    json_object_put(json_info);
    return 0;
}

static int stopRestore(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    LogUserOperError("stopRestore not support\n");
    return 0;
}

static int removeDevice(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult)
{
    json_object *json_info = json_tokener_parse(aFieldValue);
    char *diskName = NULL;

    LogUserOperDebug("removeDevice [%s]\n", aFieldValue);
    diskName = (char*)json_object_get_string(json_object_object_get(json_info, "device"));
    if(!diskName) {
        LogUserOperError("removeDevice check disk name error\n");
        return 0;
    }
    umountDiskByName(diskName);
    json_object_put(json_info);
    return 0;
}

int  disk_manager_init(ioctl_context_type_e type)
{
    char cmdStr[] = "killall -9  usbmounter";
    yos_systemcall_runSystemCMD(cmdStr, NULL);

    refreshDiskInfo();

    a_Hippo_API_JseRegister("getAllDiskNumberInfo", getAllDiskNumberInfo, NULL, type);
    a_Hippo_API_JseRegister("GetDiskInfo", getDiskDetailInfo, NULL, type);
    a_Hippo_API_JseRegister("HDDmangment.setPVRPartition", NULL, setPVRPartition, type);
    a_Hippo_API_JseRegister("HDDmangment.initialize", NULL, initializeDisk, type);
    a_Hippo_API_JseRegister("HDDmangment.initialize.progress",  getInitializeProgress, NULL, type);
    a_Hippo_API_JseRegister("HDDmangment.partition.format", NULL, formatDiskPartition, type);
    a_Hippo_API_JseRegister("HDDmangment.format.progress", getFormatProgress, NULL, type);
    a_Hippo_API_JseRegister("HDDmangment.filesystemRestore", NULL, restoreFilesystem, type);
    a_Hippo_API_JseRegister("HDDmangment.filesystemRestore.progress", getRestoreProgress, NULL, type);
    a_Hippo_API_JseRegister("HDDmangment.filesystemRestore.Stop", NULL, stopRestore, type);
    a_Hippo_API_JseRegister("HDDmangment.deviceRemove", NULL, removeDevice, type);
    return 0;
}

};//extern "C"

