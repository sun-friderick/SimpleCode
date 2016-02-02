
//这个是老版本的，新的x264好像新增了一些参数。
typedef struct x264_param_t
{
    /* CPU 标志位 */
    unsigned int cpu;
    int i_threads; /* 并行编码多帧 */
    int b_deterministic; /*是否允许非确定性时线程优化*/
    int i_sync_lookahead; /* 线程超前缓冲 */

    /* 视频属性 */
    int i_width; /* 宽度*/
    int i_height; /* 高度*/
    int i_csp; /* 编码比特流的CSP,仅支持i420，色彩空间设置 */
    int i_level_idc; /* level值的设置*/
    int i_frame_total; /* 编码帧的总数, 默认 0 */
    /*Vui参数集视频可用性信息视频标准化选项 */
    struct
    {
        /* they will be reduced to be 0 < x <= 65535 and prime */
        int i_sar_height;
        int i_sar_width; /* 设置长宽比 */

        int i_overscan; /* 0=undef, 1=no overscan, 2=overscan 过扫描线，默认"undef"(不设置)，可选项：show(观看)/crop(去除)*/

        /*见以下的值h264附件E */
        Int i_vidformat;/* 视频格式，默认"undef"，component/pal/ntsc/secam/mac/undef*/
        int b_fullrange; /*Specify full range samples setting，默认"off"，可选项：off/on*/
        int i_colorprim; /*原始色度格式，默认"undef"，可选项：undef/bt709/bt470m/bt470bg，smpte170m/smpte240m/film*/
        int i_transfer; /*转换方式，默认"undef"，可选项：undef/bt709/bt470m/bt470bg/linear,log100/log316/smpte170m/smpte240m*/
        int i_colmatrix; /*色度矩阵设置，默认"undef",undef/bt709/fcc/bt470bg,smpte170m/smpte240m/GBR/YCgCo*/
        int i_chroma_loc; /* both top & bottom色度样本指定，范围0~5，默认0 */
    } vui;

    int i_fps_num;
    int i_fps_den;
    /*这两个参数是由fps帧率确定的，赋值的过程见下：
    { 
        float fps;  
        if( sscanf( value, "%d/%d", &p->i_fps_num, &p->i_fps_den ) == 2 )
            ;
        else if( sscanf( value, "%f", &fps ) ) {
            p->i_fps_num = (int)(fps * 1000 + .5);
            p->i_fps_den = 1000;
        }
        else
            b_error = 1;
    }
    Value的值就是fps。*/

    /*流参数 */
    int i_frame_reference; /* 参考帧最大数目 */
    int i_keyint_max; /* 在此间隔设置IDR关键帧 */
    int i_keyint_min; /* 场景切换少于次值编码位I, 而不是 IDR. */
    int i_scenecut_threshold; /*如何积极地插入额外的I帧 */
    int i_bframe; /*两个相关图像间P帧的数目 */
    int i_bframe_adaptive; /*自适应B帧判定*/
    int i_bframe_bias; /*控制插入B帧判定，范围-100~+100，越高越容易插入B帧，默认0*/
    int b_bframe_pyramid; /*允许部分B为参考帧 */
    /*去块滤波器需要的参数*/
    int b_deblocking_filter;
    int i_deblocking_filter_alphac0; /* [-6, 6] -6 light filter, 6 strong */
    int i_deblocking_filter_beta; /* [-6, 6] idem */
    /*熵编码 */
    int b_cabac;
    int i_cabac_init_idc;

    int b_interlaced; /* 隔行扫描 */
    /*量化 */
    int i_cqm_preset; /*自定义量化矩阵(CQM),初始化量化模式为flat*/
    char *psz_cqm_file; /* JM format读取JM格式的外部量化矩阵文件，自动忽略其他—cqm 选项*/
    uint8_t cqm_4iy[16]; /* used only if i_cqm_preset == X264_CQM_CUSTOM */
    uint8_t cqm_4ic[16];
    uint8_t cqm_4py[16];
    uint8_t cqm_4pc[16];
    uint8_t cqm_8iy[64];
    uint8_t cqm_8py[64];

    /* 日志 */
    void (*pf_log)( void *, int i_level, const char *psz, va_list );
    void *p_log_private;
    int i_log_level;
    int b_visualize;
    char *psz_dump_yuv; /* 重建帧的名字 */

    /* 编码分析参数*/
    struct
    {
        unsigned int intra; /* 帧间分区*/
        unsigned int inter; /* 帧内分区 */

        int b_transform_8x8; /* 帧间分区*/
        int b_weighted_bipred; /*为b帧隐式加权 */
        int i_direct_mv_pred; /*时间空间队运动预测 */
        int i_chroma_qp_offset; /*色度量化步长偏移量 */

        int i_me_method; /* 运动估计算法 (X264_ME_*) */
        int i_me_range; /* 整像素运动估计搜索范围 (from predicted mv) */
        int i_mv_range; /* 运动矢量最大长度(in pixels). -1 = auto, based on level */
        int i_mv_range_thread; /* 线程之间的最小空间. -1 = auto, based on number of threads. */
        int i_subpel_refine; /* 亚像素运动估计质量 */
        int b_chroma_me; /* 亚像素色度运动估计和P帧的模式选择 */
        int b_mixed_references; /*允许每个宏块的分区在P帧有它自己的参考号*/
        int i_trellis; /* Trellis量化，对每个8x8的块寻找合适的量化值，需要CABAC，默认0 0：关闭1：只在最后编码时使用2：一直使用*/
        int b_fast_pskip; /*快速P帧跳过检测*/
        int b_dct_decimate; /* 在P-frames转换参数域 */
        int i_noise_reduction; /*自适应伪盲区 */
        float f_psy_rd; /* Psy RD strength */
        float f_psy_trellis; /* Psy trellis strength */
        int b_psy; /* Toggle all psy optimizations */

        /*，亮度量化中使用的无效区大小*/
        int i_luma_deadzone[2]; /* {帧间, 帧内} */

        int b_psnr; /* 计算和打印PSNR信息 */
        int b_ssim; /*计算和打印SSIM信息*/
    } analyse;

    /* 码率控制参数 */
    struct
    {
        int i_rc_method; /* X264_RC_* */

        int i_qp_constant; /* 0-51 */
        int i_qp_min; /*允许的最小量化值 */
        int i_qp_max; /*允许的最大量化值*/
        int i_qp_step; /*帧间最大量化步长 */

        int i_bitrate; /*设置平均码率 */
        float f_rf_constant; /* 1pass VBR, nominal QP */
        float f_rate_tolerance;
        int i_vbv_max_bitrate; /*平均码率模式下，最大瞬时码率，默认0(与-B设置相同) */
        int i_vbv_buffer_size; /*码率控制缓冲区的大小，单位kbit，默认0 */
        float f_vbv_buffer_init; /* <=1: fraction of buffer_size. >1: kbit码率控制缓冲区数据保留的最大数据量与缓冲区大小之比，范围0~1.0，默认0.9*/
        float f_ip_factor;
        float f_pb_factor;

        int i_aq_mode; /* psy adaptive QP. (X264_AQ_*) */
        float f_aq_strength;
        int b_mb_tree; /* Macroblock-tree ratecontrol. */
        int i_lookahead;

        /* 2pass 多次压缩码率控制 */
        int b_stat_write; /* Enable stat writing in psz_stat_out */
        char *psz_stat_out;
        int b_stat_read; /* Read stat from psz_stat_in and use it */
        char *psz_stat_in;

        /* 2pass params (same as ffmpeg ones) */
        float f_qcompress; /* 0.0 => cbr, 1.0 => constant qp */
        float f_qblur; /*时间上模糊量化 */
        float f_complexity_blur; /* 时间上模糊复杂性 */
        x264_zone_t *zones; /* 码率控制覆盖 */
        int i_zones; /* number of zone_t's */
        char *psz_zones; /*指定区的另一种方法*/
    } rc;

    /* Muxing parameters */
    int b_aud; /*生成访问单元分隔符*/
    int b_repeat_headers; /* 在每个关键帧前放置SPS/PPS*/
    int i_sps_id; /* SPS 和 PPS id 号 */

    /*切片（像条）参数 */
    int i_slice_max_size; /* 每片字节的最大数，包括预计的NAL开销. */
    int i_slice_max_mbs; /* 每片宏块的最大数，重写 i_slice_count */
    int i_slice_count; /* 每帧的像条数目: 设置矩形像条. */

    /* Optional callback for freeing this x264_param_t when it is done being used.
    * Only used when the x264_param_t sits in memory for an indefinite period of time,
    * i.e. when an x264_param_t is passed to x264_t in an x264_picture_t or in zones.
    * Not used when x264_encoder_reconfig is called directly. */
    void (*param_free)( void* );
} x264_param_t; 









