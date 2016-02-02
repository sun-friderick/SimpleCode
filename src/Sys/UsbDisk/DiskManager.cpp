
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <mntent.h>
#include <stdlib.h>
#include "DiskManager.h"
#include "DiskEvent.h"
#include "Assertions.h"
#include <unistd.h>

extern "C"{
extern void LocalPlayerDiskUmount(int type, const char *diskName, int position, const char *mountPoint);
extern int pvr_md5_get(char dest[32 + 1]);    
extern int disk_partition_create(char *dev, int cntOfPartitions, int *dividedPercentage, int systype);
int YX_devdisk_fmat(char *fmdev,int fs_type); 
int YX_getfmat_Progress(int *num,int fs_type);
int yhw_dm_StartDiskCheck(char * path);
int yhw_dm_getDiskCheckProgress(int *percentage);
int yhw_dm_StopDiskCheck(void);
#ifdef INCLUDE_cPVR
void cpvr_module_disable();
void cpvr_module_enable();
#endif
};

namespace Hippo{

DiskManager::DiskManager()
{
    m_havePvrDisk = 0;
    m_devNameList[0] = "sda"; 
    m_devNameList[1] = "sdb";
    m_devNameList[2] = "sdc";
    m_devNameList[3] = "sdd";
    m_devNameList[4] = "sde";
    m_devNameList[5] = "sdf";
    m_devNameList[6] = "sdg";
    m_devNameList[7] = "sdh";
    m_devCount = 8;
    this->refresh();
    
}
    
DiskManager::~DiskManager()
{
    for(std::map<std::string, DiskDev*>::iterator it=m_disk.begin(); it!=m_disk.end(); ++it) {
        delete it->second;
    }  
    m_disk.clear();
}

int DiskManager::refresh()
{
    LogUserOperDebug("DiskManager::refresh\n");
    this->devCheck();
  
    if (m_removeDiskName.size() > 0) {//disk removed
       for(int i = 0; i < m_removeDiskName.size(); i++) {
            if ( m_disk.find(m_removeDiskName.at(i)) != m_disk.end()) {
#ifdef INCLUDE_LocalPlayer
				{
					DiskDev* disk = m_disk.find(m_removeDiskName.at(i))->second;
					int count = disk->getPartitionCount();
					std::string mountPoint = "";
					if (count > 0) {
						for(int index = 0; index < count; index++) {
							DiskPartition *pPartition = disk->getPartition(index);
							mountPoint = mountPoint + "|" + pPartition->getMountPoint();
						}
						LocalPlayerDiskUmount(0, disk->getName().c_str(), 0, mountPoint.c_str());
					}
					delete disk;
				}
#endif
				int pvrFlag = 0;
				if (m_havePvrDisk && (!m_pvrDiskName.compare(m_removeDiskName.at(i)))) {
					pvrFlag = 1;
					m_havePvrDisk = 0;
				} else {
					pvrFlag = 0;
				}
				DiskEvent diskEvent;
				diskEvent.buildEvent("HDD_REMOVED", m_removeDiskName.at(i).c_str(), pvrFlag);
				diskEvent.report();
                
                m_disk.erase(m_removeDiskName.at(i));               
            } else {
                FILE * h_MntFile = setmntent("/proc/mounts", "r");
                if (h_MntFile) {
                	struct mntent *mnt;
                	while((mnt = getmntent(h_MntFile)) != NULL)
                	{          		
                		if (strstr(mnt->mnt_fsname, m_removeDiskName.at(i).c_str())) {//find item
                		    LogUserOperDebug("refresh umount [%s]\n", mnt->mnt_dir);
                			if (umount2(mnt->mnt_dir, MNT_FORCE)) {
                			    LogUserOperDebug("umount [%s] failed\n", mnt->mnt_dir);    
                			}
 
                			break;
                		}
                	}  
                }      		
                endmntent(h_MntFile);
            }
            for (std::vector<std::string>::iterator it = m_umountDiskName.begin(); it != m_umountDiskName.end(); ++it ) {
                if (!it->compare(m_removeDiskName.at(i))) {
                    LogUserOperDebug("Erase umount disk [%s]\n", m_removeDiskName.at(i).c_str());
                    m_umountDiskName.erase(it);
                    break;
                }
            }
       }  
       m_removeDiskName.clear();   
    }

    if (m_umountDiskName.size() > 0) {
        char devBuf[4096] = {0};
        FILE *fp = fopen("/proc/partitions", "r");
        if (!fp) {
            LogUserOperError("fopen /proc/partitions error\n");
            return -1;    
        }      
        fread(devBuf, 1, 4096, fp);
        fclose(fp);        
        for(int i = 0; i < m_umountDiskName.size(); i++) {
             char *pDev = strstr(devBuf, m_umountDiskName[i].c_str());
             if (!pDev) { //this disk is removed
                    m_umountDiskName.erase(m_umountDiskName.begin()+i);
             }
        }
    }

    if (m_addDiskName.size() > 0) {//new disk attached
       for(int i = 0; i < m_addDiskName.size(); i++) {
            if (m_umountDiskName.size() > 0) {
                int findUmountFlag = 0;
                for (int k = 0; k < m_umountDiskName.size(); k++) {
                    if (!m_addDiskName.at(i).compare(m_umountDiskName.at(k))) {
                        findUmountFlag = 1;
                        break;
                    }
                }
                if (findUmountFlag) {
                    LogUserOperDebug("disk [%s] is in umount list\n", m_addDiskName.at(i).c_str());  
                    continue; //this disk is  unmounted now , do not insert into the disk list
                }
            }
            DiskDev* disk = new DiskDev(m_addDiskName.at(i));
            m_disk.insert( std::pair<std::string, DiskDev*>(m_addDiskName.at(i), disk));

            int pvrFlag = 0;
            if ((!m_havePvrDisk) && (getPvrTag(m_addDiskName.at(i), "null") == 1))
                pvrFlag = 1;
            else
                pvrFlag = 0;

            DiskEvent diskEvent;
            diskEvent.buildEvent("HDD_ATTATCHED", m_addDiskName.at(i).c_str(), pvrFlag);
            diskEvent.report();
            diskEvent.buildEvent("HDD_PARTITION_MOUNTED", m_addDiskName.at(i).c_str(), pvrFlag);
            diskEvent.report();
       

       }  
       m_addDiskName.clear();         
    }    
    
    if (m_havePvrDisk) {
        if ( !getDiskDevice(m_pvrDiskName)) {
            m_havePvrDisk = 0;
        }
    }
    return 0;
}    

int DiskManager::getDiskCount()
{
    return m_disk.size();
}
    
DiskDev* DiskManager::getDiskDevice(int pIndex)
{
    if ((pIndex < 0) || (pIndex >= m_disk.size())) {
        LogUserOperError("invalied index\n");
        return NULL;    
    }
    int i = 0;
    for (std::map<std::string, DiskDev*>::iterator it=m_disk.begin(); it!=m_disk.end(); ++it) {
        if (i == pIndex) {
            return it->second;    
        }   
        i++; 
    }  
    return NULL;    
}  

DiskDev * DiskManager::getDiskDevice(const std::string &name)  
{
    std::map<std::string, DiskDev*>::iterator it = m_disk.find(name) ;
    if (it != m_disk.end()) {
        return it->second;    
    } 
    return NULL;
}

int DiskManager::devCheck()
{
    LogUserOperDebug("DiskManager::devCheck\n");
    char devBuf[4096] = {0};
    FILE *fp = fopen("/proc/partitions", "r");
    if (!fp) {
        LogUserOperError("fopen /proc/partitions error\n");
        return -1;    
    }      
    fread(devBuf, 1, 4096, fp);
    fclose(fp);
    char mountBuf[4096] = {0};
    fp = fopen("/proc/mounts", "r");
    if (!fp) {
        LogUserOperError("fopen /proc/mounts error\n");
        return -1;    
    }      
    fread(mountBuf, 1, 4096, fp);
    fclose(fp);    
    
    for(int i = 0; i < m_devCount; i++) {
        LogUserOperDebug("devCheck %d [%s]\n", i, m_devNameList[i].c_str());
        char *pDev = strstr(devBuf, m_devNameList[i].c_str());
        char *pMount = strstr(mountBuf, m_devNameList[i].c_str());
        if ((pDev) && (!pMount)) {// disk attach
            LogUserOperDebug("disk [%s] attach\n", m_devNameList[i].c_str());
            m_addDiskName.push_back(m_devNameList[i]);
 
        }
        if ((!pDev) && (pMount)) {// disk remove
            LogUserOperDebug("disk [%s] remove\n", m_devNameList[i].c_str());
            m_removeDiskName.push_back(m_devNameList[i]);     
        }   
        if ((pDev) && (pMount)) {// disk already mounted
            LogUserOperDebug("disk [%s]already mounted\n", m_devNameList[i].c_str());
            if (m_disk.find(m_devNameList[i])== m_disk.end()) {//not find disk in diskList
                m_addDiskName.push_back(m_devNameList[i]);
            }
        }     
    }
    return 0;
}

int DiskManager::setPvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName)
{
    LogUserOperDebug("setPvrTag disk [%s], partition [%s]n", pvrDiskName.c_str(), pvrPartitionName.c_str());
    if (m_havePvrDisk == 1) {
        if (!m_pvrDiskName.compare(pvrDiskName) && !m_pvrPartitionName.compare(pvrPartitionName)) {
            LogUserOperDebug("disk [%s], partition [%s] has set pvr tag\n", m_pvrDiskName.c_str(), m_pvrPartitionName.c_str());
            return 0;    
        } else {
            removePvrTag(pvrDiskName, pvrPartitionName);
        }   
    }
    DiskDev *disk = getDiskDevice(pvrDiskName);
    if (!disk) {
        LogUserOperError("setPvrTag getDiskDevice error\n");
        return -1;    
    }
    DiskPartition *partition = disk->getPartition(pvrPartitionName);
    if (!partition) {
        LogUserOperError("setPvrTag getPartition error\n");
        return -1;             
    }
    char full_path[1024] = {0};
    snprintf(full_path, 1024, "%s/pvrIdentification.sys", partition->getMountPoint().c_str());
    LogUserOperDebug( "pvr tag full path is [%s]\n", full_path);
    char dest[32 + 1];
    pvr_md5_get(dest);
    FILE *fp = fopen(full_path, "w");
    if (!fp) {
        LogUserOperError("setPvrTag fope file error\n");
        return -1;    
    }
    fwrite(dest, 1, 32, fp);
    fclose(fp);
    
    m_havePvrDisk = 1;
    m_pvrDiskName = pvrDiskName;
    m_pvrPartitionName = pvrPartitionName;
    return 0;    
}

