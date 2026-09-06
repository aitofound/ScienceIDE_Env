#!/usr/bin/env python
import numpy as np
import remap_magnetogram as rmag
import GLSETUPAlg as GL
import os
import fnmatch
import math
import subprocess
BMax = 1900.0
cPi  = np.pi
Rad2Deg = 180/cPi
Deg2Rad = cPi/180

def get_weighted_center(X,Y,Br_C,BrThreshold,nLat,nLon,Lat_I,Lon_I,\
                           IsUniformLat):
   LonIndex = GL.round_my(X)
   LatIndex = GL.round_my(Y)
   print('\n Chosen Longitude, Latitude =',Lon_I[LonIndex]*Rad2Deg,
         Lat_I[LatIndex]*Rad2Deg)
   #Occcupancy matrix
   occ = np.zeros([nLat,nLon])
   occ[LatIndex,LonIndex] = 1
   #Occupancy level
   occ_level = 0
   #Occupancy level check
   occ_check = 1
   while occ_check > 0:
      occ_level = occ_level + 1
      [row,col] = np.where(occ == occ_level)
      n = row.size
      # row= lat, col = long
      for i in np.arange(n):
         rowp = row[i]
         colp = col[i]
         if BrThreshold > 0. :
            if rowp-1 > 0:
               if (Br_C[rowp-1,colp] > BrThreshold and occ[rowp-1,colp] == 0):
                  occ[rowp-1,colp] = occ_level + 1
            if rowp+1 < nLat:
               if (Br_C[rowp+1,colp] > BrThreshold and occ[rowp+1,colp] == 0):
                  occ[rowp+1,colp] = occ_level + 1
            if colp-1 > 0:
               if (Br_C[rowp,colp-1] > BrThreshold and occ[rowp,colp-1] == 0):
                  occ[rowp,colp-1] = occ_level + 1
            if colp+1 < nLon:
               if (Br_C[rowp,colp+1] > BrThreshold and occ[rowp,colp+1] == 0):
                  occ[rowp,colp+1] = occ_level + 1
         elif BrThreshold < 0.:
            if rowp-1 > 0:
               if (Br_C[rowp-1,colp] < BrThreshold and occ[rowp-1,colp] == 0):
                  occ[rowp-1,colp] = occ_level + 1
            if rowp+1 < nLat:
               if (Br_C[rowp+1,colp] < BrThreshold and occ[rowp+1,colp] == 0):
                  occ[rowp+1,colp] = occ_level + 1
            if colp-1 > 0:
               if (Br_C[rowp,colp-1] < BrThreshold and occ[rowp,colp-1] == 0):
                  occ[rowp,colp-1] = occ_level + 1
            if colp+1 < nLon:
               if (Br_C[rowp,colp+1] < BrThreshold and occ[rowp,colp+1] == 0):
                  occ[rowp,colp+1] = occ_level + 1
      occ_check = n
   #end whileloop
   SizeMap_C = np.zeros([nLat,nLon])
   #Calculate weighted center
   [LatOcc,LonOcc] = np.where(occ>0)
   nSize = LatOcc.size
   LatCenter=0.
   LonCenter=0.
   Flux=0.
   Area = 0.
   #flux = SUM(area * Br)
   dLon    = 2.*cPi/nLon
   dLat    = cPi/nLat
   dSinLat = 2.0/nLat
   for i in np.arange(nSize):
      iLon  = LonOcc[i]
      iLat  = LatOcc[i]
      if IsUniformLat : # uniform in lat
         dArea = np.cos(Lat_I[iLat]) * dLat * dLon   # in radians^2
      else:
         dArea = dSinLat * dLon   # in radians^2
      dFlux = Br_C[iLat,iLon] * dArea
      LonCenter += Lon_I[iLon] * dFlux
      LatCenter +=  Lat_I[iLat] * dFlux
      Flux += dFlux
      Area += dArea
      SizeMap_C[iLat,iLon]=Br_C[iLat,iLon]
   LonCenter /= Flux  # in Radians
   LatCenter /= Flux  # in Radians
   if BrThreshold > 0. :
      print("\n Flux from the positive spot={:6.2f} [Gs*Rs**2]".format(
         Flux))
   else:
      print("\n Flux from the negative spot={:6.2f} [Gs*Rs**2]".format(
         Flux))
   # return the longitude and latitude of the weighted center in radians, 
   # area in radians^2, and the occupancy matrix for plotting
   return(LatCenter,LonCenter,SizeMap_C,Flux, LatOcc, LonOcc)

