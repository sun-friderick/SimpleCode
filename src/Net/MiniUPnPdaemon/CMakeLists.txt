########################################################################################
#############                生成 MiniUPnPdaemon 库文件                         ############## 
########################################################################################
LIST (APPEND MiniUPnPdaemon_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/MiniUPnPdaemon.c
    )
    
    

########################################################################################
#############                设置 MiniUPnPdaemon 库版本号                       ############## 
########################################################################################
set(MiniUPnPdaemon_LIB_VERSION     "1.9.0")
set(MiniUPnPdaemon_LIB_SOVERSION   "1")




########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Systerm
    ${PROJECT_SOURCE_DIR}/Systerm/MiniUPnPdaemon
)  



########################################################################################
#############               添加 链接库文件目录                           ############## 
########################################################################################
link_directories(  
    /usr/local/lib   
    )  

    

########################################################################################
#############           生成目标库文件 并 标明版本号                      ############## 
########################################################################################
IF (MODULE_MiniUPnPdaemon)
    #生成动态库  静态库则 STATIC  
    add_library (MiniUPnPdaemonlib SHARED      ${MiniUPnPdaemon_LIB_SRCS})  
    add_library (MiniUPnPdaemons STATIC        ${MiniUPnPdaemon_LIB_SRCS})  
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(MiniUPnPdaemonlib   PROPERTIES 
                                        VERSION     ${MiniUPnPdaemon_LIB_VERSION} 
                                        SOVERSION   ${MiniUPnPdaemon_LIB_SOVERSION} )
ELSE (MODULE_MiniUPnPdaemon)
    MESSAGE(STATUS "Not Include MiniUPnPdaemon module.")
ENDIF (MODULE_MiniUPnPdaemon)




########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_MiniUPnPdaemon)
    add_executable(TestMiniUPnPdaemon.elf    MiniUPnPdaemonTest.c)
    add_dependencies(TestMiniUPnPdaemon.elf          MiniUPnPdaemonlib loglib)
    target_link_libraries(TestMiniUPnPdaemon.elf     MiniUPnPdaemonlib loglib)

ELSE (TEST_MODULE_FLAG AND MODULE_MiniUPnPdaemon)
    MESSAGE(STATUS "Not Include mount Mini UPnP daemon module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_MiniUPnPdaemon)
