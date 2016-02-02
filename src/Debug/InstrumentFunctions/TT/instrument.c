
#include <stdio.h>
#include "instrument.h"

#define INSTRUMENT_FILE_PATH "instrument.log"

static FILE *instrument_fd = NULL;
static int _flag = 0;

#define open_instrument_file()   (instrument_fd = fopen(INSTRUMENT_FILE_PATH, "w"))

#define close_instrument_file() do{     \
        if (NULL != instrument_fd) {    \
            fclose(instrument_fd);      \
            instrument_fd = NULL;       \
        }  \
    }while(0) 

//首次打印会打开文件
#define instrument_print(args, fmt...)  do{     \
        if (0 == _flag) { break; }              \
        if (NULL == instrument_fd && NULL == open_instrument_file()) {  \
            printf("Err: Can not open output file.\n");                 \
            break;  \
        }           \
        fprintf(instrument_fd, args, ##fmt);    \
        fflush(instrument_fd);                  \
    }while(0);

//enable, disable 配对使用，在关注的函数上添加
void enable_instrument( void )
{
    _flag = 1;
}
void disable_instrument( void )
{
    _flag = 0;
}
// 一般是用不到该函数了
void main_destructor( void )
{
    close_instrument_file();
}
void __cyg_profile_func_enter( void *func_addr, void *call_site )
{
    instrument_print("Enter\n%p\n%p\n", call_site, func_addr);
}
void __cyg_profile_func_exit( void *func_addr, void *call_site )
{
    instrument_print("Exit\n%p\n%p\n", call_site, func_addr);
}