/**

前面讲到了关于NAL打包成RTP后进行发送，那么这些NAL应该怎么得到呢？当然如果有现成的H264数据就可以直接用了，但是一般我们的摄像头采集的数据都不是H264格式的，那就需要编码。而且在我们这个项目中是需要进行图像算法处理的，在这些opencv中用到的图基本上都是BGR格式的Mat图，所以处理完后的图像需要重新进行X264编码，生成一个个的NAL后打包成RTP发送出去，这样就会在实现高清的同时节省不少数据量，也就是H264的实质了。

      H.264是ITU（International Telecommunication Unite 国际通信联盟）和MPEG（Motion Picture Experts Group 运动图像专家组）联合制定的视频编码标准。H.264从1999年开始，到2003年形成草案，最后在2007年定稿有待核实。在ITU的标准里称为H.264，在MPEG的标准里是MPEG-4的一个组成部分--MPEG-4 Part 10，又叫Advanced Video Codec，因此常常称为MPEG-4 AVC或直接叫AVC。x264始于2003年，从当开源社区的MPEG4-ASP编码器Xvid小有所成时开始的，经过几年的开发，特别是Dark Shikari加入开发后，x264逐渐成为了最好的视频编码器。FFmpeg的H264编码部分就是加入了X264模块的，所以实质上想进行H264编码那就用X264库编码吧。当然是开源的，百度或者谷歌下，网上资料很多，这里我主要讲下我再使用过程的一些总结吧。

      我喜欢采取的思路是先整体再细节，然后各个击破，具体问题再具体分析，所以我就先去搞清楚X264里的整体编码流程是怎样的，搞个现成的例程跑下当然是最好不过的了。我这里自己参考着写了个，有需要的可以去我的代码片中看下。https://code.csdn.net/snippets/284343

      首先，这2个结构的添充要完成：x264_param_t     m_param， x264_picture_t      m_pic；填充m_param首先给各个域一个默认值，调用：x264_param_default( &m_param )。m_pic要用X264_picture_alloc分配下;然后自己相应的做一些改变，最常用的有：

    m_param.rc.i_qp_constant = 25;//帧率
    m_param.rc.i_rc_method = X264_RC_CQP ;//rate control method
    m_param.i_width=640;//编码图像大小。
    m_param.i_height=480;

    然后就可以用这个m_param来打开一个encoder了，打开方法：m_h = x264_encoder_open( &m_param ) ；

    编码之前要添充好m_pic：

        m_pic.i_type = X264_TYPE_AUTO;
        m_pic.i_qpplus1 = 0;
        m_pic.img.i_csp =X264_CSP_I420;
        m_pic.img.i_plane = 3;
        m_pic.img.i_stride[0] = 640;
        m_pic.img.i_stride[1] = 640 / 4;
        m_pic.img.i_stride[2] =640 / 4;
        m_pic.i_type = X264_TYPE_AUTO;
        m_pic.i_qpplus1 = 0;

    这些指明图象的大小，格式。还要指明输入一帧数据所有的缓冲区：

      m_pic.img.plane[0] 、m_pic.img.plane[1] 、m_pic.img.plane[2] 等。

    主要的就是调用x264_encoder_encode(m_h,&nal,&n_nal,&m_pic,&pic_out)编码了，编码时还要初始化一个pic_out和nal结构。nal则用于承载VCL编码后的数，编成多个nal帧。显然nal是一个数组，i_nal是数组下标的最大值，即数组边界。

     编码完成后要调用x264_picture_clean( x264_picture_t *pic )释放pic中的缓冲区，调用x264_encoder_close关闭编码器。

     下面就来具体讲讲在其中涉及的一些参数，结构以及编码得到的数据。


1、编码的输入数据

         调用x264_encoder_encode( x264_t *h,  x264_nal_t **pp_nal,  int *pi_nal,  x264_picture_t *pic_in,  x264_picture_t *pic_out )编码函时，会有一个x264_picture_t *pic_in，这就是编码输入的图像，在之前必须给与赋值，并且必须是YUV420数据，将Y、U、V各个颜色分量的数据赋值到图像的各个平面上。或者优化在一个平面上，即将所有数据输入到第一个平面也是可行的（可以验证和对比）。

         那么在得到BGR的图像后需要转换平面，这里我是用Opencv中自带的cvCvtColor函数，但是Opencv中貌似没有直接转换成YUV420的参数，有一个CV_BGR2YUV参数，我的理解是这只是转换YUV4:4:4的，要换成YUV420还需要抽样。

         YUV格式有两大类：planar和packed。对于planar的YUV格式，先连续存储所有像素点的Y，紧接着存储所有像素点的U，随后是所有像素点的V。对于packed的YUV格式，每个像素点的Y,U,V是连续交*存储的。

YUV 4:4:4采样，每一个Y对应一组UV分量。
YUV 4:2:2采样，每两个Y共用一组UV分量。 
YUV 4:2:0采样，每四个Y共用一组UV分量。

一般来说，直接采集到的视频数据是RGB24的格式，RGB24一帧的大小size＝width×heigth×3 Bit，
RGB32的size＝width×heigth×4，如果是I420（即YUV标准格式4：2：0）的数据量是 size＝width×heigth×1.5 Bit。在采集到RGB24数据后，需要对这个格式的数据进行第一次压缩。即将图像的颜色空间由RGB2YUV。因为，X264在进行编码的时候需要标准的YUV（4：2：0）。然后，经过X264编码后，数据量将大大减少。将编码后的数据打包，通过RTP实时传送。到达目的地后，将数据取出，进行解码。完成解码后，数据仍然是YUV格式的。同样可以写个简单的代码区验证下Opencv里的颜色转换。https://code.csdn.net/snippets/284433


2、编码后的数据

        输入的数据准备好了，编码后的数据都在x264_nal_t的数组。我这里设置的参数是Baseline Profile，所以编码后没有B帧，将编码后的数据保存分析后发现，第一次编码的时候会有4个NAl，分别是SPS、PPS、SEI、I帧，也即分别是00 00 00 01 67、 00 00 00 01 68、 00 00  01 06、00 00 01 65开头的四个数据段，这里注意的是SEI和I帧的开头貌似X264中就是00 00 01的起始头了，应该是和源码中这样写的关系，不过没有什么大碍，就是后面在删除这些起始头的时候会有两种判断吧。然后编码第二帧图像，得到的就是一个NAL了，是P帧 00 00 01 41.

        这里还讲几个我遇到的一些参数和困惑，起初我发现在分析数据的时候，第一次是5个NAl，即SPS、PPS、SEI、I帧、I帧，第二次编码的时候是2个NAL，即P帧、P帧，这是怎么发生的呢，原来是有多个线程跑的原因，m_param.b_sliced_threads= false;后就会发现正常了。

        还有就是我要控制一个I后几个P，即一个GOP的数目，那么需要设置m_param.i_keyint_max 的值。另外就是在m_param.b_repeat_headers = 1;  // 重复SPS/PPS 放到关键帧前面，这样设置有利于解码的是每个I帧都会有SPS和PPS。


3、大家在编码完后发现有的人调用了X264_nal_encode函数，这个函数在老版本和新版本中参数会有点问题，老版本中是将数据组装成nalu的标准格式，即 起始码  +  nalu_header +  data。使得数据流中不会再出现连续的两个以上的0。如果有的话就会插入一个0x03；但是在新版本中大家就不必要管这个函数了，因为新版本中调用x264_encoder_encode后得到的nal数组中已经是标准格式了，而且也知道大小了。所以大可不用X264_nal_encode这个函数了。

4、其它参数及结构
**/


