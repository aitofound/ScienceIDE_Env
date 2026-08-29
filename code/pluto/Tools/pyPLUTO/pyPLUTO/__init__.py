# -*- coding: utf-8 -*-
import os
from . import pload
from . import ploadparticles
from . import Tools
from . import Image


def nlast_info(w_dir=None,datatype=None):
	""" 
	Prints the information of the last step of the simulation as obtained from out files

	**Inputs**:
	
	  w_dir -- path to the directory which has the dbl.out(or flt.out) and the data\n
	  datatype -- If the data is of 'float' type then datatype = 'flt' else by default the datatype is set to 'dbl' (Double precision).

    **Outputs**:
	
	  This function returns a dictionary with following keywords - \n

	  nlast -- The ns for the last file saved.\n
	  time -- The simulation time for the last file saved.\n
	  dt -- The time step dt for the last file. \n
	  Nstep -- The Nstep value for the last file saved.


	**Usage**:
	
	  In case the data is 'float'.

	  ``wdir = /path/to/data/directory``\n
	  ``import pyPLUTO as pp``\n
	  ``A = pp.nlast_info(w_dir=wdir,datatype='float')``	
	"""

	if w_dir is None: w_dir = os.getcwd()+'/'
	if datatype == 'flt':
		fname_v = w_dir+"flt.out"
	elif datatype == 'vtk':
		fname_v = w_dir+"vtk.out"
	else:
		fname_v = w_dir+"dbl.out"

	with open(fname_v, 'r') as f:
		lines = f.read().splitlines()
		last_line = lines[-1].split()
		
	nlast = int(last_line[0])
	SimTime =  float(last_line[1])
	Dt = float(last_line[2])
	Nstep = int(last_line[3])
	    
	print("------------TIME INFORMATION--------------")
	print('nlast = %d'%nlast)
	print('time  =%f'%SimTime)
	print('dt    =%f'%Dt)
	print('Nstep =%d'%Nstep)
	print("-------------------------------------------")
	    
	return {'nlast':nlast,'time':SimTime,'dt':Dt,'Nstep':Nstep}
    

