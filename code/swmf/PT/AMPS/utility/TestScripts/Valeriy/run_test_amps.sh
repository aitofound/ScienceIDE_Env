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
source $HOME/.cshrc

#Set the working directory
set WorkDir = $HOME  
#set WorkDir = /Volumes/Data01

#update the data file repository 
#cd /Users/vtenishe/AMPS_DATA_TEST
#git pull

#Go to your home directory
cd $WorkDir

#Create a temporary directory for the tests
mkdir -p Tmp_AMPS_test
cd Tmp_AMPS_test

#checkout the new copy of AMPS if needed otherwise update the existing copy 
set CheckoutTime = `date`

if (-e AMPS) then 
  cd AMPS_Legacy; git pull 
  cd ../BATL; git pull 
  cd ../AMPS; git pull 

  rm -rf SWMF_data 
  gitclone SWMF_data
  
  cd ../
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

#rm -rf Intel
#mkdir -p Intel; cp -r AMPS Intel/; 
#cp -r BATL Intel/AMPS/

#rm -rf PGI
#mkdir -p PGI;   cp -r AMPS PGI/; 
#cp -r BATL  PGI/AMPS

#Install AMPS
cd $WorkDir/Tmp_AMPS_test/GNU/AMPS                                         
echo AMPS was checked out on $CheckoutTime > test_amps.log
./Config.pl -install -compiler=gfortran,gcc_mpicc    >>& test_amps.log    
utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10

#cd $WorkDir/Tmp_AMPS_test/Intel/AMPS                                       
#echo AMPS was checked out on $CheckoutTime > test_amps.log
#./Config.pl -f-link-option=-lc++ -install -compiler=ifort,iccmpicxx   >>& test_amps.log
#utility/TestScripts/MultiThreadLocalTestExecution.pl -nthreads=10

#cd $WorkDir/Tmp_AMPS_test/PGI/AMPS                                         
#echo AMPS was checked out on $CheckoutTime > test_amps.log
#./Config.pl -f-link-option=-lc++ -install -compiler=pgf90,pgccmpicxx  -link-option=-lc++    >>& test_amps.log    

#Execute the tests
cd $WorkDir/Tmp_AMPS_test
rm -rf Amps*Complete
rm -rf runlog scheduler

cp AMPS/utility/TestScripts/Valeriy/scheduler.cpp .
#g++ ./scheduler.cpp -o scheduler -lpthread
source ~/module/gnu
mpicxx ./scheduler.cpp -o scheduler -lpthread

./scheduler -threads 10  -path /Users/vtenishe/Tmp_AMPS_test -TestScriptsPath Valeriy -ParallelTestExecutionThreads 6  -gcc > runlog 


