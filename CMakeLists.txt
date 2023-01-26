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

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(MYSQL REQUIRED)
find_package(Threads REQUIRED)

include_directories(SYSTEM ${MYSQL_INCLUDE_DIR})
set(LIBS ${MYSQL_LIBRARIES})

FILE(GLOB SRCS src/*.c)
FILE(GLOB HEADERS include/*.h)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_library(dsd_inject SHARED ${SRCS} ${HEADERS})

#include_directories(src)
#
#add_definitions(-O2)
#add_definitions(-m64)
#add_definitions(-fPIC)
#add_definitions(-pthread)
#ADD_DEFINITIONS(-Werror)
#ADD_DEFINITIONS(-Wno-deprecated-declarations)
#add_definitions(-Wno-unused-variable)
#ADD_DEFINITIONS(-Wall)
#ADD_DEFINITIONS(-Wextra)
#
#if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
#    add_definitions(-Wno-unused-but-set-variable)
#    add_definitions(-lmariadb)
#    add_definitions(-lrt)
#    add_definitions(-fno-stack-protector -fno-stack-clash-protection)
#    #http://gcc.gnu.org/wiki/Visibility
#    add_definitions(-fvisibility=hidden)
#    include_directories(/usr/include/mariadb /usr/include/mariadb/mysql)
#elseif(CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
#    add_definitions(-Wno-unused-command-line-argument)
#    add_definitions(-L/usr/local/Cellar/mysql/8.0.31/lib -lmysqlclient -lz -L/usr/local/lib -lzstd
#            -L/usr/local/opt/openssl@1.1/lib -lssl -lcrypto -lresolv)
#    include_directories(/usr/local/Cellar/mysql/8.0.31/include/mysql)
##elseif(MSVC14 OR MSVC14)
##    #pthread-w32 issue, timespec is now part of time.h
##    ADD_DEFINITIONS(-D_TIMESPEC_DEFINED)
#endif()
#
#add_library(dsd_inject SHARED
#        include/global.h
#        include/utils.h
#        src/utils.c
#        src/dsd_inject_db_min.c
#)
