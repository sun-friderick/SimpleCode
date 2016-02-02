#include "H264LiveVideoServerMediaSubssion.hh"
#include "H264FramedLiveSource.hh"
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#define BUFSIZE 1024*200

static void announceStream(RTSPServer* rtspServer, ServerMediaSession* sms,char const* streamName)//显示RTSP连接信息
{
    char* url = rtspServer->rtspURL(sms);
    UsageEnvironment& env = rtspServer->envir();
    env <<streamName<< "\n";
    env << "Play this stream using the URL \"" << url << "\"\n";
    delete[] url;
}

int main(int argc, char** argv)
{
    //设置环境
    Boolean reuseFirstSource = False;//如果为“true”则其他接入的客户端跟第一个客户端看到一样的视频流，否则其他客户端接入的时候将重新播放
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    //创建RTSP服务器
    UserAuthenticationDatabase* authDB = NULL;
    RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554, authDB);
    if (rtspServer == NULL) {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }
    char const* descriptionString= "Session streamed by \"testOnDemandRTSPServer\"";


    //模拟实时流发送相关变量
    int datasize;//数据区长度
    unsigned char*  databuf;//数据区指针
    databuf = (unsigned char*)malloc(1024*1024);
    bool dosent;//rtsp发送标志位，为true则发送，否则退出

    //从文件中拷贝1M数据到内存中作为实时网络传输内存模拟，如果实时网络传输应该是双线程结构，记得在这里加上线程锁
    //此外实时传输的数据拷贝应该是发生在H264FramedLiveSource文件中，所以这里只是自上往下的传指针过去给它
    FILE *pf;
    pf = fopen("test.264", "rb");
    fread(databuf, 1, BUFSIZE, pf);
    datasize = BUFSIZE;
    dosent = true;
    fclose(pf);



    //上面的部分除了模拟网络传输的部分外其他的基本跟live555提供的demo一样，而下面则修改为网络传输的形式，为此重写addSubsession的第一个参数相关文件
    char const* streamName = "h264ESVideoTest";
    ServerMediaSession* sms = ServerMediaSession::createNew(*env, streamName, streamName, descriptionString);
    sms->addSubsession(H264LiveVideoServerMediaSubssion::createNew(*env, reuseFirstSource, &datasize, databuf,&dosent));//修改为自己实现的H264LiveVideoServerMediaSubssion
    rtspServer->addServerMediaSession(sms);

    announceStream(rtspServer, sms, streamName);//提示用户输入连接信息
    env->taskScheduler().doEventLoop(); //循环等待连接

    free(databuf);//释放掉内存
    return 0;
}
