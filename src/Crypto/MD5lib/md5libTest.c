/* 
md5_example.c: An simply example for routines in md5c.c

Author: Sh Yunchen. rock@bbs.isu.edu.tw.	1999.3.31
*/

#include "global.h"
#include "md5.h"

#define MD5ENCODELEN	16
unsigned char* md5(char*str)
{
	static unsigned char digest[MD5ENCODELEN];

	MD5_CTX	ctx;
	MD5Init(&ctx);
	MD5Update(&ctx, (unsigned char*)str, (unsigned int)strlen(str));
	MD5Final(digest,&ctx);
	return digest;
}

int main(int argc, char*argv[]) {
	unsigned char* d;
	int i;
	if(argc < 2)
		return 0;
	d=md5(argv[1]);
	for (i = 0; i < MD5ENCODELEN; i++)
		printf ("%02x", d[i]);
	printf("\n");
}
