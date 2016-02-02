#include <stdio.h>
#include <stdlib.h>

#include "pCap.h"
#include "pCap02.h"


extern int capture2(int argc, char *argv[]);
int main(int argc, char *argv[])
{
         printf("pCap test....\n");
         int ret = 0;
         //ret = capture02();
         capture( argc, argv);

         return 0;
}
