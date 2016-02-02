#ifndef __H264ENCODE_H__
#define __H264ENCODE_H__

extern "C"{
	#include <stdint.h>
	#include <stdio.h>
	#include <libavcodec/avcodec.h>
	#include <libavformat/avformat.h>
	#include <libswscale/swscale.h>
	#include <libavutil/pixfmt.h>
	#include <x264.h>
}


typedef unsigned char uint8_t;

class H264Encode{
public:
	H264Encode();
	~H264Encode();

	int InitEncoder(AVPicture picture, int w, int h);

	int EncodeFrame(void);
	int EncodeFrameDefault(int type,  unsigned char *in, unsigned char *out);
	int EncodeFrameYUV422(int type, unsigned char* in, unsigned char* out);

	int getH264NalCount();
	int getH264Nals(unsigned char *out);

	int UninitEncoder();

protected:


private:
	x264_t 		*m_xHandle;
	x264_param_t 	m_xParam;
	x264_picture_t 	m_xPic;   //说明一个视频序列中每帧特点   //x264_picture_t：存储压缩编码前的像素数据。
	x264_picture_t 	*m_picOut;

	x264_nal_t 	*m_nal;     //x264_nal_t：存储压缩编码后的码流数据。
	int 			m_nNal;

	int m_xPicPts;

};



#endif   //__H264ENCODE_H__

