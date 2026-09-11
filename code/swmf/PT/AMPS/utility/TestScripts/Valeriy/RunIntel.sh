#!/bin/csh

set WorkDir = $HOME
source $WorkDir/module/intel 


cd $WorkDir/Tmp_AMPS_test/Intel/AMPS; 

echo -n "Executing tests Intel....."

if ($#argv != 1) then
  make TESTMPIRUN4="mpirun -np 4" MPIRUN="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 4" TESTMPIRUN1="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 1" test_run >>& test_amps.log

  cd $WorkDir/Tmp_AMPS_test
  echo Done > AmpsTestIntelComplete
else 
  make TESTMPIRUN4="mpirun -np 4" MPIRUN="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 4" TESTMPIRUN1="export DYLD_LIBRARY_PATH=/Users/ccmc/boost/lib:/opt/intel/compilers_and_libraries/mac/lib;mpirun -np 1" test_run_thread$1 >& test_amps_thread$1.log
endif

echo " done."

