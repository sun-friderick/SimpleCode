#include <stdio.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include "Assertions.h"

#include "DiskDev.h"

namespace Hippo{

DiskDev::DiskDev(const std::string &name)
    : m_size(0)
    , m_position(-1)
{
    LogUserOperDebug("create divice [%s]\n", name.c_str());
    FILE *fileFp = fopen("/proc/partitions", "r");
    if (!fileFp) {
        LogUserOperError("fopen /proc/partitions error\n");
        return;    
    }
    char lineBuf[1024] = {0};
    while(1) {
        if (!fgets(lineBuf, 1024, fileFp) ){// to the end
            LogUserOperError("fgets  to the end\n");
            break;
        }
        char *ptr = strstr(lineBuf, name.c_str());
        if (ptr) {//find the file name
            int major;
            int minor;
            long blocks;
            char devName[256] = {0};
            sscanf(lineBuf, "%d %d %ld %s", &major, &minor, &blocks, devName);
            LogUserOperDebug("devName is [%s]\n", devName);
            if (!name.compare(devName)) {//dev name 
                m_name = name;
                m_size = blocks;          
            } else { //partition name
                bool isFormate = true;
                if (1 == blocks) {
                    isFormate = false;
                    LogUserOperDebug("Disk partition [%s] not formated\n", name.c_str());
                 }
                DiskPartition *pPartition = new DiskPartition(devName, isFormate);
                if (pPartition->getMountFlag())
                    m_partitionList.push_back(pPartition);
                else
                    delete pPartition;
            }
        }
    }
    fclose(fileFp);

/*read usb connect port 1 usb1, 2 usb2*/
#if defined (brcm7405)
    std::string  tempPath = "/sys/block/" + m_name +  "/device";
#else
    std::string  tempPath = "/sys/block/" + m_name;
#endif
    LogUserOperDebug("tempPath[%s]\n", tempPath.c_str());
#if 0
    if (!access(tempPath.c_str(), F_OK)) {
        LogUserOperError("Get connect port error, can not access tempPath\n");
        return;
    }
#endif
    char linkPath[1024] = {0};
    int retLen = readlink(tempPath.c_str(), linkPath, 1024);
    if (retLen > 0) {
        if (retLen >= 1024) {
            LogUserOperError("error, linkPath is too small\n");
            return;
        }
        LogUserOperDebug("linkPath[%s]\n", linkPath);
        if (strstr(linkPath, "1-1")) {
            m_position = 1;
        }
        else if (strstr(linkPath, "1-2")) {
            m_position = 2;
        }
        else {
            LogUserOperError("error, can not find connect port\n");
            LogUserOperDebug("linkPath[%s]\n", linkPath);
            m_position = 2; //default
        }
    } else {
        LogUserOperError("readlink error\n");
        m_position = 2; //default
    }
}

DiskDev::~DiskDev()
{
    for(int i = 0; i < m_partitionList.size(); i++) {
       delete m_partitionList.at(i);     
    }    
    m_partitionList.clear();
}    

std::string DiskDev::getName()
{
    return m_name;
}

int DiskDev::getPartitionCount()
{
    return  m_partitionList.size();
}

DiskPartition* DiskDev::getPartition(const std::string &name)
{
    for(int i = 0;i < m_partitionList.size(); i++) {
       if(!name.compare(m_partitionList.at(i)->getName())) {
            return m_partitionList.at(i);
        }   
    }      
    return NULL;    
}  

DiskPartition* DiskDev::getPartition(int pIndex)
{
    if ((pIndex < 0) || (pIndex > m_partitionList.size())) {
        LogUserOperError("getPartition index error\n");
        return NULL;    
    }    
    return m_partitionList.at(pIndex);
}  

int DiskDev::umount()
{
     for(int i = 0;i < m_partitionList.size(); i++) {
            m_partitionList.at(i)->umountPartition();
    }   
    return 0;
}

};//namespace Hippo
