#!/bin/bash
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

gcc -Werror  -Wno-format  -Wno-deprecated-declarations  -Wall -Wextra -O2   -m64 -fPIC -fno-stack-protector -fno-stack-clash-protection -shared   -ldl $(mysql_config --cflags) $PWD/src/utils.c  $PWD/src/dsd_inject_db_min.c   -o inject.so $(sed -e "s/-L.\+\/ //g" <<< $(mysql_config --libs))   -lz -pthread $([[ $OSTYPE == 'linux'* ]] && echo "-lrt")

