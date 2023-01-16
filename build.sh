#!/bin/bash

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
  -pthread $R_T -DTRACE
