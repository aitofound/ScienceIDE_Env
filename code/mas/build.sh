#!/bin/bash
#################################################################
#
# Build script for MAS.
#
# This script requires a configuration file.
#
# See the configure files in the "/conf" 
# folder for examples.
#
# We welcome contributions to the /conf folder for
# various systems and configurations.
#
#################################################################

if [[ $# -lt 1 ]]; then
  echo "ERROR: Please specify a build configuration file:"
  echo "  Example:   ./build.sh conf/gcc_cpu_ubuntu.conf"
  exit 1
fi

if [[ $# -gt 1 ]]; then
  echo "ERROR: Please specify a build configuration file:"
  echo "  Example:   ./build.sh conf/gcc_cpu_ubuntu.conf"
  exit 1
fi

conf_file=$1

while read field_name field_value; do
  # Skips blank lines and comments.
  [[ -z "${field_name}" || "${field_name:0:1}" = "#" ]] && continue

  field_name="${field_name:0:${#field_name}-1}"
  if [[ "${field_name}" = "FC" ]]; then
    FC="${field_value}"
  elif [[ "${field_name}" = "FRTFLAGS" ]]; then
    FRTFLAGS="${field_value}"
  elif [[ "${field_name}" = "HDF5_INCLUDE_DIR" ]]; then
    HDF5_INCLUDE_DIR="${field_value}"
  elif [[ "${field_name}" = "HDF5_LIB_DIR" ]]; then
    HDF5_LIB_DIR="${field_value}"
  elif [[ "${field_name}" = "HDF5_LIB_FLAGS" ]]; then
    HDF5_LIB_FLAGS="${field_value}"
  fi
done < "${conf_file}"

unset field_value
unset field_name

MAS_HOME=$PWD

cX="\033[0m"
cR="\033[1;31m"
cB="\033[1;34m"
cG="\033[32m"
cC="\033[1;96m"
cM="\033[35m"
cY="\033[1;93m"
Bl="\033[1;5;96m"
echo="echo -e"

${echo} "${cG}=== STARTING MAS BUILD ===${cX}"
${echo} "==> Entering src directory..."
pushd ${MAS_HOME}/src > /dev/null
${echo} "==> Removing old Makefile..."
if [ -e Makefile ]; then
  \rm Makefile
fi 
${echo} "==> Generating Makefile from Makefile.template..."
sed \
  -e "s#<FC>#${FC}#g" \
  -e "s#<FRTFLAGS>#${FRTFLAGS}#g" \
  -e "s#<HDF5_INCLUDE_DIR>#${HDF5_INCLUDE_DIR}#g" \
  -e "s#<HDF5_LIB_DIR>#${HDF5_LIB_DIR}#g" \
  -e "s#<HDF5_LIB_FLAGS>#${HDF5_LIB_FLAGS}#g" \
  Makefile.template > Makefile
${echo} "==> Compiling code..."
make clean 1>/dev/null 2>/dev/null ; make 1>build.log 2>build.err
if [ ! -e mas ]; then
  ${echo} "${cR}!!> ERROR!  mas executable not found.  Build most likely failed."
  ${echo} "            Contents of src/build.err:"
  cat build.err
  ${echo} "${cX}"
  exit 1
fi
$echo "==> Moving mas executable to: ${MAS_HOME}/bin/mas"
make install
${echo} "${cG}==> Build complete!${cX}"
${echo}      "    Please add the following to your shell startup (e.g. .bashrc, .profile, etc.):"
${echo} "${cC}    export PATH=${MAS_HOME}/bin:\$PATH${cX}"

