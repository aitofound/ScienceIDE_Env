
The general idea of this library is to make a variety of ionospheric electrodynamics codes available to the user.  There are three types of methods that will be available:

- Empirical models, including a variety of electric potential and auroral precipitation models.
- Files, including custom binary, netCDF, and HDF5 formats.
- Coupling to codes, including to the Space Weather Modeling Framework.


The library works in the following order:
- Initialize: set what type of model is desired.  Provide filenames or empirical models to use. Provide the number of latitudes and local times to use.
- Set Indices: for empirical models, the library needs the indices to use.
- Set Time: tell the library the current time.
- Set grid: tell the library the magnetic latitudes and magnetic local times to get results on.
- Get result: get the specific result (potential, energy flux, etc.) on the specific grid at the specific time.


On the back side, the (AMIE) file system does the following:
 - Read in a grid - at this time it has to be a regular grid, such that lats and mlts are described by a 1d array each. The ordering should be from the pole to the equator. 
 - Read in a list of variables and link those variables to types of variables the library wants to offer.
 - Read in a list of times
 - The IE library then sets a time to read, at which point, the file system:
   - checks to see if it has the particular time in memory. If it doesn't, it:
     - searches the list of times
     - reads in the data from the file filling a storage array and a time array
   - Check the time in memory to see which time index to use for the array
- The IE library calls a function to set the interpolation indices for the file
  - This function uses the findpoint function which loops through all of the locations and checks to see if the point is within all of the cells.  It is not efficient and could be written much better.
- A series of get functions will get the potential, eflux or average energy
  - Internally, these functions all call a get value function with an index saying which variable to get.
  - the get value function uses the interpolation indices to get the values at the right location.

   
