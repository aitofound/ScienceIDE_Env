import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import RegularGridInterpolator

flicks_dict = pd.read_pickle('dict_of_flicks.pickle')

# Number of steps in simulation
num_steps = len(flicks_dict)

# flick numbers ordered
flick_keys = sorted(flicks_dict.keys())

#Hard code array sizes. Is there a way to get them directly from the pickle?
nx = 889
ny = 326
Rsun = 6.95508e8 #solar radius in m.

# X,Y,Z meshes are time-independent so read only once
xmesh = np.asarray(flicks_dict[flick_keys[0]]['X[Rs]']).reshape(nx,ny)
zmesh = np.asarray(flicks_dict[flick_keys[0]]['Y[Rs]']).reshape(nx,ny) # Y is actually Z (ie. 2D axes are the xz plane)
ymesh = np.asarray(flicks_dict[flick_keys[0]]['Z[Rs]']).reshape(nx,ny) # 

#Cartesian grid.
#X,Z are regular grids so just pull a single row/column
#Origin is at solar center
x = xmesh[:,0] #*Rsun # m
z = zmesh[0,:] #*Rsun # m
y = ymesh[0,0] #*Rsun # m

#Extract MHD variables as functions of time,x,z and convert to SI units
time = np.array( [flicks_dict[k]['time[s]'][0]  for k in flick_keys] ) #s
flick = np.array( [flicks_dict[k]['flick[number]'][0] for k in flick_keys] )
rho = np.array( [np.asarray(flicks_dict[k]['Rho[gr/cm^3]/1.0e-16']*1e-16*1e3).reshape(nx,ny) for k in flick_keys]) #kg/m^3
vr = np.array([np.asarray(flicks_dict[k]['V_r[km/s]']*1e3).reshape(nx,ny) for k in flick_keys]) #m/s
vt = np.array([np.asarray(flicks_dict[k]['V_theta_lat[km/s]']*1e3).reshape(nx,ny) for k in flick_keys]) #m/s
vp = np.array([np.asarray(flicks_dict[k]['V_phi_azim[km/s]']*1e3).reshape(nx,ny) for k in flick_keys]) #m/s
P = np.array([np.asarray(flicks_dict[k]['P[dyn/cm^2]']*1e-1).reshape(nx,ny) for k in flick_keys]) #Pressure (N/m^2)
T = np.array([np.asarray(flicks_dict[k]['T[K/1.6e+6]']*1.6e6).reshape(nx,ny) for k in flick_keys]) # Temperature (K)
Br = np.array([np.asarray(flicks_dict[k]['B_r[G]']*1e-4).reshape(nx,ny) for k in flick_keys]) #Tesla
Bt = np.array([np.asarray(flicks_dict[k]['B_theta_lat[G]']*1e-4).reshape(nx,ny) for k in flick_keys]) #Tesla
Bp = np.array([np.asarray(flicks_dict[k]['B_phi_azim[G]']*1e-4).reshape(nx,ny) for k in flick_keys]) #Tesla

#Convert Spherical 2.5D vector fields to Cartesian
r = np.sqrt(xmesh**2+ymesh**2+zmesh**2)
theta = np.arccos(zmesh/r)
phi = np.arctan2(ymesh,xmesh)
def sph_to_cart_field(fr,ft,fp):
  fx = np.sin(theta)*np.cos(phi)*fr + np.cos(theta)*np.cos(phi)*ft - np.sin(phi)*fp
  fy = np.sin(theta)*np.sin(phi)*fr + np.cos(theta)*np.sin(phi)*ft + np.cos(phi)*fp
  fz = np.cos(theta)*fr - np.sin(theta)*ft
  return fx,fy,fz

Bx,By,Bz = sph_to_cart_field(Br,Bt,Bp)
vx,vy,vz = sph_to_cart_field(vr,vt,vp)

#As a first step, pick a time-step to use. Later we can interpolate in time as well.
it = -1 # Use the last time step as an example

#As an example plot |B|
fig,ax = plt.subplots() 
axisnorm = 1e8
extent = np.asarray([x[0],x[-1],z[0],z[-1]])/axisnorm
im = ax.imshow( np.sqrt(Bx[it]**2+By[it]**2+Bz[it]**2).T, extent = extent,origin='lower', interpolation = 'bilinear')
ax.set(xlabel = 'X (10$^{8}$ m)', ylabel='Z (10$^{8}$ m)',title = '|B|')

#Set up interpolators.
#	Do linear interpolation but higher orders can be set by switching method to 'cubic' or 'quintic'.
#	Set values outside the domain to be 0.
def interp_func(V):
   return RegularGridInterpolator( (x,z), V, method = 'linear', bounds_error = False, fill_value =0.)

# Define interpolator functions for each MHD variable.
# So for example Bx_interp( (x0,z0) ) will return Bx evaulated at the point (x0,z0)
Bx_interp = interp_func(Bx[it]) 
By_interp = interp_func(By[it])
Bz_interp = interp_func(Bz[it])
vx_interp = interp_func(vx[it])
vy_interp = interp_func(vy[it])
vz_interp = interp_func(vz[it])
rho_interp = interp_func(rho[it])
T_interp = interp_func(T[it])
P_interp = interp_func(P[it])

#file with input points
infile = 'points_in.txt'
fpin = open(infile,'r')

#file to store output interpolated value.
outfile = 'interp_out.txt'
fpout = open(outfile,'w')

#Assumes infile is formatted with values for x, y, z on each line.
#In doing interpolation y will be ignored since the ARMS simulation
#is 2D in the XZ plane

#Formatted output string
strfmt = lambda f: "{:.6e}".format(f)
headfmt = lambda s: s.ljust(12)
#Columns
header = ['X (m)', 'Y (m)', 'Z (m)', 'Bx (T)', 'By (T)', 'Bz (T)', 'vx (m/s)', 'vy (m/s)', 'vz (m/s)', 'rho (kg/m^3)', 'T (K)', 'P (N/m^2)']
fpout.write("\t".join(map(headfmt, header)) + "\n")

for line in fpin:
   if line.strip()[0] in '#!;c': continue # continue if comment line
   xin,yin,zin = tuple(map(float,line.split()))
   #Plot an 'X' in the |B| plot at each interpolated point. This should be removed for real runs so the image is not clogged.
   plt.plot(xin/axisnorm,zin/axisnorm,marker = 'x')
   vals = ( xin, yin, zin, 
            Bx_interp( (xin, zin)), By_interp( (xin, zin)), Bz_interp( (xin, zin)), 
            vx_interp( (xin, zin)), vy_interp( (xin, zin)), vz_interp( (xin, zin)),
            rho_interp( (xin, zin)), T_interp( (xin, zin)), P_interp( (xin, zin)) )
   lineout = "\t".join(map(strfmt, vals)) + "\n"
   fpout.write(lineout)

fpout.close()
fpin.close()
plt.savefig('bmag.pdf')
