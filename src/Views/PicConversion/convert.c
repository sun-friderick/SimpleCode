



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int Data2Hex(const void * data, int length, char * out)
{
    int i;
    unsigned char c;
    const unsigned char *p = data;

    for(i = 0; i < length; i++) {
        c = p[i];
        sprintf(&out[i * 2], "%02x", c & 0xff);
    }
    out[length * 2] = '\0';
    return length * 2 ;
}

inline static int char2int(int c)
{
    if(c >= '0' && c <= '9') {
        c = c - '0';
    } else if(c >= 'a' && c <= 'z') {
        c = c - 'a' + 10;
    } else if(c >= 'A' && c <= 'Z') {
        c = c - 'A' + 10;
    }
    return c;
}

int Hex2Data(const char *hex, void *data)
{
    int len = strlen(hex);
    int i;
    int d;
    int c;

    if(len % 2 != 0) {
        return -1;
    }

    for(i = 0; i < len / 2; i++) {
        c = hex[i * 2];
        d = char2int(c) * 16;
        c = hex[i * 2 + 1];
        d += char2int(c);
        *((char *)data + i) = (char)(d & 0xff);
    }
    return i;
}


#ifdef __CONVERT__UNITEST__
//{{{
int Convert()
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
//}}}
#endif

