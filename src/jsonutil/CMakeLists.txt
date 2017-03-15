file(GLOB jsonutil_SRCS "*.cc")

add_library(jsonutil ${jsonutil_SRCS})
set_target_properties(jsonutil PROPERTIES COMPILE_FLAGS "-std=c++0x")

if(NOT CMAKE_BUILD_NO_TEST)
    add_subdirectory(tests)
endif()
