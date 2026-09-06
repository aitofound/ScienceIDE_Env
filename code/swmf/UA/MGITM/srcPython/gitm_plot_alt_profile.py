#!/usr/bin/env python
#Plots alt profile for a single location 

from glob import glob
from datetime import datetime
from datetime import timedelta
import numpy as np
import matplotlib.pyplot as pp
import matplotlib.dates as mdates
from matplotlib.gridspec import GridSpec
from gitm_routines import *
import re
import sys

rtod = 180.0/3.141592

def get_args(argv):

    filelist = []
    IsLog = 0
    diff = '0'
    var = 15
    alt = 400.0
    lon = -100.0
    lat = -100.0
    cut = 'loc'
    smin = -100.0
    smax = -100.0
    minv = None
    maxv = None

    help = 0


    for arg in argv:

        IsFound = 0

        if (not IsFound):

            m = re.match(r'-var=(.*)',arg)
            if m:
                var = m.group(1)
                IsFound = 1

            m = re.match(r'-diff=(.*)',arg)
            if m:
                diff = m.group(1)
                IsFound = 1

            m = re.match(r'-cut=(.*)',arg)
            if m:
                cut = m.group(1)
                IsFound = 1

            m = re.match(r'-lat=(.*)',arg)
            if m:
                lat = int(m.group(1))
                IsFound = 1

            m = re.match(r'-lon=(.*)',arg)
            if m:
                lon = int(m.group(1))
                IsFound = 1

            m = re.match(r'-smin=(.*)',arg)
            if m:
                smin = int(m.group(1))
                IsFound = 1   

            m = re.match(r'-smax=(.*)',arg)
            if m:
                smax = int(m.group(1))
                IsFound = 1

            m = re.match(r'-min=(.*)',arg)
            if m:
                minv = float(m.group(1))
                IsFound = 1   

            m = re.match(r'-max=(.*)',arg)
            if m:
                maxv = float(m.group(1))
                IsFound = 1  

            m = re.match(r'-alog',arg)
            if m:
                IsLog = 1
                IsFound = 1

            m = re.match(r'-h',arg)
            if m:
                help = 1
                IsFound = 1


            if IsFound==0 and not(arg==argv[0]):
                filelist.append(arg)

    args = {'filelist':filelist,
            'var':var,
            'help':help,
            'lat':lat,
            'lon':lon,
            'IsLog':IsLog,
            'cut':cut,
            'smin':smin,
            'smax':smax,
            'diff':diff,
            'minv':minv,
            'maxv':maxv}

    return args

args = get_args(sys.argv)
header = read_gitm_header(args["filelist"])

if args['cut'] == 'loc' and args['lon'] > -50:
    plon = args['lon']
    plat = args['lat']
elif args['cut'] == 'sza' and args['smin'] > -50:
    smin = args['smin']
    smax = args['smax']
else:
    args["help"] = '-h'    

if (args["help"]):

    print('Usage : ')
    print('gitm_plot_alt_profile.py -var=N1[,N2,N3,...] -lat=lat -lon=lon -alog')
    print('                     -help [file]')
    print('   -help : print this message')
    print('   -var=number[,num2,num3,...] : number is variable to plot')
    print('   -cut=loc,sza: Plot type ')
    print('   -lat=latitude : latitude in degrees (closest) (cut=loc) ')
    print('   -lon=longitude: longitude in degrees (closest) (cut=loc)')
    print('   -smin=minsza: minimum solar zenith angle (cut=sza)')
    print('   -smax=maxsza: maximum solar zenigh angle (cut=sza)')
    print('   -min=min: minimum value to plot')
    print('   -max=max: maximum value to plot')
    print('   -alog: plot the log of the variable')
    print('   -diff=backgroundFiles: plot the difference between 2 sets of files')
    print('   Non-KW arg: files.')

    iVar = 0
    for var in header["vars"]:
        print(iVar,var)
        iVar=iVar+1

    exit()


filelist = args["filelist"]

if len(filelist) > 1:
    print('Only 1 file should be specified')
    exit(1)

file = filelist[0]

try:
    iSZA = header["vars"].index('SolarZenithAngle')
    vars = [0,1,2,iSZA]
except: 
    vars = [0,1,2]

diff = False
if args['diff'] != '0':
    diff = True
    backgroundFilelist = sorted(glob(args["diff"]))
    nBackFiles = len(backgroundFilelist)
    if nBackFiles != nFiles:
        print('Difference between sizes of perturbation and background filelists:')
        print('Lengths: {}   {}'.format(nFiles,nBackFiles))
        print('Only 1 file should be specified')
        exit(1)
    bFile = backgroundFilelist[0]

