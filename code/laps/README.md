<p align="center">
  <img src="logo.png" alt="LAPS Logo" width="200"/>
</p>

# LAPS

LAPS (UC**LA**-**P**seudo-**S**pectral) is a 3D MPI-parallelized Fourier-transform-based pseudo-spectral Hall-MHD code writen in FORTRAN, with corotating-expanding-box-model implemented. 

Brief notes on the numerical method are included as a PDF file: /src_compressible/notes_MHD3D_EBM.pdf. Some more detailed description can be found in the paper: https://iopscience.iop.org/article/10.3847/1538-4357/ab5fce. A thorough description of all the numerical methods along with fundamental tests can be found in the paper: https://www.frontiersin.org/articles/10.3389/fspas.2024.1412905/full.

---

**IMPORTANT**: If you use the code to generate any **publications**, including preprint articles, you should refer to this repository (https://github.com/chenshihelio/LAPS) in the acknowledgement and cite the following articles:

- Shi, C., Tenerani, A., Rappazzo, A. F., and Velli, M., _LAPS: An MPI-parallelized 3D pseudo-spectral Hall-MHD simulation code incorporating the expanding box model_ (2024), Frontiers in Astronomy and Space Sciences, 11, 1412905, https://www.frontiersin.org/articles/10.3389/fspas.2024.1412905/full
- Shi, C., Velli, M., Tenerani, A., et al., _Propagation of Alfvén waves in the expanding solar wind with the fast--slow stream interaction_ (2020), The Astrophysical Journal, 888, 2, 68, https://iopscience.iop.org/article/10.3847/1538-4357/ab5fce
  
In addition, you should contact the developer Dr. Chen Shi at (cshi1993@ucla.edu).

## Installation & Compilation

To compile the code, MPI and FFTW are necessary. 

### OpenMPI
You can download the latest OpenMPI from https://www.open-mpi.org/software/ompi/v5.0/. The instruction to install OpenMPI can be found here: https://docs.open-mpi.org/en/v5.0.x/installing-open-mpi/index.html.

### FFTW
You can download FFTW from https://www.fftw.org/download.html and install it follow the instruction here https://www.fftw.org/fftw3_doc/Installation-and-Customization.html.

### Compile LAPS
After installing MPI and FFTW, LAPS is ready to be compiled. From terminal, go to the directory of the desired version of the code, i.e. "_/src_compressible_" for 3D compressible version, "_/src_compressible/2D_" for 2D compressible version, "_/src_incompressible_" for 3D incompressible version, and "_/src_incompressible/2D_" for 2D incompressible version. Then run 
> make

The only thing that you may need to modify in "**makefile**" is "**fftwpath**", which by default is "**/usr/local**". You need to change it to the location your FFTW is installed. In addition, depending on the specific FORTRAN compiler you use, you may need to change the "**-r8**" flag in "**OPTIONS**" to "**-fdefault-real-8**". If successfully compiled, a "**mhd.exe**" will be generated.


## Run LAPS

Move "**mhd.exe**" and "**mhd.input**" to the directory in which you want to run the code. All the outputs will be saved in the same directory. Then use the command
> mpirun -np 8 mhd.exe > rec &

to run the code. Here "**-np 8**" specifies that 8 cores will be used, "**> rec &**" tells the program to run in the backend and save all the terminal output to the file "**rec**".

### mhd.input
The "**mhd.input**" is the FORTRAN namelist file. It contains all the input parameters, including number of grid points, domain sizes, CFL condition constant to determine time step size, method for de-aliasing, adiabatic index, resistivity, etc. Most of the parameters have obvious meanings. In addition, the namelist "**&field**" and "**&pert**" specifies the selected case numbers ("**ifield**" and "**ipert**") and associated parameters for the initial condition. 

### mhdinit.f90
Typically, the only ".f90" file that the users need to modify is "mhdinit.f90". In this file, there are two subroutines, namely "subroutine background_fields_initialize" and "subroutine perturbation_initialize", which define the initial background field and initial perturbation respectively. In each of the two subroutines, we have defined six parameters "**ixmin, ixmax, iymin, iymax, izmin, izmax**" which are the ranges of indices of the main array **uu(ixmin:ixmax,iymin:iymax,izmin:izmax,1:8)**. The fourth dimension of **uu**, from 1 to 8, refers to "**rho, Ux, Uy, Uz, Bx, By, Bz, P**" respectively. Note that at initialization, we use the regular MHD fields instead of conserved quantities for convenience. The users may also need to use the spatial coordinates **xgrid(ixmin:ixmax), ygrid(iymin:iymax), zgrid(izmin:izmax)**. Note that, the domain is defined as [0,Lx]x[0,Ly]x[0,Lz].

Sometimes the users may want to modify the **&field** and **&pert** namelists to add their own parameters. These namelists are defined in the main program "**mhd.f90**".


## Read output

Samples of Python scripts to read the output are included.

## TODO

1. Build a better user manual.
2. Implement different time-integral methods.
3. Implement expanding-box-model to the incompressible code.


<hr>
Copyright 2024 Chen Shi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
