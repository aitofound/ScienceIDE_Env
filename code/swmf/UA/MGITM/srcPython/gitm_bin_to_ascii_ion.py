#!/usr/bin/env python

#Convert GITM output to ascii for use with M-AMPS
#Example output file is at https://app.box.com/s/hr6w0i2zsm6az9gzb2s2qysxtjaf5jse/file/840383303910
# 1.Longitude(degree) 2.Latitude(degree) 3.Altitude(km) 4.Tn(K) 5.Te(K) 6.Ti(K)
# 7.nCO2(#/cc) 8.nO(#/cc) 9.nCO2+(#/cc) 10.nO+(#/cc) 11.nO2+(#/cc) 12.N2+(#/cc) 13.ne(m/s)
#

#Usage: gitm_bin_to_ascii.py filename(s)

from glob import glob
import sys
from gitm_routines import *

rtod = 180.0/3.141592

def get_args(argv):

    filelist = []
    IsFound = 0

    for arg in argv:
        if IsFound==0 and not(arg==argv[0]):
            filelist.append(arg)

        args = {'filelist':filelist,
                }
    return args


newargs = get_args(sys.argv)

filelist = newargs['filelist']
vars = [0,1,2,15,32,33,4,6,28,26,27,29,31]
nFiles = len(filelist)
i = 0

for file in filelist:

    data = read_gitm_one_file(file, vars)
    pos = file.rfind('.bin')
    tstamp = file[pos-13:pos]
    output = 'GITM_'+tstamp+'.dat'
    year = tstamp[0:2]
    month = tstamp[2:4]
    day = tstamp[4:6]
    hour = tstamp[7:9]
    minute = tstamp[9:11]
    second = tstamp[11:13]

    year = '20'+year if int(year) < 40 else '19'+year
    date = year+'-'+month+'-'+day
    time = hour+':'+minute+":"+second

    if i == 0:
        nLons = data['nLons']
        nLats = data['nLats']
        nAlts = data['nAlts']

    Alts = data[2][0][0]/1000.0;
    Lons = data[0][:,0,0]*rtod;
    Lats = data[1][0,:,0]*rtod;
    f = open(output,'w')
    f.write("#MGITM Results on "+date+" at "+time+" UT."+"\n")
    f.write("#Each column contains the following variable at the given longitude, latitude, and altitude"+"\n")
    f.write("#Number of Longitude points: "+str(nLons)+"\n")
    f.write("#Number of Latitude points: "+str(nLats)+"\n")
    f.write("#Number of Altitude points: "+str(nAlts)+"\n")
    f.write("#Units-Densities: #/m3, temperatures: K, wind velocitiy: m/s."+"\n")
    f.write("#1.Longitude(degree) 2.Latitude(degree) 3.Altitude(km) 4.Tn(K) 5.Te(K) 6.Ti(K) 7.nCO2(#/m3)\
    8.nO(#/m3) 9.nCO2+(#/m3) 10.nO+(#/m3) 11.nO2+(#/m3) 12.nN2+(#/m3) 13.ne(m/s)\n")
# 1.Longitude(degree) 2.Latitude(degree) 3.Altitude(km) 4.Tn(K) 5.Te(K) 6.Ti(K)
# 7.nCO2(#/cc) 8.nO(#/cc) 9.nCO2+(#/cc) 10.nO+(#/cc) 11.nO2+(#/cc) 12.N2+(#/cc) 13.ne(m/s)
    f.write("#START\n")

    ialtstart = np.where(Alts > 80)[0][0]

    #Begin 3D loop over data cube

    for ialt in range(ialtstart,nAlts-2):
        for ilat in range(2,nLats-2):
            for ilon in range(2,nLons-2):
                thisdata = [Lons[ilon],Lats[ilat],Alts[ialt]]
                for var in vars[3:]:
                    thisdata.append(data[var][ilon,ilat,ialt])

                f.write("    ".join('{:g}'.format(ele) for ele in thisdata)+"\n")

    i += 1
    f.close()
