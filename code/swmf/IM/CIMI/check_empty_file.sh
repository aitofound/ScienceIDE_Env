#!/bin/bash
_file="$1"
[ $# -eq 0 ] && { echo "Usage: $0 filename"; exit 1; }
[ ! -f "$_file" ] && { echo "Error: $0 file not found."; exit 2; }
if [ -s "$_file" ]
then
    echo "$_file has some data (test failed)."
    head $_file
# do something as file has data
exit 1;
else
echo "$_file is empty (test success)."
# do something as file is empty
fi
