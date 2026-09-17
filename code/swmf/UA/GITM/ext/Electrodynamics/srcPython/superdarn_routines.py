#!/usr/bin/env python3

import numpy as np
import datetime

class record(object):
    def __init__(self,**kwds):
        self.__dict__.update(kwds)

def readGlobalPot(f,seek=None):

    dtln=f.readline()
    if not dtln:
        print("EOF")
        f.close()
        return(np.nan)
    
    dtln=dtln.strip()
    dtar=dtln.split()
        
    yr=int(dtar[0])
    mo=int(dtar[1])
    dy=int(dtar[2])
    hr=int(dtar[3])
    mt=int(dtar[4])
    sc=float(dtar[5])

    read_time=datetime.datetime(yr,mo,dy,hr,mt,int(sc))

    print("read pot ",read_time)
    if seek is None:
        seek=datetime.datetime(1900,1,1,1,1,1)

    if read_time < seek:
        while read_time < seek:

            num=int(f.readline())

            for jr in range(num):
                dln=f.readline()


            dtln=f.readline()
            if not dtln:
                print("EOF")
                f.close()
                return(np.nan)
            #        exit()
        
            dtln=dtln.strip()
            dtar=dtln.split()
        
            yr=int(dtar[0])
            mo=int(dtar[1])
            dy=int(dtar[2])
            hr=int(dtar[3])
            mt=int(dtar[4])
            sc=float(dtar[5])

            read_time=datetime.datetime(yr,mo,dy,hr,mt,int(sc))
            print("read pot ",read_time)
    
                
    #aacgm.set_datetime(yr,mo,dy,hr,mt,int(sc))
    
    num=int(f.readline())
    
    if num < 0:
        return(-1)

    vec_lat=np.zeros(num)
    vec_lon=np.zeros(num)
    dlat=np.zeros(num)
    dlon=np.zeros(num)
    m_lat=np.zeros(num)
    m_lon=np.zeros(num)
    pot=np.zeros(num)
    ee=np.zeros(num)
    en=np.zeros(num)
    ve=np.zeros(num)
    vn=np.zeros(num)
    numd=np.zeros(num)
    for jr in range(num):
        dln=f.readline()
        dln=dln.strip()
        dar=dln.split()

        vec_lat[jr]=float(dar[0])
        dlat[jr]=float(dar[1])
        vec_lon[jr]=float(dar[2])
        dlon[jr]=float(dar[3])
        pot[jr]=float(dar[4])
        ee[jr]=float(dar[5])
        en[jr]=float(dar[6])
        ve[jr]=float(dar[7])
        vn[jr]=float(dar[7])
        numd[jr]=float(dar[8])
        
        #pos1=aacgm.convert(vec_lat[jr],vec_lon[jr],300,0)            
        #m_lat[jr]=pos1[0]
        #m_lon[jr]=pos1[1]
        m_lat[jr] = vec_lat[jr]
        m_lon[jr] = vec_lon[jr]



    max_pot=np.max(pot)
    min_pot=np.min(pot)
    
    dstr=record(yr=yr, mo=mo, dy=dy, hr=hr, mt=mt, sc=sc, dlat=dlat, dlon=dlon, max_pot=max_pot, min_pot=min_pot,
                num=num, vec_lat=vec_lat, vec_lon=vec_lon, m_lat=m_lat, m_lon=m_lon, pot=pot,
                ee=ee, en=en, ve=ve, vn=vn, numd=numd, time = read_time)
        

    return(dstr)
        
