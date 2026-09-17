#!/usr/bin/env python3
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf
'''
Provide a plotting example for visualizing the MFLAMPA MHTime
spectra outputs, compared with the SOHO/ERNE observational data;
Firstly written by Dr. Lulu Zhao and later modified by Weihao Liu
'''

# Load all modules
import os
import numpy as np
import datetime as dt
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# Set globle variables
time_start = dt.datetime(2013, 4, 11, 7, 24, 6)
# Set colors and plotting styles
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.serif'] = ['Helvetica'] + plt.rcParams['font.serif']
evenly_spaced_interval = np.linspace(0, 1, 30)
colors = [matplotlib.cm.rainbow(x) for x in evenly_spaced_interval]

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
def get_obs_erne(mhchannel_file, obsdatadir, res="mn"):
    # Get the time-evolved differential intensity of observational data from SOHO/ERNE

    # Declaration
    dicSOHO = {}
    EChannelLo_I = np.loadtxt(mhchannel_file, unpack=True, skiprows=2, usecols=[2], dtype=float)[1:-1]
    EChannelHi_I = np.loadtxt(mhchannel_file, unpack=True, skiprows=2, usecols=[3], dtype=float)[1:-1]

    # Load the data, 1-min or 1-hour resolution
    if res=="mn" or res=="5mn":
        strres = "mn"
    elif res=="hr":
        strres = "hr"
    datahr = np.loadtxt("%s/SOHO_ERNE_FLUX_%s.txt"%(obsdatadir, strres), \
        unpack=True, usecols=[i for i in range(24)], dtype=float)
    Time_I = np.array([dt.datetime(year=int(datahr[0][i]), month=1, day=1) + dt.timedelta( \
        days=datahr[1][i]-1, hours=datahr[2][i], minutes=datahr[3][i]) for i in range(len(datahr[0]))])
    Flux_II = datahr[4:24, :]

    # Rebin for 5-min resolution
    if res == '5mn':
        Time5_I = np.array([Time_I[i] for i in range(0, len(Time_I), 5)])
        Flux5_II = np.zeros([len(Flux_II), len(Time5_I)])
        # First moment of time
        Flux5_II[:, 0] = Flux_II[:, 0]
        # Rest moments of time
        for iTime in range(1, len(Time5_I)):
            for iEne in range(len(Flux_II)):
                msk = (Flux_II[iEne, 5*iTime-4:5*iTime+1] > 0.0) * \
                    (Flux_II[iEne, 5*iTime-4:5*iTime+1] < 1.0E+6)
                if np.sum(msk) > 0.0:
                    # Get the averaged differential intensity
                    Flux5_II[iEne, iTime] = np.mean(Flux_II[iEne, 5*iTime-4:5*iTime+1][msk])
                    # Remove some data, basically the background flux when rebinning
                    if np.sum(msk) <= 2 and Time5_I[iTime] < dt.datetime(2013, 4, 11, 10):
                        Flux5_II[iEne, iTime] = 0.0
                else:
                    Flux5_II[iEne, iTime] = 0.0
        # Assign to the dictionary
        dicSOHO['Time'] = Time5_I
        dicSOHO['EChannelLo'] = EChannelLo_I * 1.0E-3 # keV => MeV
        dicSOHO['EChannelHi'] = EChannelHi_I * 1.0E-3 # keV => MeV
        dicSOHO['FluxDiff'] = Flux5_II
        return dicSOHO

    # Assign to the dictionary
    dicSOHO['Time'] = Time_I
    dicSOHO['EChannelLo'] = EChannelLo_I * 1.0E-3 # keV => MeV
    dicSOHO['EChannelHi'] = EChannelHi_I * 1.0E-3 # keV => MeV
    dicSOHO['FluxDiff'] = Flux_II
    return dicSOHO
#==========================================================================================================
def get_mhtime_erne(mhchannel_file, mhtime_file):
    # Get MHTime data and the energy channel info saved from simulations

    # To get outputs saved in mhchannel_file and mhtime_file
    dic = {}
    EChannelLo_I = np.loadtxt(mhchannel_file, unpack=True, skiprows=2, usecols=[2], dtype=float)[1:-7]
    EChannelHi_I = np.loadtxt(mhchannel_file, unpack=True, skiprows=2, usecols=[3], dtype=float)[1:-7]
    data = np.loadtxt(mhtime_file, unpack=True, skiprows=5, usecols=[i for i in range(25)], dtype=float)
    FluxDiff_II = np.zeros([len(data[0]), len(EChannelLo_I)-1])
    for i in range(len(FluxDiff_II[0])):
        FluxDiff_II[:, i] = data[i+5]

    # Assign arrays to the dictionary
    dic['Time'] = np.array([time_start+dt.timedelta(seconds=data[0][i]) for i in range(len(data[0]))])
    dic['EChannelLo'] = EChannelLo_I * 1.0E-3 # keV => MeV
    dic['EChannelHi'] = EChannelHi_I * 1.0E-3 # keV => MeV
    dic['FluxDiff'] = FluxDiff_II.T * 1.0E+3 # per keV => per MeV
    return dic
