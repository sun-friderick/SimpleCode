
extern "C"{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <unistd.h>
#include <arpa/inet.h>
}

#include "LogC.h"


/**
    LogLevel:  "Assert", "Fatal!", "Error!", "Warning", "Info", "Verbose", "Debug", "Undefined"
                  无        1         1         3          4        2         2         无
    转义序列相关的常用参数如下(通过man console_codes命令可查看更多的参数描述)：
        显示：0(默认)、1(粗体/高亮)、22(非粗体)、4(单条下划线)、24(无下划线)、5(闪烁)、25(无闪烁)、7(反显、翻转前景色和背景色)、27(无反显)
        颜色：0(黑)、1(红)、2(绿)、 3(黄)、4(蓝)、5(洋红)、6(青)、7(白)
        前景色为30+颜色值，如31表示前景色为红色；背景色为40+颜色值，如41表示背景色为红色。
**/
static const uint8_t color[] = {0, 0x41, 0x41, 0x43, 0x04, 0x02, 0x02, 0x02};
#define setLogColor(x)  \
    {               \
        if(x < 4)   \
            fprintf(stderr, "\033[1;%d;3%dm", color[x]>>4, color[x]&15);  \
        else        \
            fprintf(stderr, "\033[%d;3%dm", color[x]>>4, color[x]&15);    \
    }
#define resetLogColor()   fprintf(stderr, "\033[0m")

/** 日志level名称 **/
static const char *textLogLevel[] = {"Assert", "Fatal!", "Error!", "Warning", "Info", "Verbose", "Debug", "Undefined"};

/** 日期字符串：月份 & 星期 **/
static const char *monthStr[] = {"Reserved", 
                                                                "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 
                                                                "Undefined"};
static const char *weekStr[]  = {"Sun.", "Mon.", "Tues.", "Wed.", "Thur.", "Fri.", "Sat.", "Undefined"};



/**
* 获取当前时间：年-月-日-星期几   时-分-秒-毫秒
* 输出格式：
*       [month-day-year, weekday][hour:min:sec.msec]
* flag: 0: no output
        1: output date
        2: output time
        3: output date&time
**/
static int get_CurrentTime(char *buf, int flag)
{
    if (flag == 0)
        return 0;
    static DateTime sDTime;
    struct timeval current;
    struct tm tempTime;

    if (!gettimeofday(&current, NULL)) {
        localtime_r(&current.tv_sec, &tempTime);
        sDTime.mYear       = tempTime.tm_year + 1900;
        sDTime.mMonth      = tempTime.tm_mon + 1;
        sDTime.mDayOfWeek  = tempTime.tm_wday;
        sDTime.mDay        = tempTime.tm_mday;
        sDTime.mHour       = tempTime.tm_hour;
        sDTime.mMinute     = tempTime.tm_min;
        sDTime.mSecond     = tempTime.tm_sec;
        sDTime.muSecond    = current.tv_usec;
    }

    if (flag & 0x01)
        sprintf(buf, "[%s-%02d-%04d, %s]", monthStr[sDTime.mMonth], sDTime.mDay, sDTime.mYear, weekStr[sDTime.mDayOfWeek]);
    if (flag & 0x02)
        sprintf(buf + strlen(buf), "[%02d:%02d:%02d.%03d] ", sDTime.mHour, sDTime.mMinute, sDTime.mSecond, sDTime.muSecond / 1000);

    //printf("get_CurrentTime: buf[%s]...\n", buf);
    return strlen(buf);
}



/**
 * 检查网卡链接状态:
 *              LinkedState_Up                    0
                 LinkedState_Down              -1
                 NetDevice_OpenError       -2
 * 
 */
 #define  LinkedState_Up                    0
 #define  LinkedState_Down              -1
 #define  NetDevice_OpenError       -2
static int checkLinkState(const char *device)  
{  
    struct ifreq ifr;  
    int retState = 0;
    int skfd = socket(AF_INET, SOCK_DGRAM, 0);  
  
    strcpy(ifr.ifr_name, device);  
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)  {  
        close(skfd);  
        return NetDevice_OpenError;  
    }  

    if(ifr.ifr_flags & IFF_RUNNING){  
        //printf(" LinkState: link up\n");  
        close(skfd);  
        retState = LinkedState_Up; // 网卡已插上网线  
    } else {  
        //printf(" LinkState: link down\n");  
        close(skfd);  
        retState = LinkedState_Down;  
    }  
    return retState;
}  


