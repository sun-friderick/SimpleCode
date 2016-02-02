
#include <stdio.h>
#include<stdlib.h> 
#include "SystemInstrument.h"


int main()
{
    printf("System Instrument comment Test ....\n");
    
    int *ptr = NULL; 
    *ptr = 0; 
    
    return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
/**  gcc -g -finstrument-functions ./*.c **/
#define DUMP(func, call)   printf("%s: func = %p, called by = %p/n", __FUNCTION__, func, call)  

void __attribute__((__no_instrument_function__))  __cyg_profile_func_enter(void *this_func, void *call_site)  
{  
    DUMP(this_func, call_site);  
}  


void __attribute__((__no_instrument_function__))  __cyg_profile_func_exit(void *this_func, void *call_site)  
{  
    DUMP(this_func, call_site);  
}  


main()  
{  
    puts("Hello World!");  
    return 0;  
}  

#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
