

#include <string.h>


const char * find_first_of(const char * str1, const char * str2)
{/*{{{*/
    const char *    p;
    int             i;
    int             len;
    if(str1 == NULL || str2 == NULL)
    {
        return NULL;
    }
    p = str1;
    len = strlen(str2);
    while(*p != '\0')
    {
        for(i=0; i<len; i++)
        {
            if(*p == str2[i])
            {
                return p;
            }
        }
        p++;
    }
    return NULL;
}/*}}}*/


const char * find_first_not_of(const char * str1, const char * str2)
{/*{{{*/
    const char *    p;
    int             i;
    int             len;
    int             flag;
    if(str1 == NULL || str2 == NULL)
    {
        return NULL;
    }
    p = str1;
    len = strlen(str2);
    while(*p != '\0')
    {
        flag = 0;
        for(i=0; i<len; i++)
        {
            if(*p == str2[i])
            {
                flag = 1;
            }
        }
        if(flag == 0)
        {
            return p;
        }
        p++;
    }
    return NULL;
}/*}}}*/


