#ifndef DISK_INFO_H
#define DISK_INFO_H

#include <sys/types.h>
#include <dirent.h>
#include "json/json.h"
#include "json/json_object.h"
#include "json/json_public.h"

#define MID_MAX_PARTITION_NUM 8
#define _NAME_LEN 32 
#define DISK_NAME_LEN _NAME_LEN
#define PARTITION_NAME_LEN _NAME_LEN
#define MOUNT_DIR_LEN 64
#define FS_TYPE_LEN 16
#define DEVICE_NAME_LEN 8

//#define YX_USBPLUGIN_DISK      0x7E
//#define YX_USBPLUGOUT_DISK     0x7F

struct pvr_partition{
    char disk_name[32];
    char partition_name[32];
    char mounted_dir[128];
    int disk_size;
    int disk_free_size;
    int partition_size;
    int partition_free_size;
};

#define yx_track()  if(OUTPUT_LEVEL >= DBG_OUTPUT_LEVEL) PRINTF("%s:%d\n", __FUNCTION__, __LINE__)
#define yx_msg(string, args... ) PRINTF(string, ##args )

typedef enum {
    HDD_ATTACHED,
    HDD_REMOVED,
    FILE_SYSTEM_NOT_SUPPORTED,
    HDD_FORMATE_FAILED,
    HDD_WITHOUT_PARTITION,
    HDD_NO_PVR_PARTITION,
    HDD_MOUNT_FAILED,
    FILE_SYSTEM_REPAIR_IN_PROGRESS,
    FILE_SYSTEM_REPAIR_FINISHED,
    HDD_PARTITION_MOUNTING,
    HDD_PARTITION_MOUNTED,
    FILE_SYSTEM_DAMAGED,
    AVAILABLE_SPACE_REACH_90,
    AVAILABLE_SPACE_REACH_100	
}STORAGE_EVENT;

typedef struct partition_info {
    char partition_name[PARTITION_NAME_LEN + 1];
    int is_mounted;
    char mount_dir[MOUNT_DIR_LEN + 1];
    char fs_type[FS_TYPE_LEN + 1];
    unsigned int partition_size;
    unsigned int partition_free_size;
}partition_info_t;

typedef struct disk_info {
    char disk_name[DISK_NAME_LEN + 1];
    unsigned int size;
    unsigned int free_size;
    int partition_num;
    partition_info_t partition[MID_MAX_PARTITION_NUM];
}disk_info_t;

void add_error_table(void * p );
void remove_error_table(void *p);
int storage_manage_init();
int storage_manage_cnt_get();
int storage_manage_info_get_by_index(int pIndex, disk_info_t * p_info);
void disk_info_print(disk_info_t * p_info);
int storage_manage_space_get_fat(const char *dev_name, const char *path, unsigned int *ptotal, unsigned int *pavail);
int pvr_flag_check(char *mount_dir);
int storage_manage_space_get(const char *path, unsigned int *ptotal, unsigned int *pavail, unsigned int *preserve);
int disk_connect_type(char *devName,int *positon);
int check_dev(char *dev_name);
int uninited_disk_get(char *device_name, int *device_size/*MB*/);
int storage_manage_info_get_by_name(char *name, disk_info_t * p_info);
int storage_manage_partition_get_by_name(char *partition_name, partition_info_t * p_partition_info);
static int pvr_flag_rm(char *mount_dir);
void remove_old_pvr_partition(char *new_pvr_partition);
int pvr_flag_set(char *mount_dir);
static int disk_mount_set(char *fs_name, char *mounted_dir, char *fs_type);
int disk_mount_info_update();
static int disk_list_print();
static inline int line_info(char *str, char *name);
int storage_manage_info_update();
int kill_process_by_name(char *process_name);
int run_process_by_name(char *process_name);
int storage_manage_partition_get_by_mount_dir(char *mount_dir, partition_info_t * p_partition_info);
int free_mount_dir_get(char *dir, int len);
int storage_state_change_event_send(STORAGE_EVENT evt, char *device);
static char *fReadLine(FILE *pf);
static int chmod_disk_mark(void);
static int chmod_disk_markcheck(void);
static int chmod_disk(void);
static inline int get_partition_name(char *str, char *name);
void _mount_partition();
int json_add_disk_str(char *dst_str, const char *src_name, char *src_value);
int json_add_disk_int(char *dst_str, const char *src_name, int src_value);
int backspace_comma_disk(char *str);
int param_string_disk_get(struct json_object* object, const char *param_name, char *ret, int ret_max_len);
int json_add_disk_rightbrace(char *dst_str);
char file_system_get(char *file_type_str);
int pvr_partition_set( );
int storage_in_out_event(int msg_type, int disk_index);

int pvr_partition_info_get(struct pvr_partition *p_pvr_partition);
#endif

