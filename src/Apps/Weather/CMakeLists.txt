 ########################################################################################
#############                设置模块名称                       ############## 
########################################################################################
 set(MODULE_Name   Weather)
 MESSAGE(STATUS " ${MODULE_Name} module.")


########################################################################################
#############                 生成目标库测试文件                          ############## 
########################################################################################
IF (MODULE_${MODULE_Name})
	add_custom_command( TARGET   APP_${MODULE_Name}.elf 
	                    POST_BUILD
	                    COMMAND /bin/sh cd ${CMAKE_CURRENT_SOURCE_DIR} 
	                    COMMAND /bin/sh make clean && /bin/sh make
	                    COMMENT "In ${CMAKE_CURRENT_SOURCE_DIR}, /bin/sh make "
	                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	                    VERBATIM
	    )
	MESSAGE("MODULE_${MODULE_Name} status.")

ELSE (MODULE_${MODULE_Name})
    MESSAGE(STATUS "Not Include  MODULE_${MODULE_Name}  module.")
ENDIF (MODULE_${MODULE_Name})





   
