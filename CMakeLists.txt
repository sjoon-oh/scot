project(scot)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE true)

add_compile_options(
    "-Wall" "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)
# cmake . -DCMAKE_BUILD_TYPE=Debug


add_subdirectory(${PROJECT_SOURCE_DIR}/hartebeest)

include_directories(${PROJECT_SOURCE_DIR}/hartebeest)
include_directories(${PROJECT_SOURCE_DIR}/extern)

link_directories(${PROJECT_SOURCE_DIR}/hartebeest/build/lib)

set(scotlib scot)
file(GLOB scotlib_src 
    CONFIGURE_DEPENDS 
    ${PROJECT_SOURCE_DIR}/src/*.cc 
    ${PROJECT_SOURCE_DIR}/src/sample/*.cc
    )

add_library(${scotlib} SHARED ${scotlib_src})
set_target_properties(
    ${scotlib} 
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/lib
)

target_link_libraries(${scotlib} PUBLIC ibverbs pthread memcached hartebeest)


# Multi-threded test binary
set(scot_mt_testbin scot-mt-testbin)
add_executable(
    ${scot_mt_testbin}
    ${PROJECT_SOURCE_DIR}/src/tests/scot-mt-test.cc
)
set_target_properties(
    ${scot_mt_testbin} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)
target_link_libraries(${scot_mt_testbin} PUBLIC ${scotlib} hartebeest)

set(scot_mt_testbin_skew scot-mt-testbin-skew)
add_executable(
    ${scot_mt_testbin_skew}
    ${PROJECT_SOURCE_DIR}/src/tests/scot-mt-test.cc
)
set_target_properties(
    ${scot_mt_testbin_skew} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)
target_link_libraries(${scot_mt_testbin_skew} PUBLIC ${scotlib} hartebeest)

# Others
# Multi-threded test binary
set(mp_mt_testbin mp-mt-testbin)
add_executable(
    ${mp_mt_testbin}
    ${PROJECT_SOURCE_DIR}/src/tests/mp-mt-test.cc
)
set_target_properties(
    ${mp_mt_testbin} 
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)
target_link_libraries(${mp_mt_testbin} PUBLIC ${scotlib} hartebeest)