#include <string>

extern "C"{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}
#include "H264Encode.h"

using namespace std;



#define DEBUG
#ifdef DEBUG
	#define LOG( args...) \
	        do {                              \
	                logH264(__FILE__, __LINE__, __FUNCTION__, args);  \
	        } while(0)
#else
	#define LOG  printf 
#endif


/**
* 净化log输出信息：
*        可输出的字符范围：   0x08(退格)———— 0x0D(回车键)； 0x20(空格)———— 0x7F(删除)
*        其他字符均使用 ‘？’替代输出；
**/
void logH264LineSanitize(unsigned char *str)
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
void logH264(const char *file, int line, const char *function, const char *fmt, ...)
{
    va_list args;
    char sLogBuffer[512] = { 0 };
    char str[1024] = {0};

    va_start(args, fmt);
    vsnprintf(sLogBuffer, 512, fmt, args);
    va_end(args);

    sprintf(str + strlen(str), "[%s:%d][%s] ", (strchr(file, '/') ?  (strchr(file, '/') + 1) : file), line, function);
    sprintf(str + strlen(str), ": %s", sLogBuffer);

    logH264LineSanitize((unsigned char *)str);
    printf("%s\n", str);
    return ;
}



H264Encode::H264Encode()
{
	LOG("\n");
	m_xPicPts = 0;
	m_nal = NULL;
	m_nNal = 0;

	//设置参数集结构体x264_param_t的缺省值
	x264_param_default(&m_xParam); //set default m_xParam

	m_picOut = new x264_picture_t;
}

H264Encode::~H264Encode()
{
	LOG("\n");
	delete m_picOut;
}


/**
 * [H264Encode::InitEncoder description]
 * @param  picture [description]
 * @param  w       [description]
 * @param  h       [description]
 * @return         [description]
 */
int H264Encode::InitEncoder(AVPicture picture, int w, int h)
{
	LOG("\n");
	
	m_xParam.i_width   = w; 	//set frame width
	m_xParam.i_height = h; 	//set frame height

	// m_xParam.i_log_level = X264_LOG_NONE;      //x264 log
	// m_xParam.i_threads  = X264_SYNC_LOOKAHEAD_AUTO;      //取空缓存区使用不死锁保证
	// m_xParam.i_frame_total = 0;
	//  m_xParam.i_keyint_max = 10;
	//  m_xParam.i_bframe = 5; //两个参考帧之间b帧的数目
	//  m_xParam.b_open_gop = 0;
	//  m_xParam.i_bframe_pyramid = 0;
	//  m_xParam.rc.i_lookahead = 0; //表示i帧向前缓冲区
	//  m_xParam.rc.i_qp_constant=0; 
         //  m_xParam.rc.i_qp_max=0; 
         //  m_xParam.rc.i_qp_min=0; 
       	//  m_xParam.rc.i_bitrate = 1024 * 10;//rate 为10 kbps
	//  m_xParam.i_bframe_adaptive = X264_B_ADAPT_TRELLIS;

	m_xParam.i_csp = X264_CSP_I420;
	m_xParam.i_fps_num = 25; //帧率分子
	m_xParam.i_fps_den = 1; //帧率分母
	//  m_xParam.i_timebase_den = m_xParam.i_fps_num; 
         //  m_xParam.i_timebase_num = m_xParam.i_fps_den; 
         //  
         //  m_xParam.i_level_idc=30;    //编码复杂度
         //  
         //  m_xParam.rc.f_rf_constant = 25;     //图像质量控制, rc.f_rf_constant是实际质量，越大图像越花，越小越清晰。
 	//  m_xParam.rc.f_rf_constant_max = 45;    //图像质量控制, rc.f_rf_constant_max ，图像质量的最大值。
 	//  
 	//  m_xParam.rc.i_rc_method = X264_RC_ABR;       //参数i_rc_method表示码率控制，CQP(恒定质量)，CRF(恒定码率)，ABR(平均码率)
	//  m_xParam.rc.i_vbv_max_bitrate=(int)((m_bitRate*1.2)/1000) ;   // 平均码率模式下，最大瞬时码率，默认0(与-B设置相同)
	//  m_xParam.rc.i_bitrate = (int)m_bitRate/1000;    //x264使用的bitrate需要/1000。
 	//  
 	//  m_xParam.b_repeat_headers = 1;  // 重复SPS/PPS 放到关键帧前面  , 该参数设置是让每个I帧都附带sps/pps。使用实时视频传输时，需要实时发送sps,pps数据
 	//  
 	//  //I帧间隔,   将I帧间隔与帧率挂钩的，以控制I帧始终在指定时间内刷新。 以下是2秒刷新一个I帧
 	//  param.i_fps_num = (int)m_frameRate;         
 	//  param.i_fps_den = 1;
 	//  param.i_keyint_max = m_frameRate * 2;
 	//  
 	//  //编码延迟
 	//  设置后就能即时编码了
 	//  主要是zerolatency该参数。
 	//  x264_param_default_preset(&param, "fast" , "zerolatency" );
 	//  
 	//  
         /*
          AVC的规格分为三等，从低到高分别为：Baseline、Main、High。
             	Baseline（最低Profile）级别支持I/P 帧，只支持无交错（Progressive）和CAVLC，一般用于低阶或需要额外容错的应用，比如视频通话、手机视频等；
             	Main（主要Profile）级别提供I/P/B 帧，支持无交错（Progressive）和交错（Interlaced），同样提供对于CAVLC 和CABAC 的支持，用于主流消费类电子产品规格如低解码（相对而言）的mp4、便携的视频播放器、PSP和Ipod等；
             	High（高端Profile，也叫FRExt）级别在Main的基础上增加了8x8 内部预测、自定义量化、无损视频编码和更多的YUV 格式（如4：4：4）用于广播及视频碟片存储（蓝光影片），高清电视的应用。     
         AVC 的规格主要是针对兼容性的，不同的规格能在相同级别上的平台应用。 
         至于Baseline@L x.x、Main@L x.x、High@L x.x形式则是在不同级别下的码流级别，数值越大码流就越大，更耗费资源。所以就码流而言High@L3.0<High@L4.0<High@L5.1。
          */
	x264_param_apply_profile(&m_xParam, "main");  //x264_profile_names[0]   //or x264_profile_names[2] : 使用baseline

	// 打开编码器
	if ((m_xHandle = x264_encoder_open(&m_xParam)) == 0) {
		LOG("x264_encoder_open error...\n");
		return -1;
	}

	/* Create a new pic */
	x264_picture_alloc(&m_xPic, X264_CSP_I420, m_xParam.i_width, m_xParam.i_height);   //X264_CSP_I422
	m_xPic.img.i_csp = X264_CSP_I420;
	m_xPic.img.i_plane = 3;

	m_xPic.img.plane[0] = picture.data[0];
	m_xPic.img.plane[1] = picture.data[1];
	m_xPic.img.plane[2] = picture.data[2];

	return 0;
}


