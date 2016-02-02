########################################################################################
#############                生成 LibUSB 库文件                         ############## 
########################################################################################
LIST (APPEND LibUSB_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/LibUSB.c
    )
    
    

########################################################################################
#############                设置 LibUSB 库版本号                       ############## 
########################################################################################
set(LibUSB_LIB_VERSION     "1.0.0")
set(LibUSB_LIB_SOVERSION   "1")




########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Systerm
    ${PROJECT_SOURCE_DIR}/Systerm/LibUSB
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
IF (MODULE_LibUSB)
    #生成动态库  静态库则 STATIC  
    add_library (LibUSBlib SHARED      ${LibUSB_LIB_SRCS})  
    add_library (LibUSBs STATIC        ${LibUSB_LIB_SRCS})  
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(LibUSBlib   PROPERTIES 
                                        VERSION     ${LibUSB_LIB_VERSION} 
                                        SOVERSION   ${LibUSB_LIB_SOVERSION} )
ELSE (MODULE_LibUSB)
    MESSAGE(STATUS "Not Include LibUSB module.")
ENDIF (MODULE_LibUSB)




########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_LibUSB)
    add_executable(TestLibUSB.elf    LibUSBTest.c)
    add_dependencies(TestLibUSB.elf          LibUSBlib loglib)
    target_link_libraries(TestLibUSB.elf     LibUSBlib loglib)

ELSE (TEST_MODULE_FLAG AND MODULE_LibUSB)
    MESSAGE(STATUS "Not Include mount Mini UPnP daemon module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_LibUSB)
