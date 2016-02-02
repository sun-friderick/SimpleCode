########################################################################################
#############           ������Ҫ�����Դ�ļ�.                             ############## 
########################################################################################
list( APPEND xmpp_Libs_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/logXmppIM.c
    ${CMAKE_CURRENT_SOURCE_DIR}/XmppIMInterface.c
    )


########################################################################################
#############               ����Դ����Ŀ¼                                ############## 
########################################################################################
add_subdirectory( gloox )
#add_subdirectory( gloox-api )
#add_subdirectory( tests )
#add_subdirectory( examples )

MESSAGE("${CMAKE_CURRENT_SOURCE_DIR} status.")



########################################################################################
#############               ���� gloox ��汾��                         ############## 
########################################################################################
set(xmpp_LIB_VERSION     "1.0.0")
set(xmpp_LIB_SOVERSION   "1")



########################################################################################
#############              ����ͷ�ļ�Ŀ¼                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Xmpp_IM
)  



########################################################################################
#############           ����Ŀ����ļ� �� �����汾��                      ############## 
########################################################################################
IF (COMPONENT_xmpp)
    #���ɶ�̬��  ��̬���� STATIC  
    add_library (xmpplib   SHARED          ${xmpp_Libs_SRCS})  
    add_library (xmpps     STATIC          ${xmpp_Libs_SRCS})  
    
    # ����������ϵ�������ǰ������ײ�Ĺ����⣬����Ҫ����
    add_dependencies (xmpplib  loglib )
    add_dependencies (xmpps    loglib )
    
    # ����Ҫ���ӵĹ�����, ���˳����Ǳ�����������ʱ˳��
    target_link_libraries (xmpplib  loglib )
    target_link_libraries (xmpps    loglib )
    
    # ���ð汾�ţ�SOVERSIONΪAPI�汾��
    set_target_properties(xmpplib  PROPERTIES 
                                    VERSION     ${xmpp_LIB_VERSION} 
                                    SOVERSION   ${xmpp_LIB_SOVERSION} )
ELSE (COMPONENT_xmpp)
    MESSAGE(STATUS "Not Include COMPONENT xmpp module.")
ENDIF (COMPONENT_xmpp)

