#!/usr/bin/env python

### Plot a GITM satellite file

import sys 
import numpy as np 
import re 
from matplotlib import pyplot as pp
from gitm_routines import *
import pandas as pd 
import ngims 
import rose

def get_args(argv):

    filelist = []
    var = -1
    help = False
    alog = False
    min = None 
    max = None 
    average = False
    stddev = False
    sats = None

    for arg in argv:

        IsFound = 0

        if (not IsFound):
            m = re.match(r'-var=(.*)',arg)
            if m:
                var = m.group(1)
                IsFound = 1

            m = re.match(r'-min=(.*)',arg)
            if m:
                min = float(m.group(1))
                IsFound = 1

            m = re.match(r'-max=(.*)',arg)
            if m:
                max = float(m.group(1))
                IsFound = 1
            
            m = re.match(r'-h',arg)
            if m:
                help = True
                IsFound = 1

            m = re.match(r'-alog',arg)
            if m:
                alog = True
                IsFound = 1
            
            m = re.match(r'-average',arg)
            if m:
                average = True
                IsFound = 1
            
            m = re.match(r'-stddev',arg)
            if m:
                stddev = True
                IsFound = 1

            m = re.match(r'-sats=(.*)',arg)
            if m:
                sats = m.group(1)
                IsFound = 1

            if IsFound==0 and not(arg==argv[0]):
                filelist.append(arg)


    args = {'filelist':filelist,
        'var':var,
        'help':help,
        'alog':alog,
        'min':min,
        'max':max,
        'average':average,
        'stddev':stddev,
        'sats':sats,
    }

    return args

plotmaxden = False
args = get_args(sys.argv)
sats = args['sats']
currentsats = ['ngims','rose']
if sats and not sats in currentsats:
    print('Only available sats are: '+",".join(currentsats))
    args['help'] = True 

if args['stddev'] and not args['average']:
    print('You must specify -average if you are using -stddev')
    args['help'] = True

header = read_gitm_header(args["filelist"])

if args['var'] == -1:
    args['help'] = True

if (args["help"]):

    print('Usage : ')
    print('gitm_plot_satellites.py -var=N -alog ')
    print('       -help [*.bin or a file]')
    print('   -help : print this message')
    print('   -var=var1,var2,var3... : variable(s) to plot')
    print('   -alog : plot the log of the variable')
    print('   -min=min: minimum value to plot')
    print('   -max=max: maximum value to plot')
    print('   -average: plot orbit averages')
    print('   -stddev: if using average, also plot stddev')
    print('   -sats=sats: overplot sat files. Current sat options')
    print('      are: ngims/rose')
    print('   At end, list the files you want to plot')

    iVar = 0
    for var in header["vars"]:
        print(iVar,var)
        iVar=iVar+1

    exit()

filelist = args['filelist']
vars = [0,1,2]
vars.extend([int(v) for v in args["var"].split(',')])
Var = [header['vars'][int(i)] for i in args['var'].split(',')]

varmap = {'29':44,'28':32,'27':16}
varcmap = {'27':'O$^+$','28':'O$_2^+$','29':'CO$_2^+$'}

fig = pp.figure() 

minv = 9.e20
maxv = -9.e20
alldata = [] 

for file in filelist:
    data = read_gitm_one_file(file,vars)
    alldata.append(data) #an array with all sat files data

alts = data[2][0,0]/1000. #Assumes the altitude grid doesn't change with file
df = pd.DataFrame(alldata)

colors = ['blue','green','orange','red','yellow']
if not args['average']:
    for ifile in range(len(alldata)):
        for pvar in args["var"].split(','):
            pdata = alldata[ifile][int(pvar)][0,0]
            if args['alog']: 
                pdata = np.log10(pdata)
            if min(pdata) < minv:
                minv = min(pdata)
            if max(pdata) > maxv:
                maxv = max(pdata)
            
            ivar = args["var"].index(pvar)
            line, = pp.plot(pdata,alts,color=colors[ivar])
    line.set_label('MGITM')
else: 
    meandata = df.mean()
    for pvar in args["var"].split(','):
        pdata = meandata[int(pvar)][0,0]
        if args['alog']: 
            pdata = np.log10(pdata)
        if min(pdata) < minv:
            minv = min(pdata)
        if max(pdata) > maxv:
            maxv = max(pdata)

        pp.plot(pdata,alts,'k',linewidth=2,label='MGITM') 

        if args['stddev']:
            tempdata = df[int(pvar)].to_numpy()
            newdata = np.zeros((len(df),np.shape(tempdata[0])[2]))
            for i in range(len(newdata)):
                newdata[i,:] = tempdata[i][0,0]
            stddata = np.std(newdata,0)
            if args['alog']: 

                stddata = np.log10(stddata)
            pp.fill_betweenx(alts,pdata-stddata,pdata+stddata)

if args['min']:
    mini = args['min']
else:
    mini = minv

if args['max']:
    maxi = args['max']
else:
    maxi = maxv

imaxden = np.argmax(pdata)
inearest = find_nearest_index(alts,270)
maxden = pdata[inearest]
if plotmaxden:
    pp.plot([-999,1e30],[alts[imaxden],alts[imaxden]],'r--')
# pp.plot([maxden,maxden],[0,300],'r--',alpha=.7)
pp.ylim([100,250])
pp.xlim([mini,maxi])



### Test the average 
def testave():
    sum = np.zeros(len(alldata[0][0][0,0]))
    for data in alldata:
        sum = sum + data[29][0,0]
        
    sum = sum/len(alldata)
    print('difference between averages is:')
    print(sum - meandata[29][0,0])

    vars = args["var"].split(',')