#==========================================================================================================
def main_plot_erne(DoSave=False, DoShow=False, figfmt='jpg'):
    # Main function for plotting mhtime spectra in ERNE channels

    # Set all directories
    rootdir = find_rootdir()
    testdir = rootdir + '/run_1304full_01'
    obsdatadir = rootdir + '/data/SOHO_ERNE'
    outsdir = testdir + '/SP/IO2/'
    figsdir = testdir + '/SP/fig/'
    if(not os.path.exists(figsdir)): os.mkdir(figsdir)
    mhchannel_file = outsdir + 'MH_data_EChannel.H'
    mhtime_file = outsdir + 'MH_data_R=0215.00_001_001.out'

    obsERNE = get_obs_erne(mhchannel_file, obsdatadir, res='5mn')
    mhERNE = get_mhtime_erne(mhchannel_file, mhtime_file)
    # Create panel
    fig, ax = plt.subplots(1, 1, figsize=(7/1.2, 5/1.2))
    plt.subplots_adjust(left=0.14, bottom=0.185, right=0.995, top=0.97, wspace=0.5, hspace=0.3)

    # Plot differential intensity along time axis, with labels
    idColor_I = [0, 5, 10, 17, 22, 28]
    for iChannel in range(len(idColor_I)):
        # Plot SOHO/ERNE data
        mskTime = obsERNE['Time'] > mhERNE['Time'][0]
        msk = mskTime*(obsERNE['FluxDiff'][2*iChannel+1] > 0.0)
        plotTime = obsERNE['Time'][msk] - dt.timedelta(minutes=2.5)
        ax.plot(list(plotTime), list(obsERNE['FluxDiff'][2*iChannel+1][msk]), \
            ls='-', lw=1.2, alpha=0.8, color=colors[idColor_I[iChannel]])

        # Plot SOFIE-mhtime outputs
        ax.plot(mhERNE['Time'], mhERNE['FluxDiff'][2*iChannel+1], \
            ls='--', lw=1.6, color=colors[idColor_I[iChannel]])

        # Legends for energy channels with colors
        label = '%.1f–%.1f MeV'%(mhERNE['EChannelLo'][2*iChannel+1], mhERNE['EChannelHi'][2*iChannel+1])
        ax.annotate(label, xy=(0.28*(iChannel%3+1),0.91-0.06*int(iChannel/3)), xycoords='figure fraction', \
            ha='center', va='bottom', fontsize=15, color=colors[idColor_I[iChannel]])

    # Legend: SOHO/ERNE & SOFIE
    line1 = ax.plot([0], [0], ls='-', label='SOHO/ERNE', color='k')
    line2 = ax.plot([0], [0], ls='--', label='SOFIE', color='k')
    ax.legend(frameon=False, loc=(0.09, 0.07), fontsize=15, handlelength=1)

    # Labels
    myFmt = mdates.DateFormatter("%H:%M\n%b%d")
    ax.xaxis.set_major_formatter(myFmt)
    ax.xaxis.set_major_locator(plt.MaxNLocator(4))
    ax.set_xlabel('2013', fontsize=18)
    ax.set_ylabel('Differential Intensity (pfu/MeV)', fontsize=18)
    # Scale and Range
    ax.set_yscale('log'); ax.set_ylim(1.0E-2, 1.0E+3)
    ax.set_xlim(mhERNE['Time'][0], mhERNE['Time'][0]+dt.timedelta(hours=48))
    ax.tick_params(axis='both', which='major', labelsize=15)

    # Save
    if(DoSave): fig.savefig(figsdir+'mhd_sep_erne_spectrum.%s'%(figfmt), dpi=360)
    if(DoShow): plt.show()
    plt.close(fig)
#==========================================================================================================

if __name__ == "__main__":
    main_plot_erne(DoSave=False, DoShow=True, figfmt='pdf')
