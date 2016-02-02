#include <stdio.h>

#include "base64.h"

char str_arr[] = "TWF0Y2hSdWxlOnxNc2dNb2RlOjAxMDB8VGltZVN0YW1wOjIwMTIwODA4MTU0MjA1MDA4M3xNc2dTcmNOYW1lOnxNc2dUeXBlOjB8UmVsYXRlSUQ6fEZpcmVkVGltZTowfFByaW9yaXR5OjV8U2F2ZUZsYWc6MHxDb250ZW50OlRlc3QgUm9sbGluZyBtZXMgNHxBbHBoYTowfEJnQ29sb3I6I0M4QzhDOHxGb250Q29sb3I6IzAwMDAwMHxGb250OjJ8Rm9udFNpemU6MzJ8U2hvd1BsYWNlOjh8U2hvd0xlZnQ6MHxTaG93VG9wOjB8U2hvd1dpZGU6MHxTaG93SGVpZ2h0OjB8Um9sbFRpbWVzOjJ8Um9sbFNwZWVkOjF8Um9sbE1vZGU6MnxIZWFkUmVzZXJ2ZTowfFRhaWxSZXNlcnZlOjB8SGVhZEljb246fFRhaWxJY29uOnxGaXhlZEhlYWRJY29uOnxGaXhlZFRhaWxJY29uOnw=";
char str_arr1[] = "MatchRule:|MsgMode:0100|TimeStamp:201208081542050083|MsgSrcName:teteqiwurqwgtwidoufq8we732453teqiwurqwgtwidoufq8we73245327weorihapdwo27weorihapdwoqiwurqwgtwidoufq8we73245327wteqiwurqwgtwidoufq8we73245teqiwurqwgtwidouteqiwurqwgtufq8fq8we73245327weorihapdwo327weorihapdwoeorihteqiwurqwgtwidoufq8we73245327weorihapdwoapdwo|MsgType:0|RelateID: 9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245ufq8we73245327we9tyriuqegr897234yr98qrhqwiurg47rgqewydgawidhwhqorihapdwufq8wtyriuqe97234yr98qrhqwiurg834riuqegr897234yr98qrhqwiurg8347rgqwe73245ufq8we73245327we9tyriuqegr897234yr98qrhqwiurg47rgqewydgawidhwhqorihapdwufq8we732453273245ufq8we7324327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdw327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg834qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhq |FiredTime:0|Priority:5|SaveFlag:0|Content:Test Rolling mes 4 teteqiwurqwgtwidoufq8we732453teqiwurqwgtwidoufq8we73245327weorihapdwo27weorihapdwoqiwurqwgtwidoufq8we73245327wteqiwurqwgtwidoufq8we73245teqiwurqwgtwidouteqiwurqwgtufq8fq8we73245327weorihapdwo327weorihapdwoeorihteqiwurqwgtwidoufq8we73245327weorihapdwoapdwo|Alpha:0|BgColor:#C8C8C8|FontColor:#000000|Font:2|FontSize:32|ShowPlace:8|ShowLeft:0|ShowTop:0|ShowWide:0|ShowHeight:0|RollTimes:2|RollSpeed:1|RollMode:2|HeadReserve:0|TailReserve:0|HeadIcon:|TailIcon:|FixedHeadIcon:|FixedTailIcon:|";

int main( int argc, char **argv )
{
    char src[ 3000 ] = { 0 };
    char src1[ 3000 ] = { 0 };
    //char en[ 3000 ] = { 0 };

    char* enstr = NULL ;
    char* destr = NULL;

    unsigned int ret;
    char* str = str_arr;
    char* str1 = str_arr1;

    strcpy(src , str);
    strcpy(src1, str1);

    ret = base64en(str1, &enstr);
    if (ret > 0)
        printf(" Encryption : \n%s => \n%s\n\n\n", src1, enstr );
    else
        printf("base64en error  %d\n", ret);

    //strcpy(en, enstr);
    printf("------------ base64decode ---------\n");
    ret = base64de( str, &destr);
    if (ret > 0)
        printf("\n Decryption: \n%s => \n%s\n", str, destr );
    else
        printf("base64en error  %d\n", ret);

    free(enstr);
    enstr = NULL;
    free(destr);
    destr = NULL;

    return 0;
}



#if 0

MatchRule:|MsgMode:0100|TimeStamp:201208081542050083|MsgSrcName:teteqiwurqwgtwidoufq8we732453teqiwurqwgtwidoufq8we73245327weorihapdwo27weorihapdwoqiwurqwgtwidoufq8we73245327wteqiwurqwgtwidoufq8we73245teqiwurqwgtwidouteqiwurqwgtufq8fq8we73245327weorihapdwo327weorihapdwoeorihteqiwurqwgtwidoufq8we73245327weorihapdwoapdwo|MsgType:0|RelateID:9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245ufq8we73245327we9tyriuqegr897234yr98qrhqwiurg47rgqewydgawidhwhqorihapdwufq8wtyriuqe97234yr98qrhqwiurg834riuqegr897234yr98qrhqwiurg8347rgqwe73245ufq8we73245327we9tyriuqegr897234yr98qrhqwiurg47rgqewydgawidhwhqorihapdwufq8we732453273245ufq8we7324327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdw327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg834qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhqorihapdwufq8we73245327we9tyriuqegr897234yr98qrhqwiurg8347rgqewydgawidhwhq|FiredTime:0|Priority:5|SaveFlag:0|Content:Test Rolling mes 4 teteqiwurqwgtwidoufq8we732453teqiwu rqwgtwidoufq8we73245327weorihapdwo27weorihapdwoqiwurqwgtwidoufq8we73245327wteqiwurqwgtwidoufq8we73245teqiwurqwgtwidouteqiwurqwgtufq8fq8we73245327weorihapdwo327weorihapdwoeorihteqiwurqwgtwidoufq8we73245327weorihapdwoapdwo|Alpha:0|BgColor:#C8C8C8|FontColor:#000000|Font:2|FontSize:32|ShowPlace:8|ShowLeft:0|ShowTop:0|ShowWide:0|ShowHeight:0|RollTimes:2|RollSpeed:1|RollMode:2|HeadReserve:0|TailReserve:0|HeadIcon:|TailIcon:|FixedHeadIcon:|FixedTailIcon:|
#endif
