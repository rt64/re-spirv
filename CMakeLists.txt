cmake_minimum_required(VERSION 3.20)
project(re-spirv)
set(CMAKE_CXX_STANDARD 17)

add_executable(re-spirv "re-spirv.cpp" "re-spirv-cli.cpp")

add_subdirectory(external/SPIRV-Headers)

set(SPIRV_HEADER_DIR ${SPIRV-Headers_SOURCE_DIR})
target_include_directories(re-spirv PUBLIC ${SPIRV_HEADER_DIR}/include)
