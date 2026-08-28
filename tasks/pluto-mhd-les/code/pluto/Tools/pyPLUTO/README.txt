NAME : pyPLUTO 

TASK : Quick Tool for Visualization of PLUTO 4.4 data [Mignone 2007]

AUTHOR : Bhargav Vaidya [Indian Institute of Technology Indore]

	  
DESCRIPTION:

The code is completely written using the Python Language. 
Further the GUI is developed with the Tkinter Interface.

Features of this code: 

1. Completely based on Python and easy to work without need of any licenses
like in IDL. 
2. The GUI environment provides a tool for quick-check of data during the
simulations runs. 
3. The code is user friendly and allows the user to even do further plotting
of contours, velocity vectors
on the surface plot.
4. Also the code can read the saved user defined variables. 


CHANGES from 4-1.0 to 4-2.0:

1. The files have now been modified to support Python version >3.6.
The version for Python 2.7.x has now become obsolete will not be supported hence forth.

2. The datareader now treats each of the reader and associated functionality as a different class, this results in slight 
change of the syntax with regard to import of pload module. 
Example : To read say data.0030.dbl, we have to do. 

import pyPLUTO.pload as pp
D = pp.pload(30)

instead of in the older version

import pyPLUTO as pp
D = pp.pload(30) 

3. The Reader also has support to read particle files that are generated using the 
newly developed Hybrid Eulerian Lagrangian Framework.

MANUAL :

Further details of the Installation and Getting Started can be found in 
the html documentation - PLUTO/Doc/pyPLUTO.html 
