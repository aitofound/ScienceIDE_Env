Installing
==========

Requirements
------------
To get eprem up and running on a \*nix-based system (only fully tested on
Debian-based systems (22.04)), you'll need to follow the following steps. 

First, close a copy of the repository: 

.. code-block:: bash

  git clone https://github.com/myoung-space-science/eprem.git

Then configure, build, and install EPREM (as an executable) in your home
directory, run:

.. code-block:: bash

  ./setup.sh --install -- --prefix=$HOME

Note that the -- before --prefix is necessary to tell setup.sh that you wish 
to pass the --prefix argument to configure.sh. In fact, this is true of any 
argument that configure.sh accepts (see ./configure --help).

Although setup.sh intends to get you up and running as quickly as possible, 
you will likely need to specify some configuration options. In particular, 
you will need to point configure.sh to installations of libconfig and NetCDF4 
if they are not already in your $PATH. To do so, provide the --with-libconfig-dir=... 
and --with-netcdf-dir=... arguments. In the less likely event that there is no MPI 
distribution in your $PATH, you will need to provide the --with-mpi-dir=... argument.

If your system is 'vanilla' (e.g., a new installation) and doesn't have support for autoconf, 
MPI, libconfig, or netCDF, you'll need to add them. For Debian-based systems 
(tested with 22.04 Ubuntu), you can run the following commands:

.. code-block:: bash

  sudo apt-get install autoconf
  sudo apt install mpich
  sudo apt install libconfig-dev
  sudo apt install libnetcdf-dev

EPREM currently does not support serial operation (though it is possible in certain 
circumstances); configure.sh will do its best to find suitable MPI compilers without 
the need for explicitly setting CC=.. and CXX=..., but if set-up fails, you may try doing so.

Users familiar with the GNU Autotools are welcome to bypass setup.sh and directly run

If you want to install the bleeding edge version, change to the directory you
want to download the source code too, and run:

.. code-block:: bash

  ./configure OPTIONS && make && make install

Testing
=======

Finally, to verify that the installation was successful, run one of the examples, such as:

.. code-block:: bash

  mpirun -n 2 eprem-latest cone.ini 
