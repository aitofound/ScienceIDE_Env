#!/bin/csh

set WorkDir = $HOME
source $WorkDir/module/pgi 

echo -n "Executing tests PGI....."

cd $WorkDir/Tmp_AMPS_test/PGI/AMPS  

if ($#argv != 1) then
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run >>& test_amps.log
else
  echo Starting test_run_thread$1
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run_thread$1 >>& test_amps_thread$1.log
endif

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsTestPGIComplete

