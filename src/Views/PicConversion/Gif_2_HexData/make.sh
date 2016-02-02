#!/bin/bash

ELF_PATH="./gif2hex.elf"
if [ -f $ELF_PATH ]; then 
	rm $ELF_PATH;
fi
make_cmd=`gcc -Wall -g ./pic_2_hexData.c -o $ELF_PATH`
echo $make_cmd
