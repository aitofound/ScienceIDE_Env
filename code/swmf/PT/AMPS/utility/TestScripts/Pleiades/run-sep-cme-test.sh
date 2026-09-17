#!/bin/csh

cd /nobackup/vtenishe/SWMF_data
git pull

mkdir -p /nobackup/vtenishe/Tmp_AMPS_Test
cd /nobackup/vtenishe/Tmp_AMPS_Test
rm -rf SWMF

git clone --depth 1 git@github.com:SWMFsoftware/SWMF

cd SWMF
ln -s /nobackup/vtenishe/SWMF_data .
./Config.pl  -sleep=10 -install -compiler=gfortran,gcc_mpicc

module unload comp-intel
module load mpi-hpe
make -f PT/AMPS/Makefile.test test_SEP-CME--focused_transport--3D_compile

rm -rf *.diff
cp ~/bin/job-sep-cme-test .
qsub job-sep-cme-test
