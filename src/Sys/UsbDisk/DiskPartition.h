#ifndef DiskPartition_H
#define DiskPartition_H

#include <vector>
#include <map>
#include <string>

namespace Hippo{

class DiskPartition {
public:
    DiskPartition(const std::string& name, bool isFormate);
    ~DiskPartition();
    
    int mountPartition();
    int umountPartition();
    int setStorageFlag(int flag){if (m_isMount) {m_storageFlag = flag;} return 0;}
    int getStorageFlag(){return m_storageFlag;}
    
    std::string getName();
    std::string getMountPoint(){return m_mountPoint;};
    long long getTotalSize();
    long long getFreeSize();
    long getFsType();
    bool getMountFlag(){return m_isMount;}

private:    
    std::string m_name;
    bool m_isFormate;
    bool m_isMount;
    //if m_isMount&m_isFormate is true, support following infomation
    int m_storageFlag;
    std::string m_mountPoint;
    long long m_totalSize;
    long long m_freeSize;
    long m_fsType;
        
    int mountMapInitFalg;
    std::string getFreeMountPoint();
    int tryMount(const std::string &devName, const std::string &point);
    int checkMountStatus();    
};//class DiskPartition    
    
};//namespace Hippo

#endif//DiskPartition_H