typedef struct
{
    int     i_csp;       //色彩空间参数 ，X264只支持I420
    int     i_stride[4]; //对应于各个色彩分量的跨度
    uint8_t *plane[4];   //对应于各个色彩分量的数据
} x264_image_t;

#define    X264_RC_CQP                  0--->恒定质量
#define    X264_RC_CRF                  1--->恒定码率
#define    X264_RC_ABR                  2--->平均码率


typedef struct
{ 
    int i_ref_idc;    //指该NAL单元的优先级  
    int i_type;       //指该NAL单元的类型  
    int b_long_startcode; // 是否采用长前缀码0x00000001   
    int i_first_mb; /* If this NAL is a slice, the index of the first MB in the slice. */
    int i_last_mb;  /* If this NAL is a slice, the index of the last MB in the slice. */
    int i_payload;     //该nal单元包含的字节数
    uint8_t *p_payload;//该NAL单元存储数据的开始地
}  x264_nal_t;
/****************************************************************************************************************
x264_nal_t中的数据在下一次调用x264_encoder_encode之后就无效了，因此必须在调用
x264_encoder_encode 或 x264_encoder_headers 之前使用或拷贝其中的数据。
*****************************************************************************************************************/

typedef struct x264_param_t
{  
unsigned int cpu;                  // CPU 标志位 
int  i_threads;                 // 并行编码多帧; 线程数，为0则自动多线程编码
int  b_sliced_threads;          // 如果为false，则一个slice只编码成一个NALU;// 否则有几个线程，在编码成几个NALU。缺省为true。
int  b_deterministic; // 是否允许非确定性时线程优化
int  b_cpu_independent; // 强制采用典型行为，而不是采用独立于cpu的优化算法
int  i_sync_lookahead; // 线程超前缓存帧数
/* 视频属性 */
int  i_width; // 视频图像的宽
int  i_height; // 视频图像的高
int  i_csp;         // 编码比特流的CSP，仅支持i420，色彩空间设置
int  i_level_idc; // 指明作用的level值，可能与编码复杂度有关
int  i_frame_total; // 编码帧的总数, 默认 0
/* hrd : hypothetical reference decoder (假定参考解码器) , 检验编码器产生的符合
  该标准的NAL单元流或字节流的偏差值。蓝光视频、电视广播及其它特殊领域有此要求 */
int  i_nal_hrd;
////////////////* vui参数集 : 视频可用性信息、视频标准化选项 *////////////////////
struct
{
    /* 宽高比的两个值相对互素，且在(0,  65535] 之间 */
 int  i_sar_height;    // 样本宽高比的高度
int  i_sar_width;    // 样本宽高比的宽度
/* 0=undef, 1=no overscan, 2=overscan 过扫描线，
    默认"undef"(不设置)，可选项：show(观看) / crop(去除) */
int  i_overscan;
/* 以下的值可以参见H264附录E */
int  i_vidformat;    // 视频在编码/数字化之前是什么类型，默认"undef".
                                    // 取值有：Component, PAL, NTSC, SECAM, MAC 等
int  b_fullrange;           // 样本亮度和色度的计算方式，默认"off"，可选项：off/on
int  i_colorprim;           // 原始色度格式，默认"undef"
int  i_transfer;            // 转换方式，默认"undef"
int  i_colmatrix;           // 设置从RGB计算得到亮度和色度所用的矩阵系数，默认"undef"
int  i_chroma_loc;          // 设置色度采样位置，范围0~5，默认0
} vui;
////////////////* 比特流参数 */////////////////////////////
int  i_frame_reference;         // 最大参考帧数目
int  i_dpb_size;                // Decoded picture buffer size
int  i_keyint_max;              // 设定IDR帧之间的最间隔，在此间隔设置IDR关键帧
int  i_keyint_min;              // 设定IDR帧之间的最小间隔, 场景切换小于此值编码位I帧, 而不是 IDR帧.
int  i_scenecut_threshold;      // 场景切换阈值，插入I帧
int  b_intra_refresh;           // 是否使用周期帧内刷新替代IDR帧
int  i_bframe;                  // 两个参考帧之间的B帧数目
int  i_bframe_adaptive;         // 自适应B帧判定, 可选取值：X264_B_ADAPT_FAST等
int  i_bframe_bias;             // 控制B帧替代P帧的概率，范围-100 ~ +100，
                                // 该值越高越容易插入B帧，默认0.
int  i_bframe_pyramid;          // 允许部分B帧为参考帧，
                                // 可选取值：0=off,  1=strict hierarchical,  2=normal
int  b_open_gop;                // Close GOP是指帧间的预测都是在GOP中进行的。
                                // 使用Open GOP，后一个GOP会参考前一个GOP的信息
int  b_bluray_compat;           // 支持蓝光碟
/* 去块滤波器需要的参数, alpha和beta是去块滤波器参数 */
int  b_deblocking_filter;               // 去块滤波开关
int  i_deblocking_filter_alphac0;       // [-6, 6] -6 light filter, 6 strong
int  i_deblocking_filter_beta;          // [-6, 6] 同上
int  b_cabac;                           // 自适应算术编码cabac开关
int  i_cabac_init_idc;                  // 给出算术编码初始化时表格的选择
int  b_interlaced;                      // 隔行扫描
int  b_constrained_intra;               // 
 /* 量化 */
int  i_cqm_preset;              // 自定义量化矩阵(CQM), 初始化量化模式为flat
char *psz_cqm_file;             // 读取JM格式的外部量化矩阵文件，忽略其他cqm选项
uint8_t  cqm_4iy[16];           // used only if i_cqm_preset == X264_CQM_CUSTOM  
uint8_t  cqm_4py[16];
uint8_t  cqm_4ic[16];
uint8_t  cqm_4pc[16];
uint8_t  cqm_8iy[64];
uint8_t  cqm_8py[64];
uint8_t  cqm_8ic[64]；
uint8_t  cqm_8pc[64];
/* 日志 */
void  (*pf_log)( void *, int i_level, const char *psz, va_list );     // 日志函数
void  *p_log_private;           // 
int    i_log_level;             // 日志级别，不需要打印编码信息时直接注释掉即可
int    b_visualize;             // 是否显示日志
char   *psz_dump_yuv;           //  重建帧的文件名
/* 编码分析参数 */
struct
{
unsigned int intra;             //  帧内分区
unsigned int inter;             //  帧间分区
int  b_transform_8x8;           // 
int  i_weighted_pred;           // P帧权重
int  b_weighted_bipred;         // B帧隐式加权
int  i_direct_mv_pred;          // 时间空间运动向量预测模式
int  i_chroma_qp_offset;        // 色度量化步长偏移量
int  i_me_method;               // 运动估计算法 (X264_ME_*)
int  i_me_range;                // 整像素运动估计搜索范围 (from predicted mv) 
int  i_mv_range;                // 运动矢量最大长度. -1 = auto, based on level
int  i_mv_range_thread;         // 线程之间的最小运动向量缓冲.  -1 = auto, based on number of threads.
int  i_subpel_refine;           // 亚像素运动估计质量
int  b_chroma_me;               // 亚像素色度运动估计和P帧的模式选择
int  b_mixed_references;        // 允许每个宏块的分区有它自己的参考号
int  i_trellis;                 // Trellis量化提高效率，对每个8x8的块寻找合适的量化值，需要CABAC，
                                // 0 ：即关闭  1：只在最后编码时使用  2：在所有模式决策上启用
int  b_fast_pskip;              // 快速P帧跳过检测
int  b_dct_decimate;            // P帧变换系数阈值
int  i_noise_reduction;         // 自适应伪盲区
int  b_psy;                     // Psy优化开关，可能会增强细节
float  f_psy_rd;                // Psy RD强度
float  f_psy_trellis;           // Psy Trellis强度
int  i_luma_deadzone[2];        // 亮度量化中使用的盲区大小，{ 帧间, 帧内 }
int  b_psnr;                    // 计算和打印PSNR信息
int  b_ssim;                    // 计算和打印SSIM信息
} analyse;
/* 码率控制参数 */
struct
{
int  i_rc_method;               // 码率控制方式 ： X264_RC_CQP恒定质量,  
                                // X264_RC_CRF恒定码率,  X264_RC_ABR平均码率
int  i_qp_constant;             // 指定P帧的量化值，0 - 51，0表示无损
int  i_qp_min;                  // 允许的最小量化值，默认10
int  i_qp_max;                  // 允许的最大量化值，默认51
int  i_qp_step;                 // 量化步长，即相邻两帧之间量化值之差的最大值
int   i_bitrate;                // 平均码率大小
float  f_rf_constant;           // 1pass VBR, nominal QP. 实际质量，值越大图像越花,越小越清晰
float  f_rf_constant_max;       // 最大码率因子，该选项仅在使用CRF并开启VBV时有效，
                                // 图像质量的最大值，可能会导致VBV下溢。
float  f_rate_tolerance;        // 允许的误差
int    i_vbv_max_bitrate;       // 平均码率模式下，最大瞬时码率，默认0
int    i_vbv_buffer_size;       // 码率控制缓冲区的大小，单位kbit，默认0
float  f_vbv_buffer_init;       // 设置码率控制缓冲区（VBV）缓冲达到多满(百分比)，才开始回放，
                                // 范围0~1.0，默认0.9
float  f_ip_factor;             // I帧和P帧之间的量化因子（QP）比值，默认1.4
float  f_pb_factor;             // P帧和B帧之间的量化因子（QP）比值，默认1.3
int   i_aq_mode;                // 自适应量化（AQ）模式。 0：关闭AQ  
                                // 1：允许AQ在整个视频中和帧内重新分配码
                                // 2：自方差AQ(实验阶段)，尝试逐帧调整强度
float  f_aq_strength;           // AQ强度，减少平趟和纹理区域的块效应和模糊度
/* MBTree File是一个临时文件，记录了每个P帧中每个MB被参考的情况。
  目前mbtree只处理P帧的MB，同时也不支持b_pyramid. */
int   b_mb_tree;                // 是否开启基于macroblock的qp控制方法
int   i_lookahead;              // 决定mbtree向前预测的帧数
/* 2pass */
int   b_stat_write;             // 是否将统计数据写入到文件psz_stat_out中
char  *psz_stat_out;            // 输出文件用于保存第一次编码统计数据
int   b_stat_read;              // 是否从文件psz_stat_in中读入统计数据
char  *psz_stat_in;             // 输入文件存有第一次编码的统计数据
/* 2pass params (same as ffmpeg ones) */
float  f_qcompress;             // 量化曲线(quantizer curve)压缩因子。
                                // 0.0 => 恒定比特率，1.0 => 恒定量化值。
float  f_qblur;                 // 时间上模糊量化，减少QP的波动(after curve compression)
float  f_complexity_blur;       // 时间上模糊复杂性，减少QP的波动(before curve compression)
x264_zone_t *zones;             // 码率控制覆盖
int    i_zones;                 // number of zone_t's
char  *psz_zones;               // 指定区的另一种方法
} rc;
/* Muxing复用参数 */
int  b_aud;                     // 生成访问单元分隔符
int  b_repeat_headers;          // 是否复制sps和pps放在每个关键帧的前面
int  b_annexb;                  // 值为true，则NALU之前是4字节前缀码0x00000001；
                                // 值为false，则NALU之前的4个字节为NALU长度
int  i_sps_id;                  // sps和pps的id号
int  b_vfr_input;               // VFR输入。1 ：时间基和时间戳用于码率控制  0 ：仅帧率用于码率控制
uint32_t  i_fps_num;            // 帧率的分子
uint32_t  i_fps_den;            // 帧率的分母
uint32_t  i_timebase_num;       // 时间基的分子
uint32_t  i_timebase_den;       // 时间基的分母
/* 以某个预设模式将输入流(隔行，恒定帧率)标记为软交错(soft telecine)。默认none. 可用预设有：
  none, 22, 32, 64, double, triple, euro.  使用除none以外任一预设，都会连带开启--pic-struct */
int  b_pulldown;
int  b_pic_struct;              // 强制在Picture Timing SEI传送pic_struct. 默认是未开启
/* 将视频流标记为交错(隔行)，哪怕并非为交错式编码。可用于编码蓝光兼容的25p和30p视频。默认是未开启 */
int b_fake_interlaced;
/* 条带参数 */
int  i_slice_max_size;          // 每个slice的最大字节数，包括预计的NAL开销
int  i_slice_max_mbs;           // 每个slice的最大宏块数，重写i_slice_count
int  i_slice_count;             // 每帧slice的数目，每个slice必须是矩形
} x264_param_t;






