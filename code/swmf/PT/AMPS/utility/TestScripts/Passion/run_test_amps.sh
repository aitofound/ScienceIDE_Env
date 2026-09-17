#!/bin/csh
# This script checks out the latest version of the AMPS,
# executes the AMPS tests
# This script is meant to be used by the AMPS developers.

# The script can be executed by simply typing
#
# ./run_test_amps.sh
#
# To run the AMPS tests perodically, you can use the crontab facility.
# Type 'crontab -e' to add a new entry to your crontab.
# Here is an example entry for nightly runs at 12:30 am:
#
# 30 0 * * * $HOME/bin/run_test_amps.sh

# NOTE:
# the body of the script is divided into blocks of the format
#  #>BlockName ##############
#  #command_1               #
#  # ...                    #
#  #command_last           <#
# certain blocks will be uncommented at the test installation,

# source shell run commands (ONLY csh AND tcsh SHELLS ARE USED FOR NOW)
# to set CVSROOT or CVS_RSH variables
# note: it is better to have these variables set in the beginning of rc file
source $HOME/.tcshrc

set echo

##module load mpi

#Set the working directory
set WorkDir = $HOME  
#set WorkDir = /Volumes/Data01

#update the data file repository 
#cd /home/vtenishe/AMPS_DATA_TEST
#git pull

#Go to your home directory
cd $WorkDir

#Create a temporary directory for the tests
mkdir -p Tmp_AMPS_test
cd Tmp_AMPS_test

#checkout the new copy of AMPS if needed otherwise update the existing copy 
set CheckoutTime = `date`

rm -rf AMPS
rm -rf AMPS_Legacy
rm -rf BATL
rm -rf SWMF_data


if (-e AMPS) then 
  cd AMPS_Legacy; git pull
  cd ../BATL; git pull
  cd ../AMPS; git pull
  cd SWMF_data; git pull

  cd ../../
else  
  gitclone AMPS_Legacy
  gitclone AMPS
  gitclone BATL
  
  cd AMPS 
  gitclone SWMF_data

  cd ../
endif

#Create separate folders for different compilers
rm -rf GNU
mkdir -p GNU;   cp -r AMPS GNU/; 
cp -r BATL GNU/AMPS/

#rm -rf NVCC 
#mkdir -p NVCC;   cp -r AMPS NVCC/;
#cp -r BATL NVCC/AMPS/

#rm -rf Intel
#mkdir -p Intel; cp -r AMPS Intel/; 
#cp -r BATL Intel/AMPS/

#rm -rf PGI
#mkdir -p PGI;   cp -r AMPS PGI/; 
#cp -r BATL  PGI/AMPS

rm -rf CUDA 
mkdir -p CUDA;   cp -r AMPS CUDA/;
cp -r BATL CUDA/AMPS/



###################### Install GNU ########################################################## 
cd $WorkDir/Tmp_AMPS_test/GNU/AMPS                                         
echo AMPS was checked out on $CheckoutTime > test_amps.log
./Config.pl -install -compiler=gfortran,gcc_mpicc    >>& test_amps.log    
#./Config.pl -fexit=exit
./Config.pl -f-link-option=-lmpi_cxx

#./Config.pl -f-link-option=-lstdc++
#./Config.pl -cpplib-rm=-lmpi_cxx
#./Config.pl -compiler-option=-g

./Config.pl -test-blocks=10
#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10 

############### Install CUDA-DEV branch to compile with gcc ################################## 
#cd $WorkDir/Tmp_AMPS_test/NVCC/AMPS
#echo AMPS was checked out on $CheckoutTime > test_amps.log

########git checkout CUDA-DEV

#./Config.pl -install -compiler=gfortran,gcc_mpicc    >>& test_amps.log
#./Config.pl -fexit=exit
#./Config.pl -f-link-option=-lmpi_cxx

#./Config.pl -compiler-option=-I/opt/openmpi-4.0.2-gcc/include/
#./Config.pl -compiler-option=-ccbin,g++
#./Config.pl -compiler-option=-x,cu
#./Config.pl -cpp-compiler=nvcc
#./Config.pl -cpp-link-option=-lcudart
#./Config.pl -f-link-option=-lcudart

#./Config.pl -compiler-option=-g

#./Config.pl -f-link-option=-lstdc++
#./Config.pl -cpplib-rm=-lmpi_cxx

#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10 

######################## Install CUDA ########################################################## 
cd $WorkDir/Tmp_AMPS_test/CUDA/AMPS
echo AMPS was checked out on $CheckoutTime > test_amps.log

cp Makefile Makefile.bak
./Config.pl -install -compiler=gfortran,gcc_mpicc -f-link-option=-lmpi_cxx

cd share/; mkdir -p lib
cd Library/src; make LIB
cd ../../..

mv Makefile.bak Makefile

########git checkout CUDA-DEV
#./Config.pl -install -compiler=gfortran,gcc_mpicc -f-link-option=-lmpi_cxx

#./Config.pl -cuda
#./Config.pl -compiler-option=-I/opt/nvidia/hpc_sdk/Linux_x86_64/20.7/comm_libs/openmpi/openmpi-3.1.5/include
#./Config.pl -compiler-option=-I/opt/openmpi/openmpi-4.1.4/include
#./Config.pl -compiler-option=-ccbin:g++
#./Config.pl -compiler-option=-x:cu

#./Config.pl -compiler-option=-G:-g:-gencode:arch=compute_75,code=sm_75:-m64:-rdc=true:--expt-extended-lambda

#./Config.pl -cpp-compiler=nvcc
#./Config.pl -cpp-link-option=-lcudart
#./Config.pl -f-link-option=-lcudart:-L/usr/local/cuda/lib64

#./Config.pl -test-blocks=10
#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10

#execute test with CUDA - at this point that is the only test that will be executed with CUDA. More tests will be added in the Table
##module load mpi
#make test_fast-wave_compile -j >>& test_amps.log 
#make test_fast-wave_rundir  >>& test_amps.log
#cd run_test_fast-wave
#mpirun -np 1 ./amps >>& test_amps.log
#cd ..
#make test_fast-wave_check >>& test_amps.log

######################## Install Intel ######################################################### 
#cd $WorkDir/Tmp_AMPS_test/Intel/AMPS                                       
#echo AMPS was checked out on $CheckoutTime > test_amps.log
#./Config.pl -install -compiler=ifort,iccmpicxx   >>& test_amps.log
#./Config.pl -cpplib-rm=-lmpi_cxx
##./Config.pl -fexit=exit
#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10 

#cd $WorkDir/Tmp_AMPS_test/PGI/AMPS                                         
#echo AMPS was checked out on $CheckoutTime > test_amps.log
#./Config.pl -f-link-option=-lmpi_cxx -install -compiler=pgf90,pgccmpicxx    >>& test_amps.log    
##./Config.pl -fexit=exit
#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10 

########################  start scheduler to compile/execute tests #############################
#Execute the tests
cd $WorkDir/Tmp_AMPS_test
rm -rf Amps*Complete
rm -rf runlog scheduler 

cp AMPS/utility/TestScripts/AMPS-gpu/scheduler.cpp .
g++ ./scheduler.cpp -g -o scheduler -lpthread
#./scheduler -threads 10  -path /home/vtenishe/Tmp_AMPS_test -intel -gcc -pgi -nvcc > runlog 


./scheduler -threads 5  -path /home/vtenishe/Tmp_AMPS_test -gcc   > runlog


