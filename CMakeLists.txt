#
# re-spirv
#
# Copyright (c) 2024 renderbag and contributors. All rights reserved.
# Licensed under the MIT license. See LICENSE file for details.
#

cmake_minimum_required(VERSION 3.20)
project(re-spirv)
set(CMAKE_CXX_STANDARD 17)

if (NOT DEFINED SPIRV-Headers_SOURCE_DIR)
    add_subdirectory(external/SPIRV-Headers)
endif()

add_library(re-spirv STATIC "re-spirv.cpp")
set(SPIRV_HEADER_DIR ${SPIRV-Headers_SOURCE_DIR})
target_include_directories(re-spirv PUBLIC ${SPIRV_HEADER_DIR}/include)

add_executable(re-spirv-cli "re-spirv-cli.cpp")
target_link_libraries(re-spirv-cli re-spirv)