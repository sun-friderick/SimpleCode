#ifndef BASE64_H_INCLUDED
#define BASE64_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>


int base64en(char* srcStr, char** desStr);
int base64de(char* srcStr, char** desStr);


#endif // BASE64_H_INCLUDED




