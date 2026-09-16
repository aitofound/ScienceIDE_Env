#!/bin/bash

# Script to load MAS environment.
# This script sets all the paths to MAS and its scripts/tools.

# To use this script, source it in your BASH terminal with:
#   . load_mas_env.sh

cR="\033[1;31m"
cG="\033[32m"
cX="\033[0m"
cM="\033[35m"
echo="echo -e"

${echo} "${cG}==> MAS Environment Setup${cX}"

(return 0 2>/dev/null) && sourced=1 || sourced=0

if [ $sourced -eq "0" ]
then
  ${echo} "${cR}==> ERROR! It seems this script was executed instead of being sourced.${cX}"
  ${echo} "${cR}    Please source this script with: '. load_mas_env.sh'${cX}"
  exit 1
fi

if [ `uname` == "Darwin" ]
then
  has_realpath=`which realpath`
  if [ "$?" == "1" ]; then
    echo "### ERROR! This version of macOS is too old to have the 'realpath' utility"
    echo "    so this load_mas_env.sh script may not work properly."
    echo "    Instead please add The MAS and MAS/bin folders to your path manually!"
  else
    mas_dir="$( dirname -- "$( realpath -- "${BASH_SOURCE[0]}"; )"; )"
  fi
else
  mas_dir="$( dirname -- "$( readlink -f -- "${BASH_SOURCE[0]}"; )"; )"
fi

if [ ! -e ${mas_dir}/bin/mas ]
then
  ${echo} "${cM}==> WARNING! MAS does not seem to be built!${cX}"
fi

echo "==> Appending MAS and its scripts/tools located at [${mas_dir}] to PATH..."

export PATH=${mas_dir}:${mas_dir}/bin:$PATH

${echo} "${cG}==> Done!${cX}"

