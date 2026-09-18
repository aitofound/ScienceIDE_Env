#!/bin/bash

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

for f in "$@"
do
pvpython $SCRIPT_DIR/converter.py $f
if [ $? -eq 0 ]; then
   rm -rf $f
fi
done
