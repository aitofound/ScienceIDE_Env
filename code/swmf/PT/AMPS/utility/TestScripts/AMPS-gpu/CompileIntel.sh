#!/bin/csh

#source /etc/csh/login.d/env.csh

set WorkDir = $HOME
source $WorkDir/module/intel 

echo -n "Compiling Intel....."

cd $WorkDir/Tmp_AMPS_test/Intel/AMPS; 
make test_compile >>& test_amps.log

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingIntelComplete
