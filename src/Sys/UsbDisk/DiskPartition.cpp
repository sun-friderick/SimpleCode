#include <stdio.h>
#include <iostream>
#include <sys/mount.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <string.h>
#include <errno.h>
#include "DiskPartition.h"
#include "Assertions.h"

extern int errno;

namespace Hippo{
    
DiskPartition::DiskPartition(const std::string& name, bool isFormate)
 :m_isMount(0)
{
    m_name = name;  
    m_isFormate = isFormate;
    this->mountPartition();
}    

DiskPartition::~DiskPartition()
{
    if (m_isMount) {
       LogUserOperDebug("~DiskPartition umount [%s]\n", m_mountPoint.c_str());
       umount2(m_mountPoint.c_str(), MNT_FORCE);  
    }
}

int DiskPartition::mountPartition()
{
    if (!m_isFormate) {
        LogUserOperError("disk partition not formated\n");
        return -1;    
    }
    if (m_isMount) {
        LogUserOperDebug("dev [%s] is already mounted to [%s] \n", m_name.c_str(), m_mountPoint.c_str());   
     } else {
        if (!checkMountStatus()) {//is not mount , try mount
            m_mountPoint = getFreeMountPoint();        
            std::string devName = "/dev/"+m_name;
            LogUserOperDebug("devname is [%s], get mount point [%s]\n", devName.c_str(), m_mountPoint.c_str());
            int ret = tryMount(devName, m_mountPoint);
            if (ret) {
                LogUserOperDebug("mount [%s] fail\n", devName.c_str());
                m_isMount = false;
                return -1;   
            }
            m_isMount = true;
        } else {//already mounted
            m_isMount = true;
            //get mount point
            FILE * h_MntFile = setmntent("/proc/mounts", "r");
            if (h_MntFile) {
            	struct mntent *mnt;
            	while((mnt = getmntent(h_MntFile)) != NULL)
            	{          		
            		if (strstr(mnt->mnt_fsname, m_name.c_str())) {//find item
            			m_mountPoint = mnt->mnt_dir;
            			//LogUserOperDebug("mount fs type [%s]\n", mnt->mnt_type);
            			if (!strcmp(mnt->mnt_type, "ext3")) {
            			   m_fsType = 0xEF53;
            			 }
            			 else if(!strcmp(mnt->mnt_type, "ufsd")) {
            			   m_fsType = 0x5346544e;
            			 }
            			 else {
            			       m_fsType = 0x5346544e;
            			 }
            			
            			break;
            		}
            	}        		
                endmntent(h_MntFile);
                LogUserOperDebug("dev [%s] is alr22eady mounted to [%s] \n", m_name.c_str(), m_mountPoint.c_str());
        	} else {
        	    LogUserOperError("setmnent /proc/mounts error\n");  
        	}
        }
               
        if (m_isMount) {//mount ok
            LogUserOperDebug("mount [%s] ok\n", m_name.c_str());
        	struct statfs buf;
        	if( statfs( m_mountPoint.c_str(), &buf ) ){
        		LogUserOperError("statfs error\n");
        		return -1;
        	}
        	m_totalSize = (buf.f_bsize* static_cast<long long>(buf.f_blocks)); /*Bytes*/
        	m_freeSize = (buf.f_bsize*static_cast<long long>(buf.f_bfree)); /*Bytes*/
        	//m_fsType = buf.f_type;
        	LogUserOperDebug("disk partition [%s] create ok\n", m_name.c_str());
        } 
     }
     return 0;
}
    
int DiskPartition::umountPartition()
{
    if (m_isMount) {
        m_isMount = false;   
        LogUserOperDebug("umount2 [%s]\n", m_mountPoint.c_str());
        umount2(m_mountPoint.c_str(), MNT_FORCE);  
        m_totalSize = 0;
        m_freeSize = 0;
        m_fsType = 0;       
    }
    return 0;
}
    
std::string DiskPartition::getName()
{
    return m_name;
}

long long DiskPartition::getTotalSize()
{
    return m_totalSize;
}

long long DiskPartition::getFreeSize()
{
    return m_freeSize;
}

long DiskPartition::getFsType()
{
    return m_fsType;
}

std::string DiskPartition::getFreeMountPoint()
{
    char mountBuf[4096] = {0};
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        LogUserOperError("fopen /proc/mounts error\n");
        return "/mnt/usb1";    
    }      
    fread(mountBuf, 1, 4096, fp);
    fclose(fp);  
    
    char mountDir[64] = {0};
    for(int i = 1; i<=16; i++) {
        memset(mountDir, 0, sizeof(mountDir));
        snprintf(mountDir, 64, "/mnt/usb%d", i);
        if (!strstr(mountBuf, mountDir)) {// 
           if(access(mountDir, R_OK) == 0) {
                return mountDir; 
           }  
        }    
    }    
    LogUserOperDebug("getFreeMountPoint error\n");
    return "/mnt/usb1";
}  

int DiskPartition::tryMount(const std::string &devName, const std::string &point)
{
	LogUserOperDebug("Try to mount [%s] to [%s] as VFAT\n", devName.c_str(), point.c_str());
	
	FILE *fp = fopen("/proc/filesystems", "r");
	if(!fp) {
	    LogUserOperError("fopen /proc/filesystems error\n");
	    return -1;    
	}
	char fsBuf[1024] = {0};
	fread(fsBuf, 1024, 1, fp);
	fclose(fp);
	
	int ret = 0;
    if (strstr(fsBuf, "vfat")) {
    	m_fsType = 0x4d44;
    	ret = mount(devName.c_str(), point.c_str(), "vfat", MS_NOEXEC, "iocharset=utf8,shortname=mixed");
    }
	if (ret && (strstr(fsBuf, "ext3"))) {
	    m_fsType = 0xEF53;
		LogUserOperDebug("Try to mount [%s] to [%s] as EXT3\n", devName.c_str(), point.c_str());
		ret = mount(devName.c_str(), point.c_str(), "ext3", 0, 0);
	}
	if (ret && (strstr(fsBuf, "ext2"))) {
	     m_fsType = 0xEF52;
		LogUserOperDebug("Try to mount [%s] to [%s] as EXT2\n", devName.c_str(), point.c_str());
		ret = mount(devName.c_str(), point.c_str(), "ext2", 0, 0);
	}
	if (ret) {
	    m_fsType = 0x5346544e;
		LogUserOperDebug("Try to mount [%s] to [%s] as NTFS\n", devName.c_str(), point.c_str());
	    ret = mount(devName.c_str(), point.c_str(), "ufsd", 0, "iocharset=utf8,force");  
	} 
	if (ret) {
	    m_fsType = 0L;
	} 
	return ret;
}  

int DiskPartition::checkMountStatus()
{   
    char mountBuf[4096] = {0};
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) {
        LogUserOperError("fopen /proc/mounts error\n");
        return -1;    
    }      
    fread(mountBuf, 1, 4096, fp);
    fclose(fp);  
    std::string temp = "/dev/" + m_name;
    if (!strstr(mountBuf, temp.c_str())) {//not find
       LogUserOperDebug("checkMountStatus [%s] not mount\n", temp.c_str());
       return 0;     
    } else {
        LogUserOperDebug("checkMountStatus [%s] is mount\n", temp.c_str());
        return 1;
    }       
}

};//namespace Hippo
