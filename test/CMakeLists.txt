include(FetchContent)

FetchContent_Declare(
	Catch2
	GIT_REPOSITORY https://github.com/catchorg/Catch2.git	
	GIT_TAG v3.5.3
)

FetchContent_MakeAvailable(Catch2)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE dcc::libdcc Catch2::Catch2WithMain)
