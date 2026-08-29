from __future__ import division
import os
import numpy as np
from matplotlib.pyplot import *
from matplotlib.mlab import *
from . import Tools

class Image(object):
	def pldisplay(self, D, var,**kwargs):
		""" This method allows the user to display a 2D data using the
			 matplotlib's pcolormesh.

		**Inputs:**
			D   -- pyPLUTO pload object.\n
			var -- 2D array that needs to be displayed.
		*Required Keywords:*
			x1 -- The 'x' array\n
			x2 -- The 'y' array
		*Optional Keywords:*
			vmin -- The minimum value of the 2D array (Default : min(var))\n
			vmax -- The maximum value of the 2D array (Default : max(var))\n
			title -- Sets the title of the image.\n
			label1 -- Sets the X Label (Default: 'XLabel')\n
			label2 -- Sets the Y Label (Default: 'YLabel')\n
			polar -- A list to project Polar data on Cartesian Grid.\n
				polar = [True, True] -- Projects r-phi plane.\n
				polar = [True, False] -- Project r-theta plane.\n
				polar = [False, False] -- No polar plot (Default)\n
			cbar -- Its a tuple to set the colorbar on or off. \n
				cbar = (True,'vertical') -- Displays a vertical colorbar\n
				cbar = (True,'horizontal') -- Displays a horizontal colorbar\n
				cbar = (False,'') -- Displays no colorbar.
		**Usage:**
			``import pyPLUTO as pp``\n
			``wdir = '/path/to/the data files/'``\n
			``D = pp.pload(1,w_dir=wdir)``\n
			``I = pp.Image()``\n
			``I.pldisplay(D, D.v2, x1=D.x1, x2=D.x2, cbar=(True,'vertical'),\
			title='Velocity',label1='Radius',label2='Height')``
		"""
		
		x1 = kwargs.get('x1')
		x2 = kwargs.get('x2')
		var = var.T

		f1 = figure(kwargs.get('fignum',1), figsize=kwargs.get('figsize',[10,10]),
									 dpi=80, facecolor='w', edgecolor='k')
		ax1 = f1.add_subplot(111)
		ax1.set_aspect('equal')

		if kwargs.get('polar',[False,False])[0]:
			xx, yy = self.getPolarData(D,kwargs.get('x2'),rphi=kwargs.get('polar')[1])
			pcolormesh(xx,yy,var,vmin=kwargs.get('vmin',np.min(var)),vmax=kwargs.get('vmax',np.max(var)))
		else:
			ax1.axis([np.min(x1),np.max(x1),np.min(x2),np.max(x2)])
			pcolormesh(x1,x2,var,vmin=kwargs.get('vmin',np.min(var)),vmax=kwargs.get('vmax',np.max(var)))
			 
		title(kwargs.get('title',"Title"),size=kwargs.get('size'))
		xlabel(kwargs.get('label1',"Xlabel"),size=kwargs.get('size'))
		ylabel(kwargs.get('label2',"Ylabel"),size=kwargs.get('size'))
		if kwargs.get('cbar',(False,''))[0] == True:
			colorbar(orientation=kwargs.get('cbar')[1])
	 
	 
	def multi_disp(self,*args,**kwargs):
		mvar = []
		for arg in args:
			mvar.append(arg.T)
			 
		xmin = np.min(kwargs.get('x1'))
		xmax = np.max(kwargs.get('x1'))
		ymin = np.min(kwargs.get('x2'))
		ymax = np.max(kwargs.get('x2'))
		mfig = figure(kwargs.get('fignum',1),figsize=kwargs.get('figsize',[10,10]))
		Ncols = kwargs.get('Ncols')
		Nrows = len(args)//Ncols
		mprod = Nrows*Ncols
		dictcbar=kwargs.get('cbar',(False,'','each'))

		for j in range(mprod):
			mfig.add_subplot(Nrows,Ncols,j+1)
			pcolormesh(kwargs.get('x1'),kwargs.get('x2'), mvar[j])
			axis([xmin,xmax,ymin,ymax])
			gca().set_aspect('equal')
							 
			xlabel(kwargs.get('label1',mprod*['Xlabel'])[j])
			ylabel(kwargs.get('label2',mprod*['Ylabel'])[j])
			title(kwargs.get('title',mprod*['Title'])[j])
			if (dictcbar[0] == True) and (dictcbar[2] =='each'):
				colorbar(orientation=kwargs.get('cbar')[1])
			if dictcbar[0] == True and dictcbar[2]=='last':
				if (j == np.max(range(mprod))):colorbar(orientation=kwargs.get('cbar')[1])
				
	def oplotbox(self, AMRLevel, lrange=[0,0], cval=['b','r','g','m','w','k'],\
										islice=-1, jslice=-1, kslice=-1,geom='CARTESIAN'):
		"""
		This method overplots the AMR boxes up to the specified level.
		**Input:**
			AMRLevel -- AMR object loaded during the reading and stored in the pload object
		*Optional Keywords:*
			lrange     -- [level_min,level_max] to be overplotted. By default it shows all the loaded levels\n
			cval       -- list of colors for the levels to be overplotted.\n
			[ijk]slice -- Index of the 2D slice to look for so that the adequate box limits are plotted.
										By default oplotbox considers you are plotting a 2D slice of the z=min(x3) plane.\n
			geom       -- Specified the geometry. Currently, CARTESIAN (default) and POLAR geometries are handled.
		"""

		nlev = len(AMRLevel)
		lrange[1] = min(lrange[1],nlev-1)
		npl  = lrange[1]-lrange[0]+1
		lpls = [lrange[0]+v for v in range(npl)]
		cols = cval[0:nlev]
		# Get the offset and the type of slice
		Slice = 0 ; inds = 'k'
		xx = 'x' ; yy ='y'
		if (islice >= 0):
				Slice = islice + AMRLevel[0]['ibeg'] ; inds = 'i'
				xx = 'y' ; yy ='z'
		if (jslice >= 0):
				Slice = jslice + AMRLevel[0]['jbeg'] ; inds = 'j'
				xx = 'x' ; yy ='z'
		if (kslice >= 0):
				Slice = kslice + AMRLevel[0]['kbeg'] ; inds = 'k'
				xx = 'x' ; yy ='y'
					 
		# Overplot the boxes
		for il in lpls:
			level = AMRLevel[il]
			for ib in range(level['nbox']):
				box = level['box'][ib]
				if ((Slice-box[inds+'b'])*(box[inds+'e']-Slice) >= 0):
					if (geom == 'CARTESIAN'):
						x0 = box[xx+'0'] ; x1 = box[xx+'1']
						y0 = box[yy+'0'] ; y1 = box[yy+'1']
						plot([x0,x1,x1,x0,x0],[y0,y0,y1,y1,y0],color=cols[il])
					elif (geom == 'POLAR') or (geom == 'SPHERICAL'):
						dn = np.pi/50.
						x0 = box[xx+'0'] ; x1 = box[xx+'1']
						y0 = box[yy+'0'] ; y1 = box[yy+'1']
						if y0 == y1:
							y1 = 2*np.pi+y0 - 1.e-3
						xb = np.concatenate([
										[x0*np.cos(y0),x1*np.cos(y0)],\
										x1*np.cos(np.linspace(y0,y1,num=int(abs(y0-y1)/dn) )),\
										[x1*np.cos(y1),x0*np.cos(y1)],\
										x0*np.cos(np.linspace(y1,y0,num=int(abs(y0-y1)/dn)))])
						yb = np.concatenate([
										[x0*np.sin(y0),x1*np.sin(y0)],\
										x1*np.sin(np.linspace(y0,y1,num=int(abs(y0-y1)/dn))),\
										[x1*np.sin(y1),x0*np.sin(y1)],\
										x0*np.sin(np.linspace(y1,y0,num=int(abs(y0-y1)/dn)))])
						plot(xb,yb,color=cols[il])
				
	def field_interp(self,var1,var2,x,y,dx,dy,xp,yp):
		""" 
			This method interpolates value of vector fields (var1 and var2) on field points (xp and yp).
			The field points are obtained from the method field_line.
			**Inputs:**
	 			var1 -- 2D Vector field in X direction\n
				var2 -- 2D Vector field in Y direction\n
				x -- 1D X array\n
				y -- 1D Y array\n
				dx -- 1D grid spacing array in X direction\n
				dy -- 1D grid spacing array in Y direction\n
				xp -- field point in X direction\n
				yp -- field point in Y direction\n	 
			**Outputs:**
				A list with 2 elements where the first element corresponds to the interpolate field
				point in 'x' direction and the second element is the field point in 'y' direction.
		"""
		q=[]
		U = var1
		V = var2
		i0 = np.abs(xp-x).argmin()
		j0 = np.abs(yp-y).argmin()
		scrhUx = np.interp(xp,x,U[:,j0])
		scrhUy = np.interp(yp,y,U[i0,:])
		q.append(scrhUx + scrhUy - U[i0,j0])
		scrhVx = np.interp(xp,x,V[:,j0])
		scrhVy = np.interp(yp,y,V[i0,:])
		q.append(scrhVx + scrhVy - V[i0,j0])
		return q
	 
	def field_line(self,var1,var2,x,y,dx,dy,x0,y0):
		""" This method is used to obtain field lines (same as fieldline.pro in PLUTO IDL tools).
		**Inputs:**
			var1 -- 2D Vector field in X direction\n
			var2 -- 2D Vector field in Y direction\n
			x -- 1D X array\n
			y -- 1D Y array\n
			dx -- 1D grid spacing array in X direction\n
			dy -- 1D grid spacing array in Y direction\n
			x0 -- foot point of the field line in X direction\n
			y0 -- foot point of the field line in Y direction\n
		**Outputs:**
			This routine returns a dictionary with keys - \n
			qx -- list of the field points along the 'x' direction.
			qy -- list of the field points along the 'y' direction.
		**Usage:**
			See the myfieldlines routine for the same.
		"""
		xbeg = x[0] - 0.5*dx[0]
		xend = x[-1] + 0.5*dx[-1]
		
		ybeg = y[0]  - 0.5*dy[0]
		yend = y[-1] + 0.5*dy[-1]

		inside_domain = x0 > xbeg and x0 < xend and y0 > ybeg and y0 < yend

		MAX_STEPS = 5000
		xln_fwd = [x0]
		yln_fwd = [y0]
		xln_bck = [x0]
		yln_bck = [y0]
		rhs = []
		k = 0
	 
		while inside_domain == True:
			R1 = self.field_interp(var1,var2,x,y,dx,dy,xln_fwd[k],yln_fwd[k])
			dl = 0.5*np.max(np.concatenate((dx,dy)))/(np.sqrt(R1[0]*R1[0] + R1[1]*R1[1] + 1.e-14))
			xscrh = xln_fwd[k] + 0.5*dl*R1[0]
			yscrh = yln_fwd[k] + 0.5*dl*R1[1]
			
			R2 = self.field_interp(var1,var2,x,y,dx,dy,xln_fwd[k],yln_fwd[k])
			
			xln_one = xln_fwd[k] + dl*R2[0]
			yln_one = yln_fwd[k] + dl*R2[1]
			
			xln_fwd.append(xln_one)
			yln_fwd.append(yln_one)
			inside_domain = xln_one > xbeg and xln_one < xend and yln_one > ybeg and yln_one < yend
			inside_domain = inside_domain and (k < MAX_STEPS-3)
			k = k + 1
	
		k_fwd = k
		qx = np.asarray(xln_fwd[0:k_fwd])
		qy = np.asarray(yln_fwd[0:k_fwd])
		flines={'qx':qx,'qy':qy}
		
		return flines
		 
		 
	def myfieldlines(self,Data,x0arr,y0arr,stream=False,**kwargs):
		""" This method overplots the magnetic field lines at the footpoints given by (x0arr[i],y0arr[i]).
		**Inputs:**
			Data -- pyPLUTO.pload object\n
			x0arr -- array of x co-ordinates of the footpoints\n
			y0arr -- array of y co-ordinates of the footpoints\n
			stream -- keyword for two different ways of calculating the field lines.\n
			True -- plots contours of rAphi (needs to store vector potential)\n
			False -- plots the fieldlines obtained from the field_line routine. (Default option)
		*Optional Keywords:*
			colors -- A list of matplotlib colors to represent the lines. The length of this list should be same as that of x0arr.\n
			lw -- Integer value that determines the linewidth of each line.\n
			ls -- Determines the linestyle of each line.
		**Usage:**
			Assume that the magnetic field is a given as **B** = B0$\hat{y}$.
			Then to show this field lines we have to define the x and y arrays of field foot points.\n
			``x0arr = linspace(0.0,10.0,20)``\n
			``y0arr = linspace(0.0,0.0,20)``\n
			``import pyPLUTO as pp``\n
			``D = pp.pload(45)``\n
			``I = pp.Image()``\n
			``I.myfieldlines(D,x0arr,y0arr,colors='k',ls='--',lw=1.0)``
		"""
				
		if len(x0arr) != len(y0arr) : print("Input Arrays should have same size")
		QxList=[]
		QyList=[]
		StreamFunction = []
		levels =[]
		if stream == True:
			X, Y = np.meshgrid(Data.x1,Data.x2.T)
			StreamFunction = X*(Data.Ax3.T)
			for i in range(len(x0arr)):
				nx = np.abs(X[0,:]-x0arr[i]).argmin()
				ny = np.abs(X[:,0]-y0arr[i]).argmin()
				levels.append(X[ny,nx]*Data.Ax3.T[ny,nx])
			contour(X,Y,StreamFunction,levels,colors=kwargs.get('colors'),linewidths=kwargs.get('lw',1),linestyles=kwargs.get('ls','solid'))
		else:
			for i in range(len(x0arr)):
				QxList.append(self.field_line(Data.bx1,Data.bx2,Data.x1,Data.x2,Data.dx1,Data.dx1,x0arr[i],y0arr[i]).get('qx'))
				QyList.append(self.field_line(Data.bx1,Data.bx2,Data.x1,Data.x2,Data.dx1,Data.dx1,x0arr[i],y0arr[i]).get('qy'))
				plot(QxList[i],QyList[i],color=kwargs.get('colors'))
				axis([min(Data.x1),max(Data.x1),min(Data.x2),max(Data.x2)])

	def getSphData(self,Data,w_dir=None,datatype=None,**kwargs):
		"""This method transforms the vector and scalar  fields from Spherical co-ordinates to Cylindrical.
		**Inputs**:
			Data -- pyPLUTO.pload object\n
			w_dir -- /path/to/the/working/directory/\n
			datatype -- If the data is of 'float' type then datatype = 'float' else by default the datatype is set to 'double'.
		*Optional Keywords*:
			rphi -- [Default] is set to False implies that the r-theta plane is transformed. If set True then the r-phi plane is transformed.\n
			x2cut -- Applicable for 3D data and it determines the co-ordinate of the x2 plane while r-phi is set to True.\n
			x3cut -- Applicable for 3D data and it determines the co-ordinate of the x3 plane while r-phi is set to False.
		"""
		Tool = Tools.Tools()
		key_value_pairs = []
		allvars = []
		for v in Data.vars:
			allvars.append(v)

		if kwargs.get('rphi',False)==True:
			R,TH = np.meshgrid(Data.x1,Data.x3)
			if Data.n3 != 1:
				for variable in allvars:
					key_value_pairs.append([variable,getattr(Data,variable)[:,kwargs.get('x2cut',0),:].T])

				SphData = dict(key_value_pairs)
				if ('bx1' in allvars) or ('bx2' in allvars):
					(SphData['b1c'],SphData['b3c']) = Tool.RTh2Cyl(R,TH,SphData.get('bx1'),SphData.get('bx3'))
					allvars.append('b1c')
					allvars.append('b3c')
				if ('vx1' in allvars) or ('vx2' in allvars):
					(SphData['v1c'],SphData['v3c']) = Tool.RTh2Cyl(R,TH,SphData.get('vx1'),SphData.get('vx3'))
					allvars.append('v1c')
					allvars.append('v3c')
			else:
				print("No x3 plane for 2D data")
		else:
			R,TH = np.meshgrid(Data.x1,Data.x2)
			if Data.n3 != 1:
				for variable in allvars:
					key_value_pairs.append([variable,getattr(Data,variable)[:,:,kwargs.get('x3cut',0)].T])
				SphData = dict(key_value_pairs)
				if ('bx1' in allvars) or ('bx2' in allvars):
					(SphData['b1c'],SphData['b2c']) = Tool.RTh2Cyl(R,TH,SphData.get('bx1'),SphData.get('bx2'))
					allvars.append('b1c')
					allvars.append('b2c')
				if ('vx1' in allvars) or ('vx2' in allvars):
					(SphData['v1c'],SphData['v2c']) = Tool.RTh2Cyl(R,TH,SphData.get('vx1'),SphData.get('vx2'))
					allvars.append('v1c')
					allvars.append('v2c')
			else:
				for variable in allvars:
					key_value_pairs.append([variable,getattr(Data,variable)[:,:].T])
				SphData = dict(key_value_pairs)
				if ('bx1' in allvars) or ('bx2' in allvars):
					(SphData['b1c'],SphData['b2c']) = Tool.RTh2Cyl(R,TH,SphData.get('bx1'),SphData.get('bx2'))
					allvars.append('b1c')
					allvars.append('b2c')
				if ('vx1' in allvars) or ('vx2' in allvars):
					(SphData['v1c'],SphData['v2c']) = Tool.RTh2Cyl(R,TH,SphData.get('vx1'),SphData.get('vx2'))
					allvars.append('v1c')
					allvars.append('v2c')
			
		for variable in allvars:
			if kwargs.get('rphi',False)==True:
				R,Z,SphData[variable]= Tool.sph2cyl(Data,SphData.get(variable),rphi=True,theta0=Data.x2[kwargs.get('x2cut',0)])
			else:
				if Data.n3 != 1:
					R,Z,SphData[variable] = Tool.sph2cyl(Data,SphData.get(variable),rphi=False)
				else:
					R,Z,SphData[variable] = Tool.sph2cyl(Data,SphData.get(variable),rphi=False)
		return R,Z,SphData
	 
	 
	def getPolarData(self, Data, ang_coord, rphi=False):
		"""To get the Cartesian Co-ordinates from Polar.
		**Inputs:**
			Data -- pyPLUTO pload Object\n
			ang_coord -- The Angular co-ordinate (theta or Phi)
		*Optional Keywords:*
			rphi -- Default value FALSE is for R-THETA data,
			Set TRUE for R-PHI data.\n
		**Outputs**:
			2D Arrays of X, Y from the Radius and Angular co-ordinates.\n
			They are used in pcolormesh in the Image.pldisplay functions.
		"""
		D = Data
		if ang_coord is D.x2:
			x2r = D.x2r
		elif ang_coord is D.x3:
			x2r = D.x3r
		else:
			print("Angular co-ordinate must be given")
				
		rcos = np.outer(np.cos(x2r), D.x1r)
		rsin = np.outer(np.sin(x2r), D.x1r)
		
		if rphi:
			xx = rcos
			yy = rsin
		else:
			xx = rsin
			yy = rcos

		return xx, yy

	def pltSphData(self,Data,w_dir=None,datatype=None,**kwargs):
		"""This method plots the transformed data obtained from getSphData using the matplotlib's imshow
		**Inputs:**
			Data -- pyPLUTO.pload object\n
			w_dir -- /path/to/the/working/directory/\n
			datatype -- Datatype.
		*Required Keywords*:
			plvar -- A string which represents the plot variable.\n
		*Optional Keywords*:
			logvar -- [Default = False] Set it True for plotting the log of a variable.\n
			rphi -- [Default = False - for plotting in r-theta plane] Set it True for plotting the variable in r-phi plane.
		"""
		if w_dir is None: w_dir=os.getcwd()+'/'
		R,Z,SphData = self.getSphData(Data,w_dir=w_dir,datatype=datatype,**kwargs)

		extent=(np.min(R.flat),max(R.flat),np.min(Z.flat),max(Z.flat))
		dRR=max(R.flat)-np.min(R.flat)
		dZZ=max(Z.flat)-np.min(Z.flat)

		isnotnan=~np.isnan(SphData[kwargs.get('plvar')])
		maxPl=max(SphData[kwargs.get('plvar')][isnotnan].flat)
		minPl=np.min(SphData[kwargs.get('plvar')][isnotnan].flat)
		normrange=False
		if minPl<0:
				normrange=True
		if maxPl>-minPl:
				minPl=-maxPl
		else:
				maxPl=-minPl
		if (normrange and kwargs.get('plvar')!='rho' and kwargs.get('plvar')!='prs'):
				SphData[kwargs.get('plvar')][-1][-1]=maxPl
				SphData[kwargs.get('plvar')][-1][-2]=minPl

		if (kwargs.get('logvar') == True):
				SphData[kwargs.get('plvar')] = np.log10(SphData[kwargs.get('plvar')])

		imshow(SphData[kwargs.get('plvar')], aspect='equal', origin='lower', cmap=cm.jet,extent=extent, interpolation='nearest')


