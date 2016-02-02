#include "WW_H264VideoServerMediaSubsession.h"


WW_H264VideoServerMediaSubsession::WW_H264VideoServerMediaSubsession(UsageEnvironment & env, FramedSource * source) 
		: OnDemandServerMediaSubsession(env, True)
{
	env << "  WW_H264VideoServerMediaSubsession::WW_H264VideoServerMediaSubsession \n";
	m_pSource = source;
	m_pSDPLine = 0;
}

WW_H264VideoServerMediaSubsession::~WW_H264VideoServerMediaSubsession(void)
{
	envir() << "  WW_H264VideoServerMediaSubsession::~WW_H264VideoServerMediaSubsession \n";
	if (m_pSDPLine) {
		free(m_pSDPLine);
	}
}

WW_H264VideoServerMediaSubsession * WW_H264VideoServerMediaSubsession::createNew(UsageEnvironment & env, FramedSource * source)
{
	#ifdef DEBUG
             fprintf(stderr, "WW_H264VideoServerMediaSubsession * WW_H264VideoServerMediaSubsession::createNew.\n\n");
         #endif
	return new WW_H264VideoServerMediaSubsession(env, source);
}

FramedSource * WW_H264VideoServerMediaSubsession::createNewStreamSource(unsigned clientSessionId, unsigned & estBitrate)
{
	envir() << " WW_H264VideoServerMediaSubsession::createNewStreamSource \n";
	return H264VideoStreamFramer::createNew(envir(), new V4L2FramedSource(envir()));
}

RTPSink * WW_H264VideoServerMediaSubsession::createNewRTPSink(Groupsock * rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource * inputSource)
{
	envir() << " WW_H264VideoServerMediaSubsession::createNewRTPSink \n";
	return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}

char const * WW_H264VideoServerMediaSubsession::getAuxSDPLine(RTPSink * rtpSink, FramedSource * inputSource)
{
	envir() << " WW_H264VideoServerMediaSubsession::getAuxSDPLine \n";
	if (m_pSDPLine) {
		return m_pSDPLine;
	}

	m_pDummyRTPSink = rtpSink;

	//mp_dummy_rtpsink->startPlaying(*source, afterPlayingDummy, this);
	m_pDummyRTPSink->startPlaying(*inputSource, 0, 0);

	chkForAuxSDPLine(this);

	m_done = 0;

	envir().taskScheduler().doEventLoop(&m_done);

	m_pSDPLine = strdup(m_pDummyRTPSink->auxSDPLine());

	m_pDummyRTPSink->stopPlaying();

	return m_pSDPLine;
}

void WW_H264VideoServerMediaSubsession::afterPlayingDummy(void * ptr)
{
	//envir() << " WW_H264VideoServerMediaSubsession::afterPlayingDummy \n";
	WW_H264VideoServerMediaSubsession * This = (WW_H264VideoServerMediaSubsession *)ptr;

	This->m_done = 0xff;
}

void WW_H264VideoServerMediaSubsession::chkForAuxSDPLine(void * ptr)
{
	//envir() << " WW_H264VideoServerMediaSubsession::chkForAuxSDPLine \n";
	WW_H264VideoServerMediaSubsession * This = (WW_H264VideoServerMediaSubsession *)ptr;

	This->chkForAuxSDPLine1();
}

void WW_H264VideoServerMediaSubsession::chkForAuxSDPLine1()
{
	envir() << " WW_H264VideoServerMediaSubsession::chkForAuxSDPLine1 \n";
	if (m_pDummyRTPSink->auxSDPLine()) {
		m_done = 0xff;
	} else {
		double delay = 1000.0 / (FRAME_PER_SEC);  // ms
		int to_delay = delay * 1000;  // us

		nextTask() = envir().taskScheduler().scheduleDelayedTask(to_delay, chkForAuxSDPLine, this);
	}
}

char const* WW_H264VideoServerMediaSubsession::sdpLines()
{
    return fSDPLines = (char *)
        "m=video 0 RTP/AVP 96\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "b=AS:96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1;profile-level-id=000000;sprop-parameter-sets=H264\r\n"
        "a=control:track1\r\n";
}