int DiskManager::removePvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName)
{
    DiskDev *disk = getDiskDevice(pvrDiskName);
    if (!disk) {
        LogUserOperError("removePvrTag getDiskDevice error\n");
        return -1;    
    }
    DiskPartition *partition = disk->getPartition(pvrPartitionName);
    if (!partition) {
        LogUserOperError("removePvrTag getPartition error\n");
        return -1;             
    }
    char full_path[1024] = {0};
    snprintf(full_path, 1024, "%s/pvrIdentification.sys", partition->getMountPoint().c_str());
    LogUserOperDebug("remove pvr tag path [%s]\n", full_path );
    unlink(full_path);    
    m_havePvrDisk = 0;
    m_pvrDiskName = "";
    m_pvrPartitionName = "";
    return 0;    
}
    
int DiskManager::getPvrTag(const std::string &pvrDiskName, const std::string &pvrPartitionName)
{
    LogUserOperDebug("getPvrTag [%s],[%s]\n", pvrDiskName.c_str(), pvrPartitionName.c_str());
    DiskDev *disk = getDiskDevice(pvrDiskName);
    if (!disk) {
        LogUserOperError("getPvrTag getDiskDevice error\n");
        return -1;    
    }
    int partitionCount = disk->getPartitionCount();
    if (!partitionCount) {
        LogUserOperError("getPvrTag partitionCount is 0\n");
        return -1;    
    }
    if (!pvrPartitionName.compare("null")) {//when pvrPartitionName is null we check the disk all partition
        for(int i = 0; i < partitionCount; i++) {
            DiskPartition *partition = disk->getPartition(i);
            if (!partition) {
                LogUserOperError("disk->getPartition error\n");
                return -1;    
            }
            if(pvrTagCheck(partition) == 0) {
                if (!m_havePvrDisk) {
                    m_pvrDiskName = pvrDiskName;
                    m_pvrPartitionName = partition->getName();
                    m_havePvrDisk = 1;    
                    LogUserOperDebug("a, default pvr disk not set ,now wet set [%s][%s] as default . \n", m_pvrDiskName.c_str(), m_pvrPartitionName.c_str());
                }                
                return 1;    
            }
        }
    } else { //we check the selected partition
        DiskPartition *partition = disk->getPartition(pvrPartitionName);
        if (!partition) {
            LogUserOperError("getPvrTag getPartition error\n");
            return -1;             
        }   
        if(pvrTagCheck(partition) == 0) {
            if (!m_havePvrDisk) {
                m_pvrDiskName = pvrDiskName;
                m_pvrPartitionName = pvrPartitionName;
                m_havePvrDisk = 1;    
                LogUserOperDebug("b, default pvr disk not set ,now wet set [%s][%s] as default . \n", m_pvrDiskName.c_str(), m_pvrPartitionName.c_str());
            }
            return 1;    
        }          
    }
    return 0;
}

