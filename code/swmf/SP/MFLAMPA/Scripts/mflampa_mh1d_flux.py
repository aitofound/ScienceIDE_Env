#!/usr/bin/env python3
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf
'''
Provide a plotting example for visualizing the MFLAMPA MH1D outputs
Firstly written by Dr. Lulu Zhao and later modified by Weihao Liu
'''

# Load all modules
import os
import glob
import pandas as pd
import numpy as np
import matplotlib
import matplotlib.pyplot as plt

# Set globle variables
cMu0 = 1.25663706E-6
nLon = 4; nLat = 4; nTime = 6
cols = ['LagrID', 'X', 'Y', 'Z', 'Rho', 'T', 'Ux', 'Uy', 'Uz', 'Bx', 'By', 'Bz',
       'Wave1', 'Wave2', 'flux_total', 'flux_Channel01', 'flux_Channel02',
       'flux_Channel03', 'flux_Channel04', 'flux_Channel05', 'flux_Channel06', 'eflux']
# Set colors and plotting styles
colors = [matplotlib.cm.jet(x) for x in np.linspace(0, 1, nTime+1)]
plt.rc('text', usetex=True); plt.rc('font', family='serif', size=12)

#==========================================================================================================
def find_rootdir():
    # Go to the rootdir (absolute directory), i.e., SP/MFLAMPA in the system

    dirCurr = os.getcwd()
    strdir = "SP/MFLAMPA"
    dirCurrId = dirCurr.index(strdir)
    if(dirCurrId < 0):
        print("Warning: Incorrect directory! Check the path!")
    rootdir = dirCurr[0:dirCurrId+len(strdir)]
    return rootdir
#==========================================================================================================
def plot_mh1d_flux_1line_1time(ifile, filename, ax):
    # Plot for each small panel

    data = pd.read_csv(filename, skiprows=16, sep='\s+', names=cols)
    data.columns = data.columns.str.lower()
    data['ux'] *= 1.0E-3; data['uy'] *= 1.0E-3; data['uz'] *= 1.0E-3
    data['r'] = np.sqrt(data['x']**2+data['y']**2+data['z']**2)
    data['u'] = np.sqrt(data['ux']**2+data['uy']**2+data['uz']**2)
    data['b'] = np.sqrt(data['bx']**2+data['by']**2+data['bz']**2)
    data['db2'] = cMu0*(data['wave1']+data['wave2'])

    # Subplot
    ax[0,0].plot(data['r'],data['rho']*data['r']**2, color=colors[ifile])
    ax[1,0].plot(data['r'],data['u'], color=colors[ifile])
    ax[2,0].plot(data['r'],data['db2']/data['b']**2, color=colors[ifile])

    ax[0,1].plot(data['r'],data['flux_channel01'], label=ifile, color=colors[ifile])
    ax[1,1].plot(data['r'],data['flux_channel02'], label=ifile, color=colors[ifile])
    ax[2,1].plot(data['r'],data['flux_channel06'], label=ifile, color=colors[ifile])

    # Set ylable and yscale
    ax[0,0].set_ylabel('$\\rho r^2$')
    ax[1,0].set_ylabel('$U$ [km/s]')
    ax[2,0].set_ylabel('$(\\delta B/B)^2$')
    ax[0,0].set_yscale('log')
    ax[2,0].set_yscale('log')

    ax[0,1].set_ylabel('$>5$ MeV')
    ax[1,1].set_ylabel('$>10$ MeV')
    ax[2,1].set_ylabel('$>100$ MeV')

    # Set ylim
    ax[0,0].set_ylim(1.0E+11, 1.0E+14)
    ax[1,0].set_ylim(0.0, 1.0E+3)
    ax[2,0].set_ylim(1.0E-3, 1.0E+1)

    ax[0,1].set_ylim(1.0E-3, 1.0E+6)
    ax[1,1].set_ylim(1.0E-3, 1.0E+5)
    ax[2,1].set_ylim(1.0E-3, 1.0E+4)
    
    for px in ax[:,0]:
        px.set_xlim([0,250])
        px.grid(visible=True, ls='-.', alpha=0.3)
    for px in ax[:,1]:
        px.set_yscale('log')
        px.set_xlim([0,250])
        px.grid(visible=True, ls='-.', alpha=0.3)
    for px in ax[2,:]:
        px.set_xlabel(r'$r ~(R_\mathrm{S})$')
    return ax
#==========================================================================================================
def main_plot_flux(DoSave=False, DoShow=False, figfmt='jpg'):
    # Main function for plotting the mh1d flux

    # Set all directories
    rootdir = find_rootdir()
    testdir = rootdir + '/run_test'
    outsdir = testdir + '/SP/IO2/'
    figsdir = testdir + '/SP/fig/'
    if(not os.path.exists(figsdir)): os.mkdir(figsdir)
    fig1ddir = figsdir + 'mh1d/'
    if(not os.path.exists(fig1ddir)): os.mkdir(fig1ddir)

    for iLon in range(1, nLon+1):
        for iLat in range(1, nLat+1):
            # Get the files for each field line
            files = sorted(glob.glob(outsdir+'MH_data_%.3d_%.3d*.out'%(iLon, iLat)))

            # Create the entire panel
            fig, ax = plt.subplots(3, 2, figsize=(5, 5))
            plt.subplots_adjust(left=0.14, bottom=0.1, right=0.99, top=0.93, wspace=0.5, hspace=0.3)
            for ifile, filename in enumerate(files):
                plot_mh1d_flux_1line_1time(ifile, filename, ax)
                fig.suptitle('Field Line: iLon_iLat = '+str(iLon).zfill(3)+'_'+str(iLat).zfill(3), y=0.99)

            # Save, show, and close
            if(DoSave): fig.savefig(fig1ddir+'/mh1d_sep_%.3d_%.3d.%s'%(iLon, iLat, figfmt), dpi=360)
            if(DoShow): plt.show()
            plt.close(fig)
#==========================================================================================================

if __name__ == "__main__":
    main_plot_flux(DoSave=True, DoShow=False, figfmt='pdf')