/**
 * [H264Encode::UninitEncoder description]
 * @return [description]
 */
int H264Encode::UninitEncoder()
 {
 	LOG("\n");
	if (m_xHandle) {    
		x264_encoder_close(m_xHandle);   //关闭编码器
	}

	return 0;
}


/**
 * [H264Encode::getH264NalCount description]
 * @return [description]
 */
int H264Encode::getH264NalCount()
{
	LOG("\n");
	int nNal = 0;
	if(m_nNal > 0)
		nNal =  m_nNal;
	else{
		LOG("m_nNal[%d] <= 0\n", m_nNal);
		nNal = -1;
	}

	return nNal;
}


/**
 * [H264Encode::getH264Nals description]
 * @param  out [description]
 * @return     [description]
 */
int H264Encode::getH264Nals(unsigned char *out, int index)
{
	LOG("\n");
	int i = 0;
	int result = 0;
	unsigned char *p_out = out;

	memmove(p_out, m_nal[index].p_payload, m_nal[index].i_payload);
	result = m_nal[index].i_payload;

	return 0;
}


/**
 * [H264Encode::EncodeFrame description]
 * @return  [description]
 */
int H264Encode::EncodeFrame(void)
{
	LOG("m_xPicPts[%d]\n", m_xPicPts);

	m_xPic.i_pts = m_xPicPts++;
	x264_encoder_encode(m_xHandle, &m_nal, &m_nNal, &m_xPic, m_picOut);  //编码一帧图像

	return 0;
}


/**
 * [H264Encode::EncodeFrameYUV422 description]
 * @param  type [description]
 * @param  in   [description]
 * @param  out  [description]
 * @return      [description]
 */
