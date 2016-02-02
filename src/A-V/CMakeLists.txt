
########################################################################################
#############               添加源码子目录                                ############## 
########################################################################################
#add_subdirectory( ffmpeg )
add_subdirectory( jrtplib )
#add_subdirectory( libRtspClient )
#add_subdirectory( libRtspServer )
add_subdirectory( live555 )
#add_subdirectory( STUN )


MESSAGE(status "Add sub-directory: ${CMAKE_CURRENT_SOURCE_DIR} .")



