/*****************************************************************************
 *
 *  死机堆栈打印模块。。。。。
 *  不用任何初始化，编译时加上-finstrument-functions参数即可。
 *  生成的可执行程序不要strip。
 *  
 *  由于动态库的加载基址不确定。。因此不支持库内符号查找。。
 *  如果死在库里，请用core dump。
 *
 *  死机后会输出一个pid一个tid。按照线程号(tid)查找调用即可。
 *
 *  这个模块本身的打印由环境变量控制。 开打印需设置 FF_LOGLEVEL=1
 *
 *  Author: Sun
 *  Date:   2015-10-15
 *
 ******************************************************************************/

 
 
/***********************************************************************************************************
 *                               linux信号signal常用信号
 ***********************************************************************************************************
 Linux支持的信号列表如下：

    处理动作一项中的字母含义如下 :
        Term        Terminate, 缺省的动作是终止进程   
        Ign         Ignore, 缺省的动作是忽略此信号 
        Term&Core   Terminate and dump core, 缺省的动作是终止进程并进行内核映像转储（dump core）
        Stop        Stop, 缺省的动作是停止进程   
        NCaptured   Signals can not be captured, 信号不能被捕获       
        NIgn        Signals can not be ignored, 信号不能被忽略      

 ***********************************************************************************
 *                  下面是POSIX.1中列出的信号：
 ***********************************************************************************
    信号        值         处理动作            发出信号的原因 
    ---------------------------------------------------------------------- 
    SIGHUP      1           Term                终端挂起或者控制进程终止 
    SIGINT      2           Term                键盘中断（如break键被按下） 
    SIGQUIT     3           Term&Core           键盘的退出键被按下 
    SIGILL      4           Term&Core           非法指令 
    SIGABRT     6           Term&Core           由abort(3)发出的退出指令 
    SIGFPE      8           Term&Core           浮点异常 
    SIGKILL     9           Term&NCaptured&NIgn Kill信号 
    SIGSEGV     11          Term&Core           无效的内存引用 
    SIGPIPE     13          Term                管道破裂: 写一个没有读端口的管道 
    SIGALRM     14          Term                由alarm(2)发出的信号 
    SIGTERM     15          Term                终止信号 
    SIGUSR1     30,10,16    Term                用户自定义信号1 
    SIGUSR2     31,12,17    Term                用户自定义信号2 
    SIGCHLD     20,17,18    Ign                 子进程结束信号 
    SIGCONT     19,18,25    Continue            进程继续（曾被停止的进程） 
    SIGSTOP     17,19,23    Stop&NCaptured&NIgn 终止进程 
    SIGTSTP     18,20,24    Stop                控制终端（tty）上按下停止键 
    SIGTTIN     21,21,26    Stop                后台进程企图从控制终端读 
    SIGTTOU     22,22,27    Stop                后台进程企图从控制终端写 

    
 ***********************************************************************************
 *             下面的信号没在POSIX.1中列出，而在SUSv2列出：
 ***********************************************************************************
    信号        值          处理动作    发出信号的原因 
    -------------------------------------------------------------------- 
    SIGBUS      10,7,10     Term&Core   总线错误(错误的内存访问) 
    SIGPOLL                 Term        Sys V定义的Pollable事件，与SIGIO同义 
    SIGPROF     27,27,29    Term        Profiling定时器到 
    SIGSYS      12,-,12     Term&Core   无效的系统调用 (SVID) 
    SIGTRAP     5           Term&Core   跟踪/断点捕获 
    SIGURG      16,23,21    Ign         Socket出现紧急条件(4.2 BSD) 
    SIGVTALRM   26,26,28    Term        实际时间报警时钟信号(4.2 BSD) 
    SIGXCPU     24,24,30    Term&Core   超出设定的CPU时间限制(4.2 BSD) 
    SIGXFSZ     25,25,31    Term&Core   超出设定的文件大小限制(4.2 BSD) 
（对于 SIGSYS,SIGXCPU,SIGXFSZ,以及某些机器体系结构下的SIGBUS，
  Linux缺省的动作是 Term(terminate)，SUSv2缺省的动作是 Term&Core(terminate and dump core)）。 


 ***********************************************************************************
 *                          下面是其它的一些信号 :
 ***********************************************************************************
    信号        值         处理动作    发出信号的原因 
    ---------------------------------------------------------------------- 
    SIGIOT      6           Term&Core   IO捕获指令，与SIGABRT同义 
    SIGEMT      7,-,7 
    SIGSTKFLT   -,16,-      Term        协处理器堆栈错误 
    SIGIO       23,29,22    Term        某I/O操作现在可以进行了(4.2 BSD) 
    SIGCLD      -,-,18      Term        与SIGCHLD同义 
    SIGPWR      29,30,19    Term        电源故障(System V) 
    SIGINFO     29,-,-      Term        与SIGPWR同义 
    SIGLOST     -,-,-       Term        文件锁丢失 
    SIGWINCH    28,28,20    Ign         窗口大小改变(4.3 BSD, Sun) 
    SIGUNUSED   -,31,-      Term        未使用的信号(will be SIGSYS) 
（在上表中，- 表示信号没有实现；）
（有三个值的含义为：第一个值通常在Alpha和Sparc上有效，中间的值对应i386和ppc以及sh，最后一个值对应mips。）
（信号29在Alpha上为SIGINFO / SIGPWR ，在Sparc上为SIGLOST。）   
 *   
 *************************************************************************************************************/

 
 
/******************************************************************************************************************
 *  
 *  收集一个函数调用的踪迹，一种方法是通过在函数的入口处和出口处插入一个打印语句来检测。这个过程非常繁琐，而且很容易出错，通常需要对源代码进行大量的修改。
    幸运的是，GNU 编译器工具链（也称为 gcc）提供了一种自动检测应用程序中的各个函数的方法。
        gcc 加上该选项 -finstrument-functions
        在检测函数入口，会调用: void __cyg_profile_func_enter( void *func_address, void *call_site )
        在检测函数出口，会调用：void __cyg_profile_func_exit ( void *func_address, void *call_site )
        __cyg 开头，是Cygnus 的贡献.
    在执行应用程序时，就可以收集调用者地址和被调用着地址，如果加上 -g 选项，增加调试信息，则用addr2line -f 可以找到地址对应的函数名称。
 *  
 ******************************************************************************************************************/
 
#ifndef __SYSTEM_INSTRUMENT_H__
#define __SYSTEM_INSTRUMENT_H__


#include <pthread.h>



#define MAX_TRACE_THREADS       128

#define NOFF        __attribute__((__no_instrument_function__))
#define GETTID()    syscall(SYS_gettid)

#define PRINTF(X, ...)\
        do {                                \
            if (log_level) {                \
                log_printf(__FILE__, __LINE__, __func__, X, ##__VA_ARGS__);\
            }                               \
        } while(0)

typedef struct  _DEBUGNODE {
    struct _DEBUGNODE   *   prev;
    struct _DEBUGNODE   *   next;
    void                *   func;
    void                *   call_site;
} DEBUGNODE;


static struct _NODEHEADS {
    pthread_t       tid;
    DEBUGNODE   *   head;
    DEBUGNODE   *   tail;
} nodes[MAX_TRACE_THREADS];


#ifdef __cplusplus__
extern "C" {
#endif

void  _dump(const char * buffer, int size);
void NOFF Trace_Out(void);
void NOFF __cyg_profile_func_enter(void * this_func, void * call_site);
void NOFF __cyg_profile_func_exit(void * this_func, void * call_site);


#ifdef __cplusplus__
}
#endif

#endif //