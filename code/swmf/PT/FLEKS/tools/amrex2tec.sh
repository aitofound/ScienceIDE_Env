#!/bin/bash

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

flag=$1

shift 1

for f in "$@"
do
pvpython $SCRIPT_DIR/amrex2tec.py $flag $f
if [ $? -eq 0 ]; then
   rm -rf $f
fi
done
