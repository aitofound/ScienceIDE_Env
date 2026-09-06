import numpy as np
import struct
import matplotlib.pyplot as plt
from matplotlib import cm
from matplotlib.gridspec import GridSpec
import glob
import sys
import plotly.graph_objs as go
from plotly.subplots import make_subplots


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


def read_output_location(filename,ix,iy,iz,nvar,nx,ny,nz):
    file = open(filename, 'rb')

    skip = (struct.unpack("f",file.read(4)))[0]
    t = (struct.unpack("f",file.read(4)))[0] 
    skip = (struct.unpack("f",file.read(4)))[0]

    offset = file.tell()

    vals = np.zeros(nvar)
    for iv in range(nvar):
        indx =  ((iv * nz + iz) * ny + iy) * nx + ix
        file.seek(indx * 8 + offset)
        vals[iv] = struct.unpack('d',file.read(8))[0]

    file.close()

    return t, vals

def read_output_slice(filename,indices,nvar,nx,ny,nz):
    indices = np.array(indices)
    dims = np.array([nx,ny,nz], dtype=int)

    axis = np.where(indices==-1)[0]
    if len(axis)!=2:
        print('In read_output_slice: two axes need to be specified!')
        return None, None

    axis_fix = np.where(indices>=0)[0][0]

    arr = np.zeros(np.append(nvar,dims[axis]))

    file = open(filename,'rb')

    skip = (struct.unpack("f",file.read(4)))[0]
    t = (struct.unpack("f",file.read(4)))[0] 
    skip = (struct.unpack("f",file.read(4)))[0]

    offset = file.tell()
    offset_now = offset

    loc = np.zeros(3,dtype=int)
    loc[axis_fix] = indices[axis_fix]
    
    for iv in range(nvar):
        for i2 in range(dims[axis[1]]):
            loc[axis[1]] = i2
            for i1 in range(dims[axis[0]]):
                loc[axis[0]] = i1 
        
                indx = ((iv * nz + loc[2]) * ny + loc[1]) * nx + loc[0]
                offset_new = indx*8 + offset

                if offset_new != offset_now:
                    file.seek(offset_new - offset_now, 1)

                arr[iv,i1,i2] = struct.unpack('d',file.read(8))[0]
                offset_now = file.tell()

    file.close()

    return t, arr

def read_output_axis(filename,indices,nvar,nx,ny,nz):
    indices = np.array(indices)
    dims = np.array([nx,ny,nz], dtype=int)

    axis = np.where(indices==-1)[0]
    if len(axis)!=1:
        print('In read_output_slice: One axis needs to be specified!')
        return None, None

    axis_fix = np.where(indices>=0)[0]

    arr = np.zeros(np.append(nvar,dims[axis]))

    file = open(filename,'rb')

    skip = (struct.unpack("f",file.read(4)))[0]
    t = (struct.unpack("f",file.read(4)))[0] 
    skip = (struct.unpack("f",file.read(4)))[0]

    offset = file.tell()
    offset_now = offset

    loc = np.zeros(3,dtype=int)
    loc[axis_fix] = indices[axis_fix]

    for iv in range(nvar):

        for i1 in range(dims[axis[0]]):
            loc[axis[0]] = i1 
            
            indx = ((iv * nz + loc[2]) * ny + loc[1]) * nx + loc[0]
            offset_new = indx*8 + offset

            if offset_new != offset_now:
                file.seek(offset_new - offset_now, 1)
                
            arr[iv,i1] = struct.unpack('d',file.read(8))[0]
            offset_now = file.tell()

    file.close()

    return t, arr


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


# read grid -------
xgrid, ygrid, zgrid = read_grid()
nx,ny,nz = len(xgrid), len(ygrid), len(zgrid)
print('nx, ny, nz =', nx, ny, nz)

# read parallelization info -----
npe, iproc, jproc, nvar = read_parallel_info()                                 
print('npe, iproc, jproc, nvar = ', npe, iproc, jproc, nvar)

# read EBM info
t_EBM, radius, Ur = read_EBM()

# generate 2D mesh
XX, YY = np.meshgrid(xgrid,ygrid,indexing='ij')
XX1, ZZ1 = np.meshgrid(xgrid,zgrid,indexing='ij')


files = sorted(glob.glob('./output/out*dat'))
nout = len(files)


time = np.zeros(nout)

kx = np.fft.fftshift(np.fft.fftfreq(nx))
kz = np.fft.fftshift(np.fft.fftfreq(nz))

kkx, kkz = np.meshgrid(kx,kz,indexing='ij')


fieldnames = ['rho', 'ux', 'uy', 'uz', 'bx', 'by', 'bz', 'p']
cmaps = ['plasma', 'RdBu', 'RdBu', 'RdBu', 'RdBu', 'RdBu', 'RdBu', 'plasma']
cranges = [[], [], [], [], [], [], [], []]

