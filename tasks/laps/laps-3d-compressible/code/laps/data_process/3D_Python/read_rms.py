import numpy as np
import struct
import matplotlib.pyplot as plt
from matplotlib import cm
from matplotlib.gridspec import GridSpec
import glob
import sys


# read parallel information ----------------
file_prl = open('./output/parallel_info.dat','rb')

skip = struct.unpack("f",file_prl.read(4))

npe = int((struct.unpack("f",file_prl.read(4)))[0])
npex = int((struct.unpack("f",file_prl.read(4)))[0])
npey = int((struct.unpack("f",file_prl.read(4)))[0])
nvar = int((struct.unpack("f",file_prl.read(4)))[0])

skip = struct.unpack("f",file_prl.read(4))

file_prl.close()
#-----------------------------------------



data = np.loadtxt('./output/rms.dat')

time_rms = data[:,0]

average_fields = data[:,1:(nvar+1)]
rms = data[:,(nvar+1):(2*nvar+1)]
Ek = data[:,(2*nvar+1):(2*nvar+4)] / 2.0

Ebtot = (rms[:,4] + rms[:,5] + rms[:,6])/2.0



Ektot = Ek[:,0] + Ek[:,1] + Ek[:,2]


data_ebm = np.loadtxt('./output/EBM_info.dat')
time_ebm = data_ebm[:,0]
radius = data_ebm[:,1]
Ur = data_ebm[0,2]
R0 = radius[0]

Rt = R0 + Ur * time_rms


fig = plt.figure()
sub = fig.add_subplot(111)
sub.plot(time_rms, Ektot, label=r'$E_k$')
sub.plot(time_rms, Ebtot, label=r'$E_b$')

# sub.plot(time_rms, average_fields[:,0], label=r'$\bar{\rho}$')
# sub.plot(time_rms, average_fields[:,7], label=r'$\bar{p}$')
# sub.plot(time_rms, average_fields[0,0] * (R0/Rt)**2, color='k', ls = '--')
# sub.plot(time_rms, average_fields[0,7] * (R0/Rt)**(10/3), color='k', ls = '--')

# sub.plot(time_rms, average_fields[:,4], label=r'$\bar{b}_x$')
# sub.plot(time_rms, average_fields[:,5], label=r'$\bar{b}_y$')
# sub.plot(time_rms, average_fields[:,6], label=r'$\bar{b}_z$')
# sub.plot(time_rms, average_fields[0,4] * (R0/Rt)**2, color='k', ls = '--')
# sub.plot(time_rms, average_fields[0,5] * (R0/Rt), color='k', ls = '--')
# sub.plot(time_rms, average_fields[0,6] * (R0/Rt), color='k', ls = '--')


sub.legend()
# sub.set_xlabel(r'$t$')
plt.show()