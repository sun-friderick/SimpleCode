
#include "base64.h"


char BASE64_TABLE[ 64 ] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
      'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
      'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
      'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
      'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
      'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
      'w', 'x', 'y', 'z', '0', '1', '2', '3',
      '4', '5', '6', '7', '8', '9', '+', '/'
};

int base64_index( char ch )
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if (ch >= 'a' && ch <= 'z')
        return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9')
        return ch - '0' + 52;
    if (ch == '+')
        return 62;
    if (ch == '/')
        return 63;
    return -1;
}


int base64en(char *srcStr, char **desStr)
{
    unsigned char srcCode[ 3 ] = {0};
    unsigned int i = 0, j = 0, len = 0;
    unsigned int lenth = strlen(srcStr);
    char* des = NULL;
    len = lenth / 3;
    printf("len=%d\n",lenth);
    *desStr = (char *)malloc(sizeof(char) * (len * 4) + 1);
    if (desStr == NULL){
        desStr = NULL;
        return -1;
    }

    for (i = 0; i < len; i++)  {
        srcCode[ 0 ] = srcStr[ i * 3 + 0 ];
        srcCode[ 1 ] = srcStr[ i * 3 + 1 ];
        srcCode[ 2 ] = srcStr[ i * 3 + 2 ];
        (*desStr)[ i * 4 + 0 ] = BASE64_TABLE[ srcCode[ 0 ] >> 2];
        (*desStr)[ i * 4 + 1 ] = BASE64_TABLE[ ((srcCode[ 0 ] & 0x03) << 4) + (srcCode[ 1 ] >> 4) ];
        (*desStr)[ i * 4 + 2 ] = BASE64_TABLE[ ((srcCode[ 1 ] & 0x0f) << 2) + (srcCode[ 2 ] >> 6) ];
        (*desStr)[ i * 4 + 3 ] = BASE64_TABLE[ srcCode[ 2 ] & 0x3f ];
    }
    i = len;
    j = strlen(srcStr) - len * 3;
    if (j > 0)  {
        srcCode[ 0 ] = srcStr[ i * 3 + 0 ];
        srcCode[ 1 ] = (j > 1) ? srcStr[ i * 3 + 1] : '\0';
        srcCode[ 2 ] = '\0';
        (*desStr)[ i * 4 + 0 ] = BASE64_TABLE[ srcCode[ 0 ] >> 2 ];
        (*desStr)[ i * 4 + 1 ] = BASE64_TABLE[ ((srcCode[ 0 ] & 0x03) << 4) + (srcCode[ 1 ] >> 4) ];
        (*desStr)[ i * 4 + 2 ] = (srcCode[ 1 ] == '\0') ? '=' : BASE64_TABLE[ (srcCode[ 1 ] & 0x0f) << 2 ];
        (*desStr)[ i * 4 + 3 ] = '=';
        i++;
    }
    (*desStr)[ i * 4 ] = '\0';

    //checking
    des = *desStr;
    for (i = 0; i < strlen(des) - 1; ){

        if (base64_index(des[ i ]) < 0){
            desStr = NULL;
            return -1;
        } else {
            i++;
        }
    }
    return strlen(*desStr);
}


int base64de( char* srcStr, char** desStr )
{
    unsigned char srcCode[ 4 ] = {};
    unsigned int i = 0, len = 0;
    unsigned int lenth = strlen(srcStr);
    len = lenth / 4 - 1;

    *desStr = (char *)malloc(sizeof(char) * (len + 1) * 3 + 1);
    if (desStr == NULL){
        desStr = NULL;
        return -1;
    }

     //checking
     for (i = 0; i < lenth-1; ){
        if (base64_index(srcStr[i]) < 0){
            desStr = NULL;printf("%d---\n",i);
            return -32;
        } else {
            i++;
        }
    }
    for (i = 0; i < len; i++)  {
        srcCode[ 0 ] = base64_index(srcStr[ i * 4 + 0 ]);
        srcCode[ 1 ] = base64_index(srcStr[ i * 4 + 1 ]);
        srcCode[ 2 ] = base64_index(srcStr[ i * 4 + 2 ]);
        srcCode[ 3 ] = base64_index(srcStr[ i * 4 + 3 ]);
        (*desStr)[ i * 3 + 0 ] = (srcCode[ 0 ] << 2) + (srcCode[ 1 ] >> 4);
        (*desStr)[ i * 3 + 1 ] = (srcCode[ 1 ] << 4) + (srcCode[ 2 ] >> 2);
        (*desStr)[ i * 3 + 2 ] = ((srcCode[ 2 ] & 0x03) << 6 ) + srcCode[ 3 ];
    }
    i = len;
    srcCode[ 0 ] = base64_index(srcStr[ i * 4 + 0 ]);
    srcCode[ 1 ] = base64_index(srcStr[ i * 4 + 1 ]);
    (*desStr)[ i * 3 + 0 ] = (srcCode[ 0 ] << 2) + ( srcCode[ 1 ] >> 4);
    if (srcStr[ i * 4 + 2 ] == '=')  {
        (*desStr)[ i * 3 + 1 ] = '\0';
    }  else if (srcStr[i * 4 + 3] == '=')  {
        srcCode[ 2 ] = base64_index(srcStr[ i * 4 + 2 ] );
        (*desStr)[ i * 3 + 1 ] = (srcCode[ 1 ] << 4) + (srcCode[ 2 ] >> 2);
        (*desStr)[ i * 3 + 2 ] = '\0';
    }  else  {
        srcCode[ 2 ] = base64_index( srcStr[ i * 4 + 2 ] );
        srcCode[ 3 ] = base64_index( srcStr[ i * 4 + 3 ] );
        (*desStr)[ i * 3 + 1 ] = (srcCode[ 1 ] << 4) + (srcCode[ 2 ] >> 2);
        (*desStr)[ i * 3 + 2 ] = ((srcCode[ 2 ] & 0x03) <<6) + srcCode[ 3 ];
        (*desStr)[ i * 3 + 3 ] = '\0';
    }

    return strlen(*desStr);
}


