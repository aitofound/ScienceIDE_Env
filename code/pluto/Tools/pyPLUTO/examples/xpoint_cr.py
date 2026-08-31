import os
import pyPLUTO as pypl
import pyPLUTO.pload as pp
import pyPLUTO.ploadparticles as pr
import matplotlib.pyplot as plt
import numpy as np

plutodir = os.environ['PLUTO_DIR']
wdir = plutodir+'/Test_Problems/Particles/CR/Xpoint/'
nlinf = pypl.nlast_info(w_dir=wdir, datatype='flt')


D = pp.pload(nlinf['nlast'],w_dir=wdir,datatype='flt')
P = pr.ploadparticles(nlinf['nlast'],w_dir=wdir,datatype='flt')

Bmag = D.Bx1**2 + D.Bx2**2
f1 = plt.figure(figsize=[8,8])
im0 = plt.imshow(Bmag.T, origin='image',extent=[D.x1.min(), D.x1.max(), D.x2.min(), D.x2.max()])
plt.colorbar(im0)
plt.xlabel(r'X-axis')
plt.ylabel(r'Y-axis')
plt.title(r'Magnetic Energy [X-point test] with Scatter Plot of Highenergy CR particles',fontsize=12)
p_eng = 0.5*(P.vx1**2 + P.vx2**2)
indx_sort = p_eng.argsort()
x1s, x2s, pengs = P.x1[indx_sort], P.x2[indx_sort], p_eng[indx_sort]
im1 = plt.scatter(x1s[-3000:], x2s[-3000:],s=10,c=pengs[-3000:],cmap='copper',alpha=0.7	)
plt.colorbar(im1, orientation='horizontal')
plt.minorticks_on()
plt.savefig('xpoint_cr.png')
