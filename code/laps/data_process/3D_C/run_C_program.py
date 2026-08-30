import subprocess
import sys 
sys.path.append('../')
from functions import *


method = 1


# read grid -------
xgrid, ygrid, zgrid = read_grid()
nx,ny,nz = len(xgrid), len(ygrid), len(zgrid)
print('nx, ny, nz =', nx, ny, nz)

# read parallelization info -----
npe, iproc, jproc, nvar = read_parallel_info()                                 
print('npe, iproc, jproc, nvar = ', npe, iproc, jproc, nvar)

# read EBM info
t_EBM, radius, Ur = read_EBM()



nt = 20
# run the C program
result = subprocess.run(["./main.exe", "{:d}".format(nt), "{:d}".format(method)], 
    capture_output=True, text=True)

# Write the output to a file
with open("log_run_nt{:03d}.txt".format(nt), "w") as file:
    file.write(result.stdout)

