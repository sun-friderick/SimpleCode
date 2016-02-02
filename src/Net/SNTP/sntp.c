//#include <stddefs.h>
//#include <stcommon.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>  
#include <string.h>  
#include <time.h>  
#include <sys/time.h>  
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <arpa/inet.h>  
#include <netdb.h>

#include <math.h>
#include "sntp.h"


/**
 *  NTP协议的基本原理
 *  
    假定时间服务器是A，我们的机器是B，则同步过程如下进行：
        B向A发送一个消息包，记录发出消息包时的时间戳t1（以B机时间为基准）
        A收到消息包立即记录时间戳t2（以A机时间为基准）
        A向B返回一个消息包，返回消息包时记录时间戳t3（以A机时间为基准）
        B收到A返回的消息包，此时记录时间戳t4（以B机时间为基准）
        
    t4和t1是以B的时间标准记录的时间戳，其差t4-t1表示整个消息传递过程所需要的时间间隔；
    t3和t2是以A机的时间标准记录的时间戳，其差t3-t2表明消息传递过程在A机逗留的时间，那么(t4-t1)-(t3-t2)应该就是信息包从B到A，再从A传回B的时间（中间刨去了在A机的逗留时间），
    如果假定信息包从A到B和从B到A所用的时间一样，那么，从A到B或者从B到A信息包的传送时间d为：
        d = ((t4 - t1) - (t3 - t2))/2
    假定B机相对于A机的时间误差是c，则有下列等式：
        t2 = t1 + c + d
        t4 = t3 - c + d
    从以上三个等式可以解出B机的时间误差c为：
        c = ((t2 - t1) + (t3 - t4)) / 2
 *   
 *  
 **/


 #define SNTP_NO_ERROR      1
 
 
 
/*#include <ngnetdb.h>*/
typedef struct _NtpTimePacket {
    unsigned long seconds;
    unsigned long fraction;
} NtpTimePacket;

typedef struct _NtpBasicInfo {
    unsigned char LiVnMode;   /* LI（Leap Indicator）： 长度为2比特，闰秒指示，值为“11”时表示告警状态，表示是否警告在当天的最后一分钟插入/删除一个闰秒。通常填0表示不需要警告。为其他值时NTP本身不做处理。
                                 VN（Version Number）： 长度为3比特，表示NTP的版本号，目前的最新版本为3（IPv4）。
                                 Mode： 长度为3比特，表示NTP的工作模式。 含义分别是：
                                       0未定义、1表示主动对等体模式、2表示被动对等体模式、3表示客户模式、4表示服务器模式、
                                       5表示广播模式或组播模式、6表示此报文为NTP控制报文、7预留给内部使用。 
                             */
    unsigned char Stratum;   /* Stratum：长度为1字节，系统时钟的层数，因为NTP的网络体系是分层结构，按照离外部UTC（国际标准时间）源的远近将所有服务器归入不同的Stratum（层）中，
                                        Stratum-0层为官方时钟所保留，Stratum-1在顶层，有外部UTC接入，因此层数为1的时钟准确度最高，而Stratum-2则从Stratum-1获取时间，Stratum-3从Stratum-2获取时间，
                                        以此类推，准确度从1到16依次递减，但Stratum层的总数限制在15以内，层数为16的时钟处于未同步状态，不能作为参考时钟。
                                        所有这些服务器在逻辑上形成阶梯式的架构相互连接，而Stratum-1的时间服务器是整个系统的基础。取值范围为1～16，它定义了时钟的准确度。
                                        这个字段一般填0即可。
                             */
    unsigned char Poll;       /* Poll：长度为1字节，轮询时间，连续查询时的最大间隔，即两个连续NTP报文之间的时间间隔。以2的x次幂秒的形式表示。实际填0即可。 */
    unsigned char Precision;  /* Precision：长度为1字节，系统时钟的精度。以2的负x次幂秒表示。实际填0，服务器返回时会填写实际精度。 */
    long RootDelay;       /* Root Delay：长度为4字节，本地到主参考时钟源的往返总延迟时间。为固定小数点数，小数点在bit 15和bit 16之间。 */
    long RootDispersion;  /* Root Dispersion：长度为4字节，系统时钟相对于主参考时钟源的最大误差。为固定小数点数，小数点在bit 15和bit 16之间。 */
    char ReferenceID[4];  /* Reference Identifier：长度为4字节，参考时钟源的标识。 */
    NtpTimePacket ReferenceTimestamp;   /* Reference Timestamp：长度为8字节，本地时中最后一次设置或修正时的时间，timestamp格式。*/
    NtpTimePacket OriginateTimestamp;   /* Originate Timestamp：长度为8字节，NTP请求报文离开发送端时发送端的本地时间。前面原理部分说到的t1 */
    NtpTimePacket ReceiveTimestamp;     /* Receive Timestamp：长度为8字节，NTP请求报文到达接收端时接收端的本地时间。前面原理部分说到的t2 */
    NtpTimePacket TransmitTimestamp;    /* Transmit Timestamp：长度为8字节，应答报文离开应答者时应答者的本地时间。前面原理部分说到的t3 */
    NtpTimePacket DestinationTimestamp; /* Destination Timestamp：应答报文到达请求者时请求者的本地时间。前面原理部分说到的t4 */
} NtpBasicInfo;

/*The optional part of an NTP packet */
typedef struct _NtpAuthenticationInfo {
    unsigned long KeyID;
    unsigned char MessageDigest[16];
} NtpAuthenticationInfo;

/*The Full NTP packet */
typedef struct _NtpFullPacket {
    NtpBasicInfo          Basic;
    NtpAuthenticationInfo Auth;
} NtpFullPacket;


