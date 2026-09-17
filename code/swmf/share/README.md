share
=====

SWMF's scripts and shared files repository. Here you will find scripts and
libraries that are shared between the different parts of the Space Weather
Modeling Framework.

Structure
---------

```bash
$ tree
.
├── build
│   ├── Makefile.*
├── IDL
│   ├── doc
│   ├── General
│   ├── Ionosphere
│   └── Solar
├── JobScripts
│   └── job.*
├── Julia
│   └── README.md
├── Library
│   ├── src
│   └── test
├── LICENSE.txt
├── Makefile
├── MATLAB
│   └── README.md
├── Prologs
│   ├── internal_subroutine.f90
│   ├── Makefile
│   ├── module.f90
│   ├── program.f90
│   ├── README.tex
│   └── subroutine.f90
├── Python
│   ├── filecache
│   ├── install-swmfpy
│   ├── pyfits
│   ├── Scripts
│   └── swmfpy
├── README.md
├── Scripts
20 directories, 142 files
```

### build

This folder stores Makefiles for the different systems and OS's as well as
different configuration styles.

### IDL

Environment and scripts to use IDL with SWMF output.

### JobScripts

The job submission files for different supercomputers. The filename style here
is `job.<hostname>` where `<hostname>` is the prefix hostname of a
supercomputer. For example `job.pfe` will work with a supercomputer hostname
`pfe21`.

This files get automatically copied onto the `run` folder once you build SWMF.

Feel free to add default and well documented job submission scripts for
supercomputers that you are familiar with and is missing.

### Library

[Library/src](Library/src) contains header files that are shared amongst different parts of
SWMF. For example plotting or wrapper libraries.

### Python

Here you can find a collection of python scripts, specifically in
[Python/Scripts](Python/Scripts).
