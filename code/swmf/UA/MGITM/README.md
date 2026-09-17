Mars Global Ionosphere-Model
============================

3D GCM of Mars atmosphere from the surface to 300 km aimed
at solving for the dynamics of the upper atmosphere.

Installation
------------
In order to run M-GITM, you should have the following packages installed:
- openmpi-bin
- openmpi-common
- libopenmpi-dev
- make
- perl

Additionally, you need a fortran compiler. On personal computers, it is common to run using 
- gfortran

While on NASA computers, 
- ifort
is typically used.

 With these packages installed, M-GITM can be installed with:
`Config.pl -install -mars [-compiler=compiler]`

    The configuration script is capable of detecting the appropriate
    compiler on some systems. However, you may opt to specify the
    appropriate compiler (e.g. -compiler=gfortran)

> One common installation error can happen if perl isn't setup to search . for libraries.
> If you see such an error, ensure that you have PERL5LIB=. set in your shell startup file.


Compilation
-----------
From the installation directory, run:

`make`

Linking
-------
Run:
`make rundir`

to create the *run directory* and link the GITM executable and input files. The files that are needed to run GITM are held in *run* and
this directory is used, in part, so that multiple instances of GITM can be run
simultaneously with different inputs. Since all files necessary to
run the code are contained with in *run*, it is fine to change the
name or location of this directory once it is created.

1D vs. 3D Mode
--------------
GITM can be run in 1D or 3D mode. In order to change from one mode to another, two files need to be modified.

1. src/ModSize.f90: This file is part of the source code so when it is changed, the code must be recompiled. After installation, this
file is set up for 3D mode. To change it to 1D, the variables `nLons` and `nLats` should both a value of 1.
2. run/UAM.in: This is the input file and is changed regularly. It
also defaults to 3D mode. To change it to 1D, the inputs for
`lons` and `lats` under `#GRID` should be set to 1 and the minimum and maximum latitude values should be the same.

Uninstalling
------------
Config.pl -uninstall

This command deletes all object files created upon compilation and
removes links that were created during the initial installation
and compilation.

.gitignore
---------
Installing and compiling results in files that are created that
are system dependent and should not be tracked in the git repository.
Such files are listed in the `.gitignore` file in the installation directory.

GitHub
------

For more information on using GitHub with this project, see: https://docs.google.com/document/d/1ZxupN-aDTWsYAqoM0ey6Q5T7bdTqQNhEk__6_Mm1txM/edit?usp=sharing
