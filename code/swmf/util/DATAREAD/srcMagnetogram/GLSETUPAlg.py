#!/usr/bin/env python
import numpy as np
import remap_magnetogram as rmag
import os
import fnmatch
import math
import subprocess
import sys

BMax = 1900.0
cPi  = np.pi
Rad2Deg = 180/cPi
Deg2Rad = cPi/180

def round_my(x):
   i = int(round(x))
   return(i)

def get_angular_dist(Point1_I,Point2_I):
   #Calculate the angular distance between two points on a sphere
   AngularDist = 2*math.asin(np.sqrt(
                           np.sin(0.5*(Point1_I[1] - Point2_I[1]))**2
                           + np.cos(Point1_I[1])*np.cos(Point2_I[1])*
                           np.sin(0.5*(Point1_I[0] - Point2_I[0]))**2))
   return(AngularDist)

def get_weighted_center(X,Y,Br_C,BrThreshold,nLat,nLong,Lat_I,Long_I,\
                           IsUniformLat):
   LonIndex = round_my(X)
   LatIndex = round_my(Y)
   print("\n Chosen Longitude = {0:7.3f}, Latitude= {1:7.3f} [deg]".format(
      Long_I[LonIndex]*Rad2Deg,Lat_I[LatIndex]*Rad2Deg))
   #Occcupancy matrix
   occ = np.zeros([nLat,nLong])
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
            if colp+1 < nLong:
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
            if colp+1 < nLong:
               if (Br_C[rowp,colp+1] < BrThreshold and occ[rowp,colp+1] == 0):
                  occ[rowp,colp+1] = occ_level + 1
      occ_check = n
   #end whileloop
   #Calculate weighted center
   [LatOcc,LonOcc] = np.where(occ>0)
   nSize = LatOcc.size
   LatCenter=0.
   LonCenter=0.
   Flux=0.
   Area = 0.
   #flux = SUM(area * Br)
   dLon    = 2.*cPi/nLong
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
      LonCenter += Long_I[iLon] * dFlux
      LatCenter +=  Lat_I[iLat] * dFlux
      Flux += dFlux
      Area += dArea
   LonCenter /= Flux  # in Radians
   LatCenter /= Flux  # in Radians
   # return the longitude and latitude of the weighted center in radians, 
   # area in radians^2, and the occupancy matrix for plotting
   return(LatCenter,LonCenter,occ,Area)

def calculate_index(Y, Y_I, n):
   # Calculate the index from an interpolated array
   # Y   - lon/lat for which index in the lon/lat array needs to be calculated
   # Y_I - lon/lat array
   # n   - resolution of lon/lat array
   X_I = np.linspace(0, n-1, n)  # array of data indices
   RealIndex = np.interp(Y, Y_I, X_I) # interpolated real value of index
   Index = round_my(RealIndex)
   return(Index)

