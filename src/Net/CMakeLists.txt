
########################################################################################
#############               添加源码子目录                                ############## 
########################################################################################
add_subdirectory( arp )
#add_subdirectory( AutoUPnP )
add_subdirectory( cURL )
add_subdirectory( DHCP )
add_subdirectory( DNS )
add_subdirectory( FTPServer )
add_subdirectory( LwIP )
#add_subdirectory( MiniUPnPclient )
add_subdirectory( MiniUPnPdaemon )
add_subdirectory( OpenNTPd )
add_subdirectory( SNTP )
add_subdirectory( SNTPlib )
add_subdirectory( TinyAdns )
add_subdirectory( TinyHttpd )
add_subdirectory( TinyTCP )
#add_subdirectory( WebBench )



MESSAGE(status "Add sub-directory: ${CMAKE_CURRENT_SOURCE_DIR} .")



