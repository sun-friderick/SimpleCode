
########################################################################################
#############               生成 pic_conversion 库文件                          ############## 
########################################################################################
LIST (APPEND pic_conversion_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/convert.c
    )
    

 
########################################################################################
#############               设置 pic_conversion 库版本号                        ############## 
########################################################################################
set(pic_conversion_LIB_VERSION     "1.0.0")
set(pic_conversion_LIB_SOVERSION   "1")


   
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
IF (MODULE_pic_conversion)
    #生成动态库  静态库则 STATIC  
    add_library (pic_conversionlib SHARED      ${pic_conversion_LIB_SRCS})  
    add_library (pic_conversions   STATIC      ${pic_conversion_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(pic_conversionlib   PROPERTIES 
                                        VERSION     ${pic_conversion_LIB_VERSION} 
                                        SOVERSION   ${pic_conversion_LIB_SOVERSION} )
ELSE (MODULE_pic_conversion)
    MESSAGE(STATUS "Not Include pic_conversion module.")
ENDIF (MODULE_pic_conversion)



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_pic_conversion)
    add_executable(TestPicConversion.elf    pic_conversionTest.c)
    add_dependencies(TestPicConversion.elf      loglib  viewslib pic_conversionlib )
    target_link_libraries(TestPicConversion.elf loglib  viewslib pic_conversionlib )

ELSE (TEST_MODULE_FLAG AND MODULE_pic_conversion)
    MESSAGE(STATUS "Not Include pic_conversion module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_pic_conversion)




