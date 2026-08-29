from __future__ import division
import numpy as np
import scipy.ndimage
from scipy.interpolate import interp1d, UnivariateSpline

class Tools(object):
	"""
		This Class has all the functions doing basic mathematical
		operations to the vector or scalar fields.
		It is called after pyPLUTO.pload object is defined.
	"""
	def find(self,condition):
		res, = np.nonzero(np.ravel(condition))
		return res
		
	def deriv(self,Y,X=None):
		"""
			Calculates the derivative of Y with respect to X.
				
			**Inputs:**
			Y : 1-D array to be differentiated.\n
			X : 1-D array with len(X) = len(Y).\n
				
			If X is not specified then by default X is chosen to be an equally spaced array
			having same number of elements as Y.
			**Outputs:**
			This returns an 1-D array having the same no. of elements as Y (or X) and
			contains the values of dY/dX.
		"""
	
		n = len(Y)
		n2 = n-2
		if X==None : X = np.arange(n)
		Xarr = np.asarray(X,dtype='float')
		Yarr = np.asarray(Y,dtype='float')
		x12 = Xarr - np.roll(Xarr,-1)   #x1 - x2
		x01 = np.roll(Xarr,1) - Xarr    #x0 - x1
		x02 = np.roll(Xarr,1) - np.roll(Xarr,-1) #x0 - x2
		DfDx = np.roll(Yarr,1) * (x12 / (x01*x02)) + Yarr * (1./x12 - 1./x01) - np.roll(Yarr,-1) * (x01 / (x02 * x12))
		# Formulae for the first and last points:
		
		DfDx[0] = Yarr[0] * (x01[1]+x02[1])/(x01[1]*x02[1]) - Yarr[1] * x02[1]/(x01[1]*x12[1]) + Yarr[2] * x01[1]/(x02[1]*x12[1])
		DfDx[n-1] = -Yarr[n-3] * x12[n2]/(x01[n2]*x02[n2]) + Yarr[n-2]*x02[n2]/(x01[n2]*x12[n2]) - Yarr[n-1]*(x02[n2]+x12[n2])/(x02[n2]*x12[n2])
		
		return DfDx


	def Grad(self,phi,x1,x2,dx1,dx2,polar=False):
		""" This method calculates the gradient of the 2D scalar phi.
			
			**Inputs:**
			
			phi -- 2D scalar whose gradient is to be determined.\n
			x1 -- The 'x' array\n
			x2 -- The 'y' array\n
			dx1 -- The grid spacing in 'x' direction.\n
			dx2 -- The grid spacing in 'y' direction.\n
			polar -- The keyword should be set to True inorder to estimate the Gradient in polar co-ordinates. By default it is set to False.
			
			**Outputs:**
			
			This routine outputs a 3D array with shape = (len(x1),len(x2),2), such that [:,:,0] element corresponds to the gradient values of phi wrt to x1 and [:,:,1] are the gradient values of phi wrt to x2.
			
		"""
		(n1, n2) = phi.shape
		grad_phi = np.zeros(shape=(n1,n2,2))
		h2 = np.ones(shape=(n1,n2))
		if polar == True:
			for j in range(n2):
					h2[:,j] = x1
							
		for i in range(n1):
				scrh1 = phi[i,:]
				grad_phi[i,:,1] = self.deriv(scrh1,x2)/h2[i,:]
		for j in range(n2):
				scrh2 = phi[:,j]
				grad_phi[:,j,0] = self.deriv(scrh2,x1)
		return grad_phi
			
	def Div(self,u1,u2,x1,x2,dx1,dx2,geometry=None):
		"""
			This method calculates the divergence of the 2D vector fields u1 and u2.
		
		**Inputs:**
		
		u1 -- 2D vector along x1 whose divergence is to be determined.\n
		u2 -- 2D vector along x2 whose divergence is to be determined.\n
		x1 -- The 'x' array\n
		x2 -- The 'y' array\n
		dx1 -- The grid spacing in 'x' direction.\n
		dx2 -- The grid spacing in 'y' direction.\n
		geometry -- The keyword *geometry* is by default set to 'cartesian'. It can be set to either one of the following : *cartesian*, *cylindrical*, *spherical* or *polar*. To calculate the divergence of the vector fields, respective geometric corrections are taken into account based on the value of this keyword.
		
		**Outputs:**
		
		A 2D array with same shape as u1(or u2) having the values of divergence.
		"""
		(n1, n2) = u1.shape
		Divergence = np.zeros(shape=(n1,n2))
		du1 = np.zeros(shape=(n1,n2))
		du2 = np.zeros(shape=(n1,n2))
		
		A1 = np.zeros(shape=n1)
		A2 = np.zeros(shape=n2)
		
		dV1 = np.zeros(shape=(n1,n2))
		dV2 = np.zeros(shape=(n1,n2))
		
		if geometry == None : geometry = 'cartesian'
		
		#------------------------------------------------
		#  define area and volume elements for the
		#  different coordinate systems
		#------------------------------------------------
		
		if geometry == 'cartesian' :
			A1[:] = 1.0
			A2[:] = 1.0
			dV1   = np.outer(dx1,A2)
			dV2   = np.outer(A1,dx2)
		
		if geometry == 'cylindrical' :
			A1 = x1
			A2[:] = 1.0
			dV1 = np.meshgrid(x1*dx1,A2)[0].T*np.meshgrid(x1*dx1,A2)[1].T
			for i in range(n1) : dV2[i,:] = dx2[:]
		
		if geometry == 'polar' :
			A1    = x1
			A2[:] = 1.0
			dV1   = np.meshgrid(x1,A2)[0].T*np.meshgrid(x1,A2)[1].T
			dV2   = np.meshgrid(x1,dx2)[0].T*np.meshgrid(x1,dx2)[1].T

		if geometry == 'spherical' :
			A1 = x1*x1
			A2 = np.sin(x2)
			for j in range(n2): dV1[:,j] = A1*dx1
			dV2   = np.meshgrid(x1,np.sin(x2)*dx2)[0].T*np.meshgrid(x1,np.sin(x2)*dx2)[1].T
		
		# ------------------------------------------------
		#              Make divergence
		# ------------------------------------------------
		for i in range(1,n1-1):
			du1[i,:] = 0.5*(A1[i+1]*u1[i+1,:] - A1[i-1]*u1[i-1,:])/dV1[i,:]
		for j in range(1,n2-1):
			du2[:,j] = 0.5*(A2[j+1]*u2[:,j+1] - A2[j-1]*u2[:,j-1])/dV2[:,j]

		Divergence = du1 + du2
		return Divergence


	def RTh2Cyl(self,R,Th,X1,X2):
		"""
			This method does the transformation from spherical coordinates to cylindrical ones.
			**Inputs:**
			
			R - 2D array of spherical radius coordinates.\n
			Th - 2D array of spherical theta-angle coordinates.\n
			X1 - 2D array of radial component of given vector\n
			X2 - 2D array of thetoidal component of given vector\n
			
			**Outputs:**
			
			This routine outputs two 2D arrays after transformation.
			
			**Usage:**
			
			``import pyPLUTO as pp``\n
			``import numpy as np``\n
			``D = pp.pload(0)``\n
			``ppt=pp.Tools()``\n
			``TH,R=np.meshgrid(D.x2,D.x1)``\n
			``Br,Bz=ppt.RTh2Cyl(R,TH,D.bx1,D.bx2)``
			
			D.bx1 and D.bx2 should be vectors in spherical coordinates. After transformation (Br,Bz) corresponds to vector in cilindrical coordinates.
		"""
		Y1=X1*np.sin(Th)+X2*np.cos(Th)
		Y2=X1*np.cos(Th)-X2*np.sin(Th)
		return Y1,Y2
			
			
	def myInterpol(self,RR,N):
		"""
			This method interpolates (linear interpolation) vector 1D vector RR to 1D N-length vector. Useful for stretched grid calculations.
			**Inputs:**
			RR - 1D array to interpolate.\n
			N  - Number of grids to interpolate to.\n
			**Outputs:**
			This routine outputs interpolated 1D array to the new grid (len=N).
			**Usage:**
			
			``import pyPLUTO as pp``\n
			``import numpy as np``\n
			``D = pp.pload(0)``\n
			``ppt=pp.Tools()``\n
			``x=linspace(0,1,10) #len(x)=10``\n
			``y=x*x``\n
			``Ri,Ni=ppt.myInterpol(y,100) #len(Ri)=100``
			
			Ri - interpolated numbers;
			Ni - grid for Ri
		"""
		
		NN=np.linspace(0,len(RR)-1,len(RR))
		spline_fit=UnivariateSpline(RR,NN,k=3,s=0)
		RRi=np.linspace(RR[0],RR[-1],N)
		NNi=spline_fit(RRi)
		NNi[0]=NN[0]+0.00001
		NNi[-1]=NN[-1]-0.00001
		return RRi,NNi


	def getUniformGrid(self,r,th,rho,Nr,Nth):
		""" This method transforms data with non-uniform grid (stretched) to uniform. Useful for stretched grid calculations.

			**Inputs:**
			r  - 1D vector of X1 coordinate (could be any, e.g D.x1).\n
			th - 1D vector of X2 coordinate (could be any, e.g D.x2).\n
			rho- 2D array of data.\n
			Nr - new size of X1 vector.\n
			Nth- new size of X2 vector.\n
			
			**Outputs:**
			
			This routine outputs 2D uniform array Nr x Nth dimension
			
			**Usage:**
			
			``import pyPLUTO as pp``\n
			``import numpy as np``\n
			``D = pp.pload(0)``\n
			``ppt=pp.Tools()``\n
			``X1new, X2new, res = ppt.getUniformGrid(D.x1,D.x2,D.rho,20,30)``
			
			X1new - X1 interpolated grid len(X1new)=20
			X2new - X2 interpolated grid len(X2new)=30
			res   - 2D array of interpolated variable
			
		"""
		Ri,NRi=self.myInterpol(r,Nr)
		Ra=np.int32(NRi);Wr=NRi-Ra
			
		YY=np.ones([Nr,len(th)])
		for i in range(len(th)):
			YY[:,i]=(1-Wr)*rho[Ra,i] + Wr*rho[Ra+1,i]
					
		THi,NTHi=self.myInterpol(th,Nth)
		THa=np.int32(NTHi);Wth=NTHi-THa
		ZZ=np.ones([Nr,Nth])
		for i in range(Nr):
			ZZ[i,:]=(1-Wth)*YY[i,THa] + Wth*YY[i,THa+1]
		return Ri,THi,ZZ
			
	def congrid(self, a, newdims, method='linear', centre=False, minusone=False):
		"""
		Arbitrary resampling of source array to new dimension sizes.
		Currently only supports maintaining the same number of dimensions.
		To use 1-D arrays, first promote them to shape (x,1).
		
		Uses the same parameters and creates the same co-ordinate lookup points
		as IDL''s congrid routine, which apparently originally came from a VAX/VMS
		routine of the same name.
		
		**Inputs:**
		
		a -- The 2D array that needs resampling into new dimensions.\n
		newdims -- A tuple which represents the shape of the resampled data\n
		method -- This keyword decides the method used for interpolation.\n
		neighbour - closest value from original data\n
		nearest and linear - uses n x 1-D interpolations using scipy.interpolate.interp1d
		(see Numerical Recipes for validity of use of n 1-D interpolations)\n
		spline - uses ndimage.map_coordinates\n
		centre -- This keyword decides the positions of interpolation points.\n
		True - interpolation points are at the centres of the bins\n
		False - points are at the front edge of the bin\n
		minusone -- This prevents extrapolation one element beyond bounds of input array\n
		For example- inarray.shape = (i,j) & new dimensions = (x,y)\n
		False - inarray is resampled by factors of (i/x) * (j/y)\n
		True - inarray is resampled by(i-1)/(x-1) * (j-1)/(y-1)\n
		
		**Outputs:**
		
		A 2D array with resampled data having a shape corresponding to newdims.
	
		"""
		if not a.dtype in [np.float64, np.float32]:
			a = np.cast[float](a)
		m1 = np.cast[int](minusone)
		ofs = np.cast[int](centre) * 0.5
		old = np.array( a.shape )
		ndims = len( a.shape )
		if len( newdims ) != ndims:
			print("[congrid] dimensions error. This routine currently only support rebinning to the same number of dimensions.")
			return None
		newdims = np.asarray( newdims, dtype=float )
		dimlist = []
		
		if method == 'neighbour':
			for i in range( ndims ):
					base = np.indices(newdims)[i]
					dimlist.append((old[i] - m1) / (newdims[i] - m1)*(base + ofs)-ofs)
			cd = np.array( dimlist ).round().astype(int)
			newa = a[list( cd )]
			return newa
		
		elif method in ['nearest','linear']:
				# calculate new dims
			for i in range( ndims ):
					base = np.arange( newdims[i])
					dimlist.append( (old[i] - m1) / (newdims[i] - m1) * (base + ofs) - ofs )
			# specify old dims
			olddims = [np.arange(i, dtype = np.float) for i in list( a.shape )]
			
			# first interpolation - for ndims = any
			mint = interp1d( olddims[-1], a, kind=method )
			newa = mint( dimlist[-1] )
			trorder = [ndims - 1] + list(range( ndims - 1 ))
			for i in range( ndims - 2, -1, -1 ):
					newa = newa.transpose( trorder )
					mint = interp1d( olddims[i], newa, kind=method )
					newa = mint( dimlist[i] )
			
			if ndims > 1:
			# need one more transpose to return to original dimensions
				newa = newa.transpose( trorder )
			return newa
		
		elif method in ['spline']:
			oslices = [ slice(0,j) for j in old ]
			oldcoords = np.ogrid[oslices]
			nslices = [ slice(0,j) for j in list(newdims) ]
			newcoords = np.mgrid[nslices]
			newcoords_dims = range(np.rank(newcoords))
			#make first index last
			newcoords_dims.append(newcoords_dims.pop(0))
			newcoords_tr = newcoords.transpose(newcoords_dims)
			# makes a view that affects newcoords
			newcoords_tr += ofs
			deltas = (np.asarray(old) - m1) / (newdims - m1)
			newcoords_tr *= deltas
			newcoords_tr -= ofs
			newa = scipy.ndimage.map_coordinates(a, newcoords)
			return newa
		else:
			print("Congrid error: Unrecognized interpolation type. Currently only \'neighbour\', \'nearest\',\'linear\' and \'spline\' are supported.")
			return None


	def sph2cyl(self,D,Dx,rphi=None,theta0=None):
		"""
			This method transforms spherical data into cylindrical applying interpolation.
			Works for stretched grid as well, transforms poloidal (R-Theta) data by default.
			Fix theta and set rphi=True to get (R-Phi) transformation.
			
			**Inputs:**
			
			D  - structure  from 'pload' method.\n
			Dx - variable to be transformed (D.rho for example).\n
			
			**Outputs:**
			
			This routine outputs transformed (sph->cyl) variable and grid.
			
			**Usage:**
			
			``import pyPLUTO as pp``\n
			``import numpy as np``\n
			``D = pp.pload(0)``\n
			``ppt=pp.Tools()``\n
			``R,Z,res = ppt.sph2cyl(D,D.rho.transpose())``
			
			R - 2D array with cylindrical radius values
			Z - 2D array with cylindrical Z values
			res - 2D array of transformed variable
			
		"""
		if rphi is None or rphi == False:
			rx=D.x1
			th=D.x2
		else:
			rx=D.x1*np.sin(theta0)
			th=D.x3

		rx,th,Dx=self.getUniformGrid(rx,th,Dx.T,200,200)
		Dx=Dx.T

		if rphi is None or rphi == False:
			r0=np.min(np.sin(th)*rx[0])
			rN=rx[-1]
		else:
			r0=np.min([np.sin(th)*rx[0] , np.sin(th)*rx[-1]])
			rN=np.max([np.sin(th)*rx[0] , np.sin(th)*rx[-1]])

		dr=rN-r0
		z0=np.min(np.cos(th)*rN)
		zN=np.max(np.cos(th)*rN)
		dz=zN-z0
		dth=th[-1]-th[0]
		rl=np.int32(len(rx)*dr/(rx[-1]-rx[0]))
		zl=np.int32(rl* dz/dr)
		thl=len(th)
		r=np.linspace(r0, rN, rl)
		z=np.linspace(z0, zN, zl)
																		
		R,Z = np.meshgrid(r, z)
		Rs = np.sqrt(R*R + Z*Z)
		
		Th = np.arccos(Z/Rs)
		kv_34=self.find(R<0)
		Th.flat[kv_34]=2*np.pi - Th.flat[kv_34]
		ddr=rx[1]-rx[0]
		ddth=th[1]-th[0]

		Rs_copy=Rs.copy()
		Th_copy=Th.copy()

		nR1=self.find(Rs<rx[0])
		Rs.flat[nR1]=rx[0]
		nR2=self.find(Rs>rN)
		Rs.flat[nR2]=rN

		nTh1=self.find(Th>th[-1])
		Th.flat[nTh1]=th[-1]
		nTh2=self.find(Th<th[0])
		Th.flat[nTh2]=th[0]


		ra = ((len(rx)-1.001)/(np.max(Rs.flat)-np.min(Rs.flat)) *(Rs-np.min(Rs.flat)))
		tha = ((thl-1.001)/dth *(Th-th[0]))

		rn = np.int32(ra)
		thn = np.int32(tha)
		dra=ra-rn
		dtha=tha-thn
		w1=1-dra
		w2=dra
		w3=1-dtha
		w4=dtha
		lrx=len(rx)
		NN1=np.int32(rn+thn*lrx)
		NN2=np.int32((rn+1)+thn*lrx)
		NN3=np.int32(rn+(thn+1)*lrx)
		NN4=np.int32((rn+1)+(thn+1)*lrx)
		n=np.transpose(np.arange(0,np.product(np.shape(R))))
		DD=Dx.copy()
		F=R.copy()
		F.flat[n]=w1.flat[n]*(w3.flat[n]*Dx.flat[NN1.flat[n]] + w4.flat[n]*Dx.flat[NN3.flat[n]] )+\
			w2.flat[n]*(w3.flat[n]*Dx.flat[NN2.flat[n]] + w4.flat[n]*Dx.flat[NN4.flat[n]] )
				
		nR1=self.find(Rs_copy<rx[0]-ddr/1.5)
		nR2=self.find(Rs_copy>rN+ddr/1.5)
		nTh1=self.find(Th_copy>th[-1]+ddth/1.5)
		nTh2=self.find(Th_copy<th[0]-ddth/1.5)
			
		nmask=np.concatenate((nR1,nR2,nTh1,nTh2))
		F.flat[nmask]=np.nan;
		return R,Z,F




