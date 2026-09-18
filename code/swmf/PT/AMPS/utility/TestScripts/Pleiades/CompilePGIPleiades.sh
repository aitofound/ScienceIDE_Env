#!/bin/csh

source /usr/share/modules/init/csh
source $HOME/.cshrc

set WorkDir = /nobackup/`whoami`


module unload gcc
module unload comp-intel

module use -a /nasa/modulefiles/testing
module load gcc/4.9.4
module load comp-pgi/19.10 mpi-hpe/mpt.2.18r160

echo -n "Compiling PGI....."                 
cd $WorkDir/Tmp_AMPS_test/PGI/AMPS           
make test_compile >>& test_amps.log          
echo " done."

cd $WorkDir/Tmp_AMPS_test
echo Done > AmpsCompilingPGIComplete
