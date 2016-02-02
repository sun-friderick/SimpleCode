#ifndef __V4L2_H__
#define __V4L2_H__

#include <string>
extern "C"{
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/pixfmt.h>
	#include <linux/videodev2.h>
}
#include "../H264Encode/H264Encode.h"

#define DEVICE_NAME "/dev/video0"

struct _buffer {
	void * start;
	size_t length;
};

class V4L2{
public:
	V4L2(std::string deviceName);
	~V4L2();

	int OpenDevice();
	int CloseDevice();

	int GetDeviceFd();

	int getWidth();
	int getHeight();

	int InitDevice(int pixW,  int pixH);
	int UninitDevice();

	int StartCapturing();
	int StopCapturing();

	int ReadFrame();
	int ReadFrame(AVPicture & pPictureDst, int FMT, int dstWidht, int dstHeight);

protected:
	int InitMmap();
	int UninitMmap();

	int setFormat(int w, int h);
	int ProcessImage(void *addr, int length);

private:
	std::string 	devName;
	int     		devFd;

	FILE *	yuvFp;
	FILE *	h264Fp;
	H264Encode*  h264Encoder;
	unsigned char * h264Buf;

	int pixWidth;
	int pixHeight;

	unsigned int 		bufNumber;
	struct _buffer* 	frameBuf;

	struct v4l2_format devFmt;
};









#endif //__V4L2_H__