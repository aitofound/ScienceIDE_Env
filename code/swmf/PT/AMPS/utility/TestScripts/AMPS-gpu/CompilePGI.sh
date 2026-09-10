#!/bin/csh

source /etc/csh/login.d/env.csh

set WorkDir = $HOME

source $WorkDir/module/pgi 
echo -n "Compiling PGI....."

cd $WorkDir/Tmp_AMPS_test/PGI/AMPS  
make test_compile >>& test_amps.log

echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingPGIComplete
