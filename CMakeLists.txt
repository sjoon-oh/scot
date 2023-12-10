project(scot)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE true)

add_compile_options(
    "-Wall" "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)


add_subdirectory(${PROJECT_SOURCE_DIR}/hartebeest)

include_directories(${PROJECT_SOURCE_DIR}/hartebeest)
include_directories(${PROJECT_SOURCE_DIR}/extern)

link_directories(${PROJECT_SOURCE_DIR}/hartebeest/build/lib)

set(scotlib scot)
file(GLOB scotlib_src 
    CONFIGURE_DEPENDS 
    ${PROJECT_SOURCE_DIR}/src/*.c
    ${PROJECT_SOURCE_DIR}/src/*.cc 
    ${PROJECT_SOURCE_DIR}/includes/*.h
    ${PROJECT_SOURCE_DIR}/includes/*.hh
    )

add_library(${scotlib} SHARED ${scotlib_src})
set_target_properties(
    ${scotlib} 
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib
)

target_link_libraries(${scotlib} PUBLIC ibverbs pthread memcached hartebeest)

# Test binary
set(scot_testbin scot-testbin)
add_executable(
    ${scot_testbin}
    ${PROJECT_SOURCE_DIR}/src/tests/test.cc
)
set_target_properties(
    ${scot_testbin} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)
target_link_libraries(${scot_testbin} PUBLIC ${scotlib} hartebeest)



