
#include "dualserverd.h"

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>

#ifdef __cplusplus
}
#endif

int main(int argc, char **argv)
{
    
    printf("net dhcp Test ....\n");
    DualDHCPAndDNSServer(argc, argv);
    
    return 0;
}




