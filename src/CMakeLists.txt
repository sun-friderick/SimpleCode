
########################################################################################
#######     生成build_info.h头文件  准备在编译链接时所需的第三方库和头文件      ######## 
########################################################################################
include (${CONFIG_CMAKE_PATH}/GernerateVersionHeader.cmake)


########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${CMAKE_CURRENT_SOURCE_DIR}/includes  
    ${CMAKE_CURRENT_SOURCE_DIR}
) 


########################################################################################
#############               添加源码子目录                                ############## 
########################################################################################
add_subdirectory( Algorithm )
add_subdirectory( Apps )
add_subdirectory( A-V )
add_subdirectory( Communication )
add_subdirectory( Crypto )
add_subdirectory( Debug )
add_subdirectory( Event )
add_subdirectory( File )
add_subdirectory( Lang )
add_subdirectory( Net )
add_subdirectory( OwnCode )
add_subdirectory( Sys )
add_subdirectory( Views )
add_subdirectory( Web )


MESSAGE(status  "SRC root dir.")



