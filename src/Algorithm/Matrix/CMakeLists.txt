########################################################################################
#############              生成 matrix 库文件                               ############## 
########################################################################################
LIST (APPEND matrix_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/matrix.cpp
#    ${CMAKE_CURRENT_SOURCE_DIR}/
    )



########################################################################################
#############               设置 matrix 库版本号                          ############## 
########################################################################################
set(matrix_LIB_VERSION     "1.0.0")
set(matrix_LIB_SOVERSION   "1")



########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Settings
    ${PROJECT_SOURCE_DIR}/Settings/HashTableCpp
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
IF (MODULE_matrix)

    #生成动态库  静态库则 STATIC  
    add_library (matrixlib     SHARED      ${matrix_LIB_SRCS})  
    add_library (matrixs       STATIC        ${matrix_LIB_SRCS})  
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(matrixlib    PROPERTIES 
                                        VERSION     ${matrix_LIB_VERSION} 
                                        SOVERSION   ${matrix_LIB_SOVERSION} )
ELSE (MODULE_matrix)
    MESSAGE(STATUS "Not Include matrix module.")
ENDIF (MODULE_matrix)



########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_matrix)
    add_executable(TestMatrix.elf  matrixTest.cpp)
    add_dependencies(TestMatrix.elf        matrixlib  loglib)
    target_link_libraries(TestMatrix.elf   matrixlib  loglib )

ELSE (TEST_MODULE_FLAG AND MODULE_matrix)
    MESSAGE(STATUS "Not Include matrix module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_matrix)




