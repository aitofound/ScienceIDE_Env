#!/bin/csh

set WorkDir = $HOME
source $WorkDir/module/gnu 

echo -n "Executing tests GNU....."

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS/

if ($#argv != 1) then
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 4" TESTMPIRUN1="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 1" test_run >>& test_amps.log

  cd $WorkDir/Tmp_AMPS_test
  echo Done > AmpsTestGNUComplete
else 
  echo Starting test_run_thread$1
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 4" TESTMPIRUN1="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 1" test_run_thread$1 >& test_amps_thread$1.log
endif

echo " done."