def TwoPointsOnSph(Lon1, Lat1, Lon2, Lat2):
    SinLon1 = np.sin(Lon1)
    CosLon1 = np.cos(Lon1)
    SinLat1 = np.sin(Lat1)
    CosLat1 = np.cos(Lat1)
    Dir1_D = np.array([CosLat1*CosLon1, CosLat1*SinLon1, SinLat1])

    SinLon2 = np.sin(Lon2)
    CosLon2 = np.cos(Lon2)
    SinLat2 = np.sin(Lat2)
    CosLat2 = np.cos(Lat2)
    Dir2_D = np.array([CosLat2*CosLon2, CosLat2*SinLon2, SinLat2])

    HalfDist2  = np.sin(
       0.50*(Lat2 - Lat1))**2 + CosLat1*CosLat2*np.sin(
          0.50*(Lon2 - Lon1))**2
    rMidPoint = np.sqrt(1 - HalfDist2)

    HalfDist = np.sqrt(HalfDist2)
    # Depth = 1 - rMidPoint
    Depth = HalfDist2/(1 + rMidPoint)

    # Unit vector toward the mid point:
    DirMid_D = Dir2_D + Dir1_D
    # Normalization coefficient
    Coeff1 = 0.50/rMidPoint
    DirMid_D = DirMid_D*Coeff1
    SinLat = DirMid_D[2]
    CosLat = np.sqrt(1 - SinLat**2)
    CosLon = DirMid_D[0]/CosLat
    SinLon = DirMid_D[1]/CosLat
    Lat = np.arcsin(SinLat)
    Lon = np.arccos(CosLon)
    if SinLon < 0.0 :
       Lon = 2*cPi - Lon
    # Direction  vectors for parallel and meridian:
    DirPar_D = np.array([-SinLon, CosLon, 0.0])
    DirMer_D = np.array([-SinLat*CosLon, -SinLat*SinLon, CosLat])
    # Direction unit vector from point 1 to point 2:
    Dir12_D = Dir2_D - Dir1_D
    # Normalization coefficient
    Coeff1  = 0.50/HalfDist
    # Normalized vector from 1 to 2
    Dir12_D = Dir12_D*Coeff1
    # Its projections on the parallel and  meridian  directions:
    CosOrientation  = Dir12_D[0]*DirPar_D[0]
    CosOrientation += Dir12_D[1]*DirPar_D[1]
    CosOrientation += Dir12_D[2]*DirPar_D[2]
    SinOrientation  = Dir12_D[0]*DirMer_D[0]
    SinOrientation += Dir12_D[1]*DirMer_D[1]
    SinOrientation += Dir12_D[2]*DirMer_D[2]
    Orientation  = np.arccos(CosOrientation)
    if SinOrientation < 0.0 :
       Orientation = 2*cPi - Orientation

    return(Lon,Lat,Orientation,HalfDist)
   
