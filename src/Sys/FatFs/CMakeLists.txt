########################################################################################
#############                生成 FatFs 库文件                         ############## 
########################################################################################
LIST (APPEND FatFs_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ff.c
    ${CMAKE_CURRENT_SOURCE_DIR}/diskio.c
    )
    
    

########################################################################################
#############                设置 FatFs 库版本号                       ############## 
########################################################################################
set(FatFs_LIB_VERSION     "1.0.0")
set(FatFs_LIB_SOVERSION   "1")




########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Systerm
    ${PROJECT_SOURCE_DIR}/Systerm/FatFs
    ${PROJECT_SOURCE_DIR}/Systerm/FatFs/option
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
IF (MODULE_FatFs)
    #生成动态库  静态库则 STATIC  
    add_library (FatFslib SHARED      ${FatFs_LIB_SRCS})  
    add_library (FatFss STATIC        ${FatFs_LIB_SRCS})  
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(FatFslib   PROPERTIES 
                                        VERSION     ${FatFs_LIB_VERSION} 
                                        SOVERSION   ${FatFs_LIB_SOVERSION} )
ELSE (MODULE_FatFs)
    MESSAGE(STATUS "Not Include FatFs module.")
ENDIF (MODULE_FatFs)




########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_FatFs)
    add_executable(TestFatFs.elf    FatFsTest.c)
    add_dependencies(TestFatFs.elf          FatFslib loglib)
    target_link_libraries(TestFatFs.elf     FatFslib loglib)

ELSE (TEST_MODULE_FLAG AND MODULE_FatFs)
    MESSAGE(STATUS "Not Include mount Mini UPnP daemon module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_FatFs)
