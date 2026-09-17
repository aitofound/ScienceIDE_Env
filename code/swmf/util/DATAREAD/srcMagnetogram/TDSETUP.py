#!/usr/bin/env python
# make FRM in SWMF/util/EMPIRICAL/srcEE/

import subprocess
import os
import fnmatch
import remap_magnetogram as rmag
import numpy as np
import argparse
import TDSETUPAlg as TD
import GLSETUPAlg as GL


BMax = 1900.0
cPi = np.pi
Rad2Deg = 180/cPi
Deg2Rad = 1/Rad2Deg
IsPositionInput = 0

if __name__ == '__main__':
   parser = argparse.ArgumentParser(
      formatter_class=argparse.RawTextHelpFormatter)
   parser.add_argument('NameFile', help='Input FITS file name including path')
   parser.add_argument('-nSmooth',type=int, default=1, help=
                       'If nSmooth is ODD integer larger than 1, apply boxcar smoothing on the magnetic field')
   parser.add_argument('--CMEGrid',action='store_true', help=
                       'Output parameters of the refined CME grid')
   parser.add_argument('-LonPosIn',type=float,default=999.0, help=
                       'Longitude for positive spot center (Deg)')
   parser.add_argument('-LatPosIn',type=float,default=999.0, help=
                       'Latitude for positive spot center (Deg)')
   parser.add_argument('-LonNegIn',type=float,default=999.0, help=
                       'Longitude for negative spot center (Deg)')
   parser.add_argument('-LatNegIn',type=float,default=999.0, help=
                       'Latitude for negative spot center (Deg)')
   parser.add_argument('-LonFPPosIn',type=float,default=999.0, help=
                       'Longitude for positive foot point of the FR (Deg)')
   parser.add_argument('-LatFPPosIn',type=float,default=999.0, help=
                       'Latitude for positive foot point of the FR (Deg)')
   parser.add_argument('-LonFPNegIn',type=float,default=999.0, help=
                       'Longitude for negative foot point of the FR (Deg)')
   parser.add_argument('-LatFPNegIn',type=float,default=999.0, help=
                       'Latitude for negative foot point of the FR (Deg)')
   parser.add_argument('-a2r0Ratio',type=float,default=0.350, help=
                       'Ratio of minor radius to major one. Can be  an input')
   parser.add_argument('-ApexIn',type=float,default=-1., help=
                       'Ratio of minor radius to major one. Can be  an input')
   parser.add_argument('-BStrapMax',type=float,default=8.,help=
                       'Maximum strapping field, white region on x=0 plane')
   args = parser.parse_args()
   ##################OPTIONAL INPUT PARAMETERS######
   NameFile    = args.NameFile
   UseCMEGrid  = args.CMEGrid
   nSmooth     = args.nSmooth
   LonPosIn   = args.LonPosIn
   LatPosIn   = args.LatPosIn
   LonNegIn   = args.LonNegIn
   LatNegIn   = args.LatNegIn
   LonFPPosIn   = args.LonFPPosIn
   LatFPPosIn   = args.LatFPPosIn
   LonFPNegIn   = args.LonFPNegIn
   LatFPNegIn   = args.LatFPNegIn
   a2r0Ratio   = args.a2r0Ratio
   ApexIn      = args.ApexIn
   BStrapMax   = args.BStrapMax
   nLon = -1
   nLat = -1
   IdlFile = 'fitsfile.out'
   UseBATS = False
   # Check if the file extension is .out
   SplitName = NameFile.split('.')
   if  SplitName[-1]=='out':
      print('\n File name '+NameFile+
            ' has extension .out, is treated as ASCII converted file')
      UseBATS = True
   
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
      cc = rmag.remap(NameFile, IdlFile, nLat, nLon, 'unspecified',
                      0, nSmooth,BMax)
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

   #Info to the idl session is passed via the fitsfile.out file####
   ############END OF PYTHON FIRST SESSION##########
   ###IDL SESSION IN THE SWMF_GLSETUP/BROWSER SESSION IN EEGGL##

   if (LonPosIn ==999. or LatPosIn ==999. or LonNegIn ==999.
       or LatNegIn ==999.):
      print('Select the CME Source Region (POSITIVE) with the left button')
      print('Then select negative region with the right button')

      FileId=open('runidl1','w')
      FileId.write(';\n')
      FileId.write('.r GLSETUP1 \n')
      FileId.write(
         "      GLSETUP1,file='"+IdlFile+"' ")
      FileId.close()
      ########SHOW MAGNETOGRAM##########################
      # GLSETUP1.pro is run, it reads the magnetogram IdlFile
      # reads the cursor x,y indices for neg and pos. AR.
      ls = subprocess.Popen(["idl", "runidl1"],stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,text=True)
      #################PROCESSING STDOUT################
      stdout,stderr=ls.communicate()
      b=stdout[stdout.index('===')+4:len(stdout)]
      a=b.split() # x,y coordinates 
      ###### TAKE TWO COORDINATES FROM TWO CLICKS#######
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
   CC=TD.Alg(nLon,nLat,nParam,Param_I,Lon_I,Lat_I,Br_C,UseCMEGrid,Time,
             LonFPPosIn,LatFPPosIn,LonFPNegIn,LatFPNegIn,a2r0Ratio,
             ApexIn,BStrapMax)
   ###IF THE MASTER SCRIPT IS IN PYTHON, AND A CHILD PROCESS IS IN IDL
   #(1) THE TIME OF IDL SESSION SHOULD BE LIMITED (30 seconds or so) 
   #(2) WINDOWS SHOULD BE CLOSED 
   #(3) FINAL EXIT COMMAND MUST BE PRESENT IN THE IDL SCRIPT######
   print('TDSETUP Session is closed. Bye!!!')
   exit()
##############################################
