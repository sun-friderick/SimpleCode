  ########################################################################################
#############                设置模块名称                       ############## 
########################################################################################
set(MODULE_Name   threadpool)
 MESSAGE(STATUS " ${MODULE_Name} module.")

   
########################################################################################
#############                生成  threadpool 库文件                       ############## 
########################################################################################
LIST (APPEND ${MODULE_Name}_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/ThreadPool.c
    )
    
    
    
########################################################################################
#############               设置 threadpool 库版本号                       ############## 
########################################################################################
set(${MODULE_Name}_LIB_VERSION     "1.0.0")
set(${MODULE_Name}_LIB_SOVERSION   "1")


   
########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${CMAKE_CURRENT_SOURCE_DIR}/includes
    ${CMAKE_CURRENT_SOURCE_DIR}
)  



########################################################################################
#############               添加 链接库文件目录                           ############## 
########################################################################################
link_directories(  
    ##/usr/local/lib   
    )  


    
########################################################################################
#############           生成目标库文件 并 标明版本号                      ############## 
########################################################################################
IF (MODULE_${MODULE_Name})
    #生成动态库  静态库则 STATIC  
    add_library (${MODULE_Name}lib  SHARED          ${${MODULE_Name}_LIB_SRCS})  
    add_library (${MODULE_Name}s    STATIC          ${${MODULE_Name}_LIB_SRCS})  
    
    # 设置依赖关系，如果当前库是最底层的共享库，则不需要设置
    add_dependencies (${MODULE_Name}lib   pthread)
    add_dependencies (${MODULE_Name}s     pthread)
    
    # 添加要链接的共享库, 库的顺序就是编译器的链接时顺序
    target_link_libraries (${MODULE_Name}lib  pthread)
    target_link_libraries (${MODULE_Name}s    pthread)

    # 设置版本号，SOVERSION为API版本号
    set_target_properties(${MODULE_Name}lib     PROPERTIES 
                                            VERSION ${${MODULE_Name}_LIB_VERSION} 
                                            SOVERSION ${${MODULE_Name}_LIB_SOVERSION} )
ELSE (MODULE_${MODULE_Name})
    MESSAGE(STATUS "Not Include ${MODULE_Name} module.")
ENDIF (MODULE_${MODULE_Name})



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_${MODULE_Name})
    add_executable(Test_${MODULE_Name}-heavy.elf    tests/heavyTest.c)
    add_dependencies(Test_${MODULE_Name}-heavy.elf  ${MODULE_Name}lib     pthread)
    target_link_libraries(Test_${MODULE_Name}-heavy.elf  ${MODULE_Name}lib  pthread)
    
    add_executable(Test_${MODULE_Name}-shutdown.elf    tests/shutdownTest.c)
    add_dependencies(Test_${MODULE_Name}-shutdown.elf  ${MODULE_Name}lib     pthread)
    target_link_libraries(Test_${MODULE_Name}-shutdown.elf  ${MODULE_Name}lib  pthread)
    
    add_executable(Test_${MODULE_Name}-thrd.elf    tests/thrdTest.c)
    add_dependencies(Test_${MODULE_Name}-thrd.elf  ${MODULE_Name}lib     pthread)
    target_link_libraries(Test_${MODULE_Name}-thrd.elf  ${MODULE_Name}lib  pthread)

ELSE (TEST_MODULE_FLAG AND MODULE_${MODULE_Name})
    MESSAGE(STATUS "Not Include ${MODULE_Name} module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_${MODULE_Name})