def Alg(nLon, nLat, nParam, Param_I, Lon_I, Lat_I, Br_C, UseCMEGrid, Time,
        LonFPPosIn,LatFPPosIn,LonFPNegIn,LatFPNegIn,a2r0Ratio,ApexIn,
        BStrapMax):
   Lon0     = Param_I[0]
   LonEarth = Param_I[1]
   xPositive = Param_I[2]
   yPositive = Param_I[3]
   xNegative = Param_I[4]
   yNegative = Param_I[5]
   dLon    = 2.*cPi/nLon
   dLat    = cPi/nLat
   dSinLat = 2.0/nLat
   DsLat_C = np.zeros([nLat,nLon])
   Ds2_C   = np.zeros([nLat,nLon])
   # Check if the latitude grid is uniform
   if abs(Lat_I[2]-2*Lat_I[1]+Lat_I[0])<1.0e-5:
      IsUniformLat = True
      print('Uniform in Latitude grid')
      for k in np.arange(nLat):
         for l in np.arange(nLon):
            DsLat_C[k,l] = dLat
            Ds2_C[k,l]   = dLat**2 + (np.cos(Lat_I[k])*dLon)**2
   else:
      IsUniformLat = False
      print('Uniform in Sin(Latitude) grid')
      for k in np.arange(nLat):
         for l in np.arange(nLon):
            DsLat_C[k,l] = dSinLat/np.cos(Lat_I[k])
            Ds2_C[k,l]   = DsLat_C[k,l]**2 + (np.cos(Lat_I[k])*dLon)**2
   # Pass the x, y indices of the clicks to calculate weighted center
   # and their indices

   # get weighted centers(Lon,Lat), occupancy matrix, Area of AR for
   # positive and negative regions
   [LatPos,LonPos,PSizeMap_C,FluxP, LatP_I, LonP_I] = \
       get_weighted_center(xPositive,yPositive,Br_C,20.,\
                              nLat,nLon,Lat_I,Lon_I,IsUniformLat)
   LonPMin =  min(LonP_I)
   LonPMax =  max(LonP_I)
   print('\n Positive spot: minimum and maximum longitude  indexes',\
         LonPMin,LonPMax)
   LatPMin =  min(LatP_I)
   LatPMax =  max(LatP_I)
   print('\n Positive spot: minimum and maximum latitude  indexes',\
         LatPMin,LatPMax)
   
   LonPosIndex = GL.calculate_index(LonPos,Lon_I,nLon)
   LatPosIndex = GL.calculate_index(LatPos,Lat_I, nLat)
   print('\n Positive Weighted Center indexes (lon,lat) =',\
          LonPosIndex, LatPosIndex)
   print(
      '\n Positive Weighted Center Lon={0:6.2f}, Lat={1:6.2f} [deg]'.format(
         LonPos*Rad2Deg, LatPos*Rad2Deg))

   [LatNeg,LonNeg, NSizeMap_C,FluxN, LatN_I, LonN_I] = \
       get_weighted_center(xNegative,yNegative,Br_C,-20.,\
                              nLat,nLon,Lat_I,Lon_I,IsUniformLat)
   LonNMin =  min(LonN_I)
   LonNMax =  max(LonN_I)
   print('\n Negative spot: minimum and maximum longitude  indexes',\
         LonNMin,LonNMax)
   LatNMin =  min(LatN_I)
   LatNMax =  max(LatN_I)
   print('\n Negative spot: minimum and maximum latitude  indexes',\
         LatNMin,LatNMax)
   
   LonNegIndex = GL.calculate_index(LonNeg,Lon_I,nLon)
   LatNegIndex = GL.calculate_index(LatNeg,Lat_I, nLat)
   print(
      '\n Negative Weighted Center indexes (lon,lat) =',
      LonNegIndex, LatNegIndex)
   print(
      '\n Negative Weighted Center Lon={0:6.2f}, Lat={1:6.2f} [deg]'.format(
         LonNeg*Rad2Deg,LatNeg*Rad2Deg))

   [LonAR,LatAR,OrientationAR,HalfDist] = TwoPointsOnSph(
      LonPos,LatPos,LonNeg,LatNeg)
   # Angles are converted to degrees:
   LonAR *=Rad2Deg
   LatAR *=Rad2Deg
   OrientationAR *=Rad2Deg
   # Major radius estimated from the distance between the spot centers
   RadiusAR = HalfDist*1.4
   print ("Mid point: Longitude: {0:5.2f} Latitude:{1:5.2f} [deg]".format(
         LonAR, LatAR))
   print ("Orientation: {0:5.2f} [deg] Major Radius: {1:5.2f}".format(
         OrientationAR, RadiusAR))

   # Find center of the active region as the point on the line
   # connecting the positive and negative center at which the MF is minimal
   # (i.e as intersection of this with PIL,
   # herewith PIL=Polarity Inversion Line
   nProfile = max([abs(LonPosIndex - LonNegIndex),
                   abs(LatPosIndex - LatNegIndex)]) + 1
   BTmp = BMax + 1.0
   for i in np.arange(nProfile):
      LonProfile = LonPos+(
         LonNeg - LonPos)*i/(nProfile - 1)
      LatProfile = LatPos+(
         LatNeg - LatPos)*i/(nProfile - 1)
      IndexLon = GL.calculate_index(LonProfile,Lon_I,nLon)
      IndexLat = GL.calculate_index(LatProfile,Lat_I,nLat)
      AbsBr = abs(Br_C[IndexLat,IndexLon])
      if (AbsBr < BTmp):
         BTmp = AbsBr
         iLonAR =IndexLon
         iLatAR =IndexLat
   LonAR  = Lon_I[iLonAR]*Rad2Deg  # in deg
   LatAR  = Lat_I[iLatAR]*Rad2Deg  # in deg
   print (
      "Center for Active region: Lon={0:4.1f}, Lat={1:4.1f} [deg]:".format(
         LonAR,LatAR),"  indexes=",iLonAR,iLatAR)
   # Rectangular box  for active region
   LonARMin=min([LonNMin,LonPMin,iLonAR-20])
   LonARMin=max([LonARMin-2,0])
   LonARMax=max([LonNMax,LonPMax,iLonAR+20])
   LonARMax=min([LonARMax+2,nLon-1])
   LatARMin =min([LatNMin, LatPMin,iLatAR-20])
   LatARMin =max([LatARMin-2,0])
   LatARMax =max([LatNMax, LatPMax,iLatAR+20])
   LatARMax =min([LatARMax+2,nLat-1])
   print('\n Box for AR: minimum and maximum longitude  indexes',\
         LonARMin,LonARMax)
   print('\n Box for AR: minimum and maximum longitude  indexes',\
         LatARMin,LatARMax)

   # Distance to negative spot
   nSizeN = LatN_I.size
   Dist2N_I=np.zeros(nSizeN)
   nSizeP=LatP_I.size
   Dist2P_I=np.zeros(nSizeP)
   nLonShort = LonARMax + 1 - LonARMin
   nLatShort = LatARMax + 1 - LatARMin
   Dist2Min_C=np.zeros([nLatShort,nLonShort])
   for k in range(LatARMin , LatARMax+1):
      CosLat=np.cos(Lat_I[k])
      for l in range(LonARMin, LonARMax+1):
         for  i in np.arange(nSizeN):
            Dist2N_I[i] = (Lat_I[LatN_I[i]]-Lat_I[k])**2+CosLat**2*(
               Lon_I[LonN_I[i]]-Lon_I[l])**2
         for  i in np.arange(nSizeP):
            Dist2P_I[i] = (Lat_I[LatP_I[i]]-Lat_I[k])**2+CosLat**2*(
               Lon_I[LonP_I[i]]-Lon_I[l])**2
         Dist2Min =max([min(Dist2N_I), min(Dist2P_I)])
         if Dist2Min<1.5*Ds2_C[k,l]:
            Dist2Min_C[k-LatARMin,l-LonARMin]=Ds2_C[k,l]/Dist2Min
   nParam = 8
   nVar=4
   Data_IV=np.zeros([nLatShort,nLonShort,nVar])
   NameVar='Longitude Latitude Br MapP MapN PIL'
   NameVar=NameVar+' Lon0 LonEarth xP yP xN yN xC yC'
   for k in np.arange(nLatShort):
      for l in np.arange(nLonShort):
         Data_IV[k,l,0]=max([-BMax,min([BMax,Br_C[k+LatARMin,l+LonARMin]])])
         Data_IV[k,l,1]=PSizeMap_C[k+LatARMin,l+LonARMin]
         Data_IV[k,l,2]=NSizeMap_C[k+LatARMin,l+LonARMin]
         Data_IV[k,l,3]=Dist2Min_C[k,l]
   Apex   = ApexIn
   BStrap = -1.
   while(Apex ==-1. or BStrap ==-1.):
      if (LonFPPosIn ==999. or LatFPPosIn ==999. or LonFPNegIn ==999.
          or LatFPNegIn ==999.):
         Param_I = np.array(
            [Lon0+LonARMin, LonEarth, LonPosIndex-LonARMin,LatPosIndex-LatARMin,
             LonNegIndex-LonARMin,LatNegIndex-LatARMin,iLonAR-LonARMin,
             iLatAR-LatARMin])
         FinalFile=rmag.save_bats(
            'ZoomMagnetogram.out', 'ZoomMagnetogram: Br[Gauss]',
            NameVar, [nLonShort,nLatShort], nVar, nParam,
            Param_I, Rad2Deg*Lon_I[LonARMin:LonARMax+1],
            Rad2Deg*Lat_I[LatARMin:LatARMax+1], Data_IV, Time)
         print(
            'Select location of positive footpoint with the left button')
         print('Then select negative  with the right button')
         FileId=open('runidl2','w')
         FileId.write(';\n')
         FileId.write('.r GLSETUP1 \n')
         FileId.write(
            "      TDSETUP1,file='"+FinalFile+"' \n")
         FileId.close()
         ########SHOW ZOOMED MAGNETOGRAM##########################
         ls = subprocess.Popen(["idl", "runidl2"],stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,text=True)
         #################PROCESSING STDOUT################
         stdout,stderr=ls.communicate()
         b=stdout[stdout.index('===')+4:len(stdout)]
         a=b.split() # x,y coordinates
         ###### TAKE COORDINATES OF THE FR FOOTPOINTS #######
         LonPosIndex = GL.round_my(float(a[0])) + LonARMin
         LatPosIndex = GL.round_my(float(a[1])) + LatARMin
         LonNegIndex = GL.round_my(float(a[2])) + LonARMin
         LatNegIndex = GL.round_my(float(a[3])) + LatARMin
      else:
         # The input locations are in degrees
         print ("\n User input  Lon/Lat for positive and negative footpoints:")
         print ("{0:4.1f} {1:4.1f} {2:4.1f} {3:4.1f} [deg]".format(
            LonFPPosIn, LatFPPosIn,LonFPNegIn, LatFPNegIn))
         # Convert coordinates in degrees to grid indexes
         LonPosIndex = GL.calculate_index(LonFPPosIn*Deg2Rad,Lon_I,nLon)
         LatPosIndex = GL.calculate_index(LatFPPosIn*Deg2Rad,Lat_I, nLat)
         LonNegIndex = GL.calculate_index(LonFPNegIn*Deg2Rad,Lon_I,nLon)
         LatNegIndex = GL.calculate_index(LatFPNegIn*Deg2Rad,Lat_I, nLat)
         LonFPPosIn = 999.
         LatFPPosIn = 999.
         LonFPNegIn = 999.
         LatFPNegIn = 999.      
      LonFPPos = Lon_I[LonPosIndex]   # in radian
      LatFPPos = Lat_I[LatPosIndex]   # in radian
      print(
         "Positive footpoint: Lon={0:6.2f},  Lat={1:6.2f} [deg]".format(
            LonFPPos*Rad2Deg, LatFPPos*Rad2Deg))
      LonFPNeg = Lon_I[LonNegIndex]   # in radian
      LatFPNeg = Lat_I[LatNegIndex]   # in radian
      print(
         "Negative footpoint: Lon={0:6.2f},  Lat={1:6.2f} [deg]".format(
            LonFPNeg*Rad2Deg, LatFPNeg*Rad2Deg))
      [LonFP,LatFP,OrientationFP,HalfDist] = TwoPointsOnSph(
         LonFPPos,LatFPPos,LonFPNeg,LatFPNeg)
      # Save indexes of the central point, in case we need to repeat FP-search
      iLonAR = GL.calculate_index(LonFP,Lon_I,nLon)
      iLatAR = GL.calculate_index(LatFP,Lat_I,nLat)
      # Major radius estimated from the distance between the footpoints
      RadiusFP = HalfDist
      # Angles are converted to degrees:
      LonFP *=Rad2Deg
      LatFP *=Rad2Deg
      OrientationFP *=Rad2Deg
      print ("Filament center: Longitude: {0:5.2f} Latitude:{1:5.2f}".format(
         LonFP, LatFP))
      print ("Filament: Orientation: {0:5.2f} Major Radius: {1:5.2f}".format(
         OrientationFP, RadiusFP))
      # Now we need both to correct OrientationFP by +/- 90 deg depending on
      # polarity, since Orientation has to determines the angle of x-axis for
      # the coordinate system, in which the direction from positive to negative
      # footpoints is parallel or antiparallel to y-axis
      if OrientationAR < 180.0:
         if(OrientationAR<OrientationFP and OrientationFP<OrientationAR+180):
            iHelicity  = 1
            OrientationFP  -=90
            if(OrientationFP<0):
               OreintationFP +=360
         else:
            iHelicity = -1
            OrientationFP +=90
            if(OrientationFP>360):
               OrientationFP -=360
      else:
         if(OrientationAR-180<OrientationFP and OrientationFP<OrientationAR):
            iHelicity = -1
            OrientationFP +=90
            if(OrientationFP>360):
               OreintationFP -=360
         else:
            iHelicity  = 1
            OrientationFP  -=90
            if(OrientationFP<0):
               OrientationFP +=360
      print("Corrected FP orientation: {:6.2f}".format(OrientationFP),
            " Helicity=",iHelicity)
      if RadiusAR > RadiusFP:
         TDLongitude = LonAR
         TDLatitude  = LatAR
         TDOrientation = OrientationAR
         TDRadius    = RadiusAR
         TDDepth     = 0.0
         PointN_I=[LonNeg,LatNeg] # negative spot
         PointP_I=[LonPos,LatPos] # positive spot
         AngularDistance = GL.get_angular_dist(PointN_I,PointP_I)
      else:
         TDLongitude = LonFP
         TDLatitude  = LatFP
         TDOrientation = OrientationFP
         TDRadius    = RadiusFP
         TDDepth     = HalfDist**2/(1 + np.sqrt(1 - HalfDist**2))
         AngularDistance = 2*math.asin(HalfDist)
      if Lon0>0:
         TDLongitude +=Lon0
         if TDLongitude>=360:
            TDLongitude-=360

      #Recommended TD flux rope parameters
      print ('========================================')
      print ('The Recommended TD FLux Rope Parameters')
      print ('========================================')
      print ('#CME')
      print ('               Longitude: %6.2f'%(TDLongitude))
      print ('                Latitude: %6.2f'%(TDLatitude))
      print ('             Orientation: %6.2f'%(TDOrientation))
      print ('      Angular size [deg]: %6.2f'%(AngularDistance*Rad2Deg))
      print (' Poloidal flux: positive: %6.2f'%(FluxP))
      print (' Poloidal flux: negative: %6.2f'%(FluxN))
      print ('-----------------------------------------')
      FileId=open('CME.in','w')
      FileId.write("#LOOKUPTABLE \n")
      FileId.write("B0                  NameTable \n")
      FileId.write("load	            NameCommand \n")
      FileId.write("harmonics_bxyz.out	NameFile \n")
      FileId.write("real4	            TypeFile \n")
      FileId.write("\n")
      FileId.write("#CME \n")
      FileId.write("T                   UseCme \n")
      FileId.write("F                   DoAddFluxRope \n")
      FileId.write("-1.0                tDecayCme \n")
      FileId.write("%-10.2f          LongitudeCme \n"% TDLongitude)
      FileId.write("%-10.2f          LatitudeCme \n"% TDLatitude)
      FileId.write("%-10.2f          OrientationCme \n"% TDOrientation)
      FileId.write("TD22                TypeCme \n")
      FileId.write("%-+d                  iHelicity \n"% iHelicity)
      FileId.write("%-10.2f	    RadiusMajor \n"%(TDRadius))
      FileId.write("%-10.2f	    RadiusMinor \n"%(a2r0Ratio*TDRadius))
      FileId.write("%-10.2f	    Depth \n"%TDDepth)
      FileId.write("1.0e-3              PlasmaBeta \n")
      FileId.write("5.0e5               EjectaTemperature \n")
      FileId.write("readbstrap          TypeBStrap \n")
      FileId.write("0.0                 bStrappingDim \n")
      FileId.write("none                TypeCharge \n")
      FileId.write(" \n")
      FileId.write("#END \n")
      FileId.write("\n")
      FileId.write("Angular Size            = %5.2f\n"%(AngularDistance
                                                        *Rad2Deg))
      FileId.write(" Poloidal flux: positive: %6.2f"%(FluxP))
      FileId.write(" Poloidal flux: negative: %6.2f"%(FluxN))
      FileId.close() 
                                                    
      # Get distribution of strapping field in the plane of filament
      FileId=open('RunFRM','w')
      FileId.write('%-3d \n'%Lon0)
      if IsUniformLat :
         FileId.write('uniform latitude \n')
      else:
         FileId.write('sin(latitude) \n')
      FileId.close()
      FileId=open('RunFRM','r')
      subprocess.call('./FRMAGNETOGRAM.exe',stdin=FileId)
      FileId.close()
      FileId=open('runidl3','w')
      FileId.write(';\n')
      FileId.write('.r GLSETUP1 \n')
      FileId.write(
         "TDSETUP2,file='FRM_x=0.out',radius={0:4.2f}, Apex={1:4.2f}, BMax={2:4.2f}\n".format(
            TDRadius,Apex,BStrapMax))
      FileId.close()
      ########SHOW STRAPPING FIELD, CHOOSE APEX #####################
      print('Now, you will see isolines of strapping field.\n')
      print('If there is an isoline of strapping field, which:\n')
      print('    -Looks like a part of circumference;\n')
      print('    -Connects footpoints marked with two crosses,\n')
      print('then, click on the apex of this isoline.\n')
      print(
         'This will become the toroidal magnetic axis of TD configuration\n')
      print(
         'Otherwise, click somewhere very far or very close to point 0,0\n')
      print(
         'You will be allowed to modify your choice of flux rope footpoints')
      ls = subprocess.Popen(["idl", "runidl3"],stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,text=True)
      stdout,stderr=ls.communicate()
      if '+++' in stdout:
         IndexOfApex = stdout.index('+++')
         b=stdout[IndexOfApex+4:len(stdout)]
         a=b.split() # x,y coordinates
         ###### TAKE APEX  AND STRAPPING FIELD #######
         Apex = float(a[0])
         BStrap = float(a[1])
         print('Apex={0:4.2f}, bStrappingDim={1:4.2f}'.format(Apex,BStrap))
   else:
      # Now, we know:
      # TDRadius - half of  distance between footpoints of the magnetic axis
      # TDDepth  - depth of the midpoint between the footpoints
      # Apex     - of  the toroidal magnetic axis
      # We need to solve the radius, RInfty of the circumference (toroidal
      # magnetic axis) passing through the footpoints and apex, and Depth
      # of the center of the said magnetic axis, from the system of eqs:
      #  RInfty = Apex + Depth
      #  RInfty**2 =  TDRadius**2 +  (Depth - TDDepth)**2
      # The solution of  this system  fo RInfty reads:
      RInfty = 0.50*( TDRadius**2/(Apex + TDDepth) + (Apex + TDDepth) )
      # Now,  calculate  Depth:
      TDDepth = RInfty - Apex
      # Expression, relating RInfty with major radius, R0, and minor radius, a
      # RInfty**2 = R0**2 - a**2
      # So that the major radius is:
      TDRadius = RInfty/np.sqrt(1 - a2r0Ratio**2)
      
      #Recommended TD flux rope parameters
      print ('========================================')
      print ('The Recommended TD FLux Rope Parameters')
      print ('========================================')
      print ('#CME')
      print ('               Longitude: %6.2f'%(TDLongitude))
      print ('                Latitude: %6.2f'%(TDLatitude))
      print ('             Orientation: %6.2f'%(TDOrientation))
      print ('      Angular size [deg]: %6.2f'%(AngularDistance*Rad2Deg))
      print (' Poloidal flux: positive: %6.2f'%(FluxP))
      print (' Poloidal flux: negative: %6.2f'%(FluxN))
      print ('-----------------------------------------')
      FileId=open('CME.in','w')
      FileId.write("#LOOKUPTABLE \n")
      FileId.write("B0                  NameTable \n")
      FileId.write("load	            NameCommand \n")
      FileId.write("harmonics_bxyz.out	NameFile \n")
      FileId.write("real4	            TypeFile \n")
      FileId.write("\n")
      FileId.write("#CME \n")
      FileId.write("T                   UseCme \n")
      FileId.write("T                   DoAddFluxRope \n")
      FileId.write("-1.0                tDecayCme \n")
      FileId.write("%-10.2f          LongitudeCme \n"% TDLongitude)
      FileId.write("%-10.2f          LatitudeCme \n"% TDLatitude)
      FileId.write("%-10.2f          OrientationCme \n"% TDOrientation)
      FileId.write("TD22                TypeCme \n")
      FileId.write("%-+d                  iHelicity \n"% iHelicity)
      FileId.write("%-10.2f	    RadiusMajor \n"%(TDRadius))
      FileId.write("%-10.2f	    RadiusMinor \n"%(a2r0Ratio*TDRadius))
      FileId.write("%-10.2f	    Depth \n"%TDDepth)
      FileId.write("1.0e-3              PlasmaBeta \n")
      FileId.write("5.0e5               EjectaTemperature \n")
      FileId.write("readbstrap          TypeBStrap \n")
      FileId.write("%-10.2f	     bStrappingDim \n"%BStrap)
      FileId.write("none                TypeCharge \n")
      FileId.write(" \n")
      FileId.write("#END \n")
      FileId.write("\n")
      FileId.write("Angular Size            = %5.2f\n"%(AngularDistance
                                                        *Rad2Deg))
      FileId.write(" Poloidal flux: positive: %6.2f"%(FluxP))
      FileId.write(" Poloidal flux: negative: %6.2f"%(FluxN))
      FileId.close() 
                                                    
      # Get distribution of strapping field in the plane of filament
      FileId=open('RunFRM','w')
      FileId.write('%-3d \n'%Lon0)
      if IsUniformLat :
         FileId.write('uniform latitude \n')
      else:
         FileId.write('sin(latitude) \n')
      FileId.close()
      FileId=open('RunFRM','r')
      subprocess.call('./FRMAGNETOGRAM.exe',stdin=FileId)
      FileId.close()
      
   return(nLon,nLat,nParam,Param_I,Lon_I,Lat_I,Br_C,PSizeMap_C,NSizeMap_C)
