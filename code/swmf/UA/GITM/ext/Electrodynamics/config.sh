#!/usr/bin/env bash

COMPILER="gfortran"
ROOTDIR="$(cd "$(dirname "$0")" && pwd)"

usage() {
  echo "Usage: $0 [--compiler=<gfortran|nagfor>]"
  echo "  Configures Electrodynamics for standalone compilation."
  exit 1
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--compiler)
      COMPILER=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 1
      ;;
    *) echo "Unknown argument: $1"; usage ;;
  esac
done

CONFFILE="${ROOTDIR}/build/Makefile.conf.${COMPILER}"
if [ ! -f "${CONFFILE}" ]; then
  echo "Error: No compiler definition found for '${COMPILER}'."
  echo "Available options: gfortran, nagfor"
  exit 1
fi

cp "${CONFFILE}" "${ROOTDIR}/build/Makefile.conf"

cat > "${ROOTDIR}/build/Makefile.local" << EOF
DIRSFILE := ${ROOTDIR}/build/Makefile.dirs
BUILDDIR  := ${ROOTDIR}/build
EOF

touch "${ROOTDIR}/src/Makefile.DEPEND"

echo "Configured for ${COMPILER}. Run 'make' to build."
