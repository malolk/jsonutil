cmake_minimum_required(VERSION 2.6)

project(glue C CXX)

set(CXX_FLAGS
	-g
	-Wall
	-Wextra
	-Werror
	-Wconversion
	-Wno-unused-parameter
	-Wold-style-cast
	-Woverloaded-virtual
	-Wpointer-arith
	-Wshadow
	-Wwrite-strings
	-march=native
	-std=c++0x
	-pthread
	-rdynamic
	)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -finline-limit=1000 -DDEBUG")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

message(STATUS "executable path " ${EXECUTABLE_OUTPUT_PATH})
message(STATUS "library path " ${LIBRARY_OUTPUT_PATH})
message(STATUS "include path " ${PROJECT_SOURCE_DIR})

include_directories(${CMAKE_SOURCE_DIR}/src)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/jsonutil)
add_subdirectory(${CMAKE_SOURCE_DIR}/demo)
