
#ifndef _H264FRAMEDLIVESOURCE_HH
#define _H264FRAMEDLIVESOURCE_HH

#include <FramedSource.hh>


class H264FramedLiveSource : public FramedSource
{
public:
    static H264FramedLiveSource* createNew(UsageEnvironment& env, int *datasize, unsigned char*  databuf, bool *dosent, unsigned preferredFrameSize = 0, unsigned playTimePerFrame = 0);

protected:
    H264FramedLiveSource(UsageEnvironment& env, int *datasize, unsigned char*  databuf, bool *dosent, unsigned preferredFrameSize, unsigned playTimePerFrame);
    ~H264FramedLiveSource();

private:
    virtual void doGetNextFrame();
    int TransportData(unsigned char* to, unsigned maxSize);

protected:
    int *Framed_datasize;//数据区大小指针
    unsigned char *Framed_databuf;//数据区指针
    bool *Framed_dosent;//发送标示

    int readbufsize;//记录已读取数据区大小
    int bufsizel;//记录数据区大小
};

#endif