for nt in range(nout):
    t, uu = read_output_location(files[nt],0,0,0,nvar,nx,ny,nz)

    print('t = {:.3f}'.format(t))

    time[nt] = t 


    # # plot along one line
    # tmp, uu_x = read_output_axis(files[nt], [-1,int(ny/2),int(nz/2)], nvar, nx,ny,nz)
    # # plt.plot(xgrid,uu_x[0,:])
    # plt.plot(xgrid,uu_x[5,:])
    # plt.plot(xgrid,uu_x[6,:])

    # plt.show()
    # sys.exit()


    
    # plot two slices----------------------------------------
    # read data x-y plane at z-center
    iz = int(nz/2)
    tmp, uu_xy = read_output_slice(files[nt],[-1,-1,iz],nvar,nx,ny,nz)


    # read data  x-z plane at y-center
    iy = int(ny/2)
    tmp, uu_xz = read_output_slice(files[nt],[-1,iy,-1],nvar,nx,ny,nz)

    
    # plot figure 
    for iv in range(nvar):
        filename = './figure/' + fieldnames[iv] + '_{:03d}.png'.format(nt)

        u_xz = uu_xz[iv,:,:]
        u_xy = uu_xy[iv,:,:]

        # for slices in x-z plane
        YY1 = np.zeros(XX1.shape) + ygrid[int(ny/2)]

        surfcolor = u_xz

        slice_xz = go.Surface(x=XX1,y=YY1,z=ZZ1,
            surfacecolor=surfcolor, colorscale=cmaps[iv] ,showscale=True,
            colorbar=dict(thickness=20, ticklen=4, 
                lenmode='fraction', len=0.75))  # tickvals=cticks[iv]

        if len(cranges[iv])==2:
            slice_xz.update(cmin=cranges[iv][0], cmax=cranges[iv][1])

        #axis properties which will be used in layout
        axis = dict(showbackground=True, 
                backgroundcolor="rgb(230, 230,230)",
                gridcolor="rgb(255, 255, 255)",      
                zerolinecolor="rgb(255, 255, 255)")
        
        #some how, if we use greek symbol $\rho$, the fontsize behaves strange...
        #for axis, we can either use titlefont=dict(size=20), 
        # or use title=dict(text='Y',font=dict(size=20))
        layout = go.Layout(title=dict(text=(fieldnames[iv]) + ' t = {:.3f}'.format(t),
                font=dict(size=20),x=0.5,y=0.9,xanchor='center',yanchor='top'), 
                width=500,height=500,
                scene=dict(  xaxis=dict(axis,range=[0,1],titlefont=dict(size=20)),
                yaxis=dict(axis,range=[0,1],title=dict(text='y',font=dict(size=20)) ) ,
                zaxis=dict(axis,range=[0,1],titlefont=dict(size=20)),
                aspectratio=dict(x=1, y=1, z=1)))

        # generate the figure
        fig = go.Figure(data=[slice_xz], layout=layout)

        # # add a line along the axis
        # line_axis = go.Scatter3d(x=np.array([xgrid[int(nx/2)], xgrid[int(nx/2)]]), 
        #     y=np.array([ygrid[int(ny/2)], ygrid[int(ny/2)]]), 
        #     z=np.array([zgrid[0], zgrid[-1]]), mode='lines', 
        #     line=dict(color='black',width=5))

        # fig.add_trace(line_axis)


        # add x-y surface
        ZZ = np.zeros(XX.shape) + zgrid[int(nz/2)]

        surfcolor = u_xy

        slice_xy = go.Surface(x=XX,y=YY,z=ZZ,
            surfacecolor=surfcolor, colorscale=cmaps[iv],showscale=False)

        if (len(cranges[iv])==2):
            slice_xy.update(cmin=cranges[iv][0],cmax=cranges[iv][1])

        fig.add_trace(slice_xy)


        #adjust the camera and aspect ratio
        camera = dict(up=dict(x=0,y=0,z=1), center = dict(x=0, y=0, z=0), 
            eye = dict(x=-1.2,y=-2.0,z=1.3))
        fig.update_layout(scene_camera = camera)

        # fig.show()

        fig.write_image(filename,scale=5) # use scale to increase resolution

        fig = None
        #------------------------------------------------
        

    # calculate v-b correlation
    t, uu = read_uu(files[nt], nx, ny, nz, nvar)

    ux = uu[:,:,:,1]
    uy = uu[:,:,:,2]
    uz = uu[:,:,:,3]
    bx = uu[:,:,:,4]
    by = uu[:,:,:,5]
    bz = uu[:,:,:,6]

    uu = None 

    ux1 = ux - np.average(ux)
    uy1 = uy - np.average(uy)
    uz1 = uz - np.average(uz)

    bx1 = bx - np.average(bx)
    by1 = by - np.average(by)
    bz1 = bz - np.average(bz)


    rho_vb = (ux1*bx1+uy1*by1+uz1*bz1)/np.sqrt(ux1**2+uy1**2+uz1**2)\
        /np.sqrt(bx1**2+by1**2+bz1**2)

    print(np.average(rho_vb))



np.save('./output/time.npy',time)


