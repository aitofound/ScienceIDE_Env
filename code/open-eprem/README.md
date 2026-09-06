# EPREM

The Energetic Particle Radiation Environment Model (EPREM) simulates acceleration and transport of energetic particles throughout the heliosphere by modeling the focused transport equation on a Lagragian grid in the frame co-moving with the solar wind. Its primary output product is a set of observer files, with each file representing a stream of linked nodes. Each node represents an element of the Lagrangian grid. It knows its spatial coordinates as functions of time, and records the local time history of plasma quantities (namely, the velocity field, magnetic field, and density) and the distribution of one or more species of energetic particles. It also contains static arrays of mass and charge, as well as speed and energy bins, for each particle species.

EPREM originated as the Energetic Particle Radiation Environment *Module* of the [Earth-Moon-Mars Radiation Environment Module framework](https://emmrem.unh.edu/index.html). You can read about the details in [this Space Weather article](https://ui.adsabs.harvard.edu/abs/2010SpWea...8.0E02S/abstract). It has since grown into an independent tool for modeling particle acceleration and transport through the heliosphere.

## Installation

### For general use

Download the latest [release](https://gitlab.com/open-eprem/eprem/releases)

Unpack the file
```
tar -zxvf eprem-X.Y.Z.tar.gz
```
after replacing `X.Y.Z` with the actual version number.

Proceed to **Setup**

### For development

Clone the repository
```
git clone https://gitlab.com/open-eprem/eprem.git
```

Proceed to **Setup**

### Setup

The simplest way to get started with EPREM is by running `setup.sh` and `build.sh`. This section will describe some common usage of those scripts. Type `setup.sh --help` and `./build.sh --help` for the most up-to-date information on their available options.

The first time through, you will likely need to install the dependencies. To do so, pass the `--download-deps` option to `setup.sh`; for future installations, you will then be able to pass `--with-deps-dir=deps`. The `--download-deps` parameter accepts an optional argument that will set the name of the subdirectory containing dependencies. If you choose to install dependencies in a non-default subdirectory, you will need to pass that same argument to `--with-deps-dir`.

Suppose have downloaded the dependencies and you want to configure EPREM for installation in a directory named `new-build`. You should first run
```
$ ./setup.sh --alias=new-build
```
followed by
```
$ ./build.sh --alias=new-build
```
Those commands will configure, build, and install the executable file `new-build/bin/eprem`. The reason for separating `./setup.sh` and `./build.sh` commands is to allow development-minded users to install once and repeatedly build. Note that if you omit the `--alias=...` argument to `setup.sh`, it will attempt to construct an appropriate alias from the name of the current git branch and other relevant parameters (as described in the `setup.sh` help text). You must then pass this name via `--alias` to `build.sh`.

More nuanced configuration is possible via the `--with-<package>-dir=...` options, with one for each dependency. You may also install dependencies in your executable path (e.g., `$PATH` on a *nix system). For example, to let EPREM use system installations of [libconfig]([libconfig](http://hyperrealm.github.io/libconfig/)) and [NetCDF4](https://www.unidata.ucar.edu/software/netcdf/) on a Debian-based system, you may run
```
$ sudo apt install mpich
$ sudo apt install libconfig-dev
$ sudo apt install libnetcdf-dev
```

If you are working with the development version, you'll also need to install the latest version of `autoconf`:
```
$ sudo apt install autoconf
```

Note that `--download-deps` will not download or install MPI. In the event that there is no MPI distribution in your `$PATH`, you will need to provide the `--with-mpi-dir=...` argument.

Users familiar with the GNU Autotools are welcome to bypass `setup.sh` + `build.sh` and directly run
```
./configure OPTIONS && make && make install
```
with whichever `OPTIONS` they require.

Finally, to verify that the installation was successful, run one of the input files in `examples` by passing it as the sole argument to `eprem`. Using the previous example build:
```
cd new-build
mpirun -n 2 bin/eprem ../examples/cone.ini 
```

## Contributions

The maintainers of this repository welcome contributions of any kind, from users with any level of experience. We especially encourage initial contributions from new users in the form of documentation fixes (e.g., spelling errors or formatting edits).

Do you think you found a bug in EPREM? You may be on the way to making a very helpful contribution!

Worried about accidentally changing the code in a way that makes us mad at you forever? No need! The merge-request system is designed to prevent that. See [GitLab's documentation on merge requests](https://docs.gitlab.com/ee/user/project/merge_requests/) for help. If you're familiar with GitHub pull requests, you'll probably feel comfortable creating a merge request.
