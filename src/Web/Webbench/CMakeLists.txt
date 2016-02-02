
########################################################################################
#############               生成 webbench 库文件                           ############## 
########################################################################################
LIST (APPEND webbench_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/webbench.c
    ${CMAKE_CURRENT_SOURCE_DIR}/socket.c
    )
    

    
########################################################################################
#############               设置 webbench 库版本号                         ############## 
########################################################################################
set(webbench_LIB_VERSION     "1.5.0")
set(webbench_LIB_SOVERSION   "1")


   
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Web
    ${PROJECT_SOURCE_DIR}/Web/Webbench
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
IF (MODULE_webbench)
    #生成动态库  静态库则 STATIC  
    add_library (webbenchlib SHARED      ${webbench_LIB_SRCS})  
    add_library (webbenchs   STATIC      ${webbench_LIB_SRCS})  
    
    # 设置依赖关系，如果当前库是最底层的共享库，则不需要设置
    add_dependencies (webbenchlib  loglib)
    add_dependencies (webbenchs    loglib)
    
    # 添加要链接的共享库, 库的顺序就是编译器的链接时顺序
    target_link_libraries (webbenchlib  loglib)
    target_link_libraries (webbenchs    loglib)
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(webbenchlib  PROPERTIES 
                                        VERSION     ${webbench_LIB_VERSION} 
                                        SOVERSION   ${webbench_LIB_SOVERSION} )
ELSE (MODULE_webbench)
    MESSAGE(STATUS "Not Include webbench module.")
ENDIF (MODULE_webbench)



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_webbench)
    add_executable(TestWebbench.elf    webbenchTest.c)
    add_dependencies(TestWebbench.elf      loglib monitorlib webbenchlib)
    target_link_libraries(TestWebbench.elf loglib monitorlib webbenchlib)

ELSE (TEST_MODULE_FLAG AND MODULE_webbench)
    MESSAGE(STATUS "Not Include webbench module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_webbench)




