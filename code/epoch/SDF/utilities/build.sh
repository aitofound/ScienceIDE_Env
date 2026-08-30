#! /bin/sh

SDFDIR=../C
EXTDIR=../extension
PYTHONCMD=$(which python3)

clean=0
errcode=0
ascii=0
system=0
CFLAGS="-DSDF_DEBUG_ALL -D_XOPEN_SOURCE=600 -I$SDFDIR/include -L$SDFDIR/lib"
LDFLAGS=""
OPT="$CFLAGS -O3 -g"

while getopts crdpash23 name
do
   case $name in
      c) clean=2 ;;
      r) clean=1 ;;
      d) OPT="$CFLAGS -g -O0 -Wall -Wno-unused-function -std=c99 -pedantic";
         PYDBG="-g" ;;
      a) ascii=1 ;;
      s) system=1 ;;
      2) PYTHONCMD=$(which python2) ;;
      3) PYTHONCMD=$(which python3) ;;
      h) cat <<EOF
build script options:
  -c: Clean up files. Do not build anything.
  -r: Rebuild. Clean up files and then build.
  -d: Build with debugging flags enabled.
  -a: Build sdf2ascii script
  -s: Install python reader system-wide
  -2: Build for python2
  -3: Build for python3
EOF
         exit ;;
   esac
done

if [ "$PYTHONCMD"x = x ]; then
  PYTHONCMD=$(which python)
fi

cd `dirname $0`/.

if [ $clean -ge 1 ] ; then
  rm -rf sdf2ascii sdf2ascii.dSYM
  rm -rf build sdf.egg-info *.dSYM/
fi
if [ $clean -le 1 ] ; then
  if [ ! -r $SDFDIR/lib/libsdfc.a ]; then
    echo "ERROR: SDF C library must be built first"
    echo "Switch to the C directory and type make"
    exit 1
  fi
  sh gen_commit_string.sh .
  if [ $ascii -ne 0 ]; then
    gcc $OPT -o sdf2ascii sdf2ascii.c -lsdfc -ldl -lm || errcode=1
    ./sdf2ascii -V > /dev/null || rm -f sdf2ascii
  fi
  gcc $OPT -o sdffilter sdffilter.c sdf_vtk_writer.c -lsdfc -ldl -lm || errcode=1
  gcc $OPT -o sdfdiff sdfdiff.c -lsdfc -ldl -lm || errcode=1
  if [ "$PYTHONCMD"x != x ]; then
    CFLAGS="$OPT" $PYTHONCMD -m pip install . || errcode=1
  fi
  which a2x > /dev/null 2>&1
  if [ $? -eq 0 ]; then
    for n in sdffilter sdf2ascii; do
      if [ "$n.adoc" -nt "$n.1" ]; then
        echo Building $n manpage
        a2x --verbose --no-xmllint -d manpage -f manpage "$n.adoc"
      fi
    done
  fi
fi

exit $errcode
