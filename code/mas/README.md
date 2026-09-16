<img height=125 src="doc/mas_logo.png" alt="MAS"/> <img height=125 src="doc/mas_logo_text.png" alt="MAS"/>
  
# Magnetohydrodynamic Algorithm outside a Sphere  

[Predictive Science Inc.](https://www.predsci.com)  
  
--------------------------------  
  
## OVERVIEW ##

<img align="right" height=200 src="doc/mas_bast_vis_sml.png" alt="MAS"/>MAS is a [Fortran](https://fortran-lang.org) code that integrates the time-dependent resistive thermodynamic magnetohydrodynamic (MHD) equations in three-dimensional spherical coordinates and is used extensively to compute models of coronal structure, coronal dynamics, and coronal mass ejections out to the Earth. It is the primary MHD model in [CORHEL](https://ccmc.gsfc.nasa.gov/models/CORHEL-MAS_WSA_ENLIL~5.0) (Corona-Heliosphere), a suite of models for describing the solar corona and inner heliosphere and [CORHEL-CME](https://ccmc.gsfc.nasa.gov/models/CORHEL-CME~1), an interface-based system for modeling CME eruptions from the Sun to Earth, both of which are available for public runs-on-demand at NASA's [Community Coordinated Modeling Center (CCMC)](https://ccmc.gsfc.nasa.gov).  
  
For more information on MAS, including model/computational details and references, see [here](https://predsci.com/mas).  
  
MAS can be used with MacOS, Linux, and Windows (through [WSL](https://github.com/microsoft/WSL)) on CPUs and NVIDIA GPUs. 
  
### Please Read First: ###  
MAS is a complex code with many years of development and contains numerous input parameters, use cases, experimental features, deprecated features, etc.  We recommend that, if possible, MAS should be used through the public interfaces hosted at the CCMC - specifically the [CORHEL](https://ccmc.gsfc.nasa.gov/models/CORHEL-MAS_WSA_ENLIL~5.0) and [CORHEL-CME](https://ccmc.gsfc.nasa.gov/models/CORHEL-CME~1) interfaces.  These allow users to run MAS for steady-state coronal and heliospheric MHD relaxations, zero-beta flux rope relaxations and eruptions, and full MHD CME eruptions from the Sun to Earth.  The MAS code provided here will become part of future CORHEL-like open-source releases as a sub-module.  Until then, it is primarily intended for reference purposes and/or for advanced users/developers.  
  
==> Please contact PSI at support@predsci.com if you plan to use this code for research purposes. <==


--------------------------------
  
## HOW TO INSTALL MAS ##
  
### Compilers ###
MAS has been tested to work with the following compilers:  
  
 - GCC's `gfortran` v14.2.0  
 - NVIDIA's `nvfortran` v26.1 (both CPU and GPU)  
 - INTEL's `ifx` v2026.1.0  
  
NOTE: MAS does not work on NVIDIA GPUs with nvfortran 25.5 or 25.9, please use 25.7 or >=26.1.  
  
### Dependencies ###
MAS requires the [HDF5](https://www.hdfgroup.org/solutions/hdf5) library and an MPI library (e.g. [OpenMPI](https://www.open-mpi.org), [MPICH](https://www.mpich.org), etc.).

### Build Instructions ###
Create or select a build configuration file in the `./conf` folder (see the included examples in `./conf`).  
Build the code with:
```
> ./build.sh ./conf/<CONF_FILE>.conf
```
where `<CONF_FILE>.conf` is your created/chosen configuration file.  
 
  
After the build, you can add the MAS code and tools into your `bash` path by sourcing the included helper script:  
```
> . load_mas_env.sh
```
### Run the Testsuite ###
  
To ensure the installation was successful, a testsuite is provided in the `testsuite` folder.  
To run the testsuite:
```
> cd testsuite
> ./run_mas_testsuite.sh
```
The available options can be viewed by running `./run_mas_testsuite.sh -h`.  
  
--------------------------------
  
## HOW TO RUN MAS ##

### Setting Input Options ###

MAS uses a Fortran [namelist](https://cyber.dabamos.de/programming/modernfortran/namelists.html) text input file to set all non-default parameters of a run.  
  
### Example Runs ###
  
In the `examples` folder, we provide inputs for several use cases of MAS.  

### Launching the Code ###
  
To run `MAS`, create an input namelist text file, then copy or link the `mas` executable into the same directory as the input file and run the command:  
```
> <MPI_LAUNCHER> -np <N> ./mas <RUN_NAME> <INPUT_FILE> 
```
where `<RUN_NAME>` is a chosen run name, `<INPUT_FILE>` is the created input file, `<N>` is the total number of MPI ranks to use (typically equal to the number of CPU cores or number of GPUs) and `<MPI_LAUNCHER>` is your MPI run command (e.g. `mpiexec`,`mpirun`, `ibrun`, `srun`, etc).  
For example:  
```
> mpiexec -np 1024 ./mas my_first_run mas.in
```  
  
--------------------------------  
  
[Predictive Science Inc.](https://www.predsci.com)   

--------------------------------  

