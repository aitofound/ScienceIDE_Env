import numpy as np 
import struct
import matplotlib.pyplot as plt 
from matplotlib import cm
import sys
import glob
from matplotlib.gridspec import GridSpec
import array
import os
from time import perf_counter
from matplotlib.colors import *

def read_parallel_info():
    # read parallel information ----------------
    file_prl = open('./output/parallel_info.dat','rb')

    skip = struct.unpack("f",file_prl.read(4))

    npe = int((struct.unpack("f",file_prl.read(4)))[0])
    iproc = int((struct.unpack("f",file_prl.read(4)))[0])
    jproc = int((struct.unpack("f",file_prl.read(4)))[0])
    nvar = int((struct.unpack("f",file_prl.read(4)))[0])

    skip = struct.unpack("f",file_prl.read(4))

    file_prl.close()
    return npe, iproc, jproc, nvar


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

def read_grid():
    # read nx, ny, nz and grid-------------------------
    file_grid = open('./output/grid.dat', 'rb')

    skip = struct.unpack("f",file_grid.read(4))

    nx = int((struct.unpack("f",file_grid.read(4)))[0])
    ny = int((struct.unpack("f",file_grid.read(4)))[0])
    nz = int((struct.unpack("f",file_grid.read(4)))[0])

    skip = struct.unpack("f",file_grid.read(4))

    xgrid = np.zeros(nx)
    ygrid = np.zeros(ny)
    zgrid = np.zeros(nz)


    skip = struct.unpack("f",file_grid.read(4))

    for i in range(nx):
        xgrid[i] = (struct.unpack("f",file_grid.read(4)))[0]
    for i in range(ny):
        ygrid[i] = (struct.unpack("f",file_grid.read(4)))[0]
    for i in range(nz):
        zgrid[i] = (struct.unpack("f",file_grid.read(4)))[0]

    skip = struct.unpack("f",file_grid.read(4))

    file_grid.close()
    return xgrid,ygrid,zgrid

def read_uu(filename,nx,ny,nz,nvar):
    file_uu = open(filename, 'rb')
    uu = np.zeros([nx,ny,nz,nvar])

    skip = (struct.unpack("f",file_uu.read(4)))[0]
    t = (struct.unpack("f",file_uu.read(4)))[0] 
    skip = (struct.unpack("f",file_uu.read(4)))[0]

    for ivar in range(nvar):
        for iz in range(nz):
            for iy in range(ny):
                for ix in range(nx):
                    uu[ix,iy,iz,ivar] = (struct.unpack(\
                        "d",file_uu.read(8)))[0] 

    return t, uu

def map_to_file_location(iv,ix,iy,iz,nvar,nx,ny,nz):
    return ((iv * nz + iz) * ny + iy) * nx + ix


def read_output_location(filename,ix,iy,iz,nvar,nx,ny,nz):
    file = open(filename, 'rb')

    skip = (struct.unpack("f",file.read(4)))[0]
    t = (struct.unpack("f",file.read(4)))[0] 
    skip = (struct.unpack("f",file.read(4)))[0]

    offset = file.tell()

    vals = np.zeros(nvar)
    for iv in range(nvar):
        indx = ((iv * nz + iz) * ny + iy) * nx + ix
        file.seek(indx * 8 + offset)
        vals[iv] = struct.unpack('d',file.read(8))[0]

    file.close()

    return t, vals


def read_output_onevariable(filename,iv,nvar,nx,ny,nz):
    arr = np.zeros([nx,ny,nz])

    file = open(filename, 'rb')

    skip = (struct.unpack("f",file.read(4)))[0]
    t = (struct.unpack("f",file.read(4)))[0] 
    skip = (struct.unpack("f",file.read(4)))[0]

    offset0 = file.tell()

    offset_now = offset0
    for iz in range(nz):
        for iy in range(ny):
            for ix in range(nx):
                indx = ((iv * nz + iz) * ny + iy) * nx + ix
                offset_new = indx * 8 + offset0

                if offset_new != offset_now:
                    file.seek(offset_new - offset_now, 1)

                arr[ix,iy,iz] = struct.unpack('d',file.read(8))[0]
                offset_now = file.tell()

    return t,arr

xgrid, ygrid, zgrid = read_grid()
nx = len(xgrid)
ny = len(ygrid)
nz = len(zgrid)

print('nx = ', nx, ', ny = ', ny, ', nz = ', nz)

