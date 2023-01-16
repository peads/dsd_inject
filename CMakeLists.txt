# This file is part of the dsd_inject distribution (https://github.com/peads/dsd_inject).
# Copyright (c) 2023 Patrick Eads.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.22)
project(dsd_inject C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE)

include_directories(src)

add_definitions(-O2)
add_definitions(-m64)
add_definitions(-fPIC)
add_definitions(-pthread)
ADD_DEFINITIONS(-Wno-deprecated-declarations)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_definitions(-lmariadb)
    ADD_DEFINITIONS(-Werror)
    ADD_DEFINITIONS(-Wall)
    ADD_DEFINITIONS(-Wextra)
    add_definitions(-lrt)
    add_definitions(-fno-stack-protector -fno-stack-clash-protection)
    #http://gcc.gnu.org/wiki/Visibility
    add_definitions(-fvisibility=hidden)
    include_directories(/usr/include/mariadb /usr/include/mariadb/mysql)
elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    include_directories(/usr/local/Cellar/mysql/8.0.31/include/mysql)
    add_definitions(-lmysqlclient)
#elseif(MSVC14 OR MSVC14)
#    #pthread-w32 issue, timespec is now part of time.h
#    ADD_DEFINITIONS(-D_TIMESPEC_DEFINED)
endif()

add_library(dsd_inject SHARED
        src/dsd_inject_db_min.c
        src/inject.h
        src/utils.c)