/*Lookup table to convert from Milliseconds (hence 1000 Entries)
  to fractions of a second expressed as a unsigned long
*/
unsigned long MsToNTP[1000] = {
    0x00000000, 0x00418937, 0x0083126f, 0x00c49ba6, 0x010624dd, 0x0147ae14,
    0x0189374c, 0x01cac083, 0x020c49ba, 0x024dd2f2, 0x028f5c29, 0x02d0e560,
    0x03126e98, 0x0353f7cf, 0x03958106, 0x03d70a3d, 0x04189375, 0x045a1cac,
    0x049ba5e3, 0x04dd2f1b, 0x051eb852, 0x05604189, 0x05a1cac1, 0x05e353f8,
    0x0624dd2f, 0x06666666, 0x06a7ef9e, 0x06e978d5, 0x072b020c, 0x076c8b44,
    0x07ae147b, 0x07ef9db2, 0x083126e9, 0x0872b021, 0x08b43958, 0x08f5c28f,
    0x09374bc7, 0x0978d4fe, 0x09ba5e35, 0x09fbe76d, 0x0a3d70a4, 0x0a7ef9db,
    0x0ac08312, 0x0b020c4a, 0x0b439581, 0x0b851eb8, 0x0bc6a7f0, 0x0c083127,
    0x0c49ba5e, 0x0c8b4396, 0x0ccccccd, 0x0d0e5604, 0x0d4fdf3b, 0x0d916873,
    0x0dd2f1aa, 0x0e147ae1, 0x0e560419, 0x0e978d50, 0x0ed91687, 0x0f1a9fbe,
    0x0f5c28f6, 0x0f9db22d, 0x0fdf3b64, 0x1020c49c, 0x10624dd3, 0x10a3d70a,
    0x10e56042, 0x1126e979, 0x116872b0, 0x11a9fbe7, 0x11eb851f, 0x122d0e56,
    0x126e978d, 0x12b020c5, 0x12f1a9fc, 0x13333333, 0x1374bc6a, 0x13b645a2,
    0x13f7ced9, 0x14395810, 0x147ae148, 0x14bc6a7f, 0x14fdf3b6, 0x153f7cee,
    0x15810625, 0x15c28f5c, 0x16041893, 0x1645a1cb, 0x16872b02, 0x16c8b439,
    0x170a3d71, 0x174bc6a8, 0x178d4fdf, 0x17ced917, 0x1810624e, 0x1851eb85,
    0x189374bc, 0x18d4fdf4, 0x1916872b, 0x19581062, 0x1999999a, 0x19db22d1,
    0x1a1cac08, 0x1a5e353f, 0x1a9fbe77, 0x1ae147ae, 0x1b22d0e5, 0x1b645a1d,
    0x1ba5e354, 0x1be76c8b, 0x1c28f5c3, 0x1c6a7efa, 0x1cac0831, 0x1ced9168,
    0x1d2f1aa0, 0x1d70a3d7, 0x1db22d0e, 0x1df3b646, 0x1e353f7d, 0x1e76c8b4,
    0x1eb851ec, 0x1ef9db23, 0x1f3b645a, 0x1f7ced91, 0x1fbe76c9, 0x20000000,
    0x20418937, 0x2083126f, 0x20c49ba6, 0x210624dd, 0x2147ae14, 0x2189374c,
    0x21cac083, 0x220c49ba, 0x224dd2f2, 0x228f5c29, 0x22d0e560, 0x23126e98,
    0x2353f7cf, 0x23958106, 0x23d70a3d, 0x24189375, 0x245a1cac, 0x249ba5e3,
    0x24dd2f1b, 0x251eb852, 0x25604189, 0x25a1cac1, 0x25e353f8, 0x2624dd2f,
    0x26666666, 0x26a7ef9e, 0x26e978d5, 0x272b020c, 0x276c8b44, 0x27ae147b,
    0x27ef9db2, 0x283126e9, 0x2872b021, 0x28b43958, 0x28f5c28f, 0x29374bc7,
    0x2978d4fe, 0x29ba5e35, 0x29fbe76d, 0x2a3d70a4, 0x2a7ef9db, 0x2ac08312,
    0x2b020c4a, 0x2b439581, 0x2b851eb8, 0x2bc6a7f0, 0x2c083127, 0x2c49ba5e,
    0x2c8b4396, 0x2ccccccd, 0x2d0e5604, 0x2d4fdf3b, 0x2d916873, 0x2dd2f1aa,
    0x2e147ae1, 0x2e560419, 0x2e978d50, 0x2ed91687, 0x2f1a9fbe, 0x2f5c28f6,
    0x2f9db22d, 0x2fdf3b64, 0x3020c49c, 0x30624dd3, 0x30a3d70a, 0x30e56042,
    0x3126e979, 0x316872b0, 0x31a9fbe7, 0x31eb851f, 0x322d0e56, 0x326e978d,
    0x32b020c5, 0x32f1a9fc, 0x33333333, 0x3374bc6a, 0x33b645a2, 0x33f7ced9,
    0x34395810, 0x347ae148, 0x34bc6a7f, 0x34fdf3b6, 0x353f7cee, 0x35810625,
    0x35c28f5c, 0x36041893, 0x3645a1cb, 0x36872b02, 0x36c8b439, 0x370a3d71,
    0x374bc6a8, 0x378d4fdf, 0x37ced917, 0x3810624e, 0x3851eb85, 0x389374bc,
    0x38d4fdf4, 0x3916872b, 0x39581062, 0x3999999a, 0x39db22d1, 0x3a1cac08,
    0x3a5e353f, 0x3a9fbe77, 0x3ae147ae, 0x3b22d0e5, 0x3b645a1d, 0x3ba5e354,
    0x3be76c8b, 0x3c28f5c3, 0x3c6a7efa, 0x3cac0831, 0x3ced9168, 0x3d2f1aa0,
    0x3d70a3d7, 0x3db22d0e, 0x3df3b646, 0x3e353f7d, 0x3e76c8b4, 0x3eb851ec,
    0x3ef9db23, 0x3f3b645a, 0x3f7ced91, 0x3fbe76c9, 0x40000000, 0x40418937,
    0x4083126f, 0x40c49ba6, 0x410624dd, 0x4147ae14, 0x4189374c, 0x41cac083,
    0x420c49ba, 0x424dd2f2, 0x428f5c29, 0x42d0e560, 0x43126e98, 0x4353f7cf,
    0x43958106, 0x43d70a3d, 0x44189375, 0x445a1cac, 0x449ba5e3, 0x44dd2f1b,
    0x451eb852, 0x45604189, 0x45a1cac1, 0x45e353f8, 0x4624dd2f, 0x46666666,
    0x46a7ef9e, 0x46e978d5, 0x472b020c, 0x476c8b44, 0x47ae147b, 0x47ef9db2,
    0x483126e9, 0x4872b021, 0x48b43958, 0x48f5c28f, 0x49374bc7, 0x4978d4fe,
    0x49ba5e35, 0x49fbe76d, 0x4a3d70a4, 0x4a7ef9db, 0x4ac08312, 0x4b020c4a,
    0x4b439581, 0x4b851eb8, 0x4bc6a7f0, 0x4c083127, 0x4c49ba5e, 0x4c8b4396,
    0x4ccccccd, 0x4d0e5604, 0x4d4fdf3b, 0x4d916873, 0x4dd2f1aa, 0x4e147ae1,
    0x4e560419, 0x4e978d50, 0x4ed91687, 0x4f1a9fbe, 0x4f5c28f6, 0x4f9db22d,
    0x4fdf3b64, 0x5020c49c, 0x50624dd3, 0x50a3d70a, 0x50e56042, 0x5126e979,
    0x516872b0, 0x51a9fbe7, 0x51eb851f, 0x522d0e56, 0x526e978d, 0x52b020c5,
    0x52f1a9fc, 0x53333333, 0x5374bc6a, 0x53b645a2, 0x53f7ced9, 0x54395810,
    0x547ae148, 0x54bc6a7f, 0x54fdf3b6, 0x553f7cee, 0x55810625, 0x55c28f5c,
    0x56041893, 0x5645a1cb, 0x56872b02, 0x56c8b439, 0x570a3d71, 0x574bc6a8,
    0x578d4fdf, 0x57ced917, 0x5810624e, 0x5851eb85, 0x589374bc, 0x58d4fdf4,
    0x5916872b, 0x59581062, 0x5999999a, 0x59db22d1, 0x5a1cac08, 0x5a5e353f,
    0x5a9fbe77, 0x5ae147ae, 0x5b22d0e5, 0x5b645a1d, 0x5ba5e354, 0x5be76c8b,
    0x5c28f5c3, 0x5c6a7efa, 0x5cac0831, 0x5ced9168, 0x5d2f1aa0, 0x5d70a3d7,
    0x5db22d0e, 0x5df3b646, 0x5e353f7d, 0x5e76c8b4, 0x5eb851ec, 0x5ef9db23,
    0x5f3b645a, 0x5f7ced91, 0x5fbe76c9, 0x60000000, 0x60418937, 0x6083126f,
    0x60c49ba6, 0x610624dd, 0x6147ae14, 0x6189374c, 0x61cac083, 0x620c49ba,
    0x624dd2f2, 0x628f5c29, 0x62d0e560, 0x63126e98, 0x6353f7cf, 0x63958106,
    0x63d70a3d, 0x64189375, 0x645a1cac, 0x649ba5e3, 0x64dd2f1b, 0x651eb852,
    0x65604189, 0x65a1cac1, 0x65e353f8, 0x6624dd2f, 0x66666666, 0x66a7ef9e,
    0x66e978d5, 0x672b020c, 0x676c8b44, 0x67ae147b, 0x67ef9db2, 0x683126e9,
    0x6872b021, 0x68b43958, 0x68f5c28f, 0x69374bc7, 0x6978d4fe, 0x69ba5e35,
    0x69fbe76d, 0x6a3d70a4, 0x6a7ef9db, 0x6ac08312, 0x6b020c4a, 0x6b439581,
    0x6b851eb8, 0x6bc6a7f0, 0x6c083127, 0x6c49ba5e, 0x6c8b4396, 0x6ccccccd,
    0x6d0e5604, 0x6d4fdf3b, 0x6d916873, 0x6dd2f1aa, 0x6e147ae1, 0x6e560419,
    0x6e978d50, 0x6ed91687, 0x6f1a9fbe, 0x6f5c28f6, 0x6f9db22d, 0x6fdf3b64,
    0x7020c49c, 0x70624dd3, 0x70a3d70a, 0x70e56042, 0x7126e979, 0x716872b0,
    0x71a9fbe7, 0x71eb851f, 0x722d0e56, 0x726e978d, 0x72b020c5, 0x72f1a9fc,
    0x73333333, 0x7374bc6a, 0x73b645a2, 0x73f7ced9, 0x74395810, 0x747ae148,
    0x74bc6a7f, 0x74fdf3b6, 0x753f7cee, 0x75810625, 0x75c28f5c, 0x76041893,
    0x7645a1cb, 0x76872b02, 0x76c8b439, 0x770a3d71, 0x774bc6a8, 0x778d4fdf,
    0x77ced917, 0x7810624e, 0x7851eb85, 0x789374bc, 0x78d4fdf4, 0x7916872b,
    0x79581062, 0x7999999a, 0x79db22d1, 0x7a1cac08, 0x7a5e353f, 0x7a9fbe77,
    0x7ae147ae, 0x7b22d0e5, 0x7b645a1d, 0x7ba5e354, 0x7be76c8b, 0x7c28f5c3,
    0x7c6a7efa, 0x7cac0831, 0x7ced9168, 0x7d2f1aa0, 0x7d70a3d7, 0x7db22d0e,
    0x7df3b646, 0x7e353f7d, 0x7e76c8b4, 0x7eb851ec, 0x7ef9db23, 0x7f3b645a,
    0x7f7ced91, 0x7fbe76c9, 0x80000000, 0x80418937, 0x8083126f, 0x80c49ba6,
    0x810624dd, 0x8147ae14, 0x8189374c, 0x81cac083, 0x820c49ba, 0x824dd2f2,
    0x828f5c29, 0x82d0e560, 0x83126e98, 0x8353f7cf, 0x83958106, 0x83d70a3d,
    0x84189375, 0x845a1cac, 0x849ba5e3, 0x84dd2f1b, 0x851eb852, 0x85604189,
    0x85a1cac1, 0x85e353f8, 0x8624dd2f, 0x86666666, 0x86a7ef9e, 0x86e978d5,
    0x872b020c, 0x876c8b44, 0x87ae147b, 0x87ef9db2, 0x883126e9, 0x8872b021,
    0x88b43958, 0x88f5c28f, 0x89374bc7, 0x8978d4fe, 0x89ba5e35, 0x89fbe76d,
    0x8a3d70a4, 0x8a7ef9db, 0x8ac08312, 0x8b020c4a, 0x8b439581, 0x8b851eb8,
    0x8bc6a7f0, 0x8c083127, 0x8c49ba5e, 0x8c8b4396, 0x8ccccccd, 0x8d0e5604,
    0x8d4fdf3b, 0x8d916873, 0x8dd2f1aa, 0x8e147ae1, 0x8e560419, 0x8e978d50,
    0x8ed91687, 0x8f1a9fbe, 0x8f5c28f6, 0x8f9db22d, 0x8fdf3b64, 0x9020c49c,
    0x90624dd3, 0x90a3d70a, 0x90e56042, 0x9126e979, 0x916872b0, 0x91a9fbe7,
    0x91eb851f, 0x922d0e56, 0x926e978d, 0x92b020c5, 0x92f1a9fc, 0x93333333,
    0x9374bc6a, 0x93b645a2, 0x93f7ced9, 0x94395810, 0x947ae148, 0x94bc6a7f,
    0x94fdf3b6, 0x953f7cee, 0x95810625, 0x95c28f5c, 0x96041893, 0x9645a1cb,
    0x96872b02, 0x96c8b439, 0x970a3d71, 0x974bc6a8, 0x978d4fdf, 0x97ced917,
    0x9810624e, 0x9851eb85, 0x989374bc, 0x98d4fdf4, 0x9916872b, 0x99581062,
    0x9999999a, 0x99db22d1, 0x9a1cac08, 0x9a5e353f, 0x9a9fbe77, 0x9ae147ae,
    0x9b22d0e5, 0x9b645a1d, 0x9ba5e354, 0x9be76c8b, 0x9c28f5c3, 0x9c6a7efa,
    0x9cac0831, 0x9ced9168, 0x9d2f1aa0, 0x9d70a3d7, 0x9db22d0e, 0x9df3b646,
    0x9e353f7d, 0x9e76c8b4, 0x9eb851ec, 0x9ef9db23, 0x9f3b645a, 0x9f7ced91,
    0x9fbe76c9, 0xa0000000, 0xa0418937, 0xa083126f, 0xa0c49ba6, 0xa10624dd,
    0xa147ae14, 0xa189374c, 0xa1cac083, 0xa20c49ba, 0xa24dd2f2, 0xa28f5c29,
    0xa2d0e560, 0xa3126e98, 0xa353f7cf, 0xa3958106, 0xa3d70a3d, 0xa4189375,
    0xa45a1cac, 0xa49ba5e3, 0xa4dd2f1b, 0xa51eb852, 0xa5604189, 0xa5a1cac1,
    0xa5e353f8, 0xa624dd2f, 0xa6666666, 0xa6a7ef9e, 0xa6e978d5, 0xa72b020c,
    0xa76c8b44, 0xa7ae147b, 0xa7ef9db2, 0xa83126e9, 0xa872b021, 0xa8b43958,
    0xa8f5c28f, 0xa9374bc7, 0xa978d4fe, 0xa9ba5e35, 0xa9fbe76d, 0xaa3d70a4,
    0xaa7ef9db, 0xaac08312, 0xab020c4a, 0xab439581, 0xab851eb8, 0xabc6a7f0,
    0xac083127, 0xac49ba5e, 0xac8b4396, 0xaccccccd, 0xad0e5604, 0xad4fdf3b,
    0xad916873, 0xadd2f1aa, 0xae147ae1, 0xae560419, 0xae978d50, 0xaed91687,
    0xaf1a9fbe, 0xaf5c28f6, 0xaf9db22d, 0xafdf3b64, 0xb020c49c, 0xb0624dd3,
    0xb0a3d70a, 0xb0e56042, 0xb126e979, 0xb16872b0, 0xb1a9fbe7, 0xb1eb851f,
    0xb22d0e56, 0xb26e978d, 0xb2b020c5, 0xb2f1a9fc, 0xb3333333, 0xb374bc6a,
    0xb3b645a2, 0xb3f7ced9, 0xb4395810, 0xb47ae148, 0xb4bc6a7f, 0xb4fdf3b6,
    0xb53f7cee, 0xb5810625, 0xb5c28f5c, 0xb6041893, 0xb645a1cb, 0xb6872b02,
    0xb6c8b439, 0xb70a3d71, 0xb74bc6a8, 0xb78d4fdf, 0xb7ced917, 0xb810624e,
    0xb851eb85, 0xb89374bc, 0xb8d4fdf4, 0xb916872b, 0xb9581062, 0xb999999a,
    0xb9db22d1, 0xba1cac08, 0xba5e353f, 0xba9fbe77, 0xbae147ae, 0xbb22d0e5,
    0xbb645a1d, 0xbba5e354, 0xbbe76c8b, 0xbc28f5c3, 0xbc6a7efa, 0xbcac0831,
    0xbced9168, 0xbd2f1aa0, 0xbd70a3d7, 0xbdb22d0e, 0xbdf3b646, 0xbe353f7d,
    0xbe76c8b4, 0xbeb851ec, 0xbef9db23, 0xbf3b645a, 0xbf7ced91, 0xbfbe76c9,
    0xc0000000, 0xc0418937, 0xc083126f, 0xc0c49ba6, 0xc10624dd, 0xc147ae14,
    0xc189374c, 0xc1cac083, 0xc20c49ba, 0xc24dd2f2, 0xc28f5c29, 0xc2d0e560,
    0xc3126e98, 0xc353f7cf, 0xc3958106, 0xc3d70a3d, 0xc4189375, 0xc45a1cac,
    0xc49ba5e3, 0xc4dd2f1b, 0xc51eb852, 0xc5604189, 0xc5a1cac1, 0xc5e353f8,
    0xc624dd2f, 0xc6666666, 0xc6a7ef9e, 0xc6e978d5, 0xc72b020c, 0xc76c8b44,
    0xc7ae147b, 0xc7ef9db2, 0xc83126e9, 0xc872b021, 0xc8b43958, 0xc8f5c28f,
    0xc9374bc7, 0xc978d4fe, 0xc9ba5e35, 0xc9fbe76d, 0xca3d70a4, 0xca7ef9db,
    0xcac08312, 0xcb020c4a, 0xcb439581, 0xcb851eb8, 0xcbc6a7f0, 0xcc083127,
    0xcc49ba5e, 0xcc8b4396, 0xcccccccd, 0xcd0e5604, 0xcd4fdf3b, 0xcd916873,
    0xcdd2f1aa, 0xce147ae1, 0xce560419, 0xce978d50, 0xced91687, 0xcf1a9fbe,
    0xcf5c28f6, 0xcf9db22d, 0xcfdf3b64, 0xd020c49c, 0xd0624dd3, 0xd0a3d70a,
    0xd0e56042, 0xd126e979, 0xd16872b0, 0xd1a9fbe7, 0xd1eb851f, 0xd22d0e56,
    0xd26e978d, 0xd2b020c5, 0xd2f1a9fc, 0xd3333333, 0xd374bc6a, 0xd3b645a2,
    0xd3f7ced9, 0xd4395810, 0xd47ae148, 0xd4bc6a7f, 0xd4fdf3b6, 0xd53f7cee,
    0xd5810625, 0xd5c28f5c, 0xd6041893, 0xd645a1cb, 0xd6872b02, 0xd6c8b439,
    0xd70a3d71, 0xd74bc6a8, 0xd78d4fdf, 0xd7ced917, 0xd810624e, 0xd851eb85,
    0xd89374bc, 0xd8d4fdf4, 0xd916872b, 0xd9581062, 0xd999999a, 0xd9db22d1,
    0xda1cac08, 0xda5e353f, 0xda9fbe77, 0xdae147ae, 0xdb22d0e5, 0xdb645a1d,
    0xdba5e354, 0xdbe76c8b, 0xdc28f5c3, 0xdc6a7efa, 0xdcac0831, 0xdced9168,
    0xdd2f1aa0, 0xdd70a3d7, 0xddb22d0e, 0xddf3b646, 0xde353f7d, 0xde76c8b4,
    0xdeb851ec, 0xdef9db23, 0xdf3b645a, 0xdf7ced91, 0xdfbe76c9, 0xe0000000,
    0xe0418937, 0xe083126f, 0xe0c49ba6, 0xe10624dd, 0xe147ae14, 0xe189374c,
    0xe1cac083, 0xe20c49ba, 0xe24dd2f2, 0xe28f5c29, 0xe2d0e560, 0xe3126e98,
    0xe353f7cf, 0xe3958106, 0xe3d70a3d, 0xe4189375, 0xe45a1cac, 0xe49ba5e3,
    0xe4dd2f1b, 0xe51eb852, 0xe5604189, 0xe5a1cac1, 0xe5e353f8, 0xe624dd2f,
    0xe6666666, 0xe6a7ef9e, 0xe6e978d5, 0xe72b020c, 0xe76c8b44, 0xe7ae147b,
    0xe7ef9db2, 0xe83126e9, 0xe872b021, 0xe8b43958, 0xe8f5c28f, 0xe9374bc7,
    0xe978d4fe, 0xe9ba5e35, 0xe9fbe76d, 0xea3d70a4, 0xea7ef9db, 0xeac08312,
    0xeb020c4a, 0xeb439581, 0xeb851eb8, 0xebc6a7f0, 0xec083127, 0xec49ba5e,
    0xec8b4396, 0xeccccccd, 0xed0e5604, 0xed4fdf3b, 0xed916873, 0xedd2f1aa,
    0xee147ae1, 0xee560419, 0xee978d50, 0xeed91687, 0xef1a9fbe, 0xef5c28f6,
    0xef9db22d, 0xefdf3b64, 0xf020c49c, 0xf0624dd3, 0xf0a3d70a, 0xf0e56042,
    0xf126e979, 0xf16872b0, 0xf1a9fbe7, 0xf1eb851f, 0xf22d0e56, 0xf26e978d,
    0xf2b020c5, 0xf2f1a9fc, 0xf3333333, 0xf374bc6a, 0xf3b645a2, 0xf3f7ced9,
    0xf4395810, 0xf47ae148, 0xf4bc6a7f, 0xf4fdf3b6, 0xf53f7cee, 0xf5810625,
    0xf5c28f5c, 0xf6041893, 0xf645a1cb, 0xf6872b02, 0xf6c8b439, 0xf70a3d71,
    0xf74bc6a8, 0xf78d4fdf, 0xf7ced917, 0xf810624e, 0xf851eb85, 0xf89374bc,
    0xf8d4fdf4, 0xf916872b, 0xf9581062, 0xf999999a, 0xf9db22d1, 0xfa1cac08,
    0xfa5e353f, 0xfa9fbe77, 0xfae147ae, 0xfb22d0e5, 0xfb645a1d, 0xfba5e354,
    0xfbe76c8b, 0xfc28f5c3, 0xfc6a7efa, 0xfcac0831, 0xfced9168, 0xfd2f1aa0,
    0xfd70a3d7, 0xfdb22d0e, 0xfdf3b646, 0xfe353f7d, 0xfe76c8b4, 0xfeb851ec,
    0xfef9db23, 0xff3b645a, 0xff7ced91, 0xffbe76c9
};

