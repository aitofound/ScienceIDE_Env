#!/bin/csh

set WorkDir = $HOME

source $WorkDir/Tmp_AMPS_test/AMPS/utility/TestScripts/CompilerSetup/set_mpi_pgi.valeriy
echo -n "Compiling PGI....."

cd $WorkDir/Tmp_AMPS_test/PGI/AMPS  
make test_compile >>& test_amps.log

echo " done."

echo -n "Executing tests PGI....."
utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=3
mv Makefile.test.split Makefile.test

make TESTMPIRUN4="mpirun -np 4" MPIRUN="mpirun -np 4" test_run_thread1  >& test_amps.run.thread1.log & 
make TESTMPIRUN4="mpirun -np 4" MPIRUN="mpirun -np 4" test_run_thread2  >& test_amps.run.thread2.log & 
make TESTMPIRUN4="mpirun -np 4" MPIRUN="mpirun -np 4" test_run_thread3  >& test_amps.run.thread3.log & 

echo " done."
