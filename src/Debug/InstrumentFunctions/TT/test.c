
#include <stdio.h>
#include "instrument.h"

int addmul(int i, int j);
int mul(int i, int j);

int main(int argc, char *argv[])
{
    enable_instrument();
    int i=addmul(3,5);
    printf("result is %d\n",i);
    disable_instrument();
    return 0;
}


int addmul(int i, int j)
{
    int k1,k2;
    k1=i+j;
    k2=mul(i,j);
    return k1+k2;
}

int mul(int i, int j)
{
    return i * j;
}