
########################################################################################
#############               生成 splittext2gif 库文件                          ############## 
########################################################################################
LIST (APPEND splittext2gif_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/splittext2gif.c
    )
    
    
 
########################################################################################
#############               设置 splittext2gif 库版本号                        ############## 
########################################################################################
set(splittext2gif_LIB_VERSION     "1.0.0")
set(splittext2gif_LIB_SOVERSION   "1")


   
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
IF (MODULE_splittext2gif)
    #生成动态库  静态库则 STATIC  
    add_library (splittext2giflib SHARED      ${splittext2gif_LIB_SRCS})  
    add_library (splittext2gifs   STATIC      ${splittext2gif_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(splittext2giflib   PROPERTIES 
                                        VERSION     ${splittext2gif_LIB_VERSION} 
                                        SOVERSION   ${splittext2gif_LIB_SOVERSION} )
ELSE (MODULE_splittext2gif)
    MESSAGE(STATUS "Not Include splittext2gif module.")
ENDIF (MODULE_splittext2gif)



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_splittext2gif)
    add_executable(TestSplittxt2gif.elf    splittext2gifTest.c)
    add_dependencies(TestSplittxt2gif.elf      loglib  viewslib splittext2giflib )
    target_link_libraries(TestSplittxt2gif.elf loglib  viewslib splittext2giflib )

ELSE (TEST_MODULE_FLAG AND MODULE_splittext2gif)
    MESSAGE(STATUS "Not Include splittext2gif module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_splittext2gif)




