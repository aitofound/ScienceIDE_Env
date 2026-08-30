from __future__ import division
import os
import sys
import array
import numpy as np
from . import Tools

####### Check for h5py to Read AMR data ######
try:
	import h5py as h5
	hasH5 = True
except ImportError:
	hasH5 = False
	
class pload(object):
	def __init__(self, ns, w_dir=None, datatype=None, level = 0, x1range=None, x2range=None, x3range=None):
		"""
		Loads the data.
		**Inputs**:
			ns -- Step Number of the data file\n
			w_dir -- path to the directory which has the data files\n
			datatype -- Datatype (default is set to read .dbl data files)
			level -- Integer value to represent AMR level (default = 0)
			x1range -- List with min and max value of x1 coordinates for zooming (applicable for AMR data)
			x2range -- List with min and max value of x2 coordinates for zooming (applicable for AMR data)
			x3range -- List with min and max value of x3 coordinates for zooming (applicable for AMR data)

		**Outputs**:
			pyPLUTO pload object whose keys are arrays of data values.
		"""
		self.NStep = ns
		self.Dt = 0.0

		self.n1 = 0
		self.n2 = 0
		self.n3 = 0

		self.x1 = []
		self.x2 = []
		self.x3 = []
		self.dx1 = []
		self.dx2 = []
		self.dx3 = []
		self.x1range = x1range
		self.x2range = x2range
		self.x3range = x3range
		self.NStepStr = str(self.NStep)
		while len(self.NStepStr) < 4:
			self.NStepStr = '0'+self.NStepStr

		if datatype is None:
			datatype = "dbl"
		self.datatype = datatype
		
		if ((not hasH5) and (datatype == 'hdf5')):
			print('To read AMR hdf5 files with python')
			print('Please install h5py (Python HDF5 Reader)')
			return
		self.level = level
		
		if w_dir is None:
			w_dir = os.getcwd() + '/'
		self.wdir = w_dir

		Data_dictionary = self.ReadDataFile(self.NStepStr)

		for keys in Data_dictionary:
			object.__setattr__(self, keys, Data_dictionary.get(keys))

	def ReadTimeInfo(self, timefile):
		""" 
		Read time info from the outfiles.
		**Inputs**:
			timefile -- name of the out file which has timing information.
		"""
		if (self.datatype == 'hdf5'):
			fh5 = h5.File(timefile,'r')
			self.SimTime = fh5.attrs.get('time')
			#self.Dt = 1.e-2 # Should be erased later given the level in AMR
			fh5.close()
		else:
			ns = self.NStep
			f_var = open(timefile, "r")
			tlist = []
			for line in f_var.readlines():
				tlist.append(line.split())
			self.SimTime = float(tlist[ns][1])
			self.Dt = float(tlist[ns][2])

	def ReadVarFile(self, varfile):
		""" 
		Read variable names from the outfiles.
		**Inputs**:
			varfile -- name of the out file which has variable information.
		"""
		if (self.datatype == 'hdf5'):
			fh5 = h5.File(varfile,'r')
			self.filetype = 'single_file'
			self.endianess = '>' # not used with AMR, kept for consistency
			self.vars = []
			for iv in range(fh5.attrs.get('num_components')):
					self.vars.append(fh5.attrs.get('component_'+str(iv)))
			fh5.close()
		else:
			vfp = open(varfile, "r")
			varinfo = vfp.readline().split()
			self.filetype = varinfo[4]
			self.endianess = varinfo[5]
			self.vars = varinfo[6:]
			vfp.close()

	def ReadGridFile(self, gridfile):
		""" Read grid values from the grid.out file.
		**Inputs**:
			gridfile -- name of the grid.out file which has information about the grid.
		"""
		xL = []
		xR = []
		nmax = []
		gfp = open(gridfile, "r")
		for i in gfp.readlines():
			if len(i.split()) == 1:
				try:
					int(i.split()[0])
					nmax.append(int(i.split()[0]))
				except:
					pass
					
			if len(i.split()) == 3:
				try:
					int(i.split()[0])
					xL.append(float(i.split()[1]))
					xR.append(float(i.split()[2]))
				except:
					if (i.split()[1] == 'GEOMETRY:'):
						self.geometry=i.split()[2]
					pass
							
		self.n1, self.n2, self.n3 = nmax
		n1 = self.n1
		n1p2 = self.n1 + self.n2
		n1p2p3 = self.n1 + self.n2 + self.n3
		self.x1 = np.asarray([0.5*(xL[i]+xR[i]) for i in range(n1)])
		self.dx1 = np.asarray([(xR[i]-xL[i]) for i in range(n1)])
		self.x2 = np.asarray([0.5*(xL[i]+xR[i]) for i in range(n1, n1p2)])
		self.dx2 = np.asarray([(xR[i]-xL[i]) for i in range(n1, n1p2)])
		self.x3 = np.asarray([0.5*(xL[i]+xR[i]) for i in range(n1p2, n1p2p3)])
		self.dx3 = np.asarray([(xR[i]-xL[i]) for i in range(n1p2, n1p2p3)])


		# Stores the total number of points in '_tot' variable in case only
		# a portion of the domain is loaded. Redefine the x and dx arrays
		# to match the requested ranges
		self.n1_tot = self.n1 ; self.n2_tot = self.n2 ; self.n3_tot = self.n3
		if (self.x1range != None):
			self.n1_tot = self.n1
			self.irange = range(abs(self.x1-self.x1range[0]).argmin(),abs(self.x1-self.x1range[1]).argmin()+1)
			self.n1  = len(self.irange)
			self.x1  = self.x1[self.irange]
			self.dx1 = self.dx1[self.irange]
		else:
			self.irange = range(self.n1)
		if (self.x2range != None):
			self.n2_tot = self.n2
			self.jrange = range(abs(self.x2-self.x2range[0]).argmin(),abs(self.x2-self.x2range[1]).argmin()+1)
			self.n2  = len(self.jrange)
			self.x2  = self.x2[self.jrange]
			self.dx2 = self.dx2[self.jrange]
		else:
			self.jrange = range(self.n2)
		if (self.x3range != None):
			self.n3_tot = self.n3
			self.krange = range(abs(self.x3-self.x3range[0]).argmin(),abs(self.x3-self.x3range[1]).argmin()+1)
			self.n3  = len(self.krange)
			self.x3  = self.x3[self.krange]
			self.dx3 = self.dx3[self.krange]
		else:
			self.krange = range(self.n3)
		self.Slice=(self.x1range != None) or (self.x2range != None) or (self.x3range != None)


		# Create the xr arrays containing the edges positions
		# Useful for pcolormesh which should use those
		self.x1r = np.zeros(len(self.x1)+1) ; self.x1r[1:] = self.x1 + self.dx1/2.0 ; self.x1r[0] = self.x1r[1]-self.dx1[0]
		self.x2r = np.zeros(len(self.x2)+1) ; self.x2r[1:] = self.x2 + self.dx2/2.0 ; self.x2r[0] = self.x2r[1]-self.dx2[0]
		self.x3r = np.zeros(len(self.x3)+1) ; self.x3r[1:] = self.x3 + self.dx3/2.0 ; self.x3r[0] = self.x3r[1]-self.dx3[0]


		prodn = self.n1*self.n2*self.n3
		if prodn == self.n1:
			self.nshp = (self.n1)
		elif prodn == self.n1*self.n2:
			self.nshp = (self.n2, self.n1)
		else:
			self.nshp = (self.n3, self.n2, self.n1)


	def DataScanVTK(self, fp, n1, n2, n3, endian, dtype):
		""" Scans the VTK data files.

		**Inputs**:
		fp -- Data file pointer\n
		n1 -- No. of points in X1 direction\n
		n2 -- No. of points in X2 direction\n
		n3 -- No. of points in X3 direction\n
		endian -- Endianess of the data\n
		dtype -- datatype
		**Output**:
			Dictionary consisting of variable names as keys and its values.
		"""
		ks = []
		vtkvar = []
		while True:
			line = fp.readline()
			try:
				line.split()[0]
			except IndexError as error:
				pass
			else:
				if line.split()[0] == b'SCALARS':
					ks.append(line.decode().split()[1])
				elif line.split()[0] == b'LOOKUP_TABLE':
					A = array.array(dtype)
					fmt = endian+str(n1*n2*n3)+dtype
					nb = np.dtype(fmt).itemsize
					A.fromstring(fp.read(nb))
					if (self.Slice):
						darr = np.zeros((n1*n2*n3))
						indxx = np.sort([self.n3_tot*self.n2_tot*k + j*self.n2_tot + i for i in self.irange for j in self.jrange for k in self.krange])
						if (sys.byteorder != self.endianess):
							A.byteswap()
						for ii,iii in enumerate(indxx):
							darr[ii] = A[iii]
						vtkvar_buf = [darr]
					else:
						vtkvar_buf = np.frombuffer(A,dtype=np.dtype(fmt))
					vtkvar.append(np.reshape(vtkvar_buf,self.nshp).transpose())
				else:
					pass
			if line == b'':
				break

		vtkvardict = dict(zip(ks,vtkvar))
		return vtkvardict
					
	def DataScanHDF5(self, fp, myvars, ilev):
		""" Scans the Chombo HDF5 data files for AMR in PLUTO.
		**Inputs**:
			fp     -- Data file pointer\n
			myvars -- Names of the variables to read\n
			ilev   -- required AMR level
		**Output**:
			Dictionary consisting of variable names as keys and its values.
		**Note**:
			Due to the particularity of AMR, the grid arrays loaded in ReadGridFile are overwritten here.
		"""
		# Read the grid information
		dim = fp['Chombo_global'].attrs.get('SpaceDim')
		nlev = fp.attrs.get('num_levels')
		il = min(nlev-1,ilev)
		lev  = []
		for i in range(nlev):
			lev.append('level_'+str(i))
		freb = np.zeros(nlev,dtype='int')
		for i in range(il+1)[::-1]:
			fl = fp[lev[i]]
			if (i == il):
				pdom = fl.attrs.get('prob_domain')
				dx = fl.attrs.get('dx')
				dt = fl.attrs.get('dt')
				ystr = 1. ; zstr = 1. ; logr = 0
				try:
					geom = fl.attrs.get('geometry')
					logr = fl.attrs.get('logr')
					if (dim == 2):ystr = fl.attrs.get('g_x2stretch')
					elif (dim == 3):zstr = fl.attrs.get('g_x3stretch')
				except:
					print('Old HDF5 file, not reading stretch and logr factors')
				freb[i] = 1
				x1b = fl.attrs.get('domBeg1')
				if (dim == 1):x2b = 0
				else:x2b = fl.attrs.get('domBeg2')
				if (dim == 1 or dim == 2):x3b = 0
				else:x3b = fl.attrs.get('domBeg3')
				jbeg = 0 ; jend = 0 ; ny = 1
				kbeg = 0 ; kend = 0 ; nz = 1
				if (dim == 1):
					ibeg = pdom[0] ; iend = pdom[1] ; nx = iend-ibeg+1
				elif (dim == 2):
					ibeg = pdom[0] ; iend = pdom[2] ; nx = iend-ibeg+1
					jbeg = pdom[1] ; jend = pdom[3] ; ny = jend-jbeg+1
				elif (dim == 3):
					ibeg = pdom[0] ; iend = pdom[3] ; nx = iend-ibeg+1
					jbeg = pdom[1] ; jend = pdom[4] ; ny = jend-jbeg+1
					kbeg = pdom[2] ; kend = pdom[5] ; nz = kend-kbeg+1
			else:
				rat = fl.attrs.get('ref_ratio')
				freb[i] = rat*freb[i+1]
	
		dx0 = dx*freb[0]

		## Allow to load only a portion of the domain
		if (self.x1range != None):
			if logr == 0:self.x1range = self.x1range-x1b
			else:self.x1range = [np.log(self.x1range[0]/x1b),np.log(self.x1range[1]/x1b)]
			ibeg0 = min(self.x1range)/dx0 ; iend0 = max(self.x1range)/dx0
			ibeg  = max([ibeg, int(ibeg0*freb[0])]) ; iend = min([iend,int(iend0*freb[0]-1)])
			nx = iend-ibeg+1
		if (self.x2range != None):
			self.x2range = (self.x2range-x2b)/ystr
			jbeg0 = min(self.x2range)/dx0 ; jend0 = max(self.x2range)/dx0
			jbeg  = max([jbeg, int(jbeg0*freb[0])]) ; jend = min([jend,int(jend0*freb[0]-1)])
			ny = jend-jbeg+1
		if (self.x3range != None):
			self.x3range = (self.x3range-x3b)/zstr
			kbeg0 = min(self.x3range)/dx0 ; kend0 = max(self.x3range)/dx0
			kbeg  = max([kbeg, int(kbeg0*freb[0])]) ; kend = min([kend,int(kend0*freb[0]-1)])
			nz = kend-kbeg+1
		
		## Create uniform grids at the required level
		if logr == 0:
			x1 = x1b + (ibeg+np.array(range(nx))+0.5)*dx
		else:
			x1 = x1b*(np.exp((ibeg+np.array(range(nx))+1)*dx)+np.exp((ibeg+np.array(range(nx)))*dx))*0.5
		x2 = x2b + (jbeg+np.array(range(ny))+0.5)*dx*ystr
		x3 = x3b + (kbeg+np.array(range(nz))+0.5)*dx*zstr
		if logr == 0:
			dx1 = np.ones(nx)*dx
		else:
			dx1 = x1b*(np.exp((ibeg+np.array(range(nx))+1)*dx)-np.exp((ibeg+np.array(range(nx)))*dx))
		dx2 = np.ones(ny)*dx*ystr
		dx3 = np.ones(nz)*dx*zstr

		# Create the xr arrays containing the edges positions
		# Useful for pcolormesh which should use those
		x1r = np.zeros(len(x1)+1) ; x1r[1:] = x1 + dx1/2.0 ; x1r[0] = x1r[1]-dx1[0]
		x2r = np.zeros(len(x2)+1) ; x2r[1:] = x2 + dx2/2.0 ; x2r[0] = x2r[1]-dx2[0]
		x3r = np.zeros(len(x3)+1) ; x3r[1:] = x3 + dx3/2.0 ; x3r[0] = x3r[1]-dx3[0]
		NewGridDict = dict([('n1',nx),('n2',ny),('n3',nz),\
							('x1',x1),('x2',x2),('x3',x3),\
							('x1r',x1r),('x2r',x2r),('x3r',x3r),\
							('dx1',dx1),('dx2',dx2),('dx3',dx3),\
							('Dt',dt)])
		
		# Variables table
		nvar = len(myvars)
		vars = np.zeros((nx,ny,nz,nvar))
		LevelDic = {'nbox':0,'ibeg':ibeg,'iend':iend,'jbeg':jbeg,'jend':jend,'kbeg':kbeg,'kend':kend}
		AMRLevel = []
		AMRBoxes = np.zeros((nx,ny,nz))
		for i in range(il+1):
			AMRLevel.append(LevelDic.copy())
			fl = fp[lev[i]]
			data = fl['data:datatype=0']
			boxes = fl['boxes']
			nbox = len(boxes['lo_i'])
			AMRLevel[i]['nbox'] = nbox
			ncount = int(0)
			AMRLevel[i]['box']=[]
			for j in range(nbox): # loop on all boxes of a given level
				AMRLevel[i]['box'].append({'x0':0.,'x1':0.,'ib':int(0),'ie':int(0),\
											'y0':0.,'y1':0.,'jb':int(0),'je':int(0),\
											'z0':0.,'z1':0.,'kb':int(0),'ke':int(0)})
				# Box indexes
				ib = boxes[j]['lo_i'] ; ie = boxes[j]['hi_i'] ; nbx = ie-ib+1
				jb = 0 ; je = 0 ; nby = 1
				kb = 0 ; ke = 0 ; nbz = 1
				if (dim > 1):
					jb = boxes[j]['lo_j'] ; je = boxes[j]['hi_j'] ; nby = je-jb+1
				if (dim > 2):
					kb = boxes[j]['lo_k'] ; ke = boxes[j]['hi_k'] ; nbz = ke-kb+1
				szb = nbx*nby*nbz*nvar
				# Rescale to current level
				kb = kb*freb[i] ; ke = (ke+1)*freb[i] - 1
				jb = jb*freb[i] ; je = (je+1)*freb[i] - 1
				ib = ib*freb[i] ; ie = (ie+1)*freb[i] - 1
	
				# Skip boxes lying outside ranges
				if ((ib > iend) or (ie < ibeg) or \
					(jb > jend) or (je < jbeg) or \
					(kb > kend) or (ke < kbeg)):
					ncount = ncount + szb
				else:
					### Read data
					q = data[ncount:ncount+szb].reshape((nvar,nbz,nby,nbx)).T
				
					### Find boxes intersections with current domain ranges
					ib0 = max([ibeg,ib]) ; ie0 = min([iend,ie])
					jb0 = max([jbeg,jb]) ; je0 = min([jend,je])
					kb0 = max([kbeg,kb]) ; ke0 = min([kend,ke])
		
					### Store box corners in the AMRLevel structure
					if logr == 0:
						AMRLevel[i]['box'][j]['x0'] = x1b + dx*(ib0)
						AMRLevel[i]['box'][j]['x1'] = x1b + dx*(ie0+1)
					else:
						AMRLevel[i]['box'][j]['x0'] = x1b*np.exp(dx*(ib0))
						AMRLevel[i]['box'][j]['x1'] = x1b*np.exp(dx*(ie0+1))
					AMRLevel[i]['box'][j]['y0'] = x2b + dx*(jb0)*ystr
					AMRLevel[i]['box'][j]['y1'] = x2b + dx*(je0+1)*ystr
					AMRLevel[i]['box'][j]['z0'] = x3b + dx*(kb0)*zstr
					AMRLevel[i]['box'][j]['z1'] = x3b + dx*(ke0+1)*zstr
					AMRLevel[i]['box'][j]['ib'] = ib0 ; AMRLevel[i]['box'][j]['ie'] = ie0
					AMRLevel[i]['box'][j]['jb'] = jb0 ; AMRLevel[i]['box'][j]['je'] = je0
					AMRLevel[i]['box'][j]['kb'] = kb0 ; AMRLevel[i]['box'][j]['ke'] = ke0
					AMRBoxes[ib0-ibeg:ie0-ibeg+1, jb0-jbeg:je0-jbeg+1, kb0-kbeg:ke0-kbeg+1] = il
					
					### Extract the box intersection from data stored in q
					cib0 = (ib0-ib)//freb[i] ; cie0 = (ie0-ib)//freb[i]
					cjb0 = (jb0-jb)//freb[i] ; cje0 = (je0-jb)//freb[i]
					ckb0 = (kb0-kb)//freb[i] ; cke0 = (ke0-kb)//freb[i]
					q1 = np.zeros((cie0-cib0+1, cje0-cjb0+1, cke0-ckb0+1,nvar))
					q1 = q[cib0:cie0+1,cjb0:cje0+1,ckb0:cke0+1,:]
					
					# Remap the extracted portion
					if (dim == 1):
						new_shape = (ie0-ib0+1,1)
					elif (dim == 2):
						new_shape = (ie0-ib0+1,je0-jb0+1)
					else:
						new_shape = (ie0-ib0+1,je0-jb0+1,ke0-kb0+1)
							
					stmp = list(new_shape)
					while stmp.count(1) > 0:
						stmp.remove(1)
					new_shape = tuple(stmp)
					
					myT = Tools.Tools()
					for iv in range(nvar):
						vars[ib0-ibeg:ie0-ibeg+1,jb0-jbeg:je0-jbeg+1,kb0-kbeg:ke0-kbeg+1,iv] = \
							myT.congrid(q1[:,:,:,iv].squeeze(),new_shape,method='linear',minusone=True).reshape((ie0-ib0+1,je0-jb0+1,ke0-kb0+1))
					ncount = ncount+szb

		h5vardict={}
		for iv in range(nvar):
			myvars[iv] = myvars[iv].decode()
			h5vardict[myvars[iv]] = vars[:,:,:,iv].squeeze()
		AMRdict = dict([('AMRBoxes',AMRBoxes),('AMRLevel',AMRLevel)])
		OutDict = dict(NewGridDict)
		OutDict.update(AMRdict)
		OutDict.update(h5vardict)
		return OutDict

	
	def DataScan(self, fp, n1, n2, n3, endian, dtype, off=None):
		""" Scans the data files in all formats.
		**Inputs**:
			fp -- Data file pointer\n
			n1 -- No. of points in X1 direction\n
			n2 -- No. of points in X2 direction\n
			n3 -- No. of points in X3 direction\n
			endian -- Endianess of the data\n
			dtype -- datatype, eg : dbl, flt, vtk, hdf5\n
			off -- offset (for avoiding staggered B fields)
		**Output**:
			Dictionary consisting of variable names as keys and its values.
		"""
		if off is not None:
			off_fmt = endian+str(off)+dtype
			nboff = np.dtype(off_fmt).itemsize
			fp.read(nboff)

		n1_tot = self.n1_tot ; n2_tot = self.n2_tot; n3_tot = self.n3_tot
		A = array.array(dtype)
		fmt = endian+str(n1_tot*n2_tot*n3_tot)+dtype
		nb = np.dtype(fmt).itemsize
		A.fromstring(fp.read(nb))
		
		if (self.Slice):
			darr = np.zeros((n1*n2*n3))
			indxx = np.sort([n3_tot*n2_tot*k + j*n2_tot + i for i in self.irange for j in self.jrange for k in self.krange])
			if (sys.byteorder != self.endianess):
					A.byteswap()
			for ii,iii in enumerate(indxx):
					darr[ii] = A[iii]
			darr = [darr]
		else:
			darr = np.frombuffer(A,dtype=np.dtype(fmt))
		
		return np.reshape(darr[0],self.nshp).transpose()

	def ReadSingleFile(self, datafilename, myvars, n1, n2, n3, endian,
										 dtype, ddict):
		"""Reads a single data file, data.****.dtype.
		**Inputs**:
			datafilename -- Data file name\n
		myvars -- List of variable names to be read\n
			n1 -- No. of points in X1 direction\n
			n2 -- No. of points in X2 direction\n
			n3 -- No. of points in X3 direction\n
			endian -- Endianess of the data\n
			dtype -- datatype\n
			ddict -- Dictionary containing Grid and Time Information
			which is updated
		**Output**:
			Updated Dictionary consisting of variable names as keys and its values.
		"""
		if self.datatype == 'hdf5':
			fp = h5.File(datafilename,'r')
		else:
			fp = open(datafilename, "rb")
		print("Reading single data file : %s"%datafilename)
		
		if self.datatype == 'vtk':
			vtkd = self.DataScanVTK(fp, n1, n2, n3, endian, dtype)
			ddict.update(vtkd)
		elif self.datatype == 'hdf5':
			h5d = self.DataScanHDF5(fp,myvars,self.level)
			ddict.update(h5d)
		else:
			for i in range(len(myvars)):
				if myvars[i] == 'bx1s':
					ddict.update({myvars[i]: self.DataScan(fp, n1, n2, n3, endian,dtype, off=n2*n3)})
				elif myvars[i] == 'bx2s':
					ddict.update({myvars[i]: self.DataScan(fp, n1, n2, n3, endian,dtype, off=n3*n1)})
				elif myvars[i] == 'bx3s':
					ddict.update({myvars[i]: self.DataScan(fp, n1, n2, n3, endian,dtype, off=n1*n2)})
				else:
					ddict.update({myvars[i]: self.DataScan(fp, n1, n2, n3, endian, dtype)})
		fp.close()

	def ReadMultipleFiles(self, nstr, dataext, myvars, n1, n2, n3, endian,
												dtype, ddict):
		"""Reads a  multiple data files, varname.****.dataext.
		**Inputs**:
			nstr -- File number in form of a string\n
			dataext -- Data type of the file, e.g., 'dbl', 'flt' or 'vtk' \n
			myvars -- List of variable names to be read\n
			n1 -- No. of points in X1 direction\n
			n2 -- No. of points in X2 direction\n
			n3 -- No. of points in X3 direction\n
			endian -- Endianess of the data\n
			dtype -- datatype\n
			ddict -- Dictionary containing Grid and Time Information
			which is updated.
		**Output**:
			Updated Dictionary consisting of variable names as keys and its values.
		"""
		for i in range(len(myvars)):
			datafilename = self.wdir+myvars[i]+"."+nstr+dataext
			print("Reading multiple variable files : %s"%datafilename)
			fp = open(datafilename, "rb")
			if self.datatype == 'vtk':
				ddict.update(self.DataScanVTK(fp, n1, n2, n3, endian, dtype))
			else:
				ddict.update({myvars[i]: self.DataScan(fp, n1, n2, n3, endian,dtype)})
			fp.close()

	def ReadDataFile(self, num):
		"""Reads the data file generated from PLUTO code.
		**Inputs**:
			num -- Data file number in form of an Integer.
		**Outputs**:
			Dictionary that contains all information about Grid, Time and
			variables.
		"""
		gridfile = self.wdir+"grid.out"
		if self.datatype == "flt":
			dtype = "f"
			varfile = self.wdir+"flt.out"
			dataext = ".flt"
		elif self.datatype == "vtk":
			dtype = "f"
			varfile = self.wdir+"vtk.out"
			dataext=".vtk"
		elif self.datatype == 'hdf5':
			dtype = 'd'
			dataext = '.hdf5'
			nstr = num
			varfile = self.wdir+"data."+nstr+dataext
		else:
			dtype = "d"
			varfile = self.wdir+"dbl.out"
			dataext = ".dbl"

		self.ReadVarFile(varfile)
		self.ReadGridFile(gridfile)
		self.ReadTimeInfo(varfile)
		nstr = num
		if self.endianess == 'big':
			endian = ">"
		elif self.datatype == 'vtk':
			endian = ">"
		else:
			endian = "<"

		D = [('NStep', self.NStep), ('SimTime', self.SimTime), ('Dt', self.Dt),
			('n1', self.n1), ('n2', self.n2), ('n3', self.n3),
			('x1', self.x1), ('x2', self.x2), ('x3', self.x3),
			('dx1', self.dx1), ('dx2', self.dx2), ('dx3', self.dx3),
			('endianess', self.endianess), ('datatype', self.datatype),
			('filetype', self.filetype)]
		ddict = dict(D)

		if self.filetype == "single_file":
			datafilename = self.wdir+"data."+nstr+dataext
			self.ReadSingleFile(datafilename, self.vars, self.n1, self.n2,
								self.n3, endian, dtype, ddict)
		elif self.filetype == "multiple_files":
			self.ReadMultipleFiles(nstr, dataext, self.vars, self.n1, self.n2,
									self.n3, endian, dtype, ddict)
		else:
			print("Wrong file type : CHECK pluto.ini for file type.")
			print("Only supported are .dbl, .flt, .vtk, .hdf5")
			sys.exit()

		return ddict