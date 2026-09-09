Getting started
===============

The simplest way to get started is to run EPREM on a single processor: 

.. code-block:: bash

  eprem-latest cone.ini

This test run will complete in a matter of a few minutes, providing output of the elapsed time as the run is underway. 

If you compiled EPREM with the mpi option (default) and your system supports it you can also run: 

.. code-block:: bash

  mpirun -n 2 eprem-latest cone.ini

Note that EPREM requires only one argument - the name of an input file, which defines all the necessary parameters to complete the run. 

In the sections below, we describe the parameters in more detail, providing 
guidance on which ones should and should not be changed (or at least attempting 
to anticipate which might be useful for your research). 

You can choose to start from one of the example inputs, if that scenario 
resembles the type of investigation you're undertaking, or you can start from 
scratch and use the ``template.ini`` file as your basis. 

Here, we'll work from the template.ini file and step through each of the 
parameters. 

The parameters are separated into nine broad categories: Grid parameters, 
physical parameters, source population parameters, ideal shock parameters, 
background solar wind parameters, particle species parameters, point observer 
parameters, IO parameters, and advanced options and parameters. These are 
self-explanatory, including the last category, which, although user-modifiable, 
can generally be left alone. 

Grid Parameters
---------------

Within the first category, the user can set:

* Number of rows per face
* Number of columns per face
* Number of nodes per stream
* Number of energy levels
* Number of pitch angle steps

The number of rows per face (numRowsPerFace) specifies the XXX.

The number of columns per face (numColumnsPerFace) specifies the XXX

The number of nodes per stream (numNodesPerStream) specifies the XXX

The number of energy levels (numEnergySteps) specifies the XXX

The number of pitch angle steps (numMuSteps) specifies the XX) specifies the
XXX.


