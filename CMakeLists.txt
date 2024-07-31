cmake_minimum_required(VERSION 3.20)
project(re-spirv)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(external/SPIRV-Headers)

add_executable(re-spirv "re-spirv.cpp" "re-spirv-cli.cpp")

target_include_directories(re-spirv PRIVATE ${SPIRV-Headers_SOURCE_DIR}/include})