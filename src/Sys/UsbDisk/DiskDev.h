#ifndef DiskDevice_H
#define DiskDevice_H

#include <vector>
#include <string>

#include "DiskPartition.h"

namespace Hippo{

class DiskDev{
public:
    DiskDev(const std::string &name);
    ~DiskDev();
    
    int getPartitionCount();
    DiskPartition* getPartition(const std::string &name);
    DiskPartition* getPartition(int pIndex);
    std::string getName();
    long getSize(){return m_size;}
    int getPosition(){return m_position;}
    int umount();
    
private:
    std::string m_name;
    long m_size;
    int m_position;//0 sata, 1 usb1, 2 usb2
    std::vector<DiskPartition*> m_partitionList;    
};//DiskDev

};//namespace Hippo

#endif//Disk_H
