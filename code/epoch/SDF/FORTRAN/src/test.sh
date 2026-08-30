#! /bin/sh

failed=""
makeflags="-j8"
build_make=0
build_any=0
run_tests=0

while getopts euvth opt
do
   case $opt in
      m) build_make=1 ; build_any=1 ;;
      t) run_tests=1 ; build_any=1 ;;
      h) cat <<EOF
test script options:
  -m: Build using Makefile
  -t: Run tests
EOF
         exit ;;
   esac
done

if [ $build_any -eq 0 ]; then
   build_make=1
   run_tests=1
fi

if [ $build_make -ne 0 ]; then
   echo Building using make...
   make clean
   make $makeflags || failed="$failed make"
   #if [ $run_tests -ne 0 ]; then
   #fi
fi

if [ "$failed"x = x ]; then
  echo All components built successfully
  exit 0
else
  echo FAILURE: The following components failed - $failed
  exit 1
fi
