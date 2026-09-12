#!/bin/csh

set WorkDir = $HOME
##source $WorkDir/module/gnu 
#module load mpi


echo -n "Compiling GNU....."

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS  
make test_compile >>& test_amps.log

cd $WorkDir/Tmp_AMPS_test/GNU/AMPS/

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingGNUComplete
