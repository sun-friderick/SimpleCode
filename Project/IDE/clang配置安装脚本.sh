#!/bin/bash

#验证机器环境

#target=x86_64-linux-gnu

#Thread model: posix

#gcc version 4.4.3 (Ubuntu 4.4.3-4ubuntu5.1) 

###

pushd pkg

 

#1 clang 3.3

mkdir /usr/local/clang3.3

tar -xzvf clang+llvm-3.3-amd64-Ubuntu-10.04.4.tar.gz  -C  /usr/local/

echo 'export PATH=/usr/local/clang+llvm-3.3-amd64-Ubuntu-10.04.4/bin:$PATH'>>/etc/profile

 

export PATH=/usr/local/clang+llvm-3.3-amd64-Ubuntu-10.04.4/bin:$PATH

tar -xzvf libcxx-3.3.src.tar.gz -C .

#libc++的库

pushd libcxx-3.3.src/lib

./buildit

if [ $? -ne 0 ];

then

echo -e "\e[1;41m $1 copile c11 library fail! \e[0m"

exit 1

fi

popd

#编译开发环境 C++11库 头文件

#  /usr/local/clang+llvm-3.3-amd64-Ubuntu-10.04.4/lib/c++/v1

#使编译可见

cp -f libcxx-3.3.src/lib/libc++.so.1.0 /usr/lib/libc++.so.1.0

ln -s /usr/lib/libc++.so.1.0 /usr/lib/libc++.so.1

ln -s /usr/lib/libc++.so.1.0 /usr/lib/libc++.so

 

rm -r -f libcxx-3.3.src

 

#使clang可见

source /etc/profile

 

#

popd

 

