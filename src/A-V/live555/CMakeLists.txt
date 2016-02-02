cmake_minimum_required(VERSION 2.8)

#project(live555)

set( CMAKE_C_FLAGS      "${CMAKE_C_FLAGS} -g -Wall -O2 -DDEBUG -O -DSOCKLEN_T=socklen_t  -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 " )  
set( CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS} -g -Wall -O2 -DDEBUG -O -DSOCKLEN_T=socklen_t  -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 " ) 

    
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/groupsock/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment/include)


############################################################################
###########                BasicUsageEnvironment                 ###########
############################################################################
file(GLOB BasicUsageEnvironmentFiles
	${CMAKE_CURRENT_SOURCE_DIR}/BasicUsageEnvironment/*.cpp)
add_library(BasicUsageEnvironment ${BasicUsageEnvironmentFiles})
         
         
############################################################################
###########                  UsageEnvironment                    ###########
############################################################################
file(GLOB UsageEnvironmentFiles 
    ${CMAKE_CURRENT_SOURCE_DIR}/UsageEnvironment/*.cpp)
add_library(UsageEnvironment ${UsageEnvironmentFiles})

			
            
############################################################################
###########                      groupsock                       ###########
############################################################################
file(GLOB groupsockfiles
	${CMAKE_CURRENT_SOURCE_DIR}/groupsock/*.cpp groupsock/*.c)
add_library(groupsock ${groupsockfiles})
add_dependencies(groupsock          BasicUsageEnvironment UsageEnvironment )
target_link_libraries (groupsock    BasicUsageEnvironment UsageEnvironment )

############################################################################
###########                      liveMedia                       ###########
############################################################################
file(GLOB liveMediaFiles
		${CMAKE_CURRENT_SOURCE_DIR}/liveMedia/*.hh liveMedia/*.cpp liveMedia/*.c)	
add_library(liveMedia ${liveMediaFiles})
add_dependencies(liveMedia  groupsock 
                BasicUsageEnvironment  UsageEnvironment 
            )
target_link_libraries(liveMedia     groupsock
                BasicUsageEnvironment UsageEnvironment 
            )
                
############################################################################
###########                      mediaServer                     ###########
############################################################################
file(GLOB mediaServerFiles
	${CMAKE_CURRENT_SOURCE_DIR}/mediaServer/*.hh mediaServer/*.cpp)
add_executable(mediaServer ${mediaServerFiles})
add_dependencies(mediaServer    BasicUsageEnvironment 
                groupsock UsageEnvironment liveMedia 
            )

target_link_libraries(mediaServer   BasicUsageEnvironment 
                groupsock UsageEnvironment liveMedia
            )
            
            