/**
* 获取设备信息： mac，hostname，ip，串号等
* flag: 0: no output
        1: output mac
        2: output ip
        3: output mac&ip
**/
static int get_MachineInfo(const char *device, char *buf, int flag)
{
    if (flag == 0)
        return 0;
    unsigned char macAddr[6] = {0};  //6是MAC地址长度
    unsigned char ipAddr[18] = {0};
    unsigned char hostName[32] = {"Linux"};
    int sockfd;
    struct ifreq ifr4dev;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //internet协议族的数据报类型套接口
    strncpy(ifr4dev.ifr_name, device, sizeof(ifr4dev.ifr_name) - 1);   //将设备名作为输入参数传入

    //获取MAC地址
    ifr4dev.ifr_hwaddr.sa_family = ARPHRD_ETHER;  //此处需要添加协议，网络所传程序没添加此项获取不了mac。
    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr4dev) == -1) {
        printf("get_MachineInfo: get device [%s] mac error.\n", device);
        return -1;
    }
    memcpy(macAddr, ifr4dev.ifr_hwaddr.sa_data, ETH_ALEN);

    //获取ip地址
    if (ioctl(sockfd, SIOCGIFADDR, &ifr4dev) == -1) {
        printf("get_MachineInfo: get device [%s] ip error.\n", device);
        return -1;
    }
    /**
        struct sockaddr是通用的套接字地址，而struct sockaddr_in则是internet环境下套接字的地址形式，
        二者长度一样，都是16个字节。二者是并列结构，指向sockaddr_in结构的指针也可以指向sockaddr。
        一般情况下，需要把sockaddr_in结构强制转换成sockaddr结构再传入系统调用函数中。
    **/
    struct sockaddr_in *sin =  (struct sockaddr_in *)&ifr4dev.ifr_addr;
    strcpy((char *)ipAddr, inet_ntoa(sin->sin_addr));
    close(sockfd);

    //get hostName
    //gethostname((char *)hostName, sizeof(hostName));
    sprintf(buf, "[%s]", hostName);

    if (flag & 0x01)
        sprintf(buf + strlen(buf), "[%02x:%02x:%02x:%02x:%02x:%02x] ", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    if (flag & 0x02)
        sprintf(buf + strlen(buf) , "[%s] ", ipAddr);

    //printf("get_MachineInfo: mac&ip buf=[%s]\n", buf);
    return (int)strlen(buf);
}


/**
* 设置log输出颜色并输出：
*       日志level :         颜色及输出设置：
        "Fatal!"            1(粗体/高亮)、4(单条下划线)、1(红)
        "Error!"            1(粗体/高亮)、4(单条下划线)、1(红)
        "Warning"           1(粗体/高亮)、4(单条下划线)、3(黄)
        "Info"              4(蓝)
        "Verbose"           2(绿)
        "Debug"             2(绿)
**/
static int useColor = -1;
static int colored_LogOutput(int level, char *str)
{
    if (useColor < 0) {
        useColor = !getenv("NO_COLOR") && !getenv("LOG_FORCE_NOCOLOR") && ((getenv("TERM") && isatty(1)) || getenv("LOG_FORCE_COLOR"));
    }

    if (useColor)
        setLogColor(level);
    if ((level <= LOG_LEVEL_Assert) || (level >= LOG_LEVEL_Undefined)) {
        printf("colored_LogOutput: level is [%d],error.\n", level);
        fputs(str, NULL);
    } else
        fputs(str, stdout);  //输出到stdout
    if (useColor)
        resetLogColor();

    return 0;
}

/**
* 净化log输出信息：
*        可输出的字符范围：   0x08(退格)———— 0x0D(回车键)； 0x20(空格)———— 0x7F(删除)
*        其他字符均使用 ‘？’替代输出；
**/
static void logLineSanitize(uint8_t *str)
{
    int k = 0;
    while (*str) {
        if (*str < 0x08 || (*str > 0x0D && *str < 0x20)) {
            *str = '?';
            k++;
        }
        str++;
    }
    return ;
}


#define NetDevice_ETH0       "eth0"
#define NetDevice_WLAN0     "wlan0"
#define NetDevice_Local     "lo"
/***
输出格式:
    [month-day-year, weekday][hour:min:sec.msec] [HostMac][HostIP] [Major.Minor.BuildVersion:ModuleVersion][ModuleName] [ErrorCode][file:line][function:LogLevel] : logBuffer_info.
***/
void logVerboseCStyle(const char *file, int line, const char *function,  int level, const char *fmt, ...)
{
    va_list args;
    char currTime[48] = {0};
    char machineInfo[64] = {0};
    static char sLogBuffer[512] = { 0 };
    static char str[1024] = {0};

    //file
    char* pFile = (char*)strchr(file, '/');
    char* tmpFile = (char*)file;
    for ( ; ; ){
        if(pFile != NULL){
            tmpFile = pFile + 1;
            pFile = (char*)strchr(tmpFile, '/');
        } else { 
            //printf("file name [%s]\n", tmpFile);
            break;
        }
    }

    // date & time
    get_CurrentTime((char *)&currTime, OUTPUTFlag_DateTime);

    // mac & ip
    char netDevice[8] = {0}; 
    if(checkLinkState(NetDevice_ETH0) != LinkedState_Up){
        if(checkLinkState(NetDevice_WLAN0) != LinkedState_Up){
            checkLinkState(NetDevice_Local) ;
            strcpy(netDevice, NetDevice_Local);
            //printf("net device [%s && %s]  all down ,only use [%s].\n", NetDevice_ETH0, NetDevice_WLAN0, NetDevice_Local);
        } else {
            //printf("net device [%s] up.\n", NetDevice_WLAN0);
            strcpy(netDevice, NetDevice_WLAN0);
        }
    } else {
        //printf("net device [%s] up.\n", NetDevice_ETH0);
        strcpy(netDevice, NetDevice_ETH0);
    }
    get_MachineInfo( netDevice, (char *)&machineInfo, OUTPUTFlag_MachineInfo);

    // args
    va_start(args, fmt);
    vsnprintf(sLogBuffer, 512, fmt, args);
    va_end(args);

    snprintf(str, strlen(currTime) + 1, "%s", currTime);
    snprintf(str + strlen(str), strlen(machineInfo) + 1, "%s", machineInfo);
    sprintf(str + strlen(str), "[%s:%d][%s:%s] ", (strchr(tmpFile, '/') ?  (strchr(tmpFile, '/') + 1) : tmpFile), line, function, textLogLevel[level]);
    sprintf(str + strlen(str), ": %s", sLogBuffer);

    logLineSanitize((uint8_t *)str);
    colored_LogOutput(level, str);
    return ;
}




