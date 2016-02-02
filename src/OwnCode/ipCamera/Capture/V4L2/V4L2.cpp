#include <string>

extern "C"{
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>  
#include <fcntl.h>  
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <linux/fb.h>
}
#include "V4L2.h"

using namespace std;

#define DEBUG
#ifdef DEBUG
	#define LOG( args...) \
	        do {                              \
	                logVerboseCStyle(__FILE__, __LINE__, __FUNCTION__, args);  \
	        } while(0)
#else
	#define LOG  printf 
#endif


/**
* 净化log输出信息：
*        可输出的字符范围：   0x08(退格)———— 0x0D(回车键)； 0x20(空格)———— 0x7F(删除)
*        其他字符均使用 ‘？’替代输出；
**/
void logLineSanitize(unsigned char *str)
{
	int k = 0;
	while (*str) {
		if (*str < 0x08 || (*str > 0x0D && *str < 0x20)) {
			*str = '?';
			k++;
		}
		str++;
	}
	return ;
}

/***
输出格式:
   [file:line][function] : logBuffer_info.
***/
void logVerboseCStyle(const char *file, int line, const char *function, const char *fmt, ...)
{
    va_list args;
    char sLogBuffer[512] = { 0 };
    char str[1024] = {0};

    va_start(args, fmt);
    vsnprintf(sLogBuffer, 512, fmt, args);
    va_end(args);

    sprintf(str + strlen(str), "[%s:%d][%s] ", (strchr(file, '/') ?  (strchr(file, '/') + 1) : file), line, function);
    sprintf(str + strlen(str), ": %s", sLogBuffer);

    logLineSanitize((unsigned char *)str);
    printf("%s\n", str);
    return ;
}


#define DEFAULT_PIX_WIDTH  640
#define DEFAULT_PIX_HEIGHT  480

#define CLEAR(x) memset (&(x), 0, sizeof (x))

/**
 * 
 */
V4L2::V4L2(std::string deviceName)
	: devName(deviceName)
{
	LOG("V4L2 devName[%s]\n", devName.c_str());
	bufNumber = 0;
	frameBuf = NULL;
	devFd = -1;
	pixWidth = 0;
	pixHeight = 0;

	CLEAR (devFmt);

	h264Encoder = new H264Encode;

}

V4L2::~V4L2()
{
	LOG("~V4L2\n");
	bufNumber = 0;
	frameBuf = NULL;
	//delete h264Encoder;   //会引起double free问题
}



/**
 * [V4L2::OpenDevice description] open camera device
 * @return [description]
 */
