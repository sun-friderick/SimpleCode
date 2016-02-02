########################################################################################
#############              生成 cURL 库文件                               ############## 
########################################################################################
LIST (APPEND cURL_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/cURL.c  
    )

    

########################################################################################
#############               设置 cURL 库版本号                          ############## 
########################################################################################
set(cURL_LIB_VERSION     "1.0.0")
set(cURL_LIB_SOVERSION   "1")

    
    
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Network
    ${PROJECT_SOURCE_DIR}/Network/cURL
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
IF (MODULE_cURL)

    #生成动态库  静态库则 STATIC  
    add_library (cURLlib SHARED      ${cURL_LIB_SRCS})  
    add_library (cURLs STATIC        ${cURL_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(cURLlib   PROPERTIES 
                                    VERSION     ${cURL_LIB_VERSION} 
                                    SOVERSION   ${cURL_LIB_SOVERSION} )
ELSE (MODULE_cURL)
    MESSAGE(STATUS "Not Include cURL module.")
ENDIF (MODULE_cURL)





########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_cURL)
    add_executable(TestcURL.elf    cURLTest.c)
    add_dependencies(TestcURL.elf          cURLlib loglib )
    target_link_libraries(TestcURL.elf     cURLlib loglib )

ELSE (TEST_MODULE_FLAG AND MODULE_cURL)
    MESSAGE(STATUS "Not Include cURL module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_cURL)



