
########################################################################################
#############               生成 pic2hexdata 库文件                          ############## 
########################################################################################
LIST (APPEND pic2hexdata_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/pic_2_hexData.c
    )
    

 
########################################################################################
#############               设置 pic2hexdata 库版本号                        ############## 
########################################################################################
set(pic2hexdata_LIB_VERSION     "1.0.0")
set(pic2hexdata_LIB_SOVERSION   "1")


   
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Views
    ${PROJECT_SOURCE_DIR}/Views/PicConversion
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
IF (MODULE_pic2hexdata)
    #生成动态库  静态库则 STATIC  
    add_library (pic2hexdatalib SHARED      ${pic2hexdata_LIB_SRCS})  
    add_library (pic2hexdatas   STATIC      ${pic2hexdata_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(pic2hexdatalib   PROPERTIES 
                                        VERSION     ${pic2hexdata_LIB_VERSION} 
                                        SOVERSION   ${pic2hexdata_LIB_SOVERSION} )
ELSE (MODULE_pic2hexdata)
    MESSAGE(STATUS "Not Include pic2hexdata module.")
ENDIF (MODULE_pic2hexdata)



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_pic2hexdata)
    add_executable(TestPic2Hexdata.elf    pic2hexdataTest.c)
    add_dependencies(TestPic2Hexdata.elf      loglib  viewslib pic2hexdatalib )
    target_link_libraries(TestPic2Hexdata.elf loglib  viewslib pic2hexdatalib )

ELSE (TEST_MODULE_FLAG AND MODULE_pic2hexdata)
    MESSAGE(STATUS "Not Include pic2hexdata module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_pic2hexdata)




