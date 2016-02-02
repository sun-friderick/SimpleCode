
########################################################################################
#############               添加源码子目录                                ############## 
########################################################################################
add_subdirectory( DebugHeap )
add_subdirectory( FunctionsStatistics )
add_subdirectory( GeneralPrintf )
add_subdirectory( InstrumentFunctions )
#add_subdirectory( pstack )
add_subdirectory( StackInfoDebug )


MESSAGE(status "Add sub-directory: ${CMAKE_CURRENT_SOURCE_DIR} .")



