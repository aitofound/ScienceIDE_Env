#!/bin/csh
# This sends the results of AMPS nightly tests to the server:
#   herot.engin.umich.edu 
# for further processing
# This script is meant to be used by the AMPS developers.

# The script can be executed by simply typing
#
# ./scp_run_amps.sh
#
# To scp the AMPS tests' resuults perodically, you can use the crontab facility.
# Type 'crontab -e' to add a new entry to your crontab.
# Here is an example entry for for scp'ing nightly runs results at 8:00 am:
#
# 30 8 * * * $HOME/bin/scp_test_amps.sh

# set the working directory
set WorkDir = $HOME  
set Server  = passion

#set Server  = vtenishe@herot.engin.umich.edu

#set echo

#>Pleiades ############################
#set WorkDir = /nobackup/`whoami`    <#

#>Stampede ############################
#set WorkDir = $WORK                 <#

#>Yellowstone ###########
#set WorkDir =         <#

# Go to your home directory
cd $WorkDir/Tmp_AMPS_test

# Create file with results' summary and scp to server

# GNU compiled test
#>GNUAll #####################################################################
if (-e GNU) then 
  cd GNU/AMPS                                                                 #
  cp test_amps.log test_amps.log.bak
  #./utility/TestScripts/CheckTests.pl

  echo "Processing GNU/AMPS"

  if (-e test_amps_thread1.log) then
    foreach f (test_amps_thread*.log) #
      cat $f >> test_amps.log
    end
  endif

  rm -rf test_amps.res

  set nonomatch

  set val=( *diff)  

  if ("$val" != "" ) then
    ls -ltr  *diff > test_amps.res                                              #
    echo '===============================================' >> test_amps.res     # 
    head -100 *diff >> test_amps.res                                            #
  else
   touch test_amps.res
  endif

  if ("$HOST" != "passion") then
    scp test_amps.res test_amps.log ${Server}:/homedata/vtenishe/AMPSTEST/Sites/Current/amps-gpu_gnu/  #
  else 
    scp test_amps.res test_amps.log /home/vtenishe/AMPSTEST/Sites/Current/amps-gpu_gnu/  #
  endif

  mv test_amps.log.bak test_amps.log
  cd ../..                                                                   
endif

# GNU compiled test
#>GNUAll #####################################################################
if (-e NVCC) then
  cd NVCC/AMPS                                                                 #
  cp test_amps.log test_amps.log.bak
  #./utility/TestScripts/CheckTests.pl

  echo "Processing NVCC/AMPS"

  if (-e test_amps_thread1.log) then
    foreach f (test_amps_thread*.log) #
      cat $f >> test_amps.log
    end
  endif

  rm -rf test_amps.res

  set nonomatch
  
  set val=( *diff)

  if ("$val" != "" ) then
    ls -ltr  *diff > test_amps.res                                              #
    echo '===============================================' >> test_amps.res     #
    head -100 *diff >> test_amps.res                                            #
  else
    touch test_amps.res
  endif

  if ("$HOST" != "passion") then
    scp test_amps.res test_amps.log ${Server}:/homedata/vtenishe/AMPSTEST/Sites/Current/amps-gpu_nvcc/  #
  else
    scp test_amps.res test_amps.log /home/vtenishe/AMPSTEST/Sites/Current/amps-gpu_nvcc/  #
  endif

  mv test_amps.log.bak test_amps.log
  cd ../..
endif


# Intel compiled test
#>IntelAll ###################################################################
if (-e Intel) then
  cd Intel/AMPS                                                               #
  cp test_amps.log test_amps.log.bak
  #./utility/TestScripts/CheckTests.pl

  echo "Processing Intel/AMPS"

  if (-e test_amps_thread1.log) then
    foreach f (test_amps_thread*.log) #
      cat $f >> test_amps.log
    end
  endif

  rm -rf test_amps.res

  set nonomatch

  set val=( *diff)

  if ("$val" != "" ) then 
    ls -ltr  *diff > test_amps.res                                              #
    echo '===============================================' >> test_amps.res     # 
    head -100 *diff >> test_amps.res                                            #
  else
    touch test_amps.res
  endif 

  if ("$HOST" != "passion") then
    scp test_amps.res test_amps.log ${Server}:/homedata/vtenishe/AMPSTEST/Sites/Current/amps-gpu_intel/  #
  else
    scp test_amps.res test_amps.log /home/vtenishe/AMPSTEST/Sites/Current/amps-gpu_intel/  #
  endif

  mv test_amps.log.bak test_amps.log
  cd ../..                                                                   
endif 

# PGI compiled test
#>PGIAll #####################################################################
if (-e PGI) then
  cd PGI/AMPS                                                                 #
  cp test_amps.log test_amps.log.bak
  #./utility/TestScripts/CheckTests.pl

  echo "Processing PGI/AMPS"

  if (-e test_amps_thread1.log) then
    foreach f (test_amps_thread*.log) #
      cat $f >> test_amps.log
    end
  endif

  rm -rf test_amps.res

  set nonomatch

  set val=( *diff)

  if ("$val" != "" ) then
    ls -ltr  *diff > test_amps.res                                              #
    echo '===============================================' >> test_amps.res     # 
    head -100 *diff >> test_amps.res                                            #
  else
    touch test_amps.res
  endif

  if ("$HOST" != "passion") then
    scp test_amps.res test_amps.log ${Server}:/homedata/vtenishe/AMPSTEST/Sites/Current/amps-gpu_pgi/  #
  else
    scp test_amps.res test_amps.log /home/vtenishe/AMPSTEST/Sites/Current/amps-gpu_pgi/  #
  endif

  mv test_amps.log.bak test_amps.log
  cd ../..                                                                   
endif

# CUDA compiled test
#>CUDAAll #####################################################################
if (-e CUDA) then
  cd CUDA/AMPS                                                                 #
  cp test_amps.log test_amps.log.bak
  #./utility/TestScripts/CheckTests.pl

  echo "Processing CUDA/AMPS"

  if (-e test_amps_thread1.log) then
    foreach f (test_amps_thread*.log) #
      cat $f >> test_amps.log
    end
  endif

  rm -rf test_amps.res

  set nonomatch

  set val=( *diff)

  if ("$val" != "" ) then
    ls -ltr  *diff > test_amps.res                                              #
    echo '===============================================' >> test_amps.res     #
    head -100 *diff >> test_amps.res                                            #
  else
    touch test_amps.res
  endif

  if ("$HOST" != "passion") then
    scp test_amps.res test_amps.log ${Server}:/homedata/vtenishe/AMPSTEST/Sites/Current/amps-gpu_cuda/  #
  else
    scp test_amps.res test_amps.log /home/vtenishe/AMPSTEST/Sites/Current/amps-gpu_cuda/  #
  endif

  mv test_amps.log.bak test_amps.log
  cd ../..
endif
