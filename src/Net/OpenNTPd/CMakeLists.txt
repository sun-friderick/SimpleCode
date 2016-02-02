########################################################################################
#############              生成 OpenNTPd 库文件                               ############## 
########################################################################################
LIST (APPEND OpenNTPd_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/OpenNTPd.c  
    )

    

########################################################################################
#############               设置 OpenNTPd 库版本号                          ############## 
########################################################################################
set(OpenNTPd_LIB_VERSION     "1.0.0")
set(OpenNTPd_LIB_SOVERSION   "1")

    
    
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Network
    ${PROJECT_SOURCE_DIR}/Network/OpenNTPd/include
    ${PROJECT_SOURCE_DIR}/Network/OpenNTPd/src
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
IF (MODULE_OpenNTPd)

    #生成动态库  静态库则 STATIC  
    add_library (OpenNTPdlib SHARED      ${OpenNTPd_LIB_SRCS})  
    add_library (OpenNTPds STATIC        ${OpenNTPd_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(OpenNTPdlib   PROPERTIES 
                                    VERSION     ${OpenNTPd_LIB_VERSION} 
                                    SOVERSION   ${OpenNTPd_LIB_SOVERSION} )
ELSE (MODULE_OpenNTPd)
    MESSAGE(STATUS "Not Include OpenNTPd module.")
ENDIF (MODULE_OpenNTPd)





########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_OpenNTPd)
    add_executable(TestOpenNTPd.elf    OpenNTPdTest.c)
    add_dependencies(TestOpenNTPd.elf          OpenNTPdlib loglib )
    target_link_libraries(TestOpenNTPd.elf     OpenNTPdlib loglib )

ELSE (TEST_MODULE_FLAG AND MODULE_OpenNTPd)
    MESSAGE(STATUS "Not Include OpenNTPd module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_OpenNTPd)



