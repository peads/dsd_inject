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
while read var; do
    echo  $(date +"%Y-%m-%dT%H:%M:%S%:z"), $var;
done < $PWD/fm-out |
awk -F', ' 'BEGIN{OFS=";"}
{ gsub(/ rms/,""); gsub(/ avg/,""); gsub(/ squelch/,"") }
{
    if ($3>=$6) {
        print $1, substr($2, 1, 3)"."substr($2, 4, 3) | "tee $PWD/db-in"
    }
}'
