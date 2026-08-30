/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Write data in VTK format (single proc mode)

  This function write a 3D array with given size in serial mode.
  It can be called separately from each processor, by supplying
  a valid filename.
  Useful for debugging purposes since it can be used to write 
  the entrire mesh, including ghost zone values.
  No grid information is used, just the cell index.  

  \b Reference

  http://www.vtk.org/VTK/img/file-formats.pdf

  \author A. Mignone (andrea.mignone@unito.it)
  \date   Aug 20, 2020
*/
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

/* ********************************************************************* */
void WriteVTKProcFile (double ***U, int nx1, int nx2, int nx3, char *filename)
/*!
 * Write VTK file in serial mode only.
 * 
 * \param [in]  U           an array in double precision
 * \param [in] nx1...nx3    the array size
 * \param [in] filename     a string containing the filename
 *
 *********************************************************************** */
{
  int i,j,k;
  char     header[1024];
  float    x1, x2, x3, Uflt;
  float  *xnode, *ynode, *znode;
  FILE *fvtk;

/* --------------------------------------------------------
   0. Allocate memory
   -------------------------------------------------------- */

  xnode = ARRAY_1D(nx1 + INCLUDE_IDIR, float);
  ynode = ARRAY_1D(nx2 + INCLUDE_JDIR, float);
  znode = ARRAY_1D(nx3 + INCLUDE_KDIR, float);
  
/* --------------------------------------------------------
   1. Open file for writing
   -------------------------------------------------------- */

  fvtk = fopen(filename, "wb");
  
/* --------------------------------------------------------
   2. Write header section (VTK format is Rectilinear grid)
   -------------------------------------------------------- */

  sprintf(header,"# vtk DataFile Version 2.0\n");
  sprintf(header+strlen(header),"PLUTO %s VTK Data\n",PLUTO_VERSION);
  sprintf(header+strlen(header),"BINARY\n");
  sprintf(header+strlen(header),"DATASET %s\n","RECTILINEAR_GRID");

  fprintf (fvtk,"%s", header);

  sprintf(header,"DIMENSIONS %d %d %d\n",
                  nx1 + INCLUDE_IDIR, nx2 + INCLUDE_JDIR, nx3 + INCLUDE_KDIR);
  fprintf (fvtk,"%s", header);
  
  for (i = 0; i < nx1 + INCLUDE_IDIR; i++){
    x1 = i;
    if (IsLittleEndian()) SWAP_VAR(x1);
    xnode[i] = x1;
  }
  for (j = 0; j < nx2 + INCLUDE_JDIR; j++){
    x2 = j;
    if (IsLittleEndian()) SWAP_VAR(x2);
    ynode[j] = x2;
  }
  for (k = 0; k < nx3 + INCLUDE_KDIR; k++){
    x3 = k;
    if (IsLittleEndian()) SWAP_VAR(x3);
      #if DIMENSIONS == 2
      znode[k] = 0.0;
      #else
      znode[k] = x3;
     #endif
  }

/* -- Write rectilinear grid information -- */

  sprintf(header,"X_COORDINATES %d float\n", nx1 + INCLUDE_IDIR);
  fprintf (fvtk,"%s", header);
  fwrite(xnode, sizeof(float), nx1 + INCLUDE_IDIR, fvtk);

  sprintf(header,"\nY_COORDINATES %d float\n", nx2 + INCLUDE_JDIR);
  fprintf (fvtk,"%s", header);
  fwrite(ynode, sizeof(float), nx2 + INCLUDE_JDIR, fvtk);

  sprintf(header,"\nZ_COORDINATES %d float\n", nx3 + INCLUDE_KDIR);
  fprintf (fvtk,"%s", header);
  fwrite(znode, sizeof(float), nx3 + INCLUDE_KDIR, fvtk);

/* -- Dataset attributes -- */

  sprintf (header,"\nCELL_DATA %d\n", nx1*nx2*nx3);
  fprintf (fvtk,"%s",header);
  
/* --------------------------------------------------------
   3. Write data
   -------------------------------------------------------- */

  sprintf (header,"\nSCALARS field float\n");
  sprintf (header+strlen(header),"LOOKUP_TABLE default\n");

  fprintf (fvtk, "%s",header);

  for (k = 0; k < nx3; k++){
  for (j = 0; j < nx2; j++){
  for (i = 0; i < nx1; i++){
    Uflt = (float)(U[k][j][i]); 
    if (IsLittleEndian()) SWAP_VAR(Uflt);
    fwrite (&Uflt, sizeof(float), 1, fvtk);
  }}}
  
/* --------------------------------------------------------
   4. Close file
   -------------------------------------------------------- */

  fclose(fvtk);
  
/* --------------------------------------------------------
   5.Free memory
   -------------------------------------------------------- */
  
  FreeArray1D((void *)xnode);
  FreeArray1D((void *)ynode);
  FreeArray1D((void *)znode);
}