vars.extend([int(v) for v in args["var"].split(',')])
Var = [header['vars'][int(i)] for i in args['var'].split(',')]
nvars = len(args['var'].split(','))
AllData = {a:[] for a in args['var'].split(',')}
AllData2D = []
AllAlts = []
AllSZA = []
j = 0

data = read_gitm_one_file(file, vars)
    
[nLons, nLats, nAlts] = data[0].shape
Alts = data[2][0][0]/1000.0
Lons = data[0][:,0,0]*rtod
Lats = data[1][0,:,0]*rtod

ialt1 = find_nearest_index(Alts,90)
ialt2 = find_nearest_index(Alts,300)

time = data["time"]

if diff:
    if bFile == '':
        #It is possible that we don't have an output file at the same time.
        print('Missing background file corresponding to: {}'.format(file))
        exit(1)
    background = read_gitm_one_file(bFile,vars)

if args['cut'] == 'loc':
    ilon = find_nearest_index(Lons,plon)
    ilat = find_nearest_index(Lats,plat)

    for ivar in args['var'].split(','):
        if diff:
            temp = (data[int(ivar)][ilon,ilat,ialt1:ialt2+1]-background[int(ivar)][ilon,ilat,ialt1:ialt2+1])/ \
                background[int(ivar)][ilon,ilat,ialt1:ialt2+1]*100.0
        else:
            temp = data[int(ivar)][ilon,ilat,ialt1:ialt2+1]          

        AllData[ivar].append(temp)


if args['cut'] == 'sza':        
    AllSZA.append(data[iSZA][:,:,0])
    mask = (AllSZA[-1] >= smin) & (AllSZA[-1] <= smax ) 
    for ivar in args['var'].split(','):
        if diff:
            #Calculate the mean of both sets of data and then calculate the percent difference.
            mean1 = data[int(ivar)][:,:,ialt1:ialt2+1][mask].mean(axis=0)
            mean2 = background[int(ivar)][:,:,ialt1:ialt2+1][mask].mean(axis=0)
            temp = (mean1-mean2)/mean2*100.

        else:
            temp = data[int(ivar)][:,:,ialt1:ialt2+1][mask].mean(axis=0)

        AllData[ivar].append(temp)

for ivar in args['var'].split(','):
    AllData[ivar] = np.array(AllData[ivar])


if args['cut']  == 'sza':
    AllSZA = np.array(AllSZA)

fig = pp.figure()

Alts = Alts[ialt1:ialt2+1]

cmap = 'plasma'
i=0
ax = pp.subplot(121)
if len(Var) == 1:
    marker = '+'
else:
    marker = '.'

for ivar in args['var'].split(','):
    AllData1D = AllData[ivar][0]
    if (ivar == '3' and (not diff)) or args['IsLog']:
        mask = (AllData1D != 0.0) 
        AllData1D = np.log10(AllData1D[mask])
        Alts = Alts[mask]
        Var[i] = Var[i].replace('!U','^')
        Var[i] = Var[i].replace('!D','_')
        Var[i] = Var[i].replace('!N','')
        Var[i] = '$'+Var[i]+'$'

    
    if len(Var) > 1: 
        label = Var[i]
    else:
        label = None
    plot = ax.plot(AllData1D,Alts,marker,label=label)  
    i+=1

if len(Var) == 1:
    svar = int(args['var'])
    if diff:
        xlabel = '{}\n% Diff'.format(Var[i])
    else:
        xlabel = Var[0]
else:
    xlabel = 'Density'
    svar = 99
    box = ax.get_position()
    ax.set_position([box.x0, box.y0, box.width * 0.8, box.height])
    ax.legend(loc='lower left', bbox_to_anchor=(1, 0.5),frameon=False)
   
# if i < len(Var)-1:
#     ax.get_xaxis().set_ticklabels([])

pp.ylabel('Alt (km)')
if args['IsLog']:
    xlabel = 'Log '+xlabel
pp.xlabel(xlabel)

if args['minv'] == None:
    minv = min(AllData1D)
else:
    minv = args['minv']
if args['maxv'] == None:
    maxv = max(AllData1D)
else:
    maxv = args['maxv']

pp.xlim([minv,maxv])
pp.ylim([90,250])

   

outfile = 'altprofile_var{:02d}_{}.png'.format(svar,time.strftime('%y%m%d_%H%M%S'))
print("Writing to file: {}".format(outfile))
pp.savefig(outfile)
