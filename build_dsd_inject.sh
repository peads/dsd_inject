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
if [[ $OSTYPE == 'darwin'* ]]; then
  R_T=''
  O_FILE='inject.dylib'
elif [[ $OSTYPE == 'linux'* ]]; then
  R_T='-lrt'
  O_FILE='inject.so'
fi

gcc -Werror -Wno-deprecated-declarations -Wall -Wextra -O2 -m64 -fPIC -shared -ldl $(mysql_config --cflags) \
  $DSD_INJECT_SRC/src/utils.c $DSD_INJECT_SRC/src/dsd_inject_db_min.c -o $PWD/$O_FILE \
  $(mysql_config --libs) -lz -fno-stack-protector -fno-stack-clash-protection \
  -pthread $R_T
