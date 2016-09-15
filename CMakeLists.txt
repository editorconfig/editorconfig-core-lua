#
# Copyright (c) 2016 João Valverde <joao.valverde@tecnico.ulisboa.pt>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

cmake_minimum_required(VERSION 2.8.8)

project(editorconfig-core-lua C)

option(WERROR "Enable warnings as errors" OFF)

set(ECL_VERSION 0.1.1)
set(ECL_VERSION_SUFFIX "" CACHE STRING "Optional extra version suffix.")
if(DEFINED VERSION_SUFFIX)
    set(ECL_VERSION "${ECL_VERSION}${VERSION_SUFFIX}")
elseif(NOT ECL_VERSION_SUFFIX STREQUAL "")
    set(ECL_VERSION "${ECL_VERSION}${ECL_VERSION_SUFFIX}")
elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ECL_VERSION "${ECL_VERSION}-git")
endif()

add_definitions(-DLEC_VERSION="${ECL_VERSION}")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

include(GNUInstallDirs)
include(FindEditorConfig)

find_package(EditorConfig REQUIRED)
include_directories(SYSTEM ${EDITORCONFIG_INCLUDE_DIRS})

set(Lua_FIND_VERSION_EXACT TRUE)
set(Lua_FIND_VERSION_COUNT 1)
set(lua_append_versions 5.3)
find_package(Lua REQUIRED)
include_directories(SYSTEM ${LUA_INCLUDE_DIRS})

set(ECL_LUA_INTERPRETER lua
    CACHE STRING "Lua 5.3 interpreter name (for testing)"
)
set(ECL_INSTALL_LIBDIR_LUA "${CMAKE_INSTALL_LIBDIR}/lua/5.3"
    CACHE PATH "Directory for Lua 5.3 module installation"
)

if (CMAKE_COMPILER_IS_GNUCC)
    set(ECL_COMPILER_FLAGS
        -std=c99
        -O2 -g -Wall -Wextra
        $<$<BOOL:${WERROR}>:-Werror -Wfatal-errors>
    )
endif()
add_compile_options("${ECL_COMPILER_FLAGS}")

set(ECL_SRC
    ${CMAKE_SOURCE_DIR}/editorconfig_lua.c
)

add_library(editorconfig_core MODULE ${ECL_SRC})
set_target_properties(editorconfig_core PROPERTIES
    PREFIX ""
)
target_link_libraries(editorconfig_core
    ${EDITORCONFIG_LIBRARIES}
    ${LUA_LIBRARIES}
)

configure_file(editorconfig.lua.in editorconfig.lua)

install(TARGETS editorconfig_core
    LIBRARY DESTINATION ${ECL_INSTALL_LIBDIR_LUA}
)

enable_testing()
set(EDITORCONFIG_CMD ${ECL_LUA_INTERPRETER} ${CMAKE_BINARY_DIR}/editorconfig.lua)
add_subdirectory(tests)
