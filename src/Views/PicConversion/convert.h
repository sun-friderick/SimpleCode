#ifndef __CONVERT_H__
#define __CONVERT_H__


int Data2Hex(const void * data, int length, char * out);

inline static int char2int(int c);

int Hex2Data(const char *hex, void *data);

int Convert();



#endif __CONVERT_H__
