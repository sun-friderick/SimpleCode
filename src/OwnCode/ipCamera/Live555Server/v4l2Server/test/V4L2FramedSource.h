/*
 * V4L2FramedSource.cpp
 *
 *  Created on: 2014年1月4日
 *      Author: ny
 */

#ifndef __V4L2FramedSource_H__
 #define __V4L2FramedSource_H__

//V4L2FramedSource类继承了FramedSource类。
//V4L2FramedSource是我们自定义的类，主要实现了我们的视频数据如何进入到live555里面去。
//首先在构造函数里面，我们对v4l2进行了初始化以及x264编码初始化。
//这个类最重要的就是doGetNextFrame函数，live555就是通过这个函数将我们的一个nal数据加载到live555里面，然后消息循环发送出去的。
//我们先是将v4l2捕捉的视频数据进行H264压缩编码，这里面值得注意的是一帧图像可能压缩成几个nal的，所以我这里面在确保一帧数据完全发送完了才向v4l2要数据。
//然后就是数据的般移了，其中数据存在fTo里面。然后就是消息了。

#include <FramedSource.hh>

#include "V4L2/V4L2.h"
#include "H264Encode/H264Encode.h"

#define DEV_NAME "/dev/video0"
#define FRAME_PER_SEC 25
 
class V4L2FramedSource :  public  FramedSource{
public:
	V4L2FramedSource(UsageEnvironment & env) ;
	~V4L2FramedSource(void);

	virtual void doGetNextFrame();
	virtual unsigned int maxFrameSize() const;

	static void getNextFrame(void * ptr);
	void GetFrameData();

protected:



private:
	V4L2*  v4l2;
	H264Encode*  pH264Encode;
	AVPicture   Picture;
	static int nalIndex;

	void *m_pToken;
};
 




#endif //__V4L2FramedSource_H__