int DiskManager::pvrTagCheck( DiskPartition *partition)   
{
    if (!partition) {
        LogUserOperError("pvrTagCheck error\n");
        return -1;    
    }
    char mountPath[256] = {0};
    snprintf(mountPath, 256, "%s", partition->getMountPoint().c_str());   
    LogUserOperDebug("pvrTag check path [%s]\n", mountPath);
    if(access(mountPath, R_OK | F_OK ) < 0){
        LogUserOperError("access mount dir error\n");
        return -1;
    }
    char key[32 + 1] = {0};
    char full_path[1024] = {0};
    sprintf(full_path, "%s/pvrIdentification.sys", mountPath);
    char dest[32 + 1] = {0};
    FILE *fp = fopen(full_path, "r");
    if (fp) {
        fread(key, 1, 32, fp);
        fclose(fp);
        pvr_md5_get(dest);
        if (memcmp(key, dest, 32) == 0){//find pvr partition
            LogUserOperDebug("find prv tag in %s\n", mountPath);
            return 0;//ok
        }
    } 
    return -1;   
}

int DiskManager::umountDisk(std::string &name)
{
     DiskDev *disk = getDiskDevice(name);
     if (!disk) {
        LogUserOperDebug("error umountDisk not find disk [%s]\n", name.c_str());
        return -1;
     }
#ifdef INCLUDE_cPVR
     if (!m_pvrDiskName.compare(disk->getName())) {
        cpvr_module_disable();
        sleep(2);
     }
#endif
#ifdef INCLUDE_LocalPlayer  
	 int count = disk->getPartitionCount();
	 std::string mountPoint = "";
	 if (count > 0) {
		 for(int index = 0; index < count; index++) {
			 DiskPartition *pPartition = disk->getPartition(index);
			 mountPoint = mountPoint + "|" + pPartition->getMountPoint();
		 }
		 LocalPlayerDiskUmount(0, disk->getName().c_str(), 0, mountPoint.c_str());
	 }
#endif

     m_umountDiskName.push_back(name);
     disk->umount();
     m_disk.erase(name);
#ifdef INCLUDE_cPVR
     if (!m_pvrDiskName.compare(disk->getName())) {
        cpvr_module_enable();
     }
#endif
     return 0;
}