def Alg(nLong, nLat, nParam, Param_I, Long_I, Lat_I, Br_C, CMESpeed, GLRadius,
        SizeFactor, GLRadiusRange_I, UseCMEGrid, Orientation,
        Stretch, Distance, Helicity, DoHMI, UsePNDist, 
        UseARArea, DoScaling, Time, DoNotPlot,
        min_GL_Bstrength = 2, max_GL_Bstrength = 40):
   Long0     = Param_I[0]
   LongEarth = Param_I[1]
   # Pass the x, y indices of the clicks to calculate weighted center
   # and their indices
   xPositive = Param_I[2]
   yPositive = Param_I[3]
   xNegative = Param_I[4]
   yNegative = Param_I[5]
   # Check if the latitude grid is uniform
   if abs(Lat_I[2]-2*Lat_I[1]+Lat_I[0])<1.0e-5:
      IsUniformLat = True
      print('Uniform in Latitude grid')
   else:
      IsUniformLat = False
      print('Uniform in Sin(Latitude) grid')

   if Distance == -1 :
      Distance = 1.8   #standard parameter
   if Stretch == -1 :
      Stretch  = 0.6   #standard parameter
   
   # get weighted centers(Lon,Lat), occupancy matrix, Area of AR for
   # positive and negative regions
   [LatPos,LonPos,occPos,AreaPos] = \
       get_weighted_center(xPositive,yPositive,Br_C,20.,\
                              nLat,nLong,Lat_I,Long_I,IsUniformLat)
   LonPosIndex = calculate_index(LonPos,Long_I,nLong)
   LatPosIndex = calculate_index(LatPos,Lat_I, nLat)

   print(
      '\n Positive Weighted Center Lon={0:6.2f}, Lat={1:6.2f} [deg]'.format(
         LonPos*Rad2Deg, LatPos*Rad2Deg))

   [LatNeg,LonNeg,occNeg,AreaNeg] = \
       get_weighted_center(xNegative,yNegative,Br_C,-20.,\
                              nLat,nLong,Lat_I,Long_I,IsUniformLat)
   LonNegIndex = calculate_index(LonNeg,Long_I,nLong)
   LatNegIndex = calculate_index(LatNeg,Lat_I, nLat)
   print(
      '\n Negative Weighted Center Lon={0:6.2f}, Lat={1:6.2f} [deg]'.format(
         LonNeg*Rad2Deg,LatNeg*Rad2Deg))

   PointN_I = [LonNeg, LatNeg] # negative spot
   PointP_I = [LonPos, LatPos] # positive spot

   # Find center of the active region as the point on the line
   # connecting the positive and negative center at which the MF is minimal
   # (i.e as intersection of this with PIL,
   # herewith PIL=Polarity Inversion Line
   nProfile = max([round_my(abs(LonPos - LonNeg)*Rad2Deg),
                   round_my(abs(LatPos-LatNeg)*Rad2Deg)]) + 1
   LonProfile_C = np.zeros(nProfile)
   LatProfile_C = np.zeros(nProfile)
   BTmp = BMax + 1.0
   for i in np.arange(nProfile):
      LonProfile = LonPos+(
         LonNeg - LonPos)*i/(nProfile - 1)
      LonProfile_C[i] = LonProfile
      LatProfile = LatPos+(
         LatNeg - LatPos)*i/(nProfile - 1)
      LatProfile_C[i] = LatProfile
      IndexLon = calculate_index(LonProfile,Long_I,nLong)
      IndexLat = calculate_index(LatProfile,Lat_I, nLat)
      AbsBr = abs(Br_C[IndexLat,IndexLon])
      if (AbsBr < BTmp):
         BTmp = AbsBr
         IndexARCenter_D = [IndexLon,IndexLat]
   iLonAR = IndexARCenter_D[0]
   iLatAR = IndexARCenter_D[1]
   LonAR  = Long_I[iLonAR]  # in radians
   LatAR  =  Lat_I[iLatAR]  # in radians
   print ("Center for Active region(Lon,Lat in deg):" )
   print ("{0:4.1f} {1:4.1f}".format(LonAR*Rad2Deg,LatAR*Rad2Deg))
   GL_Latitude  = LatAR * Rad2Deg
   GL_Longitude = LonAR * Rad2Deg
   if Long0>0:
      GL_Longitude +=Long0
      if GL_Longitude>=360:
         GL_Longitude-=360
   print ("GL_Longitude: {0:4.1f} GL_Latitude:{1:4.1f}".format(
         GL_Longitude, GL_Latitude))

   # Determines the non-zero elements of the occupancy matrix 
   # to fill the SizeMap arrays with the magnetic field values
   # within the AR
   PSizeMap_C = np.zeros([nLat,nLong])
   NSizeMap_C = np.zeros([nLat,nLong])
   # Location of points within positive and negative active regions
   [rowP,colP] = np.where(occPos > 0)
   [rowN,colN] = np.where(occNeg > 0)
   PSizeMap_C[rowP,colP] = Br_C[rowP,colP]
   NSizeMap_C[rowN,colN] = Br_C[rowN,colN]

   #Calculate the gradient of the Br field
   DDx_C = np.zeros([nLat,nLong])
   DDy_C = np.zeros([nLat,nLong]) 
   DDx_C[:,1:nLong-1] = (Br_C[:,2:nLong] - Br_C[:,0:nLong-2])/2.
   DDx_C[:,0        ] =  Br_C[:,1      ] - Br_C[:,0        ]
   DDx_C[:,nLong-1  ] =  Br_C[:,nLong-1] - Br_C[:,nLong-2  ]

   DDy_C[1:nLat-1, :] = (Br_C[2:nLat, :] - Br_C[0:nLat-2, :])/2.
   DDy_C[0       , :] =  Br_C[1     , :] - Br_C[0       , : ]
   DDy_C[nLat-1  , :] =  Br_C[nLat-1, :] - Br_C[nLat-2  , :]
   GradBr_C=np.sqrt(DDx_C**2 + DDy_C**2)
   #Calculate a Bit Map (1/0) for GradB multiplied by MF
   GradBrMap_C = np.where(GradBr_C > 0.5, Br_C, 0.)

   # Cell size is used to divide the magnetogram to sub regions 
   # in order to determine the PIL. 
   # Setup the threshold for selecting cells near the PIL.
   BThreshold = 2.0
   nCell = 1
   #Calculate the PILBitMap_C (1/0) for determining the PIL.
   M = nLong//nCell
   N = nLat//nCell
   PILBitMap_C = np.zeros([nLat,nLong])
   for j in np.arange(N-1):
      for i in np.arange(M-1): 
         if(np.amin(Br_C[j*nCell:(j+1)*nCell+1,i*nCell:(i+1)*nCell+1]) 
            < -BThreshold  and  
            np.amax(Br_C[j*nCell:(j+1)*nCell+1,i*nCell:(i+1)*nCell+1])
            >  BThreshold):
            #PIL intersects this quadrant. Among these 4 points 
            #we do not include those in which the gradient is too small: 
            PILBitMap_C[j*nCell:(j+1)*nCell+1,i*nCell:(i+1)*nCell+1] = \
                GradBrMap_C[j*nCell:(j+1)*nCell+1,i*nCell:(i+1)*nCell+1]
   # Vector along PN spots
   Diff_LonPN = LonNeg - LonPos
   Diff_LatPN = LatNeg - LatPos
   Dist_PN     = np.sqrt(Diff_LonPN**2 + Diff_LatPN**2)  # in radians
   AngularDistance = get_angular_dist(PointN_I,PointP_I)

   # Distances from the AR center and spot centers:
   DisCenter_C = np.zeros([nLat,nLong])
   for j in np.arange(nLat):
      for i in np.arange(nLong):
         DisCenter_C[j,i] = np.sqrt(
            (Long_I[i]-LonAR)**2+(Lat_I[j]-LatAR)**2)  # in radians
   # Distance cut-off for determining the PIL.
   # Min:2/3 of the distance between positive and negative spots.
   # Max:dist between the two spots
   DisThreshold = Dist_PN * (2./3.)
   DisMax      = Dist_PN
   PILMap_C = np.where(DisCenter_C<=min([DisMax,DisThreshold]),PILBitMap_C, 0.)

   # Calculate Active Region Size for determining the GL size.
   # AR size can be determined in two ways:
   # 1) based on distance between two spot centers
   # 2) based on area of the Active Region (DEFAULT)
   if UsePNDist:
      if GLRadius <= 0.:
         # Use the law of cosines to calculate the GLRadius
         # Angular width is twice the distance between positive and 
         # negative spot centers
         # AngularDist is the angle subtended by two points
         # at the center of the sphere
         AngularWidth = SizeFactor * AngularDistance   #default SizeFactor is 1
         Stretch1 = 1 + Stretch                    #Radius of the stretched Sun
         GLRadius = np.sqrt( Stretch1**2 + Distance**2 -
                             2 * Stretch1 * Distance * np.cos(AngularWidth))
   elif UseARArea:  ## Default option
      ## area is already calculated from occupancy matrix
      ARArea = AreaPos + AreaNeg # in radian^2
      #ARSize=np.count_nonzero(PSizeMap_C) + np.count_nonzero(NSizeMap_C)
      #GLRadiusOLD= 0.8/280.* ARSize
      ## Based on AR for 7 March 2011 event
      ##         (Jin+2016, DOI: 10.3847/1538-4357/834/2/172),
      ## GLRadius = 0.8, therefore, we use (area ~ 0.55) as the factor 
      GLRadius = (0.8/0.055) * ARArea
   ## Radius is limited by the range
   GLRadius= max([min([GLRadius,GLRadiusRange_I[1]]),GLRadiusRange_I[0]])
   print('GLRadius =',GLRadius)
   
   # Solve the 2nd order equation below to get the scaling factor
   # Radius^2 = (1+Stretch)^2 + (Distance)^2 - 2*(1+Stretch)Distance*cos(angle)
   # angle is the angle made by the two points at the center of the Sun
   # R -> Alpha*R , Dist -> Alpha*Dist , Stretch -> Alpha*Stretch
   if DoScaling:
      AlphaInv = Distance * np.cos(AngularDistance) - Stretch + \
          np.sqrt(GLRadius**2 - (Distance**2 * (np.sin(AngularDistance))**2))
      # Below is the negative root
      # AlphaInv = Distance * np.cos(AngularDistance) - Stretch - \
      #  np.sqrt(GLRadius**2 - (Distance**2 * (np.sin(AngularDistance))**2))
      Distance = Distance/AlphaInv
      GLRadius = GLRadius/AlphaInv
      Stretch  = Stretch/AlphaInv            
      print('Alpha= ',1/AlphaInv)
      print('Scaled Distance,GLRadius,Stretch =',
            Distance,GLRadius,Stretch)

   ## GL_Orientation calculation
   if Orientation != -1.0 :
      GL_Orientation = Orientation
   else:
      ## Calculate the GL flux rope orientation from the two weighted points.
      r1 = [PointN_I[0] - PointP_I[0], PointN_I[1] - PointP_I[1]]
      r1[0] *= np.cos(LatAR)
      GL_Orientation = np.arctan2(r1[1], r1[0])*Rad2Deg

   GL_Orientation = np.mod(GL_Orientation, 360.0)

   # Calculate the poloidal flux needed for the observed CME velocity.
   # Flux is calculated using average of the radial field around the 
   # weighted center spots.
   # The second option of taking avg of field along PIL is currently removed.
   # These relationships are based on the GONG magnetogram with nsmooth = 5
   RegionSize_ARMag=round_my((4.0*nLong)/360)
   br_ar=np.mean(
      abs(Br_C[(iLatAR-RegionSize_ARMag//2):
               (iLatAR+RegionSize_ARMag//2)+1,
               (iLonAR-RegionSize_ARMag//2):
               (iLonAR+RegionSize_ARMag//2)+1]))

   # Equation 1 from Jin+2017 (DOI: 10.3847/1538-4357/834/2/173)
   alpha = 0.440
   beta  = 3043.93
   gamma = 565.05
   GL_poloidal=(CMESpeed * br_ar**alpha - beta) / gamma

   # Removed the ARMag = 2 part
   # Relationship between the PIL length and the GL flux rope Radius.   
   # This factor is now based on the 2011 March 7 CME. More tests  
   # are needed in order to get a more precise value.
   #  From Borovikov+2017 (DOI:10.1002/2017JA024304), in text after Eqn. 3
   a = 13.169 # 4pi / ((a0r0)^2)*beta0)
   b = 21.457
   GL_Bstrength = a * GL_poloidal / (b * GLRadius**2)

   ## Print WARNING information if GL_Bstrength is below minimum.
   ##   Default minimum is 2 Gs.
   ##   Warning also notes if original BStrength would be below 0.   
   if GL_Bstrength < min_GL_Bstrength:
      print('**********************************************')
      print('WARNING: BStrength < {:.1f} Gs.'.format(min_GL_Bstrength))
      if GL_Bstrength <= 0:
         print('   CALCULATION FAILED WITH 0 Gs!')
         print('   USE WITH CAUTION!')
      print('Either the active region is too weak or the')
      print('CME speed is too small!')
      print('GL Poloidal Flux is set to minimum!')
      print('**********************************************')
      GL_Bstrength = min_GL_Bstrength
      GL_poloidal = GL_Bstrength * b * GLRadius**2 / a
   
   ## Limit BStrength to avoid non-physical values.
   ##   Default value is 40 Gs.
   if GL_Bstrength > max_GL_Bstrength:
      print('******************************************************')
      print("WARNING: BStrength > {:.1f} Gs.".format(max_GL_Bstrength))
      print("  Modifying GL parameters to restrict BStrength.")

      ## Keep GL_poloidal constant
      ## Increase GLRadius to allow GL_Bstrength to decrease to
      ##   realistic value.
      GLRadius_old = GLRadius
      GLRadius = GLRadius * np.sqrt(GL_Bstrength / max_GL_Bstrength)
      GL_Bstrength_old = GL_Bstrength
      GL_Bstrength = max_GL_Bstrength

      print("  Radius changed from {:.2f} to {:.2f}" \
            .format(GLRadius_old, GLRadius))
      print("  BStrength changed from {:.2f} to {:.2f}" \
            .format(GL_Bstrength_old, GL_Bstrength))
      print('******************************************************')
   
   if(Br_C[iLatAR,iLonAR]>0):
      iYPIL_I,iXPIL_I=np.where(PILMap_C>0)
   else:
      iYPIL_I,iXPIL_I=np.where(PILMap_C<0)
   nPIL=len(iXPIL_I)
   
   if IsUniformLat :
      grid_type = 'uniform'
   else:
      grid_type = 'sin(lat)'

   # Flux rope helicity is determined by:
   # (1) HMI vector magnetograms 
   # (2) Angle between PN spots and PIL
   # (3) Hemisphere of the GL FR 
   # Vector along PIL
   Diff_LonPIL = Long_I[int(round(iXPIL_I[nPIL-1]))] - \
       Long_I[int(round(iXPIL_I[0]))]
   Diff_LatPIL = Lat_I[int(round(iYPIL_I[nPIL-1]))] - \
       Lat_I[int(round(iYPIL_I[0]))]
   Dist_PIL    = np.sqrt(Diff_LonPIL**2 + Diff_LatPIL**2)

   IsPresentHMI = 0
   HMI_helicity = 0
   # (1) Helicity based on HMI vector map
   # Save magnetic field componets from HMI vector magnetogram
   if DoHMI :
      hmi_data = rmag.read_hmi(nLat,nLong,grid_type,1)
      IsPresentHMI = hmi_data[0]
      if (IsPresentHMI == 1):
         hmi_Brad = hmi_data[3]
         hmi_Blon = hmi_data[4]
         hmi_Blat = hmi_data[5]
         
         hmi_BradSum = 0.
         hmi_BlatSum = 0.
         hmi_BlonSum = 0.
      #Average field components along PIL
         for i in np.arange(nPIL):
            hmi_BradSum += \
                hmi_Brad[int(round(iYPIL_I[i])),int(round(iXPIL_I[i]))]
            hmi_BlatSum += \
                hmi_Blat[int(round(iYPIL_I[i])),int(round(iXPIL_I[i]))]
            hmi_BlonSum += \
                hmi_Blon[int(round(iYPIL_I[i])),int(round(iXPIL_I[i]))]
         
         hmi_BradAvg = hmi_BradSum / nPIL
         hmi_BlonAvg = hmi_BlonSum / nPIL
         hmi_BlatAvg = hmi_BlatSum / nPIL
      #helicity calculation
      #cross product of vector along the PN spots and average field along PIL
         cross_pro = \
             (hmi_BlatAvg * Diff_LonPN - hmi_BlonAvg * Diff_LatPN)/Dist_PN
         HMI_helicity = np.sign(cross_pro)

   if Helicity != 0 : # helicity input by user
      iHelicity = Helicity
      print('Using user input helicity = ', Helicity)
   elif IsPresentHMI == 0: # helicity if HMI is not used
      # based on hemisphere
      iHelicity = 1
      if GL_Latitude > 0: 
         iHelicity = -1
         print('Helicity based on hemisphere: ',iHelicity)
   else: # helicity if HMI is present & used
      iHelicity = HMI_helicity
   
   # (2) Helicity based on angle between PN vector & PIL vector
   # DOT product 
   dot_product   = (Diff_LonPN * Diff_LonPIL + Diff_LatPN * Diff_LatPIL)/\
       (Dist_PN * Dist_PIL)
   cross_product = (Diff_LatPIL * Diff_LonPN - Diff_LonPIL * Diff_LatPN)/\
       (Dist_PN * Dist_PIL)
   tan_theta = cross_product / dot_product
   theta = math.atan2(cross_product,dot_product)
   print(' ')
   hem_helicity = 1 
   if IsPresentHMI == 1 :
      print('Helicity based on HMI vector magnetogram      : ', HMI_helicity)
   
   print('Helicity based on angle b/w PN & PIL vectors  : ',np.sign(tan_theta))

   if GL_Latitude > 0:
      hem_helicity = - hem_helicity
   print('Helicity based on North/South Hemisphere      :',hem_helicity)
   
   ApexHeight = GLRadius + Distance - Stretch - 1.0

   #Recommended GL flux rope parameters
   print ('========================================')
   print ('The Recommended GL FLux Rope Parameters')
   print ('========================================')
   print ('#CME')
   print ('                Latitude: %6.2f'%(GL_Latitude))
   print ('               Longitude: %6.2f'%(GL_Longitude))
   print ('             Orientation: %6.2f'%(GL_Orientation))
   print ('                  Radius: %6.2f'%(GLRadius))
   print ('               Bstrength: %6.2f'%(GL_Bstrength))
   print ('         Stretch (FIXED): %6.2f'%(Stretch))
   print ('        Distance (FIXED): %6.2f'%(Distance))
   print ('             Height [Rs]: %6.2f'%(ApexHeight))
   print ('      Angular size [deg]: %6.2f'%(
      2*GLRadius/Distance*Rad2Deg))
   print (' Poloidal flux [1E21 Mx]: %6.2f'%(GL_poloidal))
   print ('-----------------------------------------')
   FileId=open('CME.in','a')
   FileId.write("#CME \n")
   FileId.write("T                   UseCme \n")
   FileId.write("T                   DoAddFluxRope \n")
   FileId.write("-1.0                tDecayCme \n")
   FileId.write("%-10.2f          LongitudeCme \n"% GL_Longitude)
   FileId.write("%-10.2f          LatitudeCme \n"% GL_Latitude)
   FileId.write("%-10.2f          OrientationCme \n"% GL_Orientation)
   FileId.write("GL                  TypeCme \n")
   FileId.write("%-10.2f          BStrength \n"% GL_Bstrength)
   FileId.write("%-+d                  iHelicity \n"% iHelicity)
   FileId.write("%-10.2f          Radius \n"% GLRadius)
   FileId.write("%-10.2f          aStretch \n"% Stretch)
   FileId.write("%-10.2f          ApexHeight \n"% ApexHeight)
   FileId.write(" \n")
   FileId.write("#END \n")
   FileId.write("\n")
   FileId.write("Angular Size            = %5.2f\n"%(
      2*GLRadius/Distance*Rad2Deg))
   FileId.write("Poloidal flux [1E21 Mx] = %5.2f\n"%(GL_poloidal))
   FileId.write("Average HMI field components along PIL :\n")
   if DoHMI:
      FileId.write("HMI Brad = %-5.2f \n"%hmi_BradAvg)
      FileId.write("HMI Blon = %-5.2f \n"%hmi_BlonAvg)
      FileId.write("HMI Blat = %-5.2f \n"%hmi_BlatAvg)
      FileId.write("\n")
      FileId.write("Before HMI helicity correction : \n")
      FileId.write("GL_BStrength = %5.2f \n"%(GL_Bstrength/HMI_helicity))
      FileId.write("GL_poloidal  = %5.2f \n"%GL_poloidal)
      FileId.write("Gl_Radius    = %5.2f \n"%GLRadius)
      FileId.write("\n")
   FileId.write("Helicity: \n")
   FileId.write("Input Helicity                     : %4.2f\n"%Helicity)
   FileId.write("From HMI vector mag                : %4.2f\n"%HMI_helicity)
   FileId.write("From angle between PN & PIL vector : %4.2f\n"%np.sign(tan_theta))
   FileId.write("From hemisphere                    : %4.2f\n"%hem_helicity)
   FileId.write('Dot product   : value, angle b/w PN & PIL vectors, sign = {0:5.2f} {1:5.2f} {2:5.2f} \n'.format(dot_product,math.acos(dot_product)*Rad2Deg, np.sign(dot_product)))
   FileId.write('Cross product : value, angle b/w PN & PIL vectors, sign = {0:5.2f} {1:5.2f} {2:5.2f} \n'.format(cross_product,math.asin(cross_product)*Rad2Deg,np.sign(cross_product)))
   FileId.write('Tan(theta)    : value, angle b/w PN & PIL vectors, sign = {0:5.2f} {1:5.2f} {2:5.2f} \n'.format(tan_theta,theta*Rad2Deg, np.sign(tan_theta)))
   FileId.close() 

   if UseCMEGrid:
      #Calculate the CME grid refinement parameters based on the flux rope
      #location and size.                                                

      CMEbox_Start=[1.15,GL_Longitude-40.*GLRadius,GL_Latitude-20.*GLRadius]
      CMEbox_End=[22.0,GL_Longitude+40.*GLRadius,GL_Latitude+20.*GLRadius]
      
      print ('==========================================')
      print ('The Recommended Grid Refinement Parameters')
      print ('==========================================')
      print ('              R_Start: %6.2f'% (CMEbox_Start[0]))
      print ('                R_End: %6.2f'% (CMEbox_End[0]))
      print ('      Longitude_Start: %6.2f'% ( CMEbox_Start[1]))
      print ('        Longitude_End: %6.2f'% ( CMEbox_End[1]))
      print ('       Latitude_Start: %6.2f'% ( CMEbox_Start[2]))
      print ('         Latitude_End: %6.2f'% ( CMEbox_End[2]))
      print ('-----------------------------------------')
      FileId=open('CME_AMR.in','w')
      FileId.write("#AMRREGION \n")
      FileId.write("CMEbox              NameRegion \n")
      FileId.write("box_gen             StringShape \n")
      FileId.write("%-10.2f          XyzMinBox Radius \n"% CMEbox_Start[0])
      FileId.write("%-10.2f          XyzMinBox Longitude \n"%  CMEbox_Start[1])
      FileId.write("%-10.2f          XyzMinBox Latitude \n"% CMEbox_Start[2])
      FileId.write("%-10.2f          XyzMaxBox Radius \n"% CMEbox_End[0])
      FileId.write("%-10.2f          XyzMaxBox Longitude \n"%  CMEbox_End[1])
      FileId.write("%-10.2f          XyzMaxBox Latitude \n"% CMEbox_End[2])
      FileId.write(" \n")
      FileId.write("#END \n")
      FileId.close()
   #For comparison, make magnetogram of a flux rope field
   FileId=open('RunFRM','w')
   FileId.write('%-3d \n'%Long0)
   if IsUniformLat :
      FileId.write('uniform latitude \n')
   else:
       FileId.write('sin(latitude) \n')
   FileId.close()
   FileId=open('RunFRM','r')
   subprocess.call('./FRMAGNETOGRAM.exe',stdin=FileId)
   FileId.close()
   if not DoNotPlot:
      nParam = 8 + 2*nPIL
      Param_I = np.zeros(nParam)
      Param_I[0:8] = [Long0,LongEarth,LonPosIndex,LatPosIndex,LonNegIndex,
                      LatNegIndex,iLonAR,iLatAR]
      Param_I[8:8+nPIL]=iXPIL_I
      Param_I[8+nPIL:8+2*nPIL]=iYPIL_I
      nVar=5
      Data_IV=np.zeros([nLat,nLong,nVar])
      NameVar='Longitude Latitude Br PMap NMap occPos occNeg Long0 LongEarth xP yP xN yN xC yC xPIL1({0:1d}) yPIL1({0:1d})'.format(nPIL)
      for k in np.arange(nLat):
         for l in np.arange(nLong):
            Data_IV[k,l,0]=max([-BMax,min([BMax,Br_C[k,l]])])
            Data_IV[k,l,1]=PSizeMap_C[k,l]
            Data_IV[k,l,2]=NSizeMap_C[k,l]
            Data_IV[k,l,3]=occPos[k,l]
            Data_IV[k,l,4]=occNeg[k,l]
      FinalFile=rmag.save_bats('AfterGLSETUP.out', 'After GLSETUP: Br[Gauss]',
                               NameVar, [nLong,nLat], nVar, nParam, Param_I,
                               Rad2Deg*Long_I,Rad2Deg*Lat_I, Data_IV, Time)
   return(nLong,nLat,nParam,Param_I,Long_I,Lat_I,Br_C,PSizeMap_C,NSizeMap_C,occPos,occNeg)