const double NTP_FRACTIONAL_TO_MS = (((double)1000.0) / 0xFFFFFFFF);
const double NTP_TO_SECOND = (((double)1.0) / 0xFFFFFFFF);
const long   JAN_1ST_1900  = 2415021;    /* 1900年1月1号 */

static SystemTimeStoreStru SNTP_LocalTime;
static int SNTP_TimeZone;

void SystemTimeToNTPPacket(NtpTimePacket *dst, const SYSTEM_TIME *st);
void AddNtpPacketWithTicks(NtpTimePacket *dstPacket, unsigned long ulTicks);
void NTPTimeToSystemTime(SYSTEM_TIME *st, NtpTimePacket *src);
long GetJulianDay(unsigned short Year, unsigned short Month, unsigned short Day);



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Initialize local time system
Created : 9-May-2004
****************************************************************************/
void Init_LocalTime(void)
{
    /*  struct tm *UtcTime;
    time_t ulTime; */
    SNTP_LocalTime.TickCount = clock(); /*get current tick count*/
    /*  time(&ulTime);
    UtcTime = gmtime(&ulTime);
    */
    SNTP_LocalTime.SystemTime.Seconds.tm_year = 104;
    SNTP_LocalTime.SystemTime.Seconds.tm_mon = 4;
    SNTP_LocalTime.SystemTime.Seconds.tm_mday = 28;
    SNTP_LocalTime.SystemTime.Seconds.tm_yday = 90;
    SNTP_LocalTime.SystemTime.Seconds.tm_hour = 3;
    SNTP_LocalTime.SystemTime.Seconds.tm_min = 32;
    SNTP_LocalTime.SystemTime.Seconds.tm_sec = 20;
    SNTP_LocalTime.SystemTime.Seconds.tm_wday = 2;
    SNTP_LocalTime.SystemTime.Seconds.tm_isdst = 0;
    SNTP_LocalTime.SystemTime.Milliseconds = 0;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Filling dst with current time using NTP time packet format
Created : 9-May-2004
****************************************************************************/
void NtpGetLocalTime(NtpTimePacket *dst)
{
    unsigned long TickDiff;
    unsigned long ulTicks = clock(); /*get current tick count*/
    
    SystemTimeToNTPPacket(dst, &SNTP_LocalTime.SystemTime); /*convert recorded local time to ntp packert format*/
    if (ulTicks < SNTP_LocalTime.TickCount) { /*if the tick count over flow*/
        TickDiff = 0xffffffff - SNTP_LocalTime.TickCount + ulTicks;
    } else { /*the tick count don't over flow*/
        TickDiff = ulTicks - SNTP_LocalTime.TickCount;
    }
    AddNtpPacketWithTicks(dst, TickDiff); /*added the time elapsed from recording */
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Filling dst with current UTC time using system time format
Created : 9-May-2004
****************************************************************************/
void SNTP_GetUTCTime(SYSTEM_TIME *dst)
{
    NtpTimePacket TimePacket;
    unsigned long ulTicks = clock(); /*get current tick count*/
    NtpGetLocalTime(&TimePacket);    /*Get current time with ntp time packet format*/
    NTPTimeToSystemTime(&SNTP_LocalTime.SystemTime, &TimePacket); /*convert to system time format*/
    NTPTimeToSystemTime(dst, &TimePacket);
    *dst = SNTP_LocalTime.SystemTime; /*recorded current time*/
    SNTP_LocalTime.TickCount = ulTicks; /*recorded current tick count*/
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Filling dst with local time using system time format
Created : 9-May-2004
****************************************************************************/
void GetLocalTime(SYSTEM_TIME *dst)
{
    NtpTimePacket TimePacket, tmpTimePacket;
    unsigned long ulTicks = clock(); /*get current tick count*/
    
    NtpGetLocalTime(&TimePacket);    /*Get current time with ntp time packet format*/
    tmpTimePacket.seconds =  TimePacket.seconds + SNTP_TimeZone * 3600; /*Convert to local time*/
    NTPTimeToSystemTime(&SNTP_LocalTime.SystemTime, &TimePacket);       /*convert to system time format*/
    SNTP_LocalTime.TickCount = ulTicks;     /*recorded current tick count*/
    NTPTimeToSystemTime(dst, &tmpTimePacket);
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : set the local time zone with the giving value
Created : 9-May-2004
****************************************************************************/
void SetTimeZone(int TimeZone)
{
    SNTP_TimeZone = TimeZone;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : reset current time with the giving value
Created : 9-May-2004
****************************************************************************/
void SetUTCTime(SYSTEM_TIME *src)
{
    SNTP_LocalTime.TickCount = clock(); /*recorded current tick count*/
    SNTP_LocalTime.SystemTime = *src;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : convert the fraction part of a second to a second expressed with decimal fraction
Created : 9-May-2004
****************************************************************************/
__inline double NtpFractionToSecond(unsigned long ulFraction)
{
    return ((double)ulFraction) * NTP_TO_SECOND;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : added time expressed with ntp time packet  with ticks
Created : 9-May-2004
****************************************************************************/
void AddNtpPacketWithTicks(NtpTimePacket *dstPacket, unsigned long ulTicks)
{
    unsigned long clock_per_sec = CLOCKS_PER_SEC;//ST_GetClocksPerSecond();
    unsigned long frac = MsToNTP[(ulTicks % clock_per_sec) * 1000 / clock_per_sec];     /*get the fraction part*/
    dstPacket->seconds += ulTicks / clock_per_sec;  /*added the time with integer part*/
    if (0xffffffff - frac <= dstPacket->fraction) { /*if the value added two fraction more than 1 second*/
        dstPacket->seconds++;    /*added integer part*/
    }
    dstPacket->fraction += frac; /*added fraction parts*/
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : convert system time to ntp packet format,
   Currently this function only operates correctly in the 1900 - 2036 primary epoch defined by NTP
Created : 9-May-2004
****************************************************************************/
void SystemTimeToNTPPacket(NtpTimePacket *dst, const SYSTEM_TIME *st)
{
    unsigned long Seconds;

    long JD = GetJulianDay(st->Seconds.tm_year /*+ 1900*/, st->Seconds.tm_mon, st->Seconds.tm_mday);
    JD -= JAN_1ST_1900;
    Seconds = JD;
    Seconds = (Seconds * 24) + st->Seconds.tm_hour;
    Seconds = (Seconds * 60) + st->Seconds.tm_min;
    dst->seconds = (Seconds * 60) + st->Seconds.tm_sec;
    dst->fraction = MsToNTP[st->Milliseconds];
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : exchange the bytes sequence of a NtpTimePacket struct from host to network
Created : 9-May-2004
****************************************************************************/
__inline NtpTimePacket htonl_NTPPacket(NtpTimePacket src)
{
    NtpTimePacket np;
    np.seconds = htonl(src.seconds);
    np.fraction = htonl(src.fraction);
    return np;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : exchange the bytes sequence of a NtpTimePacket struct from network to host
Created : 9-May-2004
****************************************************************************/
__inline NtpTimePacket ntohl_NTPPacket(NtpTimePacket src)
{
    NtpTimePacket np;
    np.seconds = ntohl(src.seconds);
    np.fraction = ntohl(src.fraction);
    return np;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : convert a y-m-d format time to Julian date format
Created : 9-May-2004
****************************************************************************/
long GetJulianDay(unsigned short Year, unsigned short Month, unsigned short Day)
{
    long c, ya, j;
    long y = (long) Year;
    long m = (long) Month + 1;
    long d = (long) Day;

    if (m > 2)
        m = m - 3;
    else {
        m = m + 9;
        y = y - 1;
    }
    c = y / 100;
    ya = y - 100 * c;
    j = (146097L * c) / 4 + (1461L * ya) / 4 + (153L * m + 2) / 5 + d + 1721119L;
    return j;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : compare two time packet
Created : 9-May-2004
****************************************************************************/
int CompareNtpTimePacket(NtpTimePacket *time1, NtpTimePacket *time2)
{
    /*compare integer parts*/
    if (time1->seconds > time2->seconds)
        return 1;   /*time1 > time2*/
    if (time1->seconds < time2->seconds)
        return -1;  /*time1 < time2*/
    /*compare fraction parts*/
    if (time1->fraction == time2->fraction)
        return 0;   /*time1 == time2*/
    if (time1->fraction > time2->fraction)
        return 1;   /*time1 > time2*/
    else
        return -1;  /*time1 < time2*/
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : subtract time1 with time2, return a time expressed with decimal fraction seconds
Created : 9-May-2004
****************************************************************************/
double SubtractTimePacket(NtpTimePacket *time1, NtpTimePacket *time2)
{
    unsigned long MediumInteger, MediumFraction;
    if (CompareNtpTimePacket(time1, time2) >= 0) {
        MediumInteger = time1->seconds - time2->seconds;
        if (time1->fraction >= time2->fraction) {
            MediumFraction = time1->fraction - time2->fraction;
        } else {
            MediumInteger--;
            MediumFraction = 0xffffffff - time2->fraction +  time1->fraction;
        }
        return (double)MediumInteger + NtpFractionToSecond(MediumFraction);
    } else {
        MediumInteger = time2->seconds - time1->seconds;
        if (time2->fraction >= time1->fraction) {
            MediumFraction = time2->fraction - time1->fraction;
        } else {
            MediumInteger--;
            MediumFraction = 0xffffffff - time1->fraction +  time2->fraction;
        }
        return -((double)MediumInteger + NtpFractionToSecond(MediumFraction));
    }
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : add dst with the time expressed with decimal fraction seconds
Created : 9-May-2004
****************************************************************************/
void AddNtpTimePacket(NtpTimePacket *dst, double src)
{
    unsigned long MediumFrac;
    if (src >= 0) {
        double intpart;
        double frac = modf(src, &intpart);
        MediumFrac = (unsigned long) (frac * 0xFFFFFFFF);
        dst->seconds += (unsigned int)src;
        if (0xFFFFFFFF - MediumFrac >= dst->fraction) {
            dst->seconds++;
        }
        dst->fraction += MediumFrac;
    } else {
        double intpart, frac;

        src *= -1;
        frac = modf(src, &intpart);
        MediumFrac = (unsigned long) (frac * 0xFFFFFFFF);
        dst->seconds -= (unsigned int)src;
        if (MediumFrac > dst->fraction) {
            dst->seconds--;
            dst->fraction += (long)(0xFFFFFFFF - MediumFrac);
        } else {
            dst->fraction -= MediumFrac;
        }
    }
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : get year,month and day from a time with Gregorian date format
Created : 9-May-2004
****************************************************************************/
void GetGregorianDate(long JD, int *Year, int *Month, int *Day)
{
    long d, m;
    long j = JD - 1721119;
    long y = (4 * j - 1) / 146097;

    printf("GetGregorianDate, JD[%ld], y[%ld], j[%ld]\n", JD, y, j);

    j = 4 * j - 1 - 146097 * y;
    d = j / 4;
    j = (4 * d + 3) / 1461;
    d = 4 * d + 3 - 1461 * j;
    d = (d + 4) / 4;
    m = (5 * d - 3) / 153;
    d = 5 * d - 3 - 153 * m;
    d = (d + 5) / 5;
    y = 100 * y + j;
    printf("GetGregorianDate, y[%ld]\n",  y);
    if (m < 10)
        m = m + 3;
    else {
        m = m - 9;
        y = y + 1;
    }
    printf("GetGregorianDate,  y[%ld]\n", y);

    *Year = (unsigned short) y;
    *Month = (unsigned char) m - 1;
    *Day = (unsigned char) d;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : convert a time expressed with decimal fraction seconds to a time exprressed with microseconds
Created : 9-May-2004
****************************************************************************/
__inline unsigned short NtpFractionToMs(unsigned long ulFraction)
{
    return (unsigned short)((((double)ulFraction) * NTP_FRACTIONAL_TO_MS) + 0.5);
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : convert a time with ntp packet format to SYSTEM_TIME format;
  Currently this function only operates correctly in the 1900 - 2036 primary epoch defined by NTP
Created : 9-May-2004
****************************************************************************/
void NTPTimeToSystemTime(SYSTEM_TIME *st, NtpTimePacket *src)
{
    long JD;
    unsigned long s = src->seconds;

    st->Seconds.tm_sec = (unsigned char)(s % 60);
    s /= 60;
    st->Seconds.tm_min = (unsigned char)(s % 60);
    s /= 60;
    st->Seconds.tm_hour = (unsigned char)(s % 24);
    s /= 24;
    JD = s + JAN_1ST_1900;
    
    st->Seconds.tm_wday = (unsigned short)((JD + 1) % 7);
    GetGregorianDate(JD, &st->Seconds.tm_year, &st->Seconds.tm_mon, &st->Seconds.tm_mday);
    st->Milliseconds = NtpFractionToMs(src->fraction);
    
    printf("NTPTimeToSystemTime, st->Seconds.tm_sec[%d], st->Seconds.tm_min[%d], st->Seconds.tm_hour[%d], st->Seconds.tm_wday[%d], st->Milliseconds[%d]\n", 
            st->Seconds.tm_sec, st->Seconds.tm_min, st->Seconds.tm_hour, st->Seconds.tm_wday, st->Milliseconds);
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Initialize SNTP driver with a block of memory
Created : 9-May-2004
****************************************************************************/
SNTP_Handle SNTP_Init(SNTP_Manage_Info *info)
{
    if (NULL == info)
        return NULL;
    memset(info, 0, sizeof(SNTP_Manage_Info));
    return (SNTP_Handle)info;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : open the network port
Created : 9-May-2004
****************************************************************************/
int SNTP_Open(SNTP_Handle handle)
{
    int nSetResult;
    struct timeval TimeOut;
    
    if  (NULL == handle)
        return SNTP_HANDLE_INVALID;
    handle->iNetSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == handle->iNetSocket) {
        return SNTP_NETWORK_ERROR;
    }
    TimeOut.tv_sec = DEFAULT_TIMEOUT_MS / 1000;
    TimeOut.tv_usec = DEFAULT_TIMEOUT_MS % 1000;
    nSetResult = setsockopt( (int)handle->iNetSocket, SOL_SOCKET, SO_RCVTIMEO,  &TimeOut, sizeof(TimeOut));

    return SNTP_NO_ERROR;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Get the time from the server with indicated ip address
Created : 9-May-2004
****************************************************************************/
int SNTP_Get(SNTP_Handle handle, char *pServerIP)
{
    NtpBasicInfo  SendPacket = {0};
    NtpFullPacket ReceivePacket = {};
    double LocalClockOffset = 0;
    NtpTimePacket tempLocalTime = {0};
    
    SYSTEM_TIME ServerRec, ServerSend, ClientRec, ClientSent; 
    
    printf("SNTP_Get  11111111111111111111111 \n");
    
    if  ((NULL == pServerIP) || (' ' == pServerIP[0]))
        return SNTP_NETWORK_ERROR;
    if (0 == handle->iNetPort)    /*using default port when no port indicated */
        handle->iNetPort = SNTP_UDP_PORT_DEFAULT;  //123
    handle->ServerAddr.sin_family      = AF_INET;
    handle->ServerAddr.sin_port        = htons(handle->iNetPort);
    handle->ServerAddr.sin_addr.s_addr = (unsigned long)inet_addr(pServerIP);
    
    printf("SNTP_Get  22222222222222222222222 \n");
    /*If the address is not dotted notation, then do a DNS lookup of it. */
    if (handle->ServerAddr.sin_addr.s_addr == INADDR_NONE){
        struct hostent *phost;
        phost = gethostbyname(pServerIP);
        if (phost != NULL){
            handle->ServerAddr.sin_addr.s_addr = ((struct in_addr*)phost->h_addr)->s_addr;
        } else {
            printf("DNS lookup failed.\n");
            return SNTP_NETWORK_ERROR;
        }
    }
    printf("SNTP_Get  33333333333333333333333 \n");
   
    if (connect(handle->iNetSocket, (const struct sockaddr *)&handle->ServerAddr, sizeof(handle->ServerAddr))) {
        printf("Socket connected failed.\n");
        return SNTP_NETWORK_ERROR;
    }
    
    printf("SNTP_Get  44444444444444444444444 \n");
    /* Encoded representation which represents NTP Client Request & NTP version 3.0 */
    SendPacket.LiVnMode = DEFAULT_LI_VN_MODE;  /* 27即:'00 011 011' */
    NtpGetLocalTime(&SendPacket.DestinationTimestamp);

    printf("SNTP_Get  55555555555555555555555 \n");
    SendPacket.DestinationTimestamp = htonl_NTPPacket(SendPacket.DestinationTimestamp);
    /* Send off the NtpBasicInfo packet */
    if (send(handle->iNetSocket, &SendPacket, sizeof(NtpBasicInfo), 0) == -1) {
        printf("Send packet to NTP server failed.\n");
        return SNTP_NETWORK_ERROR;
    }

    printf("SNTP_Get  66666666666666666666666 \n");
    /*receive packet*/
    if (recv(handle->iNetSocket, &ReceivePacket, sizeof(NtpFullPacket), 0) == -1) {
        printf("Receive packet from NTP server failed.\n");
        return SNTP_NETWORK_ERROR;
    }
    memmove(&ReceivePacket.Basic.OriginateTimestamp, &ReceivePacket.Basic.ReceiveTimestamp, sizeof(NtpTimePacket) * 3);
    NtpGetLocalTime(&ReceivePacket.Basic.DestinationTimestamp);
    
    printf("SNTP_Get  77777777777777777777777777777 \n");
    /*exchange byte sequence*/
    ReceivePacket.Basic.ReceiveTimestamp = ntohl_NTPPacket(ReceivePacket.Basic.ReceiveTimestamp);
    ReceivePacket.Basic.OriginateTimestamp = ntohl_NTPPacket(ReceivePacket.Basic.OriginateTimestamp);
    ReceivePacket.Basic.TransmitTimestamp = ntohl_NTPPacket(ReceivePacket.Basic.TransmitTimestamp);
    
    /*print time stamp*/
    NTPTimeToSystemTime(&ServerRec,  &ReceivePacket.Basic.ReceiveTimestamp);
    NTPTimeToSystemTime(&ServerSend, &ReceivePacket.Basic.TransmitTimestamp);
    NTPTimeToSystemTime(&ClientSent, &ReceivePacket.Basic.OriginateTimestamp);
    NTPTimeToSystemTime(&ClientRec,  &ReceivePacket.Basic.DestinationTimestamp);
    printf("nServer received time: %snServer sent time: %sn", asctime(&ServerRec.Seconds), asctime(&ServerSend.Seconds));
    printf("nClient sent time: %snClient received time: %sn", asctime(&ClientSent.Seconds), asctime(&ClientRec.Seconds));
    
    
    /*calculate local time offset*/
    LocalClockOffset = SubtractTimePacket(&ReceivePacket.Basic.ReceiveTimestamp, &ReceivePacket.Basic.OriginateTimestamp);
    LocalClockOffset += SubtractTimePacket(&ReceivePacket.Basic.TransmitTimestamp, &ReceivePacket.Basic.DestinationTimestamp);
    LocalClockOffset /= 2;
    
    /*Caculate local time*/
    NtpGetLocalTime(&tempLocalTime);
    AddNtpTimePacket(&tempLocalTime, LocalClockOffset);

    NTPTimeToSystemTime(&handle->SystemTime, &tempLocalTime);
    return SNTP_NO_ERROR;

}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : close the network port
Created : 9-May-2004
****************************************************************************/
int SNTP_Close(SNTP_Handle handle)
{
    if (handle->iNetSocket != -1) {
        if (close(handle->iNetSocket) == -1) {
            return SNTP_NETWORK_ERROR;
        }
    }
    return SNTP_NO_ERROR;
}



/****************************************************************************
COPYRIGHT (C) ST Microelectronics 2004
Source file name    : SNTP.c
Author  : Chuanping WU
Description : Stop the sntp driver
Created : 9-May-2004
****************************************************************************/
int SNTP_Term(SNTP_Handle handle)
{
    return SNTP_NO_ERROR;
}








/* some ntp server 
time-a.nist.gov 
time-b.nist.gov 
time-a.timefreq.bldrdoc.gov 
time-b.timefreq.bldrdoc.gov 
time-c.timefreq.bldrdoc.gov 
utcnist.colorado.edu 
ntp.tuxfamily.net 
time-nw.nist.gov 
nist1.datum.com 
nist1-ny.glassey.com 
nist1.aol-ca.truetime.com 
ntp2.belbone.be 
ntp.doubleukay.com 
ntp0.cs.mu.OZ.AU 
time.apple.com 
0.pool.ntp.org 
1.pool.ntp.org 
2.pool.ntp.org 
2.europe.pool.ntp.org 
3.europe.pool.ntp.org 
1.north-america.pool.ntp.org 
2.north-america.pool.ntp.org 
0.oceania.pool.ntp.org 
1.oceania.pool.ntp.org 
0.au.pool.ntp.org 
3.au.pool.ntp.org 
*/  
 int test1();
 int test2();
int main()
{
    printf("main start....\n");
    
    Init_LocalTime();
    SetTimeZone(0);
    printf("main 1111111111111111\n");
    
    SYSTEM_TIME dst;
    GetLocalTime(&dst);
    printf("main 111111111111111111222222222222222222222\n");
    SNTP_GetUTCTime(&dst);
    printf("main 111111112222222111111111111112222222222222\n");
    
    
    SNTP_Manage_Info info;
    SNTP_Handle handle = SNTP_Init(&info);
    SNTP_Open(handle);
    printf("main 222222222222222\n");
    char ntpServer[] = "202.112.10.60"; //"s1a.time.edu.cn";   
    SNTP_Get(handle, (char*)ntpServer);
    printf("main 3333333333333333333\n");
    SNTP_Close(handle);
    SNTP_Term(handle);
    printf("main 4444444444444444\n");
    
    
    test1();
    printf("main 55555555555555555555\n");
    test2();
    
    
    return 0;
}

 int test1()
 {
    time_t t;
    struct tm *p;
    t=time(NULL);
    p=localtime(&t);
    printf("%s\n", asctime(p));
    return 0;
 }

int test2()
{
    time_t timep;
    time (&timep);
    printf("%s",asctime(gmtime(&timep)));
    return 0;
}