int DiskManager::formatPartition(const std::string &disk, const std::string &partition, int type)
{
    if (m_formateStatus == 1) {
        LogUserOperDebug("formatPartition thread is running\n");
        return 0;        
    }
    m_formateStatus = 1;
    m_formatProgress = 0;
    m_formatDisk = disk;
    m_formatPartition = partition;
    m_formatType = type;    
    LogUserOperDebug("DiskManager::formatPartition \n");
    if (pthread_create(&m_formatThreadID, NULL, format, (void *)this)) {
        LogUserOperError("pthread_create format error\n");
        m_formateStatus = 0;
    }
    
    return 0;
}

int DiskManager::getFormatProgress()
{
    return m_formatProgress;
}

int DiskManager::checkPartition(const std::string &disk, const std::string &partition)
{
    if (m_checkStatus == 1) {
        LogUserOperDebug("checkPartition thread is running\n");
        return 0;
    }
    m_checkStatus = 1;
    m_checkProgress = 0;
    m_checkDisk = disk;
    m_checkPartition= partition; 
    LogUserOperDebug("DiskManager::checkPartition \n");
    if (pthread_create(&m_checkThreadID, NULL, check, (void *)this)) {
        LogUserOperError("pthread_create checkPartition\n");
        m_checkStatus = 0;
    }

    return 0;
}

