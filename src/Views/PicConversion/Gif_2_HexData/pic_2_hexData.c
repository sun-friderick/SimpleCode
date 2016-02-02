/*************************************************************************
    > File Name: ./main.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: Tue 30 Jul 2013 08:45:23 PM CST
 ************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE (4096*1)


int main(int argc, char* argv[])
{
    FILE* fp1 = NULL;
    char f_name[32] = {"aa.gif"};
    char func_name[32] = {};
    //char buf[BUF_SIZE] = {0};
    char* pbuf = NULL;
    int i = 0;
    long int value = 0;

    if(argc > 1)
        strcpy(f_name, argv[1]);
    
    printf("\n---------------------------[<%s>]---------------------------\n", f_name);
    fp1 = fopen(f_name, "rb");
    if(fp1 == NULL){
        printf("open file [%s] error.", f_name);
        return -1;
    }
    
    fseek(fp1, 0, SEEK_SET);
    long int f_len = ftell(fp1);
    fseek(fp1, 0, SEEK_END);
    f_len = ftell(fp1) - f_len;
    printf("==f_len=%ld\n", f_len);
    rewind(fp1);
    
    pbuf = (char*)malloc(f_len * sizeof(char));
    
    fread(pbuf, 1, BUF_SIZE, fp1);
    fclose(fp1);
    fp1 = NULL;
    

    snprintf(func_name, (size_t)(strstr(f_name, ".gif") - f_name + 1), "%s", f_name);
    printf("app_%s = {", func_name);
    for (i = 0; i < BUF_SIZE; i++){
        if (pbuf[i] > 0x8b){
            printf("0x%2x,=-= ", pbuf[i]);
            value = pbuf[i];
            pbuf[i] = value- 0xffffff00;
        }
        printf("0x%2x, ",(unsigned char) pbuf[i]);
        if((i + 8)%16 == 0) 
            printf("\n");
    }
    printf("}\n\n");
    
    free(pbuf);
    return 0;
}
