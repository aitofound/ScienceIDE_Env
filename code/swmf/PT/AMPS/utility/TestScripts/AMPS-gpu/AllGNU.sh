#!/bin/csh

set WorkDir = $HOME
source $WorkDir/module/gnu 

echo -n "Compiling GNU....."

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS  
make test_compile >>& test_amps.log

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS/

echo " done."

echo -n "Executing tests GNU....."
make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run >>& test_amps.log

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsTestGNUComplete
