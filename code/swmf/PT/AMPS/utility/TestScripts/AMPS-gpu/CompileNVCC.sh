#!/bin/csh

set WorkDir = $HOME
##source $WorkDir/module/gnu 

echo -n "Compiling NVCC....."

cd $WorkDir/Tmp_AMPS_test/NVCC/AMPS  
make test_compile >>& test_amps.log

cd $WorkDir/Tmp_AMPS_test/NVCC/AMPS/

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingGNUComplete
