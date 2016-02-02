########################################################################################
#############              生成 LwIP 库文件                               ############## 
########################################################################################
LIST (APPEND LwIP_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/LwIP.c  
    )

    

########################################################################################
#############               设置 LwIP 库版本号                          ############## 
########################################################################################
set(LwIP_LIB_VERSION     "1.0.0")
set(LwIP_LIB_SOVERSION   "1")

    
    
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Network
    ${PROJECT_SOURCE_DIR}/Network/LwIP
    ${PROJECT_SOURCE_DIR}/Network/LwIP/src/include
    ${PROJECT_SOURCE_DIR}/Network/LwIP/system/OS
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
IF (MODULE_LwIP)

    #生成动态库  静态库则 STATIC  
    add_library (LwIPlib SHARED      ${LwIP_LIB_SRCS})  
    add_library (LwIPs STATIC        ${LwIP_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(LwIPlib   PROPERTIES 
                                    VERSION     ${LwIP_LIB_VERSION} 
                                    SOVERSION   ${LwIP_LIB_SOVERSION} )
ELSE (MODULE_LwIP)
    MESSAGE(STATUS "Not Include LwIP module.")
ENDIF (MODULE_LwIP)





########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_LwIP)
    add_executable(TestSNTP.elf    LwIPTest.c)
    add_dependencies(TestSNTP.elf          LwIPlib loglib )
    target_link_libraries(TestSNTP.elf     LwIPlib loglib )

ELSE (TEST_MODULE_FLAG AND MODULE_LwIP)
    MESSAGE(STATUS "Not Include LwIP module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_LwIP)



