/*
 * V4L2FramedSource.cpp
 *
 *  Created on: 2014年1月4日
 *      Author: ny
 */

//V4L2FramedSource类继承了FramedSource类。
//V4L2FramedSource是我们自定义的类，主要实现了我们的视频数据如何进入到live555里面去。
//首先在构造函数里面，我们对v4l2进行了初始化以及x264编码初始化。
//这个类最重要的就是doGetNextFrame函数，live555就是通过这个函数将我们的一个nal数据加载到live555里面，然后消息循环发送出去的。
//我们先是将v4l2捕捉的视频数据进行H264压缩编码，这里面值得注意的是一帧图像可能压缩成几个nal的，所以我这里面在确保一帧数据完全发送完了才向v4l2要数据。
//然后就是数据的般移了，其中数据存在fTo里面。然后就是消息了。

#include "V4L2/V4L2.h"
#include "H264Encode/H264Encode.h"

#include "V4L2FramedSource.h"


#define DEFAULT_WIDTH    	640
#define DEFAULT_HEIFHT 	480


int V4L2FramedSource::nalIndex = 0;

V4L2FramedSource::V4L2FramedSource(UsageEnvironment & env) 
	:  FramedSource(env), m_pToken(0)
{
	std::string devName(DEV_NAME);
	v4l2 = new V4L2(devName);
	pH264Encode = new H264Encode();

	printf("creater\n");
	v4l2->OpenDevice();
	v4l2->InitDevice(DEFAULT_WIDTH, DEFAULT_HEIFHT);

	avpicture_alloc(&Picture, PIX_FMT_YUV420P, v4l2->getWidth(), v4l2->getHeight());
	v4l2->StartCapturing();

	pH264Encode->InitEncoder(Picture, DEFAULT_WIDTH, DEFAULT_HEIFHT);
}

V4L2FramedSource::~V4L2FramedSource()
{
	v4l2->StopCapturing();

	v4l2->UninitDevice();
	v4l2->CloseDevice();
	pH264Encode->UninitEncoder();
	
	envir().taskScheduler().unscheduleDelayedTask(m_pToken);
}


void V4L2FramedSource::doGetNextFrame()
{
	// 根据 fps，计算等待时间
	double delay = 1000.0 / (FRAME_PER_SEC * 2);  // ms
	int to_delay = delay * 1000;  // us

	m_pToken = envir().taskScheduler().scheduleDelayedTask(to_delay, getNextFrame, this);
} 

unsigned int V4L2FramedSource::maxFrameSize() const
{
	return 1024 * 200;
}

void V4L2FramedSource::getNextFrame(void * ptr)
{
	((V4L2FramedSource *)ptr)->GetFrameData();
}

void V4L2FramedSource::GetFrameData()
{
	gettimeofday(&fPresentationTime, 0);

	if (V4L2FramedSource::nalIndex == pH264Encode->getH264NalCount())
	{
		v4l2->ReadFrame(Picture, PIX_FMT_YUV420P, v4l2->getWidth(),v4l2->getHeight());
		pH264Encode->EncodeFrame();
		V4L2FramedSource::nalIndex = 0;
		gettimeofday(&fPresentationTime, NULL);
	}

	fFrameSize = pH264Encode->getH264Nals(fTo, V4L2FramedSource::nalIndex);
	V4L2FramedSource::nalIndex++;

/**
    	fFrameSize = 0;

	int len = 0;
	unsigned char buffer[BUFFER_SIZE] = {0};
	while((len = read(m_hFifo,buffer,BUFFER_SIZE))>0)
	{
		memcpy(m_pFrameBuffer+fFrameSize,buffer,len);
		fFrameSize+=len;
	}
	//printf("[MEDIA SERVER] GetFrameData len = [%d],fMaxSize = [%d]\n",fFrameSize,fMaxSize);

	// fill frame data
	memcpy(fTo,m_pFrameBuffer,fFrameSize);

	if (fFrameSize > fMaxSize)
	{
		fNumTruncatedBytes = fFrameSize - fMaxSize;
		fFrameSize = fMaxSize;
	}
	else
	{
		fNumTruncatedBytes = 0;
	}
**/	 
	afterGetting(this);
}
