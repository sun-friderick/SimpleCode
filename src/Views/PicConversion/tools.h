

#pragma once

#ifndef __TOOLS_H__
#define __TOOLS_H__

#ifdef __cplusplus
extern "C" {
#endif

// 同std::string里的功能。
const char * find_first_of(const char * str1, const char * str2);
const char * find_first_not_of(const char * str1, const char * str2);

// 获取开机计数。
int     GetTickCount(void);

// /var目录剩余空间大小。
unsigned long GetFreeVarSize(void);


// 数据块转16进制串。 返回串长度，补0
int Data2Hex(const void * data, int length, char * out);

// 16进制串转数据块。 返回块长度， 不补0
int Hex2Data(const char * hex, void * data);

// 判断文件是否存在.
int IsFileExists(const char * filename);

// 获取文件大小.
long GetFileSizeBytes(const char * filename);

#ifdef __cplusplus
}
#endif


#endif

