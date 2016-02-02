
#ifndef __LIVE_LOG_H__
#define __LIVE_LOG_H__

#include "LogC.h"


/** 
* g_liveLogLevel： 设置当前模块的log输出等级，每个模块都可设置独立的log输出等级 
**/

static int g_liveLogLevel = LOG_LEVEL_Debug;


#define liveLogFatal(args...)     		LogFatal( g_liveLogLevel, args)
#define liveLogError(args...)     		LogError( g_liveLogLevel, args)
#define liveLogWarning(args...)   	LogWarning( g_liveLogLevel, args)
#define liveLogInfo(args...)      		LogInfo( g_liveLogLevel, args)
#define liveLogVerbose(args...)   	LogVerbose( g_liveLogLevel, args)
#define liveLogDebug(args...)     		LogDebug( g_liveLogLevel, args)





#endif   //__LIVE_LOG_H__
