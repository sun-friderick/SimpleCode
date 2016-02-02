
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "convert.h"


int main()
{
    const char * str = "abcdefg";
    char temp[1024];
    int ret;
    
    ret = Data2Hex(str, strlen(str), temp);
    
    temp[ret] = '\0';
    printf("%s\n", temp);

    char tmp[1024];
    ret = Hex2Data(temp, tmp);
    tmp[ret] = '\0';
    printf("%s\n", tmp);
    return 0;
}

