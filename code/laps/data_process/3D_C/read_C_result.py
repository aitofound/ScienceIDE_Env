import sys 
sys.path.append('../')
from functions import *


def read_1d_array_generated_by_C(filename, n):
    file = open(filename,'rb')

    # arr = np.zeros(n)
    # for i in range(n):
    #     arr[i] = struct.unpack("d",file.read(8))[0]

    arr = np.fromfile(filename,float,n)

    file.close()

    return arr


# read grid -------
xgrid, ygrid, zgrid = read_grid()
nx,ny,nz = len(xgrid), len(ygrid), len(zgrid)
print('nx, ny, nz =', nx, ny, nz)

# read parallelization info -----
npe, iproc, jproc, nvar = read_parallel_info()                                 
print('npe, iproc, jproc, nvar = ', npe, iproc, jproc, nvar)

# read EBM info
t_EBM, radius, Ur = read_EBM()



method = 1
nt = 0


theta_ub_lx = read_1d_array_generated_by_C(
    './output/theta_ub_perp_only_lx_method{:1d}_{:03d}.dat'.format(method,nt),nx)
theta_ub_ly = read_1d_array_generated_by_C(
    './output/theta_ub_perp_only_ly_method{:1d}_{:03d}.dat'.format(method,nt),nx)
theta_ub_lz = read_1d_array_generated_by_C(
    './output/theta_ub_perp_only_lz_method{:1d}_{:03d}.dat'.format(method,nt),nx)


theta_zpm_lx = read_1d_array_generated_by_C(
    './output/theta_zpm_perp_only_lx_method{:1d}_{:03d}.dat'.format(method,nt),nx)
theta_zpm_ly = read_1d_array_generated_by_C(
    './output/theta_zpm_perp_only_ly_method{:1d}_{:03d}.dat'.format(method,nt),nx)
theta_zpm_lz = read_1d_array_generated_by_C(
    './output/theta_zpm_perp_only_lz_method{:1d}_{:03d}.dat'.format(method,nt),nx)



fig = plt.figure(figsize=[11,5])
sub1 = fig.add_subplot(121)
sub1.plot(xgrid[1:], theta_ub_lx[1:] * 180 / np.pi)
sub1.plot(ygrid[1:], theta_ub_ly[1:] * 180 / np.pi)
sub1.plot(zgrid[1:], theta_ub_lz[1:] * 180 / np.pi)
sub1.set_xscale('log',base=10)


sub2 = fig.add_subplot(122)
sub2.plot(xgrid[1:], theta_zpm_lx[1:] * 180 / np.pi)
sub2.plot(ygrid[1:], theta_zpm_ly[1:] * 180 / np.pi)
sub2.plot(zgrid[1:], theta_zpm_lz[1:] * 180 / np.pi)
sub2.set_xscale('log',base=10)

plt.show()