int DiskManager::getPartitionCheckProgress()
{
    return m_checkProgress;
}

void* DiskManager::format(void *p)
{
    if (!p) {
        LogUserOperError("format error\n");
        return NULL;
    }

    DiskManager *self = (DiskManager*)p;
    self->m_formateStatus = 1;
    self->m_formatProgress = 5;
    pthread_detach(self->m_formatThreadID);
    LogUserOperDebug("##start format##\n" );
    if (!self->m_formatPartition.compare("null")) { //format total disk, merge partition
        LogUserOperDebug("format merge disk partition\n");
#ifdef INCLUDE_cPVR
        if (!self->m_pvrDiskName.compare(self->m_formatDisk)) {//we format pvr disk
            cpvr_module_disable();
            sleep(4);
        }
#endif
        std::string diskName = "/dev/" + self->m_formatDisk;
        self->umountDisk(self->m_formatDisk);      
        self->m_formatProgress = 10;
        sleep(1);
        self->m_formatProgress = 30;
        int dividedPercentage = 100;
        disk_partition_create((char *)diskName.c_str(), 1, &dividedPercentage, 0x83);
        self->m_formatProgress = 50;
        std::string newPartition = diskName + "1";
        sleep(1);
        self->m_formatProgress = 60;
        YX_devdisk_fmat((char *)newPartition.c_str(), 3); 
        self->m_formatProgress = 90;
        sleep(1);
        self->m_umountDiskName.clear();
        self->refresh();
        self->m_formatProgress = 95;
        sleep(1);
#ifdef INCLUDE_cPVR
        if (!self->m_pvrDiskName.compare(self->m_formatDisk)) {//we format pvr disk
            cpvr_module_enable();
        }
#endif
    } else {//format select partition
        LogUserOperDebug("Format partition [%s]\n", self->m_formatPartition.c_str());
#ifdef INCLUDE_cPVR
        if (!self->m_pvrPartitionName.compare(self->m_formatPartition)) {//we format pvr disk
            cpvr_module_disable();
            sleep(3);
        }
#endif
        DiskDev *disk = self->getDiskDevice(self->m_formatDisk);
        if (!disk) {
            LogUserOperError("disk is NULL\n");
            goto ERR;
        }
        DiskPartition * partition = disk->getPartition(self->m_formatPartition);
        if (!partition) {
            LogUserOperError("partition is NULL\n");
            goto ERR;            
        }
        std::string partitionName = "/dev/" + self->m_formatPartition;
        self->m_formatProgress = 10;
        sleep(2);
        partition->umountPartition();
        self->m_formatProgress = 30;
        sleep(2);
        self->m_formatProgress = 50;
        LogUserOperDebug("YX_devdisk_fmat partition [%s]\n", partitionName.c_str());
        YX_devdisk_fmat((char *)partitionName.c_str(), 3);  // type 1 fat 2 xfs 3 ext3 
        self->m_formatProgress = 70;
        sleep(1);
        self->m_formatProgress = 80;
        partition->mountPartition();
        sleep(2);
#ifdef INCLUDE_cPVR
        if (!self->m_pvrPartitionName.compare(self->m_formatPartition)) {//we format pvr disk
            cpvr_module_enable();
        }
#endif
    }

ERR:
    LogUserOperDebug("###format finish\n");
    self->m_formatProgress = 100;
    self->m_formateStatus = 2;
    return NULL;
}

