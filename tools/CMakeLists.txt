add_executable(dcc dcc.cpp)
include(GNUInstallDirs)
target_link_libraries(dcc PRIVATE dcc::libdcc PkgConfig::readline) 
install(
	TARGETS dcc
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
