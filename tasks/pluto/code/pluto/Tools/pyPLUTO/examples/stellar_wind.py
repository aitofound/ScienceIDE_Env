import os
import sys
from numpy import *
from matplotlib.pyplot import *
import pyPLUTO as pypl
import pyPLUTO.pload as pp
import pyPLUTO.Image as img
import pyPLUTO.Tools as tl

#To run this example [definitions_01.h] of Test_Problems/HD/Stellar_Wind
#using pluto_01.ini and set the data in flt datatype.

plutodir = os.environ['PLUTO_DIR']
wdir = plutodir+'/Test_Problems/HD/Stellar_Wind/'
nlinf = pypl.nlast_info(w_dir=wdir,datatype='flt')

D = pp.pload(nlinf['nlast'],w_dir=wdir,datatype='flt') # Loading the data into a pload object D.

I = img.Image()
I.pldisplay(D, log10(D.rho[:,0,:]),x1=D.x1,x2=D.x3, label1='x',label2='y',title=r'Log Density $\rho$ [Stellar Wind]',cbar=(True,'vertical'),figsize=[8,12])

# Code to plot arrows. --> Spacing between the arrow can be adjusted by modifying the newdims tuple of conrid function.
T = tl.Tools()
newdims = 2*(20,)
Xmesh, Ymesh = meshgrid(D.x1.T,D.x3.T)
xcong = T.congrid(Xmesh,newdims,method='linear')
ycong = T.congrid(Ymesh,newdims,method='linear')
velxcong = T.congrid(D.vx1[:,0,:].T,newdims,method='linear')
velycong = T.congrid(D.vx3[:,0,:].T,newdims,method='linear')
gca().quiver(xcong, ycong, velxcong, velycong,color='w')

savefig('stellar_wind_1.png')
show()
