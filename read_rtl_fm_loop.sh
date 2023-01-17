#!/bin/bash

while read var; do
    echo  $(date +"%Y-%m-%dT%H:%M:%S%:z"), $var;
done < $PWD/fm-out |
awk -F', ' 'BEGIN{OFS=" "}
{ gsub(/ rms/,""); gsub(/ avg/,""); gsub(/ squelch/,"") }
{
    if ($3>=$6) {
        print $1, substr($2, 1, 3)"."substr($2, 4, 3) | "tee $PWD/db-in;
    }
}'