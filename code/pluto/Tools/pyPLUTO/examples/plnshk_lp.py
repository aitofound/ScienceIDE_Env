import os
import pyPLUTO.pload as pp
import pyPLUTO as pypl
import pyPLUTO.ploadparticles as pr
import matplotlib.pyplot as plt
import numpy as np
plutodir = os.environ['PLUTO_DIR']
wdir = plutodir+'/Test_Problems/Particles/LP/Planar_Shock/'
nlinf = pypl.nlast_info(w_dir=wdir)
P0  = pr.ploadparticles(0,w_dir=wdir,ptype='LP')
Pnl = pr.ploadparticles(3,w_dir=wdir,ptype='LP')
Dnl = pp.pload(3,w_dir=wdir)
myid = 4.
#For time 0
P0_indx = np.where(P0.id == myid)[0][0]
eng0 = P0.eng[P0_indx,:]
emids0 = np.array([0.5*(eng0[i+1]+eng0[i]) for i in range(len(eng0)-1)])
chi0 = P0.chi[P0_indx,:]
normchi0 = emids0*emids0*chi0/np.max(emids0*emids0*chi0)

#For time NLast
Pnl_indx = np.where(Pnl.id == myid)[0][0]
engnl = Pnl.eng[Pnl_indx,:]
emidsnl = np.array([0.5*(engnl[i+1]+engnl[i]) for i in range(len(engnl)-1)])
chinl = Pnl.chi[Pnl_indx,:]
normchinl = emidsnl*emidsnl*chinl/np.max(emidsnl*emidsnl*chinl)


#Now plotting distribution of paticles on Fluid density (left) and
f1 = plt.figure(figsize=[10,6])
ax1 = f1.add_subplot(211)
ax1.imshow(Dnl.prs.T, origin='image', extent=[Dnl.x1.min(), Dnl.x1.max(), Dnl.x2.min(), Dnl.x2.max()],cmap='copper')
#ax1.scatter(P0.x1, P0.x2, c='w',s=30)
#ax1.scatter(P0.x1[Pnl_indx], P0.x2[Pnl_indx], c='r',s=30,ec='k')
ax1.scatter(Pnl.x1, Pnl.x2, c='w',s=30,ec='k')
ax1.scatter(Pnl.x1[Pnl_indx], Pnl.x2[Pnl_indx], c='r',s=30,ec='k')
ax1.set_title('Particle position overlaid on Fluid density')

ax2 = f1.add_subplot(212)
ax2.loglog(emids0, normchi0, 'k--',lw=2)
ax2.loglog(emidsnl, normchinl, 'r-',lw=2)
ax2.set_xlabel(r'E/E$_0$')
ax2.set_ylabel(r'$(E/E_0)^2 \chi(E) n_0$')
ax2.set_ylim([1.0e-19,1.0e1])
ax2.set_title('Spectral Evolution of Particle ID : 4 [marked as red]')
plt.savefig('plnshk_1.png')


