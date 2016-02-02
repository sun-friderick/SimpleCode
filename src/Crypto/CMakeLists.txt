
########################################################################################
#############               添加源码子目录                                ############## 
########################################################################################
add_subdirectory( AES )
add_subdirectory( Base64c )
add_subdirectory( Base64cpp )
add_subdirectory( Base64live555 )
add_subdirectory( DESc )
add_subdirectory( DEScpp )
add_subdirectory( DEScpp_own )
add_subdirectory( MD5 )
add_subdirectory( MD5lib )
add_subdirectory( MD5rfc1321 )
#add_subdirectory( PolarSSL )
add_subdirectory( RSA )
add_subdirectory( SHA128 )
add_subdirectory( SHA256 )


MESSAGE(status "Add sub-directory: ${CMAKE_CURRENT_SOURCE_DIR} .")



