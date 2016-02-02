########################################################################################
#############                生成 BCStackBluetooth 库文件                         ############## 
########################################################################################
LIST (APPEND BCStackBluetooth_LIB_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux/main.c
    )
    
    

########################################################################################
#############                设置 BCStackBluetooth 库版本号                       ############## 
########################################################################################
set(BCStackBluetooth_LIB_VERSION     "1.0.0")
set(BCStackBluetooth_LIB_SOVERSION   "1")




########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Systerm
    ${PROJECT_SOURCE_DIR}/Systerm/BCStackBluetooth/platform
    ${PROJECT_SOURCE_DIR}/Systerm/BCStackBluetooth/bluetooth/include
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
IF (MODULE_BCStackBluetooth)
    #生成动态库  静态库则 STATIC  
    add_library (BCStackBluetoothlib SHARED      ${BCStackBluetooth_LIB_SRCS})  
    add_library (BCStackBluetooths STATIC        ${BCStackBluetooth_LIB_SRCS})  
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(BCStackBluetoothlib   PROPERTIES 
                                        VERSION     ${BCStackBluetooth_LIB_VERSION} 
                                        SOVERSION   ${BCStackBluetooth_LIB_SOVERSION} )
ELSE (MODULE_BCStackBluetooth)
    MESSAGE(STATUS "Not Include BCStackBluetooth module.")
ENDIF (MODULE_BCStackBluetooth)




########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (TEST_MODULE_FLAG AND MODULE_BCStackBluetooth)
    add_executable(TestBCStackBluetooth.elf    BCStackBluetoothTest.c)
    add_dependencies(TestBCStackBluetooth.elf          BCStackBluetoothlib loglib)
    target_link_libraries(TestBCStackBluetooth.elf     BCStackBluetoothlib loglib)

ELSE (TEST_MODULE_FLAG AND MODULE_BCStackBluetooth)
    MESSAGE(STATUS "Not Include mount BCStack Bluetooth module.")
ENDIF (TEST_MODULE_FLAG AND MODULE_BCStackBluetooth)