int H264Encode::EncodeFrameYUV422(int type, unsigned char *in, unsigned char *out)
{
	LOG("\n");
	x264_picture_t pic_out;
	int result = 0;
	int i = 0;
	unsigned char *p_out = out;

	unsigned char *y = m_xPic.img.plane[0];
	unsigned char *u = m_xPic.img.plane[1];
	unsigned char *v = m_xPic.img.plane[2];

	int is_y = 1, is_u = 1;
	int y_index = 0, u_index = 0, v_index = 0;

	int yuv422_length = 2 * m_xParam.i_width * m_xParam.i_height;

	//序列为YU YV YU YV，一个yuv422帧的长度 width * height * 2 个字节
	for (i = 0; i < yuv422_length; ++i) {
		if (is_y) {
			*(y + y_index) = *(in + i);
			++y_index;
			is_y = 0;
		} else {
			if (is_u) {
				*(u + u_index) = *(in + i);
				++u_index;
				is_u = 0;
			} else {
				*(v + v_index) = *(in + i);
				++v_index;
				is_u = 1;
			}
			is_y = 1;
		}
	}

	switch (type) {
	case 0:
		m_xPic.i_type = X264_TYPE_P;
		break;
	case 1:
		m_xPic.i_type = X264_TYPE_IDR;
		break;
	case 2:
		m_xPic.i_type = X264_TYPE_I;
		break;
	default:
		m_xPic.i_type = X264_TYPE_AUTO;
		break;
	}

	m_xPic.i_pts = m_xPicPts++;   //在 Encode frames时，这边的pts值要不停增加, 否则,一直出现x264 [warning]: non-strictly-monotonic PTS
	if (x264_encoder_encode(m_xHandle, &(m_nal), &m_nNal, &m_xPic, &pic_out) < 0) {   //编码一帧图像
		LOG("x264_encoder_encode error...\n");
		return -1;
	}

	for (i = 0; i < m_nNal; i++) {
		memmove(p_out, m_nal[i].p_payload, m_nal[i].i_payload);
		p_out += m_nal[i].i_payload;
		result += m_nal[i].i_payload;
	}

	return result;
}


/**
 * [H264Encode::EncodeFrameDefault description]
 * @param  type [description]
 * @param  in   [description]
 * @param  out  [description]
 * @return      [description]
 */
int H264Encode::EncodeFrameDefault(int type,  unsigned char *in, unsigned char *out)
{
	LOG("m_xPicPts[%d]\n", m_xPicPts);
	int y_size; 
	int ret = 0;
	int result = 0;
	x264_picture_t pic_out;
	unsigned char *p_out = out;
	static int frameTotal = 0;
	int csp = X264_CSP_I420;

	switch(type){
	case 0:
		csp = X264_CSP_I444;
		break;
	case 1:
		csp = X264_CSP_I420;
		break;
	default:
		LOG("Colorspace Not Support.\n");
		break;
	}

	y_size = m_xParam.i_width * m_xParam.i_height;  
          
         //Loop to Encode  
	switch(csp){  
	case X264_CSP_I444:{  
		memcpy(m_xPic.img.plane[0], in, y_size);        	//Y  
		memcpy(m_xPic.img.plane[1], in, y_size);        	//U  
		memcpy(m_xPic.img.plane[2], in, y_size);        	//V  
		break;
		}  
	case X264_CSP_I420:{  
		memcpy(m_xPic.img.plane[0], in, y_size);        	//Y  
		memcpy(m_xPic.img.plane[1], in, y_size / 4);        	//U  
		memcpy(m_xPic.img.plane[2], in, y_size / 4);        	//V  
		break;
		}  
	default:{  
		LOG("Colorspace Not Support.\n");  
		return -1;
		}  
	}  
	m_xPic.i_pts = m_xPicPts++;
	LOG("Succeed encode frame: %5d\n",m_xPicPts);  

	ret = x264_encoder_encode(m_xHandle, &(m_nal), &m_nNal, &m_xPic, &pic_out);   
	if (ret <  0){  
		LOG("Error.\n");  
		return -1;  
	}  

   	int i = 0;
         for (i = 0; i < m_nNal; i++) {
		memmove(p_out, m_nal[i].p_payload, m_nal[i].i_payload);
		p_out += m_nal[i].i_payload;
		result += m_nal[i].i_payload;
	}
         i=0;  

         //flush encoder  
         while(1){  
                   ret = x264_encoder_encode(m_xHandle, &(m_nal), &m_nNal, &m_xPic, &pic_out);   
                   if(ret == 0){  
                            break;  
                   }  
                   LOG("Flush 1 frame.\n");  
                   for (i = 0; i < m_nNal; i++) {
			memmove(p_out, m_nal[i].p_payload, m_nal[i].i_payload);
			p_out += m_nal[i].i_payload;
			result += m_nal[i].i_payload;
		}
                   i++;  
         }  
	return result;
}
