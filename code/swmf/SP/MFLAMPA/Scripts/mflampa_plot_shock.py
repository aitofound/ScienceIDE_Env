#!/usr/bin/env python3
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf
'''
Provide a plotting example for visualizing the shock surface
by MH_data_Shock_* data, written by Weihao Liu
'''

import os
import re
import argparse
import configparser
import numpy as np
import datetime as dt
from scipy.spatial import Delaunay
import matplotlib.pyplot as plt
from matplotlib import cm
from matplotlib.colors import Normalize
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

# Set default values
default_param_file = 'Param/' + 'PARAM.ini.test.201304.shock'
default_path = 'run_actual13_test_shock4/' + 'SP/IO2/'
# Fontstyple
plt.rc('text', usetex=True); plt.rc('font', family='serif', size=14)

# Create an ArgumentParser object
parser = argparse.ArgumentParser(description='Process some paths and parameters.')

# Add an argument for the parameter file
parser.add_argument('--PARAMin', type=str, default=default_param_file,
                    help='Specify the parameter file')
# Add an argument for the path with a default value
parser.add_argument('--PATH', type=str, default=default_path, 
                    help=f'Specify the path (default: {default_path})')

# Parse the arguments
args = parser.parse_args()

# Retrieve the path and parameter file
path = args.PATH
param_file = args.PARAMin

# Get the config file and path from command-line arguments if provided
# param_file = --PARAMin PARAMIN else default_param_file
# path = --PATH PATH else default_path

# Create a ConfigParser object
config = configparser.ConfigParser()
# Load the config file
config.read(param_file)
nFileRead = int(config['MHDATA']['nFileRead'])

# Declare shock coordinate variable number
nShockVar = 8

#==========================================================================================================
def read_line(filename, linenum):
    # Given filename, read No.{linenum} in this file, and return string
    try:
        with open(filename, 'r') as file:
            lines = file.readlines()
            return lines[linenum - 1].strip()
    except FileNotFoundError:
        return f"File {filename} not found."
    except IndexError:
        return f"File {filename} does not have {filename} lines."
    except Exception as e:
        return f"An error occurred: {e}"
#==========================================================================================================
# Colorbar plot
def add_cax(ax, pad, width, loc="right"):
    '''
    Add a panel to the right/top with the same height
    pad = cadence between cax & ax
    width = the width of cax 
    From: https://zhajiman.github.io/post/matplotlib_colorbar/
    '''
    import matplotlib.transforms as mtransforms
    axpos = ax.get_position()
    if loc == "left":
        caxpos = mtransforms.Bbox.from_extents(
            axpos.x1 - pad,
            axpos.y0,
            axpos.x1 - pad - width,
            axpos.y1
        )
    elif loc == "right":
        caxpos = mtransforms.Bbox.from_extents(
            axpos.x1 + pad,
            axpos.y0,
            axpos.x1 + pad + width,
            axpos.y1
        )
    elif loc == "top":
        caxpos = mtransforms.Bbox.from_extents(
            axpos.x0,
            axpos.y1 + pad,
            axpos.x1,
            axpos.y1 + pad + width
        )
    elif loc == "bottom":
        caxpos = mtransforms.Bbox.from_extents(
            axpos.x0,
            axpos.y1 - pad,
            axpos.x1,
            axpos.y1 - pad - width
        )
    cax = ax.figure.add_axes(caxpos)

    return cax
#==========================================================================================================
def load_shock_files():
    # Get all the files in the path
    files = sorted(os.listdir(path))
    dic_files_shock = {}
    
    # Filter the files with "MH_data_Shock_e*_*_n*.out"
    files_shock = []; files_time = []
    fmt_file_shock = re.compile(r"^MH_data_Shock_e\d{8}_\d{6}_n\d{6}\.out$")
    for file in files:
        if fmt_file_shock.match(file):
            files_shock.append(file)
            files_time.append(dt.datetime(year=int(file[15:19]), month=int(file[19:21]), \
                day=int(file[21:23]), hour=int(file[24:26]), minute=int(file[26:28])))
    files_shock = np.array(files_shock)
    dic_files_shock['datetime'] = np.array(files_time)
    
    # Read the header
    headerShock = read_line(path+files_shock[0], linenum=5)
    print("Shock file header:", headerShock)
    headerShock = headerShock.split(" ")
    for iHeader in range(len(headerShock)-1, -1, -1):
        if headerShock[iHeader] == "": del headerShock[iHeader]
    dic_files_shock['header'] = headerShock
    
    # Read the files for the shock surface
    print("... Reading in shock files ...")
    for iFile in range(nFileRead):
        # Make sure the file does exist
        if path+files_shock[iFile] is None:
            print("file %s does not exist"%(path+files_shock[iFile]))
            return
        CoorShock_II = np.loadtxt(path+files_shock[iFile], skiprows=5, \
            unpack=True, usecols=[i for i in range(nShockVar)], dtype=float)
        CompRatio_I = np.loadtxt(path+files_shock[iFile], skiprows=5, \
            unpack=True, usecols=[nShockVar], dtype=str)
        
        # Redo CompRatio
        CompRatioRe_I = np.zeros(len(CompRatio_I))
        for i in range(len(CompRatio_I)):
            if "E-" in CompRatio_I[i] or "E+" in CompRatio_I[i]:
                CompRatioRe_I[i] = float(CompRatio_I[i])
            else:
                CompRatioRe_I[i] = -1.0
        # Merge
        CoorShock_II = np.array(list(CoorShock_II)+[CompRatioRe_I])
        
        # Remove those very large values (bugs)
        mskEffShockLine1 = CoorShock_II[-1] > 0.0
        RShockMedian = np.median(CoorShock_II[5][mskEffShockLine1])
        mskEffShockLine2 = CoorShock_II[5][mskEffShockLine1] < 2.0*RShockMedian
        # Get the effective field lines constructing the shock surface
        CoorShockEff_II = CoorShock_II[:, mskEffShockLine1][:, mskEffShockLine2]
        dic_files_shock['%d'%(iFile)] = CoorShockEff_II
    
    # Return dictionary
    return dic_files_shock
