#ifndef DiskManager_H
#define DiskManager_H

#include <string>
#include <map>
#include <pthread.h>

#include "DiskDev.h"

#define maxDevList 16

namespace Hippo {

class DiskManager {
public:    
    DiskManager();
    virtual ~DiskManager();
    int refresh();
    int getDiskCount();
    DiskDev * getDiskDevice(int pIndex);
    DiskDev * getDiskDevice(const std::string &name);
    int setPvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName);
    int getPvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName);
    std::string getPvrDiskName(){return m_pvrDiskName;}
    std::string getPvrPartitionName(){return m_pvrPartitionName;}
    int getPvrDiskFlag(){return m_havePvrDisk;}
    int umountDisk(std::string &);
    int formatPartition(const std::string &disk, const std::string &partition, int type);
    int getFormatProgress();
    int checkPartition(const std::string &disk, const std::string &partition);
    int getPartitionCheckProgress();
         
protected:
    std::string m_devNameList[maxDevList];
    int m_devCount;
    std::map<std::string, DiskDev*> m_disk;
    std::vector<std::string> m_addDiskName;
    std::vector<std::string> m_removeDiskName;
    std::vector<std::string> m_umountDiskName;
    int devCheck();
    
    int m_havePvrDisk;
    std::string m_pvrDiskName;
    std::string m_pvrPartitionName;          
    int removePvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName); 
    int pvrTagCheck( DiskPartition *partition);    
    int m_formatType;
    std::string m_formatDisk;
    std::string m_formatPartition;
    int m_formatProgress;
    int m_formateStatus;//0 stop, 1 run 2 finish
    pthread_t m_formatThreadID;
    static void* format(void *);
    std::string m_checkDisk;
    std::string m_checkPartition;
    int m_checkProgress;
    int m_checkStatus; //0 stop, 1 run 2 finish
    pthread_t m_checkThreadID;
    static void* check(void *);
};
    
};//namespace Hippo

#endif//DiskManage_H

