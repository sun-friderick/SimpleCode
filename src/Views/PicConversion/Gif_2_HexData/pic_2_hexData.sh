#!/bin/bash

#find . -name "*.gif" | xargs echo
#find . -name "*.gif" | xargs -0 echo
#find . -name "*.gif" | xargs -0 ./gif2hex.elf  >> /mnt/hgfs/Proj_Code/_own/out.txt

mkdir -p ./out

find . -name "*.gif" -exec ./gif2hex.elf {} \; >> ./out/gif_out.txt

find . -name "*.png" -exec ./gif2hex.elf {} \; >> ./out//png_out.txt

find . -name "*.jpg" -exec ./gif2hex.elf {} \; >> ./out//jpg_out.txt

find . -name "*.bmp" -exec ./gif2hex.elf {} \; >> ./out//bmp_out.txt

