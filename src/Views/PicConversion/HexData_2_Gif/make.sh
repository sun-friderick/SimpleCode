#! /bin/bash 

ELF_PATH="./hex2gif.elf"
if [ -f $ELF_PATH ]; then 
	rm $ELF_PATH;
fi
make_cmd=`gcc -Wall split_txt2gif.c -o $ELF_PATH`

echo $make_cmd
