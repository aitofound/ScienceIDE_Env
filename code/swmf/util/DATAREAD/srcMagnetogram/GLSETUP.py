#!/usr/bin/env python
####################### MASTER SCRIPT FOR EEGGL or SWMF_GLSETUP#######
####################### CAN BE IMPLEMENTED IN ANY SCRIPT LANGUAGE#####
####April 2020: Updated script for different types of magnetograms
# make FRM in SWMF/util/EMPIRICAL/srcEE/

import subprocess
import os
import fnmatch
import remap_magnetogram as rmag
import numpy as np
import argparse
import GLSETUPAlg as GL
from swmfpy.web import download_magnetogram_hmi as hmi_map
import datetime as dt
import sys

BMax = 1900.0
cPi = np.pi
Rad2Deg = 180/cPi
Deg2Rad = 1/Rad2Deg
IsPositionInput = 0

if __name__ == '__main__':
   parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
   parser.add_argument('NameFile', help='Input FITS file name including path')
   parser.add_argument('nlat', nargs='?', type=int, default=180, help=
                       'Number of latitude points in output. Default is 180.')
   parser.add_argument('nlon', nargs='?', type=int, default=360, help= 
                       'Number of longitude points in output. Default is 360.')
   parser.add_argument('-outgrid',choices=['uniform','sinlat'],help=
                       'type of latitude grid in the output. ' + \
                       'Default is same as input.')
   parser.add_argument('-index', type=int, default=1, help=
                       'Initial map index. Default is the first map.')
   parser.add_argument('-nSmooth',type=int, default=5, help=
                       'If nSmooth is ODD integer larger than 1, ' + \
                       'apply boxcar smoothing on the magnetic field. ' + \
                       'This can help finding the PIL')
   parser.add_argument('-CMESpeed',type=float, default=-1.0, help=
                       'CME speed in km/s, recovered from observations')
   parser.add_argument('--CMEGrid',action='store_true', help=
                       'Output parameters of the refined CME grid')
   parser.add_argument('-Orientation',type=float,default=-1.0, help=
                       'Use specified orientation for GL_Orientation')
   parser.add_argument('-Helicity',type=int,default=0, help=
                       'Use specified helicity')
   parser.add_argument('-LonPosIn',type=float,default=999.0, help=
                       'Longitude for positive spot center (Deg)')
   parser.add_argument('-LatPosIn',type=float,default=999.0, help=
                       'Latitude for positive spot center (Deg)')
   parser.add_argument('-LonNegIn',type=float,default=999.0, help=
                       'Longitude for negative spot center (Deg)')
   parser.add_argument('-LatNegIn',type=float,default=999.0, help=
                       'Latitude for negative spot center (Deg)')
   parser.add_argument('-GLRadius',type=float, default=-1., help=
                       'Radius of the flux-rope before stretching.')
   parser.add_argument('-Stretch',type=float, default=-1., help=
                       'Stretching parameter of the flux-rope.')
   parser.add_argument('-Distance',type=float, default=-1., help=
                       'Distance parameter of the flux-rope.')
   parser.add_argument('-MinBStrength',type=float, default=2.0, help=
                       'Limit BStrength of flux rope to minimum value.' + \
                       'Weak events may produce unphysically low magnetic ' + \
                       'field strength. Default MinBStrength is 5.0 Gs.')
   parser.add_argument('-MaxBStrength',type=float, default=40.0, help=
                       'Limit BStrength of flux rope to maximum value.' + \
                       'Adjusts flux rope radius to maintain realistic' + \
                       'magnetic field. Default MaxBStrength is 40.0 Gs.')
   parser.add_argument('--UsePNDist',action='store_true', help=
                       'Use the angular distance between positive and negative spot centers to calculate AR size and GL Radius')
   parser.add_argument('--UseARArea',action='store_true',help=
                       'Use Active Region area to calculate AR size and GL Radius')
   parser.add_argument('--DoScaling',action='store_true',help=
                       'Scale Distance, Stretch, Radius by a factor Alpha(a). Distance->a*Distance, Stretch->a*Stretch, GLRadius->a*GLRadius')
   parser.add_argument('-SizeFactor',type=float, default=1.0, help=
                       'Calculated from the angular distance between the  positive and negative spot centers. If specified, then the Angular width of the flux rope on the solar surface is SizeFactor * Distance between the spot centers. Default SizeFactor is 1.0.')
   parser.add_argument('--GLRadiusRange',nargs='+',type=float, 
                       default=[0.2,0.8], help=
                       'GLRadius is limited by GLRadiusRange = 2-elements array. Default is [0.2,2.0].')
   parser.add_argument('--DoHMI',action='store_true', help=
                       'Use HMI map for helicity determination')
   parser.add_argument('--UseBATS',action='store_true', help=
                       'Reading magnetogram in the ModPlotFile format')
   parser.add_argument('--DoNotPlot',action='store_true', help=
                       'To be used on machines with no idl')

   args = parser.parse_args()
   ##################OPTIONAL INPUT PARAMETERS######
   NameFile    = args.NameFile
   CMESpeed    = args.CMESpeed
   GLRadius    = args.GLRadius
   SizeFactor  = args.SizeFactor
   GLRadiusRange_I = args.GLRadiusRange
   UseCMEGrid  = args.CMEGrid
   nLat        = args.nlat
   nLon        = args.nlon
   i           = args.index
   nSmooth     = args.nSmooth
   Orientation = args.Orientation
   Helicity    = args.Helicity
   Stretch     = args.Stretch
   Distance    = args.Distance
   MaxBStrength= args.MaxBStrength
   MinBStrength= args.MinBStrength
   DoHMI       = args.DoHMI # default is False
   LonPosIn   = args.LonPosIn
   LatPosIn   = args.LatPosIn
   LonNegIn   = args.LonNegIn
   LatNegIn   = args.LatNegIn
   UsePNDist   = args.UsePNDist
   UseARArea   = args.UseARArea
   DoScaling   = args.DoScaling
   UseBATS     = args.UseBATS
   DoNotPlot   = args.DoNotPlot

   IdlFile = 'fitsfile.out'
   if UseBATS==False:
      # Check if the file extension is .out
      SplitName = NameFile.split('.')
      if  SplitName[-1]=='out':
         print('\n File name '+NameFile+
               ' has extension .out, is treated as ASCII converted file')
         UseBATS = True

   if not UsePNDist and not UseARArea and GLRadius <=0. :
      print('\n WARNING: User did not specify how to calculate GLRadius.'+
            ' Default is using Active Region area to estimate GLRadius.')
      UseARArea = True
  
   
   # setting ouput grid if remapping is required
   grid_type = 'unspecified' # assumes the grid of the map
   if args.outgrid == 'sinlat':
      grid_type = 'sin(lat)'
   elif args.outgrid == 'uniform':
      grid_type = 'uniform'

   ##################END OF PARSER#####################
   #################SERVER SIDE, PYTHON################
   #################PROCESS MAGNETOGRAM###
   ##READ AND SMOOTH, IF DESIRED########################
   if UseBATS:
      cc =  rmag.read_bats(NameFile)
      nIndex_I     = cc[0]
      nLon        = nIndex_I[0]
      nLat         = nIndex_I[1]
      nVar         = cc[1]
      nParam       = cc[2]
      Param_I      = cc[3]
      Lon0        = Param_I[0] # Longitude of left edge
      Time         = cc[7]
      LonEarth    = Param_I[1]         # CR number
      Lon_I       = cc[4]*Deg2Rad      # in radians
      Lat_I        = cc[5]*Deg2Rad      # in radians
      data         = cc[6]
      if nVar ==1:
         Br_C = data
      else:
         Br_C = data[:,:,0]
      if nSmooth > 2:
         Br_C = rmag.smooth(nLon,  nLat,  nSmooth, Br_C)
         StrHeader = cc[8]
         NameVar   = cc[9]
         if nVar==1:
            data = Br_C
         else:
            data[:,:,0] = Br_C
         IdlFile = rmag.save_bats('Smoothed.out',StrHeader, NameVar,
                                  [nLon,nLat], nVar, nParam, Param_I,
                                  Lon_I*Rad2Deg, Lat_I*Rad2Deg, data, Time)
      else:
         IdlFile = NameFile
   else:
      # fits magnetogram is read, remapped (if required) using
      # remap_magnetogram.py to fitsfile.out
      cc = rmag.remap(NameFile, IdlFile, nLat, nLon, grid_type,
                      i-1, nSmooth,BMax)
      nLon        = cc[0]
      nLat         = cc[1]
      nParam       = cc[2]
      Param_I      = cc[3]
      Lon0        = Param_I[0] # Longitude of left edge
      LonEarth    = Param_I[1] # CR number of central meridian
      Lon_I       = cc[4]      # in radians
      Lat_I        = cc[5]      # in radians
      Br_C         = cc[6]
      Time         = cc[9]
      if DoHMI:
         date         = cc[8]
         hmi_yymm = date.split("-")
         hmi_dd = hmi_yymm[2].split("T")
         hmi_hh = hmi_dd[1].split(":")
         hmi_yyyy     = int(hmi_yymm[0])
         hmi_mm       = int(hmi_yymm[1])
         hmi_dd       = int(hmi_dd[0])
         hmi_hh       = int(hmi_hh[0])
         cwd = os.getcwd()
         time_mag = dt.datetime(hmi_yyyy, hmi_mm, hmi_dd, hmi_hh)
         hmi_file = hmi_map(mag_time=time_mag, hmi_map='hmi.b_synoptic_small',
                         download_dir=cwd)

   FileId=open('CME.in','w')
   if NameFile=='field_2d.out':
      FileId.write("#LOOKUPTABLE \n")
      FileId.write("B0			NameTable \n")
      FileId.write("load			NameCommand \n")
      FileId.write("harmonics_bxyz.out		NameFile \n")
      FileId.write("real4			TypeFile \n")
   FileId.write("\n")
   FileId.close()
   #Info to the idl session is passed via the fitsfile.out file####
   ############END OF PYTHON FIRST SESSION##########
   ###IDL SESSION IN THE SWMF_GLSETUP/BROWSER SESSION IN EEGGL##

   if CMESpeed<= 0.0: 
      print("\n Please Input the Observed CME Speed (km/s). For example: ")
      print("\n python3 GLSETUP.py fitsfile.fits -CMESpeed 600 ")
      exit()
   if (LonPosIn ==999. or LatPosIn ==999. or LonNegIn ==999.
       or LatNegIn ==999.):
      print('Select the CME Source Region (POSITIVE) with the left button')
      print('Then select negative region with the right button')

      FileId=open('runidl1','w')
      FileId.write('.r GLSETUP1\n')
      FileId.write("GLSETUP1,file='"+IdlFile+"' ")
      FileId.close()
      ########SHOW MAGNETOGRAM##########################
      # GLSETUP1.pro is run, it reads the magnetogram(fitsfile.out)
      # reads the cursor x,y indices for neg and pos. AR.
      ls = subprocess.Popen(["idl", "runidl1"],stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,text=True)
      #################PROCESSING STDOUT################
      stdout,stderr=ls.communicate()
      b=stdout[stdout.index('===')+4:len(stdout)]
      a=b.split() # x,y coordinates 
      ###### TAKE TWO COORDINATES FROM TWO CLICKS#######
       # In this case, these values once rounded are grid indexes
      LonPos = float(a[0])
      LatPos = float(a[1])
      LonNeg = float(a[2])
      LatNeg = float(a[3])
   else:
      # The input locations are in degrees
      print ("\n User input  Lon/Lat for Positive and negative spots:")
      print ("{0:4.1f} {1:4.1f} {2:4.1f} {3:4.1f} [deg]".format(
            LonPosIn, LatPosIn,LonNegIn, LatNegIn))
      # Convert coordinates in degrees to grid indexes
      LonPos = GL.calculate_index(LonPosIn*Deg2Rad,Lon_I,nLon)
      LatPos = GL.calculate_index(LatPosIn*Deg2Rad,Lat_I, nLat)
      LonNeg = GL.calculate_index(LonNegIn*Deg2Rad,Lon_I,nLon)
      LatNeg = GL.calculate_index(LatNegIn*Deg2Rad,Lat_I, nLat)
   ##########SHAPE INPUTS FOR THE SECOND SERVER-SIDE SESSION####
   nParam  = 6
   Param_I = np.zeros(nParam)
   Param_I[0] = Lon0
   Param_I[1] = LonEarth
   # Below the x,y positions are equal to location of clicks OR 
   # the assumed locations of the spot centers as input by the user.
   # These are passed to GLSETUPALg.py and weighted centers are calculated
   Param_I[2] = float(LonPos)
   Param_I[3] = float(LatPos)
   Param_I[4] = float(LonNeg)
   Param_I[5] = float(LatNeg)

   ##SECOND SERVER-SIDE SESSION (PYTHON)#######################
   CC=GL.Alg(nLon,nLat,nParam,Param_I,Lon_I,Lat_I,Br_C,
             CMESpeed,GLRadius,SizeFactor,
             GLRadiusRange_I, UseCMEGrid, Orientation,
             Stretch, Distance, Helicity, DoHMI,
             UsePNDist, UseARArea, DoScaling, Time, DoNotPlot,
             MinBStrength, MaxBStrength)
   if DoNotPlot:
      sys.exit(0)
   FileId=open('runidl','w')
   FileId.write('.r GLSETUP2\n')
   FileId.write("GLSETUP2, file='AfterGLSETUP.out',/UseBATS \n")
   FileId.close()
   ###FINAL SESSION: SHOW MAGNETOGRAM AND BIPOLAR STRUCTURE OF AR
   subprocess.call(['idl','runidl'])
   ###IF THE MASTER SCRIPT IS IN PYTHON, AND A CHILD PROCESS IS IN IDL
   #(1) THE TIME OF IDL SESSION SHOULD BE LIMITED (30 seconds or so) 
   #(2) WINDOWS SHOULD BE CLOSED 
   #(3) FINAL EXIT COMMAND MUST BE PRESENT IN THE IDL SCRIPT######
   #print 'GLSETUP Session is closed. Bye!!!'
##############################################
