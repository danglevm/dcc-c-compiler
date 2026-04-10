add_library(libdcc lexer.cpp parser.cpp asm.cpp tacky.cpp)
add_library(dcc::libdcc ALIAS libdcc)

set_target_properties(
	libdcc
	PROPERTIES OUTPUT_NAME dcc
)

target_compile_features(libdcc PUBLIC cxx_std_23)

target_include_directories(
	libdcc
	PRIVATE 
		${CMAKE_SOURCE_DIR}/src/include
	PUBLIC
       		$<INSTALL_INTERFACE:include>	
		$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

include(GNUInstallDirs)
install(TARGETS libdcc
	EXPORT dcc-targets
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
	DIRECTORY ${PROJECT_SOURCE_DIR}/include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

