

########################################################################################
##################               配置所包含的功能模块                 ##################
########################################################################################
## Algorithm
option  (MODULE_hash                 		"Enable hash crypto module"                ON)
option  (MODULE_hashTable                    "Enable AES crypto module"                   OFF)
option  (MODULE_hashTableCpp      	"Enable DES crypto module"                   OFF)
option  (MODULE_llmSort                    	"Enable MD5 crypto module"                   OFF)
option  (MODULE_mergeSort                   	"Enable RSA crypto module"                   OFF)
option  (MODULE_tree                 		"Enable SHA128 crypto module"          	OFF)
option  (MODULE_zSort                 		"Enable SHA256 crypto module"          	OFF)


## Apps
option  (MODULE_Astyle                  		"Enable astyle module"                		ON)
option  (MODULE_Weather                 	"Enable weather APP module"           	ON)


## A-V 
option  (MODULE_jrtplib                 	"Enable astyle module"                		ON)
option  (MODULE_live555                 	"Enable weather APP module"          	ON)


## Communication
option  (MODULE_webrtc                		"Enable astyle module"                		ON)
option  (MODULE_xmpp                 		"Enable weather APP module"           	ON)


## Crypto
option  (MODULE_base64c                	"Enable base64 crypto module"         	ON)
option  (MODULE_aes                    		"Enable AES crypto module"                   	ON)
option  (MODULE_base64cpp                    	"Enable DES crypto module"                   	ON)
option  (MODULE_base64live555     	"Enable MD5 crypto module"                   	ON)
option  (MODULE_desc                    		"Enable RSA crypto module"                   	ON)
option  (MODULE_descpp                		"Enable SHA128 crypto module"         	ON)
option  (MODULE_descpp_own                 	"Enable SHA256 crypto module"          	ON)
option  (MODULE_md5                 		"Enable SHA128 crypto module"           	ON)
option  (MODULE_md5lib                 		"Enable SHA256 crypto module"         	ON)
option  (MODULE_md5rfc1321                 	"Enable SHA128 crypto module"           	ON)
option  (MODULE_rsa                 		"Enable SHA256 crypto module"         	ON)
option  (MODULE_sha128                		"Enable SHA128 crypto module"         	ON)
option  (MODULE_sha256                 		"Enable SHA256 crypto module"           	ON)


## Debug
option  (MODULE_debugHeap              		"Enable debug heap module"                  		ON)
option  (MODULE_functionsStatics         	"Enable function stackinfo module"          ON)
option  (MODULE_generaPrintf    			"Enable functions_statistics module"   	ON)
option  (MODULE_systemInstrument         	"Enable log component."             			ON)
option  (MODULE_stackInfo   			"Enable functions_statistics module" 	ON)


## Event
option  (MODULE_libev                  		"Enable message component"          	ON)
option  (MODULE_threadpool                  "Enable thread component"           		ON)


## File
option  (MODULE_iniParser                 	"Enable json module"                        	ON)
option  (MODULE_hashTable              	"Enable ini-parser module"                  	ON)
option  (MODULE_cppJson                     	"Enable xml module"                         	ON)
option  (MODULE_sqlite3                 	"Enable sqlite3 module"                     	ON)
option  (MODULE_unix2dos                 	"Enable sqlite3 module"                     	ON)


## Lang
#option  (MODULE_                     		"Enable xml module"                         	ON)


## Net
option  (MODULE_tinytcp                	"Enable tiny tcp module"                     	ON)
option  (MODULE_arp                    		"Enable arp module"                          	ON)
option  (MODULE_autoupnp                   	"Enable rarp module"                         	ON)
option  (MODULE_dhcp                   		"Enable dhcp module"                         	ON)
option  (MODULE_dns                    		"Enable dns module"                          	ON)
option  (MODULE_sntplib                   	"Enable adns module"                         	ON)
option  (MODULE_sntp                   		"Enable sntp module"                         	ON)
option  (MODULE_ftpServer              	"Enable monitor ftp server module"          ON)
option  (MODULE_ftpClient              	"Enable monitor ftp client module"          ON)
option  (MODULE_tinyadns                	"Enable tiny tcp module"                     	ON)
option  (MODULE_tinyhttpd                    "Enable arp module"                          	ON)
option  (MODULE_tinytcp                   	"Enable rarp module"                         	ON)

#option  (MODULE_rarp                   		"Enable dhcp module"                         OFF)
#option  (MODULE_adns                   		"Enable adns module"                         OFF)


## OwnCode
option  (MODULE_keepSettingModify          	"Enable modify record keeping module"       ON)
option  (MODULE_httpFetcher           		"Enable modify record keeping module"       ON)
option  (MODULE_logc         				"Enable modify record keeping module"       ON)
option  (MODULE_message           			"Enable modify record keeping module"       ON)
option  (MODULE_pCap         				"Enable modify record keeping module"       ON)
option  (MODULE_threads           			"Enable modify record keeping module"       ON)


## Sys
option  (MODULE_mountusb          		"Enable system monitor module"              	ON)

## Views
option  (MODULE_gif                     		"Enable gif codec module"                   		ON)
option  (MODULE_tinyjpeg                	"Enable tiny jpeg codec module"             	ON)
option  (MODULE_lodepng                 	"Enable lodepng codec module"               	ON)
option  (MODULE_easybmp                 	"Enable easybmp codec module"               	ON)
option  (MODULE_picConvert                 "Enable easybmp codec module"               	ON)


## Web






########################################################################################
##################            功能模块 or 组件 测试标志位             ##################
########################################################################################
option  (TEST_MODULE_FLAG                   "Enable module test"                ON)









