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
#make sure they're dead
screen -ls | grep 'dsd\|rtl_fm\|sox' | awk '{print $1}' | xargs -I % -t screen -X -S % quit
ps aux | grep  -E '*\-in | *\-out' | grep socat | awk '{print $2}' | xargs -I % -t kill %
sleep 1

#REALLY REALLY make sure they're dead
killall -s SIGKILL socat
killall -s SIGKILL dsd
killall -s SIGKILL rtl_fm
sleep 1

#start the show
rm -f $PWD/*-in $PWD/*-out
touch fm-in fm-out db-in db-out
socat -d -d pty,raw,link=$PWD/fm-in,echo=0 pty,raw,link=$PWD/fm-out,echo=0 &
socat -d -d pty,raw,link=$PWD/db-in,echo=0 pty,raw,link=$PWD/db-out,echo=0 &

#create rtl_fm to sox plumbing via socat
screen -d -m -S sox bash --noprofile --norc -c 'rtl_fm -L 50 -p -1 -T -f 154685k -f 155190k -f 154935k -f 155370k -f 155625k -f 155700k -f 155685k -f 156210k -M nfm -s 12.5k -l 550 -g 49.2 -t 2 - 2>$PWD/fm-in | \
sox -t raw -r 12.5k -v 1 -es -b16 -L -c1 - -b16 -es -c1 -r 48000 -L -t raw - | \
socat -d -d -u - tcp-listen:1234,reuseaddr,fork'

#create sox to dsd plumbing via socat
screen -d -m -S dsd bash --noprofile --norc  -c 'socat -d -d tcp-connect:localhost:1234,reuseaddr - | LD_PRELOAD=$PWD/inject.so dsd -i - -o /dev/null -u 7 -g 20 -f1 -pu -mc -d $OUT_DIR'

#create rtl_fm stderr to db read loop, you guessed it, via socat (albeit indirectly)
screen -d -m -S rtl_fm bash --noprofile --norc -c "$1/read_rtl_fm_loop.sh"

screen -S freq bash --noprofile --norc -c "$PWD/correlate_frequencies $PWD/db-out"