if sats:
    satsdir = '/home/dpawlows/Docs/Research/MGITM-MAVENcomparison2023/DD2/NGIMS/'
    start = alldata[0]['time'].strftime('%Y%m%d')
    end = alldata[-1]['time'].strftime('%Y%m%d')

    averageAltBin = 3.5 #km
    minalt = 100
    maxalt = 302
    altbins = np.arange(minalt,maxalt,averageAltBin)
    nbins = len(altbins)
    totaldata = np.zeros(nbins-1)
    counts = np.zeros(nbins-1)

    if sats=='ngims':   
        speciesColumn = 'ion_mass'
        qualityFlag = ['SCP','SC0']
        version = 'v08'
        type = 'ion'
        inboundonly = True 

        varlist = [varmap[v] for v in vars]
        files = ngims.getfiles(start,end,type=type,version=version,dir=satsdir)

        if not args['average']:

            for fi in files:
                data = ngims.readNGIMS(fi)
                data = data[(data["alt"] < 350)]
                data = data[data["quality"].isin(qualityFlag)]

                for pvar in varlist:
                    newdf = data[(data[speciesColumn] == int(pvar))]
                    if newdf.shape[0] == 0:
                        print("Error in ngims_plot_profile: Empty data frame from {}".format(fi))
                        
                    if inboundonly:
                        minalt = newdf['alt'].idxmin()
                        indices = list(newdf.index.values)
                        imin = indices.index(minalt)+1
                        newdf = newdf.loc[indices[0:imin]] #update the df with only inbound data
                    
                    if args['alog']:
                        density = np.log10(newdf.loc[newdf["alt"] < maxalt,"abundance"]*1e6)
                    else:
                        density = newdf.loc[newdf["alt"] < maxalt,"abundance"]*1e6

                    starred = ''
                    temp = newdf['quality'].isin(['SC0'])
                    if temp.values.sum() / newdf.shape[0] > .75:
                        starred = '*'
                    
                    altitude = newdf.loc[newdf["alt"] < maxalt,'alt'].values
                    line, = pp.plot(density,altitude,'.',markersize = 5,color='dimgrey')
                    # if allions:
                    #     line.set_label(varmap[pvar])
                    # else:
                    #     line.set_label(str(data.orbit.values[0])+starred)
            line.set_label('NGIMS')

        else:
            orbitavedensity = np.zeros((len(files),nbins-1))
            for pvar in varlist:
                ifile = 0 

                for fi in files:
                
                
                    data = ngims.readNGIMS(fi)
                    data = data[(data["alt"] < 350)]
                    data = data[data["quality"].isin(qualityFlag)]
                    newdf = data[(data[speciesColumn] == int(pvar))]

                    if inboundonly:
                            minalt = newdf['alt'].idxmin()
                            indices = list(newdf.index.values)
                            imin = indices.index(minalt)+1
                            newdf = newdf.loc[indices[0:imin]]

                    for ibin in range(len(altbins)-1):
                        lower = altbins[ibin] 
                        upper = altbins[ibin+1] 
                        tempdata = newdf.loc[(newdf["alt"] <= upper) & \
                            (newdf["alt"] > lower)]
                        orbitavedensity[ifile,ibin] = np.nanmean(tempdata['abundance'].to_numpy())*1.e6
                        count = tempdata['abundance'].count()

                        if count > 0:
                            counts[ibin] = counts[ibin] + count 
                            totaldata[ibin] = totaldata[ibin] + tempdata['abundance'].sum()*1.e6
                    
                    ifile += 1


                totaldata = totaldata/counts
                density2 = np.nanmean(orbitavedensity,axis=0)
                stddevdata = np.std(orbitavedensity,axis=0)
                averagebins = (altbins[0:-1] + altbins[1:])/2.
                
                pp.plot(totaldata,averagebins,'k--',linewidth=1,label='NGIMS')
                pp.fill_betweenx(averagebins,totaldata-stddevdata,totaldata+stddevdata,\
                    color='lightgrey',alpha=.8)

    if sats=='rose':
        satsdir = '/home/dpawlows/Docs/Research/MGITM-MAVENcomparison2023/ROSE/'
        files = rose.getRoseFiles(start,end,dir=satsdir)

        if not args['average']:
            for f in files:
                data = rose.readRoseTab(f)
                newdf = data[(data['altitude'] >= minalt) & (data['altitude'] <= maxalt)]
                pp.plot(newdf['nelec'],newdf['altitude'],'.',markersize = 5,color='dimgrey')
        
        else:
            orbitavedensity = np.zeros((len(files),nbins-1))
            ifile = 0
            for f in files:
                data = rose.readRoseTab(f)
                newdf = data[(data['altitude'] >= minalt) & (data['altitude'] <= maxalt)]

                for ibin in range(len(altbins)-1):
                    lower = altbins[ibin] 
                    upper = altbins[ibin+1] 
                    tempdata = newdf.loc[(newdf["altitude"] <= upper) & \
                        (newdf["altitude"] > lower)]
                    orbitavedensity[ifile,ibin] = np.nanmean(tempdata['nelec'].to_numpy())

                ifile += 1

            density2 = np.nanmean(orbitavedensity,axis=0)
            stddevdata = np.std(orbitavedensity,axis=0)
            averagebins = (altbins[0:-1] + altbins[1:])/2.
            
            pp.plot(density2,averagebins,'k--',linewidth=1,label='ROSE')
            pp.fill_betweenx(averagebins,density2-stddevdata,density2+stddevdata,\
                color='lightgrey',alpha=.8)


pp.legend(loc='upper right',frameon=False)
# pp.xlabel(varcmap[args['var'].split()[0]]+' Density [m$^{-3}$]')
pp.xlabel('[e-] [m$^{-3}$]')
pp.ylabel('Altitude (km)')
pp.savefig('plot.png')