void* DiskManager::check(void *p)
{
    if (!p) {
        LogUserOperError("check error\n");
       return NULL;
    }
    DiskManager *self = (DiskManager*)p;
    self->m_checkStatus = 1;
    self->m_checkProgress = 1;
    pthread_detach(self->m_checkThreadID);
    LogUserOperDebug("###start check###\n");
    DiskDev *disk =  self->getDiskDevice(self->m_checkDisk);
    if (!disk) {
        LogUserOperError("disk is null\n");
        goto FINISH;
    }    
    
    if (!self->m_checkPartition.compare("null")) {//check every partition
#ifdef INCLUDE_cPVR
        if (!self->m_pvrDiskName.compare(self->m_checkDisk)) {//we check pvr disk
            cpvr_module_disable();
            sleep(4);
        }
#endif
        for (int i = 0; i < disk->getPartitionCount(); i++) {
            if (self->m_checkProgress < 80)
                self->m_checkProgress += 8;
            DiskPartition * partition = disk->getPartition(i);
            if (!partition) {
                LogUserOperError("partition is null\n");
                goto FINISH;
            }
            if (partition->getFsType() != 0xEF53) { //we now only support ext filesystem check
                LogUserOperDebug("we now only support ext filesystem check\n");
                continue;
            }
            self->m_checkPartition = partition->getName();
            std::string partitionName = "/dev/" + partition->getName();
            std::string mountPoint = partition->getMountPoint();
            partition->umountPartition();
            
            yhw_dm_StartDiskCheck((char *)partitionName.c_str());
            partition->mountPartition();     
            self->m_checkProgress = 90;
        }         
    } else {
#ifdef INCLUDE_cPVR
        if (!self->m_pvrPartitionName.compare(self->m_checkPartition)) {//we format pvr disk partition
            cpvr_module_disable();
            sleep(4);
        }
#endif
        DiskPartition * partition = disk->getPartition(self->m_checkPartition);
        if (!partition) {
            LogUserOperError("partition is null\n");
            goto FINISH;
        }
        if (partition->getFsType() != 0xEF53) { //we now only support ext filesystem check
            LogUserOperDebug("we now only support ext filesystem check\n");
            goto FINISH;
        }      
        self->m_checkProgress = 10;
        std::string partitionName = "/dev/" + self->m_checkPartition;
        std::string mountPoint = partition->getMountPoint();
        partition->umountPartition();
        yhw_dm_StartDiskCheck((char *)partitionName.c_str());
        while(1) {
            int progress = 0;
            yhw_dm_getDiskCheckProgress(&progress);
            LogUserOperDebug("\nget check progress [%d]\n", progress );
            self->m_checkProgress += progress/2;
            if ((progress < 0) || (progress >= 100)) {
                break;
            }
            sleep(1);
        }
        self->m_checkProgress = 75;
        sleep(1);
        self->m_checkProgress = 80;
        sleep(1);
        self->m_checkProgress = 90;
        partition->mountPartition();
    }

FINISH:
#ifdef INCLUDE_cPVR
    if (!self->m_pvrPartitionName.compare(self->m_checkPartition) 
        || !self->m_pvrDiskName.compare(self->m_checkDisk)) {//we check pvr disk
        cpvr_module_enable();
    }
#endif
    LogUserOperDebug("###check finish\n");
    self->m_checkProgress = 100;
    self->m_checkStatus = 2;
    return NULL;
}

};//namespace Hippo

