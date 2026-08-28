#!/usr/bin/python3
from __future__ import division
import os
import sys
import numpy as np

class ploadparticles(object):
	def __init__(self, ns, w_dir=None, datatype=None, ptype=None, chnum=None):
		"""
		Loads the Particle datafile.

		**Inputs**:
		ns 			 -- Step Number of the data file\n
		w_dir 	 -- path to the directory which has the data files\n
		datatype -- Datatype (default is set to read .dbl data files)
		ptype 	 -- A string denoting the type of particles ('LP', 'CR', 'DUST' etc. Default is 'CR')
		chnum    -- 2 digit integer denoting chunk number
		(Only used if ptype = 'LP' to read particles.nnnn_chxx.dbl file, where nnnn is 4 digit integer denotong ns and xx is a 2 digit integer for chunk number : Default value is 0)
		
		**Outputs**:
		pyPLUTO pload object whose keys are arrays of data values.

		"""
		self.Nstep = ns
		
		
		if w_dir is None:
			w_dir = os.getcwd() + '/'
		
		self.wdir = w_dir

		if datatype is None:
			datatype = "dbl"
		self.datatype = datatype
		if (self.datatype == "vtk"):
			print("Particle VTK File is not yet supported. Choose either dbl or flt")
			sys.exit()
		
		if ptype == 'LP':
			if chnum is None : chnum=0
			self.fname = self.wdir+"particles.%04d_ch%02d.%s"%(ns, chnum, self.datatype)
		else:
			self.fname = self.wdir+"particles.%04d.%s"%(ns, self.datatype)
			
		
		Part_dictionary = self.ReadParticleFile()
		for keys in Part_dictionary:
			object.__setattr__(self, keys, Part_dictionary.get(keys))

	def ReadParticleFile(self):
		print("Reading particle file : %s"%self.fname)
		fp = open(self.fname,'rb')
		#GET THE HEADER ROUTINE HERE
		h_lines = 0
		header = []
		while (True):
			ln = fp.readline()
			lns = ln.split()
			if lns[0] == b'#':
				if lns[1] != b'PLUTO': header.append((lns[1].decode('utf8'), [i.decode('utf8') for i in lns[2:]]))
			else:
				break
			h_lines += 1
		self.Head_Dict = dict(header)
		fp.close()
		
		# SKIP HEADER LINES
		cnt = 0
		fp = open(self.fname,'rb')
		while (cnt < h_lines):
			fp.readline()
			cnt += 1

		data_ = fp.read()
		#Create Data type tuple
		
		data_typ_tup = []
		for i in range(int(self.Head_Dict['nfields'][0])):
			if self.datatype == 'flt':
				data_typ_tup.append((self.Head_Dict['field_names'][i], np.float32, int(self.Head_Dict['field_dim'][i])))
			else:
				data_typ_tup.append((self.Head_Dict['field_names'][i], np.double, int(self.Head_Dict['field_dim'][i])))
	
		dt = np.dtype(data_typ_tup)
		
		val_ = np.frombuffer(data_,dtype=dt)
		val_dict = {name:val_[name] for name in self.Head_Dict['field_names']}
		return val_dict

