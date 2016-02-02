#########################################################################
# File Name: ./make.sh
# Author: ma6174
# mail: ma6174@163.com
# Created Time: Tue 04 Mar 2014 04:02:25 PM CST
#########################################################################
#!/bin/bash

ELF_PATH="./base64_test.elf"
LIB_Share_PATH="./libbase64.so"
LIB_Static_PATH="./libbase64.a"
LIB_TEMP_PATH="./base64.o"


if [ -f $ELF_PATH ]; then
	rm $ELF_PATH;
fi

if [ -f $LIB_Share_PATH ]; then
	#rm $LIB_TEMP_PATH;
	rm $LIB_Share_PATH*;
fi

if [ -f $LIB_Static_PATH ]; then
	#rm $LIB_TEMP_PATH;
	rm $LIB_Static_PATH;
fi

#make_lib_static
if [ $1 == ".a" ]; then
	echo `gcc -Wall -c ./base64.c -o base64.o`
	echo `ar cq $LIB_Static_PATH base64.o`

	echo `gcc -Wall main.c -o $ELF_PATH $LIB_Static_PATH`
	echo `rm $LIB_TEMP_PATH`
fi 

#make_lib_share
if [ $1 == ".so" ]; then
	echo `gcc -Wall -shared -fPIC -DPIC -c base64.c -o base64.o`
	echo `ld -shared -soname libbase64.so.1 -o libbase64.so.1.0 -lc base64.o`

	echo `ln -sf libbase64.so.1.0 libbase64.so`
	echo `gcc -Wall main.c -L. -lbase64 -o $ELF_PATH`
	echo `rm $LIB_TEMP_PATH`
fi


if [ $1 == "all" ]; then
	make_cmd=`gcc -Wall main.c base64.c -o $ELF_PATH`
	echo $make_cmd
fi
