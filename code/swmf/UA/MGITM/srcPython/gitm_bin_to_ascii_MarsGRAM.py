#!/usr/bin/env python

#Convert GITM output to ascii for use with MarsGram

# 1.Longitude(degree) 2.Latitude(degree) 3.SZA(degree) 4.Altitude(km) 5.Tn(K) 6.Ti(K) 7.Te(K)
# 8.nCO2(#/cc) 9.nO(#/cc) 10.nN2(#/cc) 11.nCO(#/cc) 12.nO2P(#/cc) 13.ne(#/cc) 14.UN(m/s)
# 15.VN(m/s), 16.WN(m/s)


from glob import glob
import sys
from gitm_routines import *
import gitm_coordinates as gc 
import re 

rtod = 180.0/3.141592

def get_args(argv):

    filelist = []
    coordinates = 'geographic'
    help = False 
    minalt = 90

    for arg in argv:
        IsFound = 0

        if (not IsFound):
            m = re.match(r'-h',arg)
            if m:
                help = 1
                IsFound = 1

            m = re.match(r'-coordinates=(.*)',arg)
            if m:
                coordinates = m.group(1)
                IsFound = 1

            m = re.match(r'-minalt=(.*)',arg)
            if m:
                minalt = float(m.group(1))
                IsFound = 1

        if IsFound==0 and not(arg==argv[0]):
                filelist.append(arg)

    args = {'filelist':filelist,
            'coordinates':coordinates.lower(),
            'help':help,
            'minalt':minalt,
            }

    return args


args = get_args(sys.argv)
coordoptions = ['geographic','geodetic']
coordinates = args['coordinates'].lower()
if coordinates not in coordoptions:
    print('{} is not a coordinate option'.format(coordinates))
    args['help'] = True 

filelist = args['filelist']
vars = [0,1,2,15,4,5,6,7,8,9,14,16,17,18]
minalt = args['minalt']
nFiles = len(filelist)
header = read_gitm_header(args["filelist"])

if args['help'] or len(filelist) < 1:
    print('Usage : ')
    print('gitm_bin_to_ascii_MarsGRAM.py -coordinates=coordinates -help')
    print('                   -minalt=minalt  [*.bin or a file]')
    print('   -help : print this message')
    print('   -coordinates=geographic or geodetic (default = geographic)')
    print('   At end, list the files you want to plot')
    
    iVar = 0
    for var in header["vars"]:
        print(iVar,var)
        iVar=iVar+1

    exit()


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

    Alts = data[2][0][0]/1000.
    ialtstart = np.where(Alts > minalt)[0][0]
    tempalts = Alts[ialtstart:-2]/1000.0
    Lons = data[0][:,0,0]
    Lats = data[1][0,:,0]
    
    totalLons = len(Lons[2:-2])
    totalLats = len(Lats[2:-2])
    totalAlts = len(tempalts)

    f = open(output,'w')
    f.write("#MGITM Results on "+date+" at "+time+" UT."+"\n")
    f.write("#Each column contains the following variable at the given longitude, latitude, and altitude"+"\n")
    f.write("#Number of Longitude points: "+str(totalLons)+"\n")
    f.write("#Number of Latitude points: "+str(totalLats)+"\n")
    f.write("#Number of Altitude points: "+str(totalAlts)+"\n")
    f.write("#Units   Densities: #/m3, temperatures: K, wind speeds: m/s."+"\n")
    myvars = ["".join(data['vars'][i].decode().split()) for i in vars]  
    myvars2 = "   ".join(name_dict[i] for i in myvars)      
    f.write(myvars2+'   rho'+"\n")
    f.write("#START\n")


    #Begin 3D loop over data cube
    rhovars = [4,5,6,7,8,9,14]
    for ialt in range(ialtstart,nAlts-2):
        for ilat in range(2,nLats-2):
            for ilon in range(2,nLons-2):
                if coordinates == 'geodetic':
                    gcoordinates = np.array([[Lats[ilat],Lons[ilon],Alts[ialt]]]).transpose()
                    gd = gc.geo2geodetic(gcoordinates,planet='mars')
                    thisdata = [gd[1,0]*180/np.pi,gd[0,0]*180/np.pi,gd[2,0]] #Lon first
                  
                else:   
                    thisdata = [Lons[ilon],Lats[ilat],Alts[ialt]]
                
                for var in vars[3:]:
                    thisdata.append(data[var][ilon,ilat,ialt])
                rho = 0.0    
                for i in rhovars:
                    thisDensity = data[i][ilon,ilat,ialt]
                    rho += thisDensity                    
                thisdata.append(rho)
                f.write("    ".join('{:g}'.format(ele) for ele in thisdata)+"\n")

    i += 1
    f.close()
    print(ialt-ialtstart,ilat,ilon)
