import numpy as np
import struct
import matplotlib.pyplot as plt
from matplotlib import cm
from matplotlib.gridspec import GridSpec
import glob
import sys


def read_parallel_info():
    # read parallel information ----------------
    file_prl = open('./output/parallel_info.dat','rb')

    skip = struct.unpack("f",file_prl.read(4))

    npe = int((struct.unpack("f",file_prl.read(4)))[0])
    nvar = int((struct.unpack("f",file_prl.read(4)))[0])

    skip = struct.unpack("f",file_prl.read(4))

    file_prl.close()
    return npe, nvar


def read_grid():
    # read nx, ny, nz and grid-------------------------
    file_grid = open('./output/grid.dat', 'rb')

    skip = struct.unpack("f",file_grid.read(4))

    nx = int((struct.unpack("f",file_grid.read(4)))[0])
    ny = int((struct.unpack("f",file_grid.read(4)))[0])

    skip = struct.unpack("f",file_grid.read(4))

    xgrid = np.zeros(nx)
    ygrid = np.zeros(ny)

    skip = struct.unpack("f",file_grid.read(4))

    for i in range(nx):
        xgrid[i] = (struct.unpack("f",file_grid.read(4)))[0]
    for i in range(ny):
        ygrid[i] = (struct.unpack("f",file_grid.read(4)))[0]

    skip = struct.unpack("f",file_grid.read(4))

    file_grid.close()
    return xgrid,ygrid

def read_EBM():
    #read the EBM_info.dat
    #which includes time,radius,Ur

    file_EBM = np.array(np.loadtxt('./output/EBM_info.dat'))

    if len(file_EBM.shape)==1:
        file_EBM = np.reshape(file_EBM,(int(len(file_EBM)/3),3))

    t_EBM = file_EBM[:,0]
    radius = file_EBM[:,1]
    Ur_EBM = file_EBM[:,2]

    return t_EBM,radius,Ur_EBM
def read_uu(filename,nx,ny,nvar):
    file_uu = open(filename, 'rb')
    uu = np.zeros([nx,ny,nvar])

    skip = (struct.unpack("f",file_uu.read(4)))[0]
    t = (struct.unpack("f",file_uu.read(4)))[0] 
    skip = (struct.unpack("f",file_uu.read(4)))[0]

    for ivar in range(nvar):
            for iy in range(ny):
                for ix in range(nx):
                    uu[ix,iy,ivar] = (struct.unpack(\
                        "d",file_uu.read(8)))[0] 

    return t, uu


xgrid, ygrid = read_grid()
nx,ny= len(xgrid), len(ygrid)
print('nx, ny =', nx, ny)

kx = np.fft.rfftfreq(nx, xgrid[1] - xgrid[0])
ky = np.fft.rfftfreq(ny, ygrid[1] - ygrid[0])

np.save('./output/xgrid.npy',xgrid)
np.save('./output/ygrid.npy',ygrid)


npe,nvar = read_parallel_info()                                 
print('npe, nvar = ', npe, nvar)

t_EBM, radius, Ur = read_EBM()

files = sorted(glob.glob('./output/out*dat'))
nout = len(files)

time = np.zeros(nout)

rhomax_t = np.zeros(nout)
rhomin_t = np.zeros(nout)

pmax_t = np.zeros(nout)
pmin_t = np.zeros(nout)

uxmax_t = np.zeros(nout)
uxmin_t = np.zeros(nout)

uymax_t = np.zeros(nout)
uymin_t = np.zeros(nout)

uzmax_t = np.zeros(nout)
uzmin_t = np.zeros(nout)

Bxmax_t = np.zeros(nout)
Bxmin_t = np.zeros(nout)

Bymax_t = np.zeros(nout)
Bymin_t = np.zeros(nout)

Bzmax_t = np.zeros(nout)
Bzmin_t = np.zeros(nout)


