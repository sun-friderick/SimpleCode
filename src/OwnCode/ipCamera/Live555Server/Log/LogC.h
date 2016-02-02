#ifndef __LOG_C_H__
#define __LOG_C_H__

#include <inttypes.h>


/** 
* Date & Time output:
    0: no output
    1: output date 
    2: output time
    3: output date&time
**/
#define OUTPUTFlag_DateTime      2

/** 
* MachineInfo output:
    0: no output
    1: output mac 
    2: output ip
    3: output mac&ip
 **/
#define OUTPUTFlag_MachineInfo   0

/***
* LogLevel: 
*        "Assert"  : 0
*        "Fatal!"  : 1
*        "Error!"  : 2
*        "Warning" : 3
*        "Info"    : 4
*        "Verbose  : 5
*        "Debug"   : 6
*        "Undefined" : default
***/
#define LogFatal( level, args...) \
        do {                              \
            if (level >= LOG_LEVEL_Fatal) \
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Fatal, args); \
        } while(0)

#define LogError( level, args...) \
        do {                              \
            if (level >= LOG_LEVEL_Error) \
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Error, args); \
        } while(0)

#define LogWarning( level, args...) \
        do {                                \
            if (level >= LOG_LEVEL_Warning) \
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Warning, args); \
        } while(0)

#define LogInfo( level, args...)  \
        do {                              \
            if (level >= LOG_LEVEL_Info)\
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Info, args);  \
        } while(0)

#define LogVerbose( level, args...) \
        do {                                \
            if (level >= LOG_LEVEL_Verbose) \
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Verbose, args); \
        } while(0)

#define LogDebug( level, args...) \
        do {                              \
            if (level >= LOG_LEVEL_Debug) \
                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__,  LOG_LEVEL_Debug, args);  \
        } while(0)

        
/**
* 时间：年-月-日-星期几   时-分-秒-毫秒 
* 格式：
*   [month-day-year, weekday][hour:min:sec.msec]
**/
typedef struct DT {
	uint16_t mYear;          //!< e.g. 2005
	uint8_t  mMonth;         //!< 1...12
	uint8_t  mDayOfWeek;     //!< 0...6, 0==Sunday
	uint8_t  mDay;           //!< 1...31
	uint8_t  mHour;          //!< 0...23
	uint8_t  mMinute;        //!< 0...59
	uint8_t  mSecond;        //!< 0...59
    uint32_t muSecond;       //!< 0...999999
} DateTime;




/** 
* 在代码中使用了不同等级的log输出宏函数，要控制日至输出的多少，只需要设置logLevel的等级即可；
* 等级越高（或者说是值越大），可输出的log越多，举例：
* 		当logLevel设置为 LOG_LEVEL_INFO ，即 level = 3 时，在代码中设置的 level = 4（或是使用
* MODULE_LOG_VERBOSE()宏函数）的打印日志不能输出，设置为 level <= 3（或是使用MODULE_LOG_ERROR()，
* MODULE_LOG_WARNING()，MODULE_LOG()宏函数）的日志都可以输出；
* 详细说明参考以下代码；
 **/
enum LogLevel {
    LOG_LEVEL_Assert   = 0x00,  /** for pre-compile print log **/
    LOG_LEVEL_Fatal    = 0x01,
    LOG_LEVEL_Error    = 0x02,  /**  **/
    LOG_LEVEL_Warning  = 0x03,  /** **/
    LOG_LEVEL_Info     = 0x04,  /** **/
    LOG_LEVEL_Verbose  = 0x05,  /** **/
    LOG_LEVEL_Debug    = 0x06,
    LOG_LEVEL_Undefined 
};



/***
* 注册模块到 g_moduleList
* 返回 当前模块在g_moduleList列表中的位置
*       g_moduleCount表示当前注册拥有的module数量
**/
int registerModule(char* name, char* version, int logtype);

/***
输出格式:
    [month-day-year, weekday][hour:min:sec.msec] [HostMac][HostName][Major.Minor.BuildVersion:ModuleVersion] [Module][file:line][function:log_level] : logBuffer_info.
***/
void logVerboseCStyle(const char* file, int line, const char* function, int level, const char* fmt, ...);




#endif
