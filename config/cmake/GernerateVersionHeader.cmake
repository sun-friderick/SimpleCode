########################################################################################
#########    生成buildInfo.h头文件  准备在编译时所需的头文件    ######### 
########################################################################################
add_custom_command(OUTPUT buildInfo.h
    PRE_BUILD
    COMMAND exec "${CONFIG_SCRIPT_PATH}/make_version.sh"
    COMMENT "To making VERSION  ... "
    VERBATIM
)
add_custom_target (buildInfo
    ALL DEPENDS buildInfo.h
    COMMENT "test buildInfo"
    VERBATIM
    )   

    
    
    
########################################################################################
###############              cmake用于测试的hello world 文件              ##############
########################################################################################
SET(TEST_COMMAND_FILE    "${CMAKE_BINARY_DIR}/TestHelloWorld.cmake")

FILE(WRITE ${TEST_COMMAND_FILE}  
            "SET(ENV{LANG en})\n"
        )
FILE(APPEND   ${TEST_COMMAND_FILE}  
            "EXECUTE_PROCESS(COMMAND   \"./hello-world\"  
             WORKING_DIRECTORY   \"${CMAKE_BINARY_DIR}/bin\")\n"
        )
FILE(APPEND    ${TEST_COMMAND_FILE}  
            "EXECUTE_PROCESS(COMMAND   \"${GCOVR_EXECUTABLE}\"   --html  -r  \"${CMAKE_SOURCE_DIR}\"   --output \"${COVERAGE_HTML_FILE}\"   
            WORKING_DIRECTORY   \"${CMAKE_BINARY_DIR}\")\n" 
        )
EXECUTE_PROCESS(COMMAND   hello-world   
                                    WORKING_DIRECTORY    ${CMAKE_BINARY_DIR})

ADD_CUSTOM_TARGET(TestHelloWorld
                COMMAND ${CMAKE_COMMAND} ARGS -P ${TEST_COMMAND_FILE}
                DEPENDS hello-world
    )

 
