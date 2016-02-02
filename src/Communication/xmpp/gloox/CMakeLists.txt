########################################################################################
#############           添加所要编译的源文件.                             ############## 
########################################################################################
list( APPEND gloox_Libs_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/adhoc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/amp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/annotations.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/atomicrefcount.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/attention.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base64.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bookmarkstorage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capabilities.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/carbons.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/chatstate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/chatstatefilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/clientbase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/component.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compressiondefault.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compressionzlib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionbosh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionhttpproxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectionsocks5proxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectiontcpbase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectiontcpclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectiontcpserver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectiontls.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/connectiontlsserver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dataform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dataformfield.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dataformfieldcontainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dataformitem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dataformreported.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/delayeddelivery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/disco.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dns.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/eventdispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/featureneg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/flexoff.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/forward.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gloox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpgencrypted.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gpgsigned.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inbandbytestream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/instantmucroom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iodata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/iq.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jinglecontent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jinglefiletransfer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jingleiceudp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jinglepluginfactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jinglesession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jinglesessionmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/lastactivity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linklocalclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linklocalmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logsink.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/md5.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/message.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messageevent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messageeventfilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messagefilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/messagesession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mucinvitationhandler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mucmessagesession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mucroom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mutex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nickname.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nonsaslauth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/oob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/prep.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/privacyitem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/privacymanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/privatexml.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pubsubevent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pubsubitem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pubsubmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/receipt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/registration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rosteritem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rostermanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/search.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sha.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/simanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/siprofileft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socks5bytestream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socks5bytestreammanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/socks5bytestreamserver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/softwareversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stanza.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stanzaextensionfactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/subscription.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsdefault.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsgnutlsbase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsgnutlsclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsgnutlsclientanon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsgnutlsserveranon.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsopensslbase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsopensslclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsopensslserver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tlsschannel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/uniquemucroom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vcard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vcardmanager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vcardupdate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/version.rc
    ${CMAKE_CURRENT_SOURCE_DIR}/xhtmlim.cpp
    )

    

########################################################################################
#############               设置 gloox 库版本号                         ############## 
########################################################################################
set(gloox_LIB_VERSION     "1.0.0")
set(gloox_LIB_SOVERSION   "1")



########################################################################################
#############              添加头文件目录                                 ############## 
########################################################################################
include_directories(  
    ${PROJECT_SOURCE_DIR}/includes  
    ${PROJECT_SOURCE_DIR}/Xmpp_IM
    ${PROJECT_SOURCE_DIR}/Xmpp_IM/gloox
    ${PROJECT_SOURCE_DIR}/Xmpp_IM/gloox/includes
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
IF (MODULE_gloox)
    #生成动态库  静态库则 STATIC  
    add_library (glooxlib   SHARED          ${gloox_Libs_SRCS})  
    add_library (glooxs     STATIC          ${gloox_Libs_SRCS})  
    
    # 设置依赖关系，如果当前库是最底层的共享库，则不需要设置
    add_dependencies (glooxlib  loglib dl)
    add_dependencies (glooxs    loglib dl)
    
    # 添加要链接的共享库, 库的顺序就是编译器的链接时顺序
    target_link_libraries (glooxlib  loglib dl)
    target_link_libraries (glooxs    loglib dl)
    
    # 设置版本号，SOVERSION为API版本号
    set_target_properties(glooxlib  PROPERTIES 
                                    VERSION     ${gloox_LIB_VERSION} 
                                    SOVERSION   ${gloox_LIB_SOVERSION} )
ELSE (MODULE_gloox)
    MESSAGE(STATUS "Not Include MODULE gloox module.")
ENDIF (MODULE_gloox)


