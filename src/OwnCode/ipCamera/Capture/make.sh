#!/bin/sh
#

# v4l2  &&  h264
echo "g++ complie v4l2 && h264encode ... "
g++  main.cpp  V4L2/V4L2.cpp  H264Encode/H264Encode.cpp  -o ./bin/test  -L /usr/local/lib  -lavformat  -lswscale  -lavcodec  -lavutil -lpthread -lx264 -lm    


## test 
#	cd  ./bin/
#   	chmod 0777 ./test
#   	./test