for nt in range(nout):
    t, uu = read_uu(files[nt],nx,ny,nvar)

    print('t = {:.3f}'.format(t))

    time[nt] = t 

    rho = uu[:,:,0]

    ux = uu[:,:,1]
    uy = uu[:,:,2]
    uz = uu[:,:,3]

    Bx = uu[:,:,4]
    By = uu[:,:,5]
    Bz = uu[:,:,6]

    p = uu[:,:,7]

    if nt==0:
        pmax0 = np.max(p)
        pmin0 = np.min(p)

        rhomax0 = np.max(rho)
        rhomin0 = np.min(rho)

        uxmax0 = np.max(ux)
        uxmin0 = np.min(ux)

        uymax0 = np.max(uy)
        uymin0 = np.min(uy)

        uzmax0 = np.max(uz)
        uzmin0 = np.min(uz)

        Bxmax0 = np.max(Bx)
        Bxmin0 = np.min(Bx)

        Bymax0 = np.max(By)
        Bymin0 = np.min(By)

        Bzmax0 = np.max(Bz)
        Bzmin0 = np.min(Bz)


    rhomax_t[nt] = np.max(rho)
    rhomin_t[nt] = np.min(rho)

    pmax_t[nt] = np.max(p)
    pmin_t[nt] = np.min(p)

    uxmax_t[nt] = np.max(ux)
    uxmin_t[nt] = np.min(ux)

    uymax_t[nt] = np.max(uy)
    uymin_t[nt] = np.min(uy)

    uzmax_t[nt] = np.max(uz)
    uzmin_t[nt] = np.min(uz)

    Bxmax_t[nt] = np.max(Bx)
    Bxmin_t[nt] = np.min(Bx)

    Bymax_t[nt] = np.max(By)
    Bymin_t[nt] = np.min(By)

    Bzmax_t[nt] = np.max(Bz)
    Bzmin_t[nt] = np.min(Bz)

np.save('./output/time.npy',time)

gs = GridSpec(4,2)
fig = plt.figure(figsize=[8,8])

sub = fig.add_subplot(gs[0,0])
sub.plot(time,rhomax_t,'o',color='C0')
sub.plot(time,rhomin_t,'^',color='C1')
sub.plot(t_EBM,rhomax0 * (radius[0]/radius)**2,':',color='k' )
sub.set_ylabel(r'$\rho$',fontsize=15)

sub = fig.add_subplot(gs[0,1])
sub.plot(time,pmax_t,'o',color='C0')
sub.plot(time,pmin_t,'^',color='C1')
sub.plot(t_EBM,pmax0 * (radius[0]/radius)**(10/3),':',color='k' )
sub.set_ylabel(r'$p$',fontsize=15)

sub = fig.add_subplot(gs[1,0])
sub.plot(time,uxmax_t,'o',color='C0')
sub.plot(time,uxmin_t,'^',color='C1')
sub.plot(t_EBM,uxmax0 * (radius[0]/radius)**(0),':',color='k' )
sub.set_ylabel(r'$u_x$',fontsize=15)

sub = fig.add_subplot(gs[1,1])
sub.plot(time,uymax_t,'o',color='C0')
sub.plot(time,uymin_t,'^',color='C1')
sub.plot(t_EBM,uymax0 * (radius[0]/radius)**(1),':',color='k' )
sub.set_ylabel(r'$u_y$',fontsize=15)


sub = fig.add_subplot(gs[2,0])
sub.plot(time,uzmax_t,'o',color='C0')
sub.plot(time,uzmin_t,'^',color='C1')
sub.plot(t_EBM,uzmax0 * (radius[0]/radius)**(1),':',color='k' )
sub.set_ylabel(r'$u_z$',fontsize=15)

sub = fig.add_subplot(gs[2,1])
sub.plot(time,Bxmax_t,'o',color='C0')
sub.plot(time,Bxmin_t,'^',color='C1')
sub.plot(t_EBM,Bxmax0 * (radius[0]/radius)**(2),':',color='k' )
sub.set_ylabel(r'$B_x$',fontsize=15)

sub = fig.add_subplot(gs[3,0])
sub.plot(time,Bymax_t,'o',color='C0')
sub.plot(time,Bymin_t,'^',color='C1')
sub.plot(t_EBM,Bymax0 * (radius[0]/radius)**(1),':',color='k' )
sub.set_ylabel(r'$B_y$',fontsize=15)

sub.set_xlabel(r'$t$',fontsize=15)


sub = fig.add_subplot(gs[3,1])
sub.plot(time,Bzmax_t,'o',color='C0')
sub.plot(time,Bzmin_t,'^',color='C1')
sub.plot(t_EBM,Bzmax0 * (radius[0]/radius)**(1),':',color='k' )
sub.set_ylabel(r'$B_z$',fontsize=15)

sub.set_xlabel(r'$t$',fontsize=15)

gs.tight_layout(fig)

fig.savefig('./figure/time_evolution.png',dpi=400)
plt.close(fig)