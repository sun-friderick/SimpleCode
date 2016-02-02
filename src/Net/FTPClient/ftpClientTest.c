#include <stdio.h>
#include "FTPClient.h"

#include "Log/LogC.h"
#include "logMonitor.h"
#include "Monitors.h"



extern int ClientMain(int argc, char* argv[]);
int main (int argc, char** argv)
{
    int i = 0;
    
    printf("===========================main===============\n");

    printf("main param:: argc[%d], argv[%p].\n", argc, argv);
    for(i = 0; i < argc; i++){
        printf("param:: the [%d] param is [%s].\n", i, argv[i]);
    }
    
    ClientMain(argc, argv);
    
    return 0;
}