kx = np.fft.fftshift(np.fft.fftfreq(nx,xgrid[1]-xgrid[0]))
ky = np.fft.fftshift(np.fft.fftfreq(ny,ygrid[1]-ygrid[0]))
kz = np.fft.fftshift(np.fft.fftfreq(nz,zgrid[1]-zgrid[0]))

ind_kx0 = np.where(kx==0)[0][0]
ind_ky0 = np.where(ky==0)[0][0]
ind_kz0 = np.where(kz==0)[0][0]

KK1_xy, KK2_xy = np.meshgrid(kx, ky, indexing='ij')
KK1_xz, KK2_xz = np.meshgrid(kx, kz, indexing='ij')
KK1_yz, KK2_yz = np.meshgrid(ky, kz, indexing='ij')

nvar = 8

files = sorted(glob.glob('./output/out*.dat'))

nout = len(files)

time = np.zeros(nout)
for nt in range(nout):
    t, uu = read_output_location(files[nt],0,0,0,nvar,nx,ny,nz)
    print('Read file: t = {:.3f}'.format(t))

    # magnetic field spectrum
    #-----------------------------------------------
    print('Begin reading file...')
    time0 = perf_counter()

    tmp, bx = read_output_onevariable(files[nt], 4,nvar,nx,ny,nz)
    tmp, by = read_output_onevariable(files[nt], 5,nvar,nx,ny,nz)
    tmp, bz = read_output_onevariable(files[nt], 6,nvar,nx,ny,nz)


    time1 = perf_counter()
    print('Time spent: {:.3E} sec'.format(time1-time0))

    print('Begin Fourier transform...')
    time0 = perf_counter()
    bx_fft = np.fft.fftshift(np.fft.fftn(bx, norm='ortho'))
    by_fft = np.fft.fftshift(np.fft.fftn(by, norm='ortho'))
    bz_fft = np.fft.fftshift(np.fft.fftn(bz, norm='ortho'))
    time1 = perf_counter()
    print('Time spent: {:.3E} sec'.format(time1-time0))

    B_pow = np.abs(bx_fft)**2 + np.abs(by_fft)**2 +\
        np.abs(bz_fft)**2 


    np.save('./output/B_power_spectra_{:03d}.npy'.format(nt), B_pow)

    B_pow = np.load('./output/B_power_spectra_{:03d}.npy'.format(nt))

    fig = plt.figure(figsize=[12,4])
    ylim = [1e-3,5e5]
    sub = fig.add_subplot(131)
    sub.plot(kx, B_pow[:, ind_ky0, ind_kz0])

    # fit spectrum
    kmin = 1
    kmax = 10
    ind_k1 = np.where(np.abs(kx - kmin) == np.min(np.abs(kx - kmin)))[0][0]
    ind_k2 = np.where(np.abs(kx - kmax) == np.min(np.abs(kx - kmax)))[0][0]


    fit_ = np.polyfit(np.log10(kx[ind_k1:ind_k2]), np.log10(
        B_pow[ind_k1:ind_k2, ind_ky0, ind_kz0]), 1)
    y_fit = 10**(fit_[0] * np.log10(kx[ind_k1:ind_k2]) + fit_[1])
    sub.plot(kx[ind_k1:ind_k2], y_fit, ls='--', color='k',
        label=r'${:.2f}$'.format(fit_[0]))
    sub.legend()
    #---------

    sub.set_xlabel(r'$k_x \times 10 R_e$', fontsize=14)
    sub.set_xscale('log', base=10)
    sub.set_yscale('log', base=10)
    sub.set_ylim(ylim)

    sub = fig.add_subplot(132)
    sub.plot(ky, B_pow[ind_kx0, :, ind_kz0])

    # fit spectrum
    kmin = 1
    kmax = 10
    ind_k1 = np.where(np.abs(ky - kmin) == np.min(np.abs(ky - kmin)))[0][0]
    ind_k2 = np.where(np.abs(ky - kmax) == np.min(np.abs(ky - kmax)))[0][0]


    fit_ = np.polyfit(np.log10(ky[ind_k1:ind_k2]), np.log10(
        B_pow[ind_kx0, ind_k1:ind_k2, ind_kz0]), 1)
    y_fit = 10**(fit_[0] * np.log10(ky[ind_k1:ind_k2]) + fit_[1])
    sub.plot(ky[ind_k1:ind_k2], y_fit, ls='--', color='k',
        label=r'${:.2f}$'.format(fit_[0]))
    sub.legend()
    #---------

    sub.set_xlabel(r'$k_y \times 10 R_e$', fontsize=14)
    sub.set_xscale('log', base=10)
    sub.set_yscale('log', base=10)
    sub.set_ylim(ylim)

    sub = fig.add_subplot(133)
    sub.plot(kz, B_pow[ind_kx0, ind_ky0, :])

    # fit spectrum
    kmin = 1
    kmax = 10
    ind_k1 = np.where(np.abs(kz - kmin) == np.min(np.abs(kz - kmin)))[0][0]
    ind_k2 = np.where(np.abs(kz - kmax) == np.min(np.abs(kz - kmax)))[0][0]


    fit_ = np.polyfit(np.log10(kz[ind_k1:ind_k2]), np.log10(
        B_pow[ind_kx0, ind_ky0, ind_k1:ind_k2]), 1)
    y_fit = 10**(fit_[0] * np.log10(kz[ind_k1:ind_k2]) + fit_[1])
    sub.plot(kz[ind_k1:ind_k2], y_fit, ls='--', color='k',
        label=r'${:.2f}$'.format(fit_[0]))
    sub.legend()
    #---------

    sub.set_xlabel(r'$k_z \times 10 R_e$', fontsize=14)
    sub.set_xscale('log', base=10)
    sub.set_yscale('log', base=10)
    sub.set_ylim(ylim)

    fig.suptitle(r'$t = {:.3f}$'.format(t))
    fig.tight_layout(rect=[0,0,1,0.95])

    # plt.show()
    fig.savefig('./figure/B_power_spectra_1D_{:03d}.png'.format(nt), dpi=400)
    plt.close(fig)

    sys.exit()

    # # plot contour k
    # fig = plt.figure(figsize=[10,3])
    # sub = fig.add_subplot(131)
    # # pm = sub.pcolormesh(KK1_xy, KK2_xy, B_pow[:,:,ind_kz0],
    # #     norm=LogNorm(vmin=1e-2,vmax=B_pow.max()),
    # #     cmap='plasma',shading='gouraud')
    # # cb = fig.colorbar(pm,extend='both',shrink=0.6)
    # ct = sub.contour(KK1_xy, KK2_xy, B_pow[:,:,ind_kz0])
    # sub.set_xscale('symlog', base=10)
    # sub.set_yscale('symlog', base=10)
    # sub.set_xlabel(r'$k_x$', fontsize=14)
    # sub.set_ylabel(r'$k_y$', fontsize=14)
    # sub.set_aspect('equal')

    # sub = fig.add_subplot(132)
    # # pm = sub.pcolormesh(KK1_xz, KK2_xz, B_pow[:,ind_ky0,:],
    # #     norm=LogNorm(vmin=1e-2,vmax=B_pow.max()),
    # #     cmap='plasma',shading='gouraud')
    # # cb = fig.colorbar(pm,extend='both',shrink=0.6)
    # ct = sub.contour(KK1_xz, KK2_xz, B_pow[:,ind_ky0,:])
    # sub.set_xscale('symlog', base=10)
    # sub.set_yscale('symlog', base=10)
    # sub.set_xlabel(r'$k_x$', fontsize=14)
    # sub.set_ylabel(r'$k_z$', fontsize=14)
    # sub.set_aspect('equal')

    # sub = fig.add_subplot(133)
    # # pm = sub.pcolormesh(KK1_yz, KK2_yz, B_pow[ind_kx0,:,:],
    # #     norm=LogNorm(vmin=1e-2,vmax=B_pow.max()),
    # #     cmap='plasma',shading='gouraud')
    # # cb = fig.colorbar(pm,extend='both',shrink=0.6)
    # ct = sub.contour(KK1_yz, KK2_yz, B_pow[ind_kx0,:,:])
    # sub.set_xscale('symlog', base=10)
    # sub.set_yscale('symlog', base=10)
    # sub.set_xlabel(r'$k_y$', fontsize=14)
    # sub.set_ylabel(r'$k_z$', fontsize=14)
    # sub.set_aspect('equal')

    # fig.suptitle(r'$t = {:.3f}$'.format(t))
    # # fig.subplots_adjust(bottom=-0.2,top=1.2,wspace=0.5)
    # fig.tight_layout(rect=[0,0,1,0.95])

    # # fig.savefig('./figure/B_power_spectra_2D_{:03d}.png'.format(nt), dpi=400)
    # plt.show()
    # plt.close(fig)

    # sys.exit()



    # density spectrum
    tmp, rho = read_output_onevariable(files[nt], 0,nvar,nx,ny,nz)


    # rho_xy = np.average(rho,axis=2)

    # pow = np.fft.fftshift(np.abs(np.fft.fft2(rho_xy, norm='ortho'))**2)


    # plt.plot(kx,pow[:,ind_ky0])
    # plt.plot(ky,pow[ind_kx0,:])

    # pow_diag = np.zeros(len(kx))
    # for i in range(len(kx)):
    #     pow_diag[i] = pow[i,i]

    # plt.plot(np.sqrt(kx**2 + ky**2),pow_diag)
    # plt.xscale('log', base=10)
    # plt.yscale('log', base=10)
    # plt.ylim([1e-6,1e-2])

    # plt.show()

    # sys.exit()

    # fig = plt.figure()
    # sub = fig.add_subplot(111)
    # ct = sub.pcolormesh(KK1_xy, KK2_xy, pow, shading='gouraud',
    #     norm=LogNorm(vmin=1e0,vmax=2e4))
    # sub.set_xscale('symlog' ,base=10)
    # sub.set_yscale('symlog',base=10)
    # sub.set_aspect('equal')
    # cb = fig.colorbar(ct)
    # plt.show()
    # sys.exit()


    rho_fft = np.fft.fftshift(np.fft.fftn(rho, norm='ortho'))
    rho_pow = np.abs(rho_fft)**2

    # plot contour k
    fig = plt.figure(figsize=[10,3])
    sub = fig.add_subplot(131)
    # pm = sub.pcolormesh(KK1_xy, KK2_xy, B_pow[:,:,ind_kz0],
    #     norm=LogNorm(vmin=1e-2,vmax=B_pow.max()),
    #     cmap='plasma',shading='gouraud')
    # cb = fig.colorbar(pm,extend='both',shrink=0.6)
    ct = sub.contour(KK1_xy, KK2_xy, rho_pow[:,:,ind_kz0])
    sub.set_xscale('symlog', base=10)
    sub.set_yscale('symlog', base=10)
    sub.set_xlabel(r'$k_x$', fontsize=14)
    sub.set_ylabel(r'$k_y$', fontsize=14)
    sub.set_aspect('equal')

    sub = fig.add_subplot(132)
    # pm = sub.pcolormesh(KK1_xz, KK2_xz, B_pow[:,ind_ky0,:],
    #     norm=LogNorm(vmin=1e-2,vmax=B_pow.max()),
    #     cmap='plasma',shading='gouraud')
    # cb = fig.colorbar(pm,extend='both',shrink=0.6)
    ct = sub.contour(KK1_xz, KK2_xz, rho_pow[:,ind_ky0,:])
    sub.set_xscale('symlog', base=10)
    sub.set_yscale('symlog', base=10)
    sub.set_xlabel(r'$k_x$', fontsize=14)
    sub.set_ylabel(r'$k_z$', fontsize=14)
    sub.set_aspect('equal')

    sub = fig.add_subplot(133)
    pm = sub.pcolormesh(KK1_yz, KK2_yz, rho_pow[ind_kx0,:,:],
        norm=LogNorm(vmin=1e-4,vmax=1e2),
        cmap='plasma',shading='gouraud')
    cb = fig.colorbar(pm,extend='both',shrink=0.6)
    # levels = 10**(np.arange(0,6.1,1))
    # print(levels)
    # ct = sub.contour(KK1_yz, KK2_yz, rho_pow[ind_kx0,:,:], levels=levels)
    # cb = fig.colorbar(ct)
    sub.set_xscale('symlog', base=10)
    sub.set_yscale('symlog', base=10)
    sub.set_xlabel(r'$k_y$', fontsize=14)
    sub.set_ylabel(r'$k_z$', fontsize=14)
    sub.set_aspect('equal')

    fig.suptitle(r'$t = {:.3f}$'.format(t))
    # fig.subplots_adjust(bottom=-0.2,top=1.2,wspace=0.5)
    fig.tight_layout(rect=[0,0,1,0.95])

    plt.show()
    plt.close(fig)

    sys.exit()