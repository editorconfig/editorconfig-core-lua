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

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0074 NEW)

project(editorconfig-core-lua VERSION 0.3.1 LANGUAGES C)

set(ECL_VERSION "${PROJECT_VERSION}-git")

#
# EditorConfig Core Lua is a very thin shim around the EditorConfig C
# Library. The vast majority if not all test failures will be because
# the C library version being used is failing the test as well.
#
option(ENABLE_TESTS "Enable running the test suite" OFF)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

include(GNUInstallDirs)

find_package(EditorConfig CONFIG REQUIRED)

set(Lua_FIND_VERSION 5.2) # minimum Lua version
find_package(Lua REQUIRED)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRE ON)

add_library(editorconfig_lua MODULE editorconfig_lua.c)
target_compile_definitions(editorconfig_lua
    PRIVATE "ECL_VERSION=\"${ECL_VERSION}\""
)
if(CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(editorconfig_lua
        PUBLIC -Wall -Wextra
    )
endif()
target_include_directories(editorconfig_lua SYSTEM
    PRIVATE ${LUA_INCLUDE_DIR}
)
target_link_libraries(editorconfig_lua
    editorconfig_shared
)
set_target_properties(editorconfig_lua PROPERTIES
    # Omit the lib<name>.so prefix from the DSO
    PREFIX ""
    OUTPUT_NAME "editorconfig"
)

set(ECL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
    CACHE PATH "Target installation directory."
)

install(TARGETS editorconfig_lua
    DESTINATION ${ECL_LIBDIR}
)

find_program(LDOC_CMD ldoc)
add_custom_target(doc
    COMMAND ${LDOC_CMD} ${CMAKE_SOURCE_DIR}/editorconfig_lua.c
)

# Testing. Type "make test" to run tests. Only if the test submodule is
# checked out should we do this.
if(ENABLE_TESTS)
    if(EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
        enable_testing()
        find_program(LUA_CMD lua${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR})
        configure_file(test.lua.in test.lua @ONLY)
        set(EDITORCONFIG_CMD ${LUA_CMD} ${CMAKE_BINARY_DIR}/test.lua)
        add_subdirectory(tests)
    else()
        message(WARNING
            "Testing files not found. Testing will not be available. If you obtained the source tree through git, please run 'git submodule update --init' to update the tests submodule.")
    endif()
endif()