#==========================================================================================================
def plot_shock_surf(CoorXyz_IB, time, values_I, DoSave=False, \
    figname='Delaunay3d_shock_sphere', DoShow=False):

    # Perform Delaunay triangulation in 3D
    points = CoorXyz_IB
    mesh = Delaunay(points)

    # Define the tetrahedra
    tetrahedra = mesh.simplices
    # print(tetrahedra)

    # Create a new figure and 3D axis
    fig = plt.figure(figsize=(5.5, 4.5))
    fig.subplots_adjust(left=-0.22, right=0.88, top=0.99, bottom=0.04)
    ax = fig.add_subplot(111, projection='3d'); ax.view_init(22.5, -22.5)
    plt.title(r"3D Shock Surface Triangulation, at %.4d-%.2d-%.2d/%.2d:%.2d" \
        %(time.year, time.month, time.day, time.hour, time.minute), x=0.68, y=1.02, fontsize=14)

    # Define colormap and normalization
    vmin = 1.0; vmax = 2.0
    print("vmin = %.3f, vmax = %.3f, val_min = %.3f, values_max = %.3f"% \
        (vmin, vmax, np.min(values_I), np.max(values_I)))
    norm = Normalize(vmin=vmin, vmax=vmax)
    cmap = cm.rainbow
    
    # Plot the points with colors based on their values
    sc = ax.scatter(points[:, 0], points[:, 1], points[:, 2], \
        c=values_I, cmap=cmap, norm=norm, marker='o', s=1)  # Reduced size

    # Plot the Delaunay tetrahedra
    for tet in tetrahedra:
        verts = points[tet[1:]]
        poly = Poly3DCollection([verts], alpha=1.0, edgecolor=cmap(norm(np.mean(values_I[tet[1:]]))), norm=norm, linewidths=0.22)
        # Color the tetrahedra based on some criteria (e.g., centroid value)
        centroid_value = np.mean(values_I[tet[1:]])
        poly.set_facecolor(cmap(norm(centroid_value)))
        ax.add_collection3d(poly)
        
        # # Plot the edges of the tetrahedron with colors based on the vertex values
        # edges = [
        #     [tet[0], tet[1]],
        #     [tet[0], tet[2]],
        #     [tet[1], tet[2]]
        # ]
        # for edge in edges:
        #     edge_color = cmap(norm(np.mean(values_I[edge])))  # Average value of the edge vertices
        #     ax.plot3D([points[edge][0][0], points[edge][1][0]], \
        #             [points[edge][0][1], points[edge][1][1]], \
        #             [points[edge][0][2], points[edge][1][2]], color=edge_color, lw=1.225)

    # Add colorbar with custom aspect ratio for thinness
    # cax = add_cax(ax=ax, pad=0.03, width=0.02, loc="right")
    cbar = plt.colorbar(sc, ax=ax, fraction=0.02, pad=0.1, aspect=29)  # Adjust colorbar
    cbar.set_label('Density Compression Ratio')
    cbar.set_ticks(np.arange(vmin, vmax+0.1, 0.2))
    cbar.set_ticklabels(["%.1f"%(i) for i in np.arange(vmin, vmax+0.1, 0.2)], fontsize=12)

    # Set labels
    ax.set_xlabel(r'X [$R_\mathrm{sun}$]'); ax.tick_params(axis='x', which='major', pad=0)
    ax.set_ylabel(r'Y [$R_\mathrm{sun}$]'); ax.tick_params(axis='y', which='major', pad=0)
    ax.set_zlabel(r'Z [$R_\mathrm{sun}$]'); ax.tick_params(axis='z', which='major', pad=0)

    # Set equal aspect ratio
    ax.set_box_aspect([1, 1, 1])  # Aspect ratio is 1:1:1

    # Show/save the figure
    if(DoSave):
        fig.savefig(default_path+"../"+figname+".pdf", transparent=True)
        fig.savefig(default_path+"../"+figname+".png", dpi=768)
    if(DoShow): plt.show()
#==========================================================================================================

if __name__ == "__main__":
    # Load the data from simulations
    dic_files_shock = load_shock_files()
    XShock_ = dic_files_shock['header'].index("XShock")
    ZShock_ = dic_files_shock['header'].index("ZShock")
    CompRatio_ = dic_files_shock['header'].index("CompRatio")

    # Plot and show the shock surface by Delaunay triangulation
    for iFiles in range(11, 12, 1):
        time = dic_files_shock['datetime'][iFiles]
        print("time = ", time)
        # print(dic_files_shock['%d'%(iFiles)][CompRatio_, :])
        plot_shock_surf(CoorXyz_IB=dic_files_shock['%d'%(iFiles)][XShock_:ZShock_+1, :].T, \
            time=time, values_I=dic_files_shock['%d'%(iFiles)][CompRatio_, :], DoShow=True)