int V4L2::OpenDevice()
{
	LOG("\n");
	struct stat st;  

	if (-1 == stat (devName.c_str(), &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n", devName.c_str(), errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (!S_ISCHR (st.st_mode)) {
		fprintf (stderr, "%s is no device\n", devName.c_str());
		exit (EXIT_FAILURE);
	}

	//open camera
	devFd = open (devName.c_str(), O_RDWR| O_NONBLOCK, 0);   /*O_RDWR| O_NONBLOCK*/
	if (-1 == devFd) {
		fprintf (stderr, "Cannot open '%s': %d, %s\n", devName.c_str(), errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	
	char yuvFileName[16] = "scb.yuv\0";
	yuvFp = fopen(yuvFileName, "wa+");
	char h264FileName[16] = "scb.h264\0";
	h264Fp = fopen(h264FileName, "wa+");

	LOG(" open device[%s] done...\n", devName.c_str());
	return 0;
}

/**
 * [V4L2::CloseDevice description] close camera device
 * @return [description]
 */
int V4L2::CloseDevice()
{
	LOG("\n");
	if (-1 == close (devFd)) {
		fprintf (stderr, "close device error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
	devFd = -1;

	fclose(yuvFp);
	fclose(h264Fp);
	LOG(" close device[%s] done...\n", devName.c_str());
	return 0;
}


/**
 * [V4L2::GetDeviceFd description] get device fd
 * @return [description]
 */
int V4L2::GetDeviceFd()
{
	LOG("\n");
	if(devFd > 0)
		return devFd;
	else{
		fprintf (stderr, "get DeviceFd error..\n");
		return -1;
	}
}

/**
 * [V4L2::getWidth description]
 * @return [description]
 */
int V4L2::getWidth()
{
	LOG("\n");
	return this->pixWidth;
}

/**
 * [V4L2::getHeight description]
 * @return [description]
 */
int V4L2::getHeight()
{
	LOG("\n");
	return this->pixHeight;
}


int xioctl (int fd,int request,void * arg)
{
	LOG("\n");
	//LOG("fd[%d], request[%d], arg[%s]\n", fd, request, (char*)arg);
	int r;
	do {
		r = ioctl (fd, request, arg);
		//LOG("r[%d], error[%d], [%s]\n", r, errno, strerror (errno) );
	}while (-1 == r && EINTR == errno);

	return r;
}

/**
 * [V4L2::setFormat description]
 * @param  w [description]
 * @param  h [description]
 * @return   [description]
 */
int V4L2::setFormat(int w, int h)
{
	LOG("w[%d], h[%d]\n", w,h);

	devFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if(devFmt.fmt.pix.width != w)
		devFmt.fmt.pix.width = w;
	else
		devFmt.fmt.pix.width = DEFAULT_PIX_WIDTH; 

	if(devFmt.fmt.pix.height != h)
		devFmt.fmt.pix.height = h;
	else
		devFmt.fmt.pix.height = DEFAULT_PIX_HEIGHT;

	devFmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;   //V4L2_PIX_FMT_YUYV
	devFmt.fmt.pix.field = V4L2_FIELD_INTERLACED;//V4L2_FIELD_NONE;  //V4L2_FIELD_INTERLACED

	if (-1 == xioctl (devFd, VIDIOC_S_FMT, &devFmt)){
		fprintf (stderr, "VIDIOC_S_FMT error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	if (-1 == xioctl (devFd, VIDIOC_G_FMT, &devFmt)){
		fprintf (stderr, "VIDIOC_G_FMT error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}
	this->pixWidth = devFmt.fmt.pix.width;
	this->pixHeight = devFmt.fmt.pix.height;

	return 0;
}



#define  FRAME_BUFFER_NUM 	4  //帧缓冲个数

/**
 * [V4L2::InitMmap description] init frame buffer mmap to memery
 * @return [description]
 */
int V4L2::InitMmap()
{
	LOG("\n");
	struct v4l2_requestbuffers req;

	CLEAR (req);

	req.count = FRAME_BUFFER_NUM;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (devFd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s does not support memory mapping\n", devName.c_str());
			exit (EXIT_FAILURE);
		} else {
			fprintf (stderr, "VIDIOC_REQBUFS error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	if (req.count < FRAME_BUFFER_NUM) {    
		fprintf (stderr, "Insufficient buffer memory on %s\n",devName.c_str());
		exit (EXIT_FAILURE);
	}

	frameBuf = (struct _buffer *)calloc (req.count, sizeof (*frameBuf));
	if (!frameBuf) {
		fprintf (stderr, "Out of memory\n");
		exit (EXIT_FAILURE);
	}

	for (bufNumber = 0; bufNumber < req.count; ++bufNumber) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = bufNumber;

		if (-1 == xioctl (devFd, VIDIOC_QUERYBUF, &buf)){
			fprintf (stderr, "VIDIOC_QUERYBUF error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}

		frameBuf[bufNumber].length = buf.length;
		frameBuf[bufNumber].start =mmap (NULL, buf.length, PROT_READ| PROT_WRITE, MAP_SHARED, devFd, buf.m.offset);   /*PROT_READ | PROT_WRITE */

		if (MAP_FAILED == frameBuf[bufNumber].start){
			fprintf (stderr, "mmap error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	LOG(" Init Mmap done...\n");
	return 0;
}

/**
 * [V4L2::UninitMmap description] free frame buffer munmap from memery
 * @return [description]
 */
int V4L2::UninitMmap()
{
	LOG("\n");
	unsigned int i;

	for (i = 0; i < bufNumber; ++i){
		if (-1 == munmap (frameBuf[i].start, frameBuf[i].length)){
			fprintf (stderr, "munmap error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}
		LOG("munmap the [%d] buffer\n", i);
	}
	free (frameBuf);

	LOG(" Uninit Mmap done...\n");
	return 0;
}

/**
 * [V4L2::InitDevice description]
 * @param  pixW [description]
 * @param  pixH [description]
 * @return      [description]
 */
int V4L2::InitDevice(int pixW,  int pixH)
{
	LOG("\n");
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	//struct v4l2_format fmt;

	if (-1 == xioctl (devFd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf (stderr, "%s is no V4L2 device\n",devName.c_str());
			exit (EXIT_FAILURE);
		} else {
			fprintf (stderr, "VIDIOC_QUERYCAP error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf (stderr, "%s is no video capture device\n",devName.c_str());
		exit (EXIT_FAILURE);
	}
	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf (stderr, "%s does not support streaming i/o\n",devName.c_str());
		exit (EXIT_FAILURE);
	}

	CLEAR (cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (0 == xioctl (devFd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (-1 == xioctl (devFd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:    
					break;
				default:
					break;
			}
			LOG("errno[%d]\n", errno);
		}
	}else {     
		LOG("xioctl return not 0.\n");
	}

	setFormat( pixW,  pixH);

	InitMmap ();

#if 1
	int w = 640;
	int h = 480;
	AVPicture avPic;
	avpicture_alloc(&avPic, PIX_FMT_YUV420P, w, h);
	h264Encoder->InitEncoder(avPic, w, h);
	h264Buf = (unsigned char *) malloc( sizeof(unsigned char) * w * h * 3); // 设置缓冲区
#endif

	LOG(" init device[%s] done...\n", devName.c_str());
	return 0;
}

/**
 * [V4L2::UninitDevice description] uninit device
 * @return [description]
 */
int V4L2::UninitDevice()
{
	LOG("\n");
	UninitMmap();

	LOG(" un-init device[%s] done...\n", devName.c_str());
	return 0;
}

/**
 * [V4L2::StartCapturing description] start capture from camera 
 * @return [description]
 */
int V4L2::StartCapturing()
{
	LOG("\n");
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < bufNumber; ++i) {
		struct v4l2_buffer buf;

		CLEAR (buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;
		if (-1 == xioctl (devFd, VIDIOC_QBUF, &buf)){
			fprintf (stderr, "VIDIOC_QBUF error %d, %s\n", errno, strerror (errno));
			exit (EXIT_FAILURE);
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (devFd, VIDIOC_STREAMON, &type)){
		fprintf (stderr, "VIDIOC_STREAMON error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	LOG(" Start Capturing done...\n");
	return 0;
}

/**
 * [V4L2::StopCapturing description] stop capture
 * @return [description]
 */
int V4L2::StopCapturing()
{
	LOG("\n");
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (devFd, VIDIOC_STREAMOFF, &type)){
		fprintf (stderr, "VIDIOC_STREAMOFF error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	LOG(" Stop Capturing done...\n");
	return 0;
}



#define JPG         "./images/image%d.jpg"
/**
 * [V4L2::ProcessImage description]
 * @param  addr   [description]
 * @param  length [description]
 * @return        [description]
 */
int V4L2::ProcessImage(void *addr, int length)
{
	FILE *fp;
	static int num = 0;
	char imageName[20];

	sprintf(imageName, JPG, num++);
	if ((fp = fopen(imageName, "w")) == NULL) {
		perror("Fail to fopen");
		exit(EXIT_FAILURE);
	}
	fwrite(addr, length, 1, fp);
	usleep(500);
	fclose(fp);

#if 1
	int h264Len = h264Encoder->EncodeFrameYUV422(-1, (unsigned char*)addr, (unsigned char*)h264Buf);
	//int h264Len = h264Encoder->EncodeFrameDefault(1, (unsigned char*)addr, length, (unsigned char*)h264Buf);
	if (h264Len > 0) {
		printf("fwrite   h264_buf \n");
		//写h264文件
		fwrite(h264Buf, h264Len, 1, h264Fp);
	}
#endif
	//写yuv文件
	fwrite(addr, length, 1, yuvFp);

    	return 0;
}

/**
 * [V4L2::ReadFrame description] read frame from camera 
 * @return [description]
 */
int V4L2::ReadFrame()
{
	LOG("\n");
	struct v4l2_buffer buf;

	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (-1 == xioctl (devFd, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:   
				 /* Could ignore EIO, see spec. */
				/* fall through */
			default:{
				fprintf (stderr, "VIDIOC_DQBUF error %d, %s\n", errno, strerror (errno));
				exit (EXIT_FAILURE);
			}
		}
		LOG("errno[%d]\n", errno);
	}

	assert (buf.index < bufNumber);
	assert (buf.field ==V4L2_FIELD_NONE);
	ProcessImage(frameBuf[buf.index].start, frameBuf[buf.index].length);

	if (-1 == xioctl (devFd, VIDIOC_QBUF, &buf)){
		fprintf (stderr, "VIDIOC_QBUF error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	LOG("Read Frame done...\n");
	return 1;
}

static int count = 0;
/**
 * [ReadFrame description]
 * 	摄像头数据 主要是YUYV格式数据，所以需要进行对格式进行转化，
 * 		转化后的数据保存在AVPicture里面，
 * 		格式是由FMT制定的，
 * 		输出图像尺寸也是dstWidht，dstHeight决定的
 * @param  pPictureDst [description]
 * @param  FMT         [description]
 * @param  dstWidht   [description]
 * @param  dstHeight  [description]
 * @return             [description]
 */
int V4L2::ReadFrame(AVPicture & pPictureDst, int FMT, int dstWidht, int dstHeight)
{
	LOG("ReadFrame\n");

	struct v4l2_buffer buf;
	AVPicture pPictureSrc;
	//AVFrame pPictureSrc;
	struct SwsContext * pSwsCtx;
	int i = 0;

	CLEAR (buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (-1 == xioctl (devFd, VIDIOC_DQBUF, &buf)) { //读取
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:   
				 /* Could ignore EIO, see spec. */
				/* fall through */
			default:{
				fprintf (stderr, "VIDIOC_DQBUF error %d, %s\n", errno, strerror (errno));
				exit (EXIT_FAILURE);
			}
		}
		LOG("errno[%d]\n", errno);
	}

	count ++;
	LOG("Read Frame to AVPicture count[%d]...\n", count);
	assert (buf.index < bufNumber);
	assert (buf.field ==V4L2_FIELD_NONE);

	pPictureSrc.data[0] = (unsigned char *) frameBuf[buf.index].start;
	pPictureSrc.data[1] = (unsigned char *) NULL; 
	pPictureSrc.data[2] = (unsigned char *) NULL;
	pPictureSrc.data[3] = (unsigned char *) NULL;
	pPictureSrc.linesize[0] = devFmt.fmt.pix.bytesperline;
	for (i = 1; i < 4; i++) {
		pPictureSrc.linesize[i] = 0;
	}

	pSwsCtx = sws_getContext(pixWidth, pixHeight, ( PixelFormat)PIX_FMT_YUYV422,    	/* 原始图像数据的高和宽 及PixelFormat  PIX_FMT_YUV420P// */
						      	dstWidht, dstHeight, ( PixelFormat)FMT,   		/* 输出图像数据的高和宽 及PixelFormat */
						      	(int)SWS_BICUBIC,   				/* 第七個參數則代表要使用哪種scale的方法；用法在 libswscale/swscale.h 內 */
						      	(SwsFilter *)NULL, (SwsFilter *)NULL, (const double *)NULL
						);
	int rs = sws_scale(pSwsCtx,   /*由 sws_getContext 所取得的參數*/
	 				pPictureSrc.data,   		/* 指向input 的 buffer */
					pPictureSrc.linesize,    	/* 指向 input 的 stride, 如果不知道什麼是 stride，姑且可以先把它看成是每一列的 byte 數 */
					0,   			/* 指第一列要處理的位置；這裡是從頭處理，所以直接填0。詳細說明可以參考 swscale.h 的註解。  */
					pixHeight, 	 /* 指的是 source slice 的高度 */
					pPictureDst.data,  	 /* output 的 buffer */
					pPictureDst.linesize  /* output 的 stride */
				);
	if (rs == -1) {
		printf("Can open to change to des image");
		exit (EXIT_FAILURE);
	}
	sws_freeContext(pSwsCtx);

	//LOG("devFd[%d], buf[%s]\n", devFd, buf);
	if (-1 == xioctl(devFd, VIDIOC_QBUF, &buf)) {   //放回缓存
		fprintf (stderr, "VIDIOC_QBUF error %d, %s\n", errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	LOG("Read Frame to AVPicture done...\n");
	return 0;
}
