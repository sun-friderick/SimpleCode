cmake_minimum_required(VERSION 2.8)

project(live555)

set( CMAKE_C_FLAGS      "${CMAKE_C_FLAGS} -g -Wall -O2 -DDEBUG -O -DSOCKLEN_T=socklen_t  -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64  " )  
set( CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} -g -Wall -O2 -DDEBUG -O -DSOCKLEN_T=socklen_t  -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64  " ) 


include_directories(${CMAKE_SOURCE_DIR}/BasicUsageEnvironment/include)
include_directories(${CMAKE_SOURCE_DIR}/groupsock/include)
include_directories(${CMAKE_SOURCE_DIR}/liveMedia/include)
include_directories(${CMAKE_SOURCE_DIR}/UsageEnvironment/include)


############################################################################
###########                                              LiveLog                                                      ###########
############################################################################
include_directories(${CMAKE_SOURCE_DIR}/Log)
file(GLOB LiveLogFiles
                Log/*.cpp)
add_library(LiveLog ${LiveLogFiles})
         

############################################################################
###########                                   BasicUsageEnvironment                                       ###########
############################################################################
file(GLOB BasicUsageEnvironmentFiles
        		BasicUsageEnvironment/*.cpp)
add_library(BasicUsageEnvironment ${BasicUsageEnvironmentFiles})
         
         
############################################################################
###########                                          UsageEnvironment                                           ###########
############################################################################
file(GLOB UsageEnvironmentFiles 
        		UsageEnvironment/*.cpp)
add_library(UsageEnvironment ${UsageEnvironmentFiles})

			
############################################################################
###########                                                 groupsock                                                    ###########
############################################################################
file(GLOB groupsockfiles
        		groupsock/*.cpp groupsock/*.c)
add_library(groupsock ${groupsockfiles})
add_dependencies(groupsock          
			BasicUsageEnvironment UsageEnvironment 
		)
target_link_libraries (groupsock    
			BasicUsageEnvironment UsageEnvironment  LiveLog
		)


############################################################################
###########                                               liveMedia                                                      ###########
############################################################################
file(GLOB liveMediaFiles
		liveMedia/*.hh liveMedia/*.cpp liveMedia/*.c )	
add_library(liveMedia ${liveMediaFiles})
add_dependencies(liveMedia  groupsock 
                		BasicUsageEnvironment  UsageEnvironment  LiveLog
            	)
target_link_libraries(liveMedia     groupsock
                		BasicUsageEnvironment UsageEnvironment  LiveLog
            	)
            	
                
############################################################################
###########                                            mediaServer                                                    ###########
############################################################################
file(GLOB mediaServerFiles
        		mediaServer/*.hh mediaServer/*.cpp )
add_executable(mediaServer ${mediaServerFiles})
add_dependencies(mediaServer    
                		BasicUsageEnvironment groupsock UsageEnvironment liveMedia  LiveLog
            	)
target_link_libraries(mediaServer   
                		BasicUsageEnvironment  groupsock UsageEnvironment liveMedia LiveLog
            	)
            
 
############################################################################
###########                                                V4L2Server                                                 ###########
############################################################################
include_directories(${CMAKE_SOURCE_DIR}/v4l2Server/H264Encode)
include_directories(${CMAKE_SOURCE_DIR}/v4l2Server/V4L2)

file(GLOB   V4L2ServerFiles
        		v4l2Server/*.hh      v4l2Server/*.cpp
        		v4l2Server/V4L2/*.hh    v4l2Server/V4L2/*.cpp
        		v4l2Server/H264Encode/*.hh     v4l2Server/H264Encode/*.cpp)
add_executable(v4l2Server   ${V4L2ServerFiles})

link_directories(/usr/local/lib)
add_dependencies(v4l2Server    
                		BasicUsageEnvironment groupsock UsageEnvironment liveMedia  LiveLog
                		avformat  swscale  avcodec  avutil pthread x264  m
            	)
target_link_libraries(v4l2Server   
                		BasicUsageEnvironment  groupsock UsageEnvironment liveMedia LiveLog
                		avformat  swscale  avcodec  avutil pthread x264  m
            	)
            




