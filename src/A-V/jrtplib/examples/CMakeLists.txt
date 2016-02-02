apply_include_paths("${JRTPLIB_INTERNAL_INCLUDES}")
apply_include_paths("${JRTPLIB_EXTERNAL_INCLUDES}")

foreach(IDX 1 2 3 4 5)
	add_executable(Test_jrtplib_${IDX}  example${IDX}.cpp)
	if (UNIX OR JRTPLIB_COMPILE_STATIC)
		target_link_libraries(Test_jrtplib_${IDX} jrtplib-static)
	else ()
		target_link_libraries(Test_jrtplib_${IDX} jrtplib-shared)
	endif ()
endforeach(IDX)

