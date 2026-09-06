#!/usr/bin/env python

#This program plots 1D GITM data. It prompts the user to select the type of
#file to plot as well as the variable to plot from within that filetype.

#The final figure will have 3 subplots: A contour (variable vs. alt & time)
#and also the variable plotted at two user specified altitudes.

from glob import glob
from datetime import datetime
from datetime import timedelta
from struct import unpack
import numpy as np
import matplotlib.pyplot as pp
import matplotlib.dates as dates
from matplotlib.gridspec import GridSpec
import matplotlib.ticker as mticker
import matplotlib.dates as mdates
from mpl_toolkits.axes_grid1 import ImageGrid
from gitm_routines import *
import re
import sys


def closestidx(lst, K):
    '''Quick function to find the index of the value in an array (lst) that is closest
    to another number (K))'''
    lst = np.asarray(lst)
    idx = (np.abs(lst - K)).argmin()
    return idx

#1st find all files that start with 1D.
files = glob("1D*")
filetypes = list(set([f[0:5] for f in files])) #make list of unique filetypes
filetypes.sort() #and sort

#print the unique filetypes
for i in range(len(filetypes)):
    print("{}: {}".format(i,filetypes[i]))

#Ask user to select filetype to plot
itype = int(input("Which filetype to plot:"))

#Based on the type selected, get a list of all files of this type
filestoplot = [thisfile for thisfile in files if thisfile[0:5] == filetypes[itype]]
filestoplot.sort() #sort so they are in order based on time
nfiles = len(filestoplot) #determine number of files to plot

#Get basic information about data. Even though all the files are passed as an
#argument, only the first file is actually read by read_gitm_header().
header = read_gitm_header(filestoplot)

#print the variables for which there is data
for i in range(header['nVars']):
    print("{}: {}".format(i,header['vars'][i]))

#Have user select which variable to plot
pvar = int(input("Which variable to plot: "))
#Only 4 variables will be saved during file read: longitude, latitude, altitude
#and the user selected variable
vars = [0,1,2,pvar]
time = [] #initialize time array

#Have user input the altitudes for the 2 line plots
#If nothing is entered, 120km and 250km are set by default
palt1 = input("Altitude of first cut (120): ")
palt1 = int(palt1) if palt1 else 120
palt2 = input("Altitude of first cut (250): ")
palt2 = int(palt2) if palt2 else 250

#Loop through all files and extract relevent data
for ifile in range(len(filestoplot)):
    #read_gitm_one_file will return only the data specified in the vars list
    #and the time
    data = read_gitm_one_file(filestoplot[ifile],vars)
    time.append(data["time"])
    if ifile == 0:
        #initialize the main data array and save altitude information
        #during first time through for loop only
        altitude = data[2][0][0]/1000. #Convert to km
        longitude = data[0][0][0][0]*180/np.pi
        latitude = data[1][0][0][0]*180/np.pi

        plotdata = np.zeros((nfiles,data['nAlts'])) #initialize main data array
        ialt1 = closestidx(altitude,palt1) #use the function that is defined at the top
        ialt2 = closestidx(altitude,palt2)

    plotdata[ifile,:] = data[pvar] #populate the main array

#Don't plot the ghost cells (the lowest and highest 2 cells)
plotdata = plotdata[:,2:-2] #plotdata is a 2D array. Keep all elements in 1st dimension
altitude = altitude[2:-2]

#get minimim and maximum values
minvalue = np.min(plotdata)
maxvalue = np.max(plotdata)



#### Plotting stuff #################################

#Generic font size variables
SMALL_SIZE = 8
MEDIUM_SIZE = 10
BIGGER_SIZE = 12
pp.rc('axes', labelsize=MEDIUM_SIZE)    # fontsize of the x and y labels

#Create 2D independent variables for the contour plot
X,Y = np.meshgrid(time,altitude)

#Set up the figure
#3 rows, 1 column, all three rows share the same x axis, set the figure size
#to be 6x8, make the 3rd row 2x as large as the 1st and second row
fig, ax = plt.subplots(3,1,sharex='col',figsize=(6,8),\
    gridspec_kw={'height_ratios': [1, 1,2]})

fig.suptitle(r"{} ({:n}$^o$ Lon, {:n}$^o$ Lat)".format(\
    header['vars'][pvar],longitude,latitude),y=0.92)

#Plot the data!
p1 = ax[1].plot(time,plotdata[:,ialt1]) #line plot at alt1 (lower alt)
p2 = ax[0].plot(time,plotdata[:,ialt2]) #line plot at alt2

#allow the user to specify the levels to be plotted on the contour
#or use the min and max values (default)
cmin = input("Enter min value to contour (enter for auto): ")
cmin = int(cmin) if cmin else (minvalue-(minvalue*.03))
cmax = input("Enter max value to contour (enter for auto): ")
cmax = int(cmax) if cmax else (maxvalue+(maxvalue*.03))
#Specify the contour levels
levels = np.linspace(cmin,cmax,30)

#Make contour with our custom levels
p3 = ax[2].contourf(X,Y,plotdata.transpose(),cmap="jet",levels=levels)

#add a dashed black line on the contour at the altitudes plotted in the line plots
ax[2].plot([time[0],time[-1]],[altitude[ialt1],altitude[ialt1]],'k--')
ax[2].plot([time[0],time[-1]],[altitude[ialt2],altitude[ialt2]],'k--')

#Add axis labels
ax[2].set_ylabel('Altitude (km)')
ax[1].set_ylabel('{} \n({} km)'.format(header['vars'][pvar],altitude[ialt1]))
ax[0].set_ylabel('{} \n({} km)'.format(header['vars'][pvar],altitude[ialt2]))

#Make it so we can actually read the dates on the xaxis
fig.autofmt_xdate()

#Make room for the colorbar
fig.subplots_adjust(right=0.85)
pos = ax[2].get_position() #get position of the contour plot
#use contour plot position to specify colorbar position
cbar_ax = fig.add_axes([pos.x1+.02,pos.y0,.02,pos.height])
cbar = fig.colorbar(p3,cax=cbar_ax) #create the color bar
cbar.set_label('{}'.format(header['vars'][pvar])) #add label to color bar

#Indicate min and max on the colorbar, Colorbars are in data coordinates.
#The xaxis and yaxis both  go from the minimum value of the data to the
#maxvalue of the data. This is a little strange (especially for the xaxis)
cbar.ax.plot([minvalue,maxvalue],[minvalue,minvalue],'w')
cbar.ax.plot([minvalue,maxvalue],[maxvalue,maxvalue],'w')

#Finally, save the figure.
pp.savefig('plot.png')
