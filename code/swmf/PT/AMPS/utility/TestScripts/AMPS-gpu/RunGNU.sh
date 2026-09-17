#!/bin/csh

set WorkDir = $HOME
##source $WorkDir/module/gnu 

echo -n "Executing tests GNU....."

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS  

if ($#argv != 1) then
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run >>& test_amps.log
  echo " done."
else
  echo Starting test_run_thread$1
  make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run_thread$1 >>& test_amps_thread$1.log
  echo  done test_run_thread$1.
endif

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsTestGNUComplete
