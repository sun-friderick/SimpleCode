
#include "H264FramedLiveSource.hh"

H264FramedLiveSource::H264FramedLiveSource(UsageEnvironment& env, int *datasize, unsigned char*  databuf, bool *dosent, unsigned preferredFrameSize, unsigned playTimePerFrame)
            : FramedSource(env)
{
    env << "  H264FramedLiveSource::H264FramedLiveSource \n";
    Framed_datasize = datasize;//数据区大小指针
    Framed_databuf = databuf;//数据区指针
    Framed_dosent = dosent;//发送标示
}

H264FramedLiveSource* H264FramedLiveSource::createNew(UsageEnvironment& env, int *datasize, unsigned char*  databuf, bool *dosent, unsigned preferredFrameSize, unsigned playTimePerFrame)
{
    #ifdef DEBUG
             fprintf(stderr, " H264LiveVideoServerMediaSubssion* H264LiveVideoServerMediaSubssion::createNew.\n\n");
     #endif
    H264FramedLiveSource* newSource = new H264FramedLiveSource(env, datasize, databuf, dosent, preferredFrameSize, playTimePerFrame);
    return newSource;
}

H264FramedLiveSource::~H264FramedLiveSource()
{
    envir() << "  H264FramedLiveSource::~ H264FramedLiveSource \n";
}

void H264FramedLiveSource::doGetNextFrame()
{
    envir() << "  H264FramedLiveSource::doGetNextFrame \n";
    if (*Framed_dosent == true)
    {
        *Framed_dosent = false;
        bufsizel = *Framed_datasize;
        readbufsize = 0;

        fFrameSize = fMaxSize;
        memcpy(fTo, Framed_databuf + readbufsize, fFrameSize);
        readbufsize += fFrameSize;
    }
    else
    {
        if (bufsizel - readbufsize>fMaxSize)
        {
            fFrameSize = fMaxSize;
            memcpy(fTo, Framed_databuf + readbufsize, fFrameSize);
            readbufsize += fFrameSize;
        }
        else
        {
            memcpy(fTo, Framed_databuf + readbufsize, bufsizel - readbufsize);
            *Framed_dosent = true;
        }
    }

    nextTask() = envir().taskScheduler().scheduleDelayedTask(0,(TaskFunc*)FramedSource::afterGetting, this);//表示延迟0秒后再执行 afterGetting 函数
    return;
}