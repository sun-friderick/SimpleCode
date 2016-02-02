/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2007 Live Networks, Inc.  All rights reserved.
// Any source that feeds into a "H264VideoRTPSink" must be of this class.
// This is a virtual base class; subclasses must implement the
// "currentNALUnitEndsAccessUnit()" virtual function.
// Implementation

// #include "H264VideoStreamFramer.hh"
// 
// H264VideoStreamFramer::H264VideoStreamFramer(UsageEnvironment& env, FramedSource* inputSource)
//   : FramedFilter(env, inputSource) {
// }
// 
// H264VideoStreamFramer::~H264VideoStreamFramer() {
// }
// 
// Boolean H264VideoStreamFramer::isH264VideoStreamFramer() const {
//   return True;
// }

// ***************************** GOING FOR A RIDE YEAH

#include <iostream>
#include "H264VideoStreamFramer.hh"
#include "H264VideoStreamParser.hh"

#include <string.h>
#include <GroupsockHelper.hh>

int check = 0;
///////////////////////////////////////////////////////////////////////////////
////////// h264VideoStreamFramer implementation //////////
//public///////////////////////////////////////////////////////////////////////
H264VideoStreamFramer* H264VideoStreamFramer::createNew(
                                                         UsageEnvironment& env,
                                                         FramedSource* inputSource)
{
   // Need to add source type checking here???  #####
//     std::cout << "H264VideoStreamFramer: in createNew" << std::endl;
   H264VideoStreamFramer* fr;
   fr = new H264VideoStreamFramer(env, inputSource);
   return fr;
}


///////////////////////////////////////////////////////////////////////////////
H264VideoStreamFramer::H264VideoStreamFramer(
                              UsageEnvironment& env,
                              FramedSource* inputSource,
                              Boolean createParser)
                              : FramedFilter(env, inputSource),
                                fFrameRate(0.0), // until we learn otherwise 
                                fPictureEndMarker(False)
{
   // Use the current wallclock time as the base 'presentation time':
   gettimeofday(&fPresentationTimeBase, NULL);
//     std::cout << "H264VideoStreamFramer: going to create H264VideoStreamParser" << std::endl;
   fParser = createParser ? new H264VideoStreamParser(this, inputSource) : NULL;
}

///////////////////////////////////////////////////////////////////////////////
H264VideoStreamFramer::~H264VideoStreamFramer()
{
   delete   fParser;
}


///////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG
static struct timeval firstPT;
#endif


///////////////////////////////////////////////////////////////////////////////
void H264VideoStreamFramer::doGetNextFrame()
{
//   std::cout << "H264VideoStreamFramer: in doGetNextFrame" << std::endl;
  fParser->registerReadInterest(fTo, fMaxSize);
  continueReadProcessing();
}


///////////////////////////////////////////////////////////////////////////////
Boolean H264VideoStreamFramer::isH264VideoStreamFramer() const
{
  return True;
}

///////////////////////////////////////////////////////////////////////////////
Boolean H264VideoStreamFramer::currentNALUnitEndsAccessUnit() 
{
  return True;
}


///////////////////////////////////////////////////////////////////////////////
void H264VideoStreamFramer::continueReadProcessing(
                                   void* clientData,
                                   unsigned char* /*ptr*/, unsigned /*size*/,
                                   struct timeval /*presentationTime*/)
{
   H264VideoStreamFramer* framer = (H264VideoStreamFramer*)clientData;
   framer->continueReadProcessing();
}

///////////////////////////////////////////////////////////////////////////////
void H264VideoStreamFramer::continueReadProcessing()
{
   unsigned acquiredFrameSize; 
//     std::cout << "H264VideoStreamFramer: in continueReadProcessing" << std::endl;
   u_int64_t frameDuration;  // in ms

//    acquiredFrameSize = fParser->parse(frameDuration);
      acquiredFrameSize = fParser->parse();
// Calculate some average bitrate information (to be adapted)   
//  avgBitrate = (totalBytes * 8 * H263_TIMESCALE) / totalDuration;
//     std::cout << "continueReadProcessing, acquiredFrameSize: " << acquiredFrameSize << std::endl;

//     while (!acquiredFrameSize) {
//         std::cout << "waiting for data!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
//     }

   if (acquiredFrameSize > 0) {
//       std::cout << "continueReadProcessing, acquiredFrameSize: " << acquiredFrameSize << std::endl;
        check++;
//         std::cout << "NAL # " << check << std::endl;
      // We were able to acquire a frame from the input.
      // It has already been copied to the reader's space.
      fFrameSize = acquiredFrameSize;
//    fNumTruncatedBytes = fParser->numTruncatedBytes(); // not needed so far
      frameDuration = 30;
      fFrameRate = frameDuration == 0 ? 0.0 : 1000./(long)frameDuration;

      // Compute "fPresentationTime" 
      if (acquiredFrameSize == 5) // first frame
         fPresentationTime = fPresentationTimeBase;
      else 
         fPresentationTime.tv_usec += (long) frameDuration*1000;

      while (fPresentationTime.tv_usec >= 1000000) {
         fPresentationTime.tv_usec -= 1000000;
         ++fPresentationTime.tv_sec;
      }

      // Compute "fDurationInMicroseconds" 
      fDurationInMicroseconds = (unsigned int) frameDuration*1000;;


      // Call our own 'after getting' function.  Because we're not a 'leaf'
      // source, we can call this directly, without risking infinite recursion.
      afterGetting(this);
   } else {
//         std::cout << "need to do somemore reading here to get a frame!!!!" << std::endl;
//         std::cout << "lets check parse state: " << fParser->getParseState() << std::endl;
//         afterGetting(this);
      // We were unable to parse a complete frame from the input, because:
      // - we had to read more data from the source stream, or
      // - the source stream has ended.
   }
}
