cmake_minimum_required(VERSION 3.19)

project ("dcc" LANGUAGES CXX)

# set default build type
set (CMAKE_BUILD_TYPE "Debug")

find_package(PkgConfig REQUIRED)
pkg_check_modules(readline REQUIRED IMPORTED_TARGET readline)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/writing-a-c-compiler-tests"
      DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/test"
      FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


include (CTest)

add_subdirectory("src")
add_subdirectory("tools")

if (BUILD_TESTING)
	add_subdirectory("test")
endif()
