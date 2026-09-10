#!/bin/csh

set WorkDir = $HOME

source $WorkDir/module/pgi 
echo -n "Compiling PGI....."

cd $WorkDir/Tmp_AMPS_test/PGI/AMPS  
make test_compile >>& test_amps.log

echo " done."

echo -n "Executing tests PGI....."
make TESTMPIRUN4="mpirun -np 4"  MPIRUN="mpirun -np 8" TESTMPIRUN1="mpirun -np 1" test_run >>& test_amps.log
echo " done."
