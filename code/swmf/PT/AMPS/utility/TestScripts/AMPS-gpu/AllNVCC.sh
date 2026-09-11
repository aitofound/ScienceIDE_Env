#!/bin/csh

set WorkDir = $HOME
source $WorkDir/module/gnu 

echo -n "Compiling NVCC....."

cd $WorkDir/Tmp_AMPS_test/NVCC/AMPS  
make test_compile >>& test_amps.log

cd $WorkDir/Tmp_AMPS_test/NVCC/AMPS/

echo " done."

echo -n "Executing tests NVCC....."
make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run >>& test_amps.log
echo " done."
