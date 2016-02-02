#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

struct format_info{
    char fmdir[128 + 1];
    char mountedDir[128 + 1];
    int fs_type;
};

static int disk_manager_device_info(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_disk_info(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_pvr_flag_set(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_disk_format(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_partition_format(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_disk_initialize(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
static int disk_manager_format_progress_get(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
//static int disk_manager_fs_restore_start(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
//static int disk_manager_fs_restore_progress_get(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );
//static int disk_manager_fs_restore_stop(const char *aFieldName, const char *param_json, char *aFieldValue, int aResult );

#endif

