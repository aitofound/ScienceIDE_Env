#!/bin/csh

source /usr/share/modules/init/csh
source $HOME/.cshrc

set WorkDir = /nobackup/`whoami`

module unload gcc
module unload comp-pgi
module load comp-intel


echo -n "Compiling Intel....."                 
cd $WorkDir/Tmp_AMPS_test/Intel/AMPS           
make test_compile >>& test_amps.log          
echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingIntelComplete
