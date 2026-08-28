/* ///////////////////////////////////////////////////////////////////// */
/*! 
  \file  
  \brief Provide basic functionality for reading input data files.

  Collects a number of functions for opening, reading and interpolating
  initial conditions from user-supplied binary data file(s).
  The geometry and dimensions of the input grid can be different from 
  the actual grid employed by PLUTO, as long as the coordinate geometry
  transformation has been implemented.
  The input grid and data files should employ the same format and 
  conventions employed by PLUTO.
  Specifically:
  
  - Gridfile: coordinates should be written using the standard
              PLUTO 4 grid format.
  - Datafile: variables should be written in single or multiple
              binary data file using eighter single or double precision.
              The file extension must be ".flt" or ".dbl" for the former and
              the latter, respectively.
     
  InputDataOpen() initializes the inputData structure for a given
  input field (e.g. density) iby reading grid size and coordinates,
  geometry, precision, endianity, etc..

  InputDataInterpolate() is used to interpolate the given field from the
  input coordinates to the desired coordinate location using bi- or
  tri-linear interpolation to fill the data array.

  The input data is stored in a buffer by reading ::ID_NZ_MAX planes at
  a time to save computational memory.
  The value of ::ID_NZ_MAX can be changed from your personal \c definitions.h.
  This task is performed in the InputDataReadSlice() function.

  \authors A. Mignone (andrea.mignone@unito.it)\n
  \date    May 11, 2023
*/
/* ///////////////////////////////////////////////////////////////////// */
#include"pluto.h"

#define ID_NVAR_MAX 64
#ifndef ID_NZ_MAX
 #define ID_NZ_MAX   4  /**< Maximum size (in the 3rd dimension) of the
                             input buffer. */
#endif

typedef struct inputData_{
  char fname[256];
  size_t dsize;
  int nx1;
  int nx2; 
  int nx3;
  int geom;
  int swap_endian;
  int klast;        /**< The index of the last read plane */
  int vpos;         /**< Field position in the cell (CENTER, X1FACE, etc...) */
  double *x1;
  double *x2;
  double *x3;
  double ***Vin;    /**< Input buffer array (containing at most ::ID_NZ_MAX
                         planes at a time */
  long int offset;
} inputData;

static inputData id_stack[ID_NVAR_MAX];

/* ********************************************************************* */
int InputDataOpen(char *data_fname, char *grid_fname, char *endianity,
                  long int offset, int vpos)
/*!
 * Initialize access to input data file by assigning values to 
 * grid-related information (geometry, number of points, etc...).
 * This function should be called just once for input-data initialization.
 *
 * \param [in] data_fname    the name of the input binary data file 
 * \param [in] grid_fname    the name of the corresponding grid file
 * \param [in] endianity     an input string ("little" or "big") giving
 *                           the byte-order of how the input data file
 *                           was originally written.
 *                           If an empty string is supplied, no change is
 *                           made.
 * \param [in] offset        an integer specifying the offset of
 *                           the variable (in units of double or float)
 *                           in the file.
 * \param [in] vpos          the variable position with respect to the cell
 *                           center (CENTER / X1FACE / X2FACE / X3FACE).
 *
 * \return                   The index of the id_stack[] array 
 *********************************************************************** */
{
  char *sub_str, sline[256];
  const char delimiters[] = " \t\r\f\n";
  int    i, ip, success;
  int    indx = 0; /* The 1st available element in the id_stack structure */
  size_t str_length;
  double xl, xr;
  inputData *id;
  fpos_t file_pos;
  FILE *fp;

  printLog ("> Input data:\n\n");

/* --------------------------------------------------------------------- */
/*! 0. Find the 1st available (NULL pointer) element in the stack        */
/* --------------------------------------------------------------------- */

  while (id_stack[indx].Vin != NULL){
    indx++;
    if (indx == ID_NVAR_MAX){
      printLog ("! InputDataOpen(): max number of variables exceeded\n");
      printLog ("!                  indx = %d\n",indx);
      QUIT_PLUTO(1);
    }
  }

  printLog ("  Allocating memory for struct # %d\n",indx);
  id = id_stack + indx;

/* --------------------------------------------------------------------- */
/*! 1. Scan grid data file and try to determine the grid geometry 
       (\c id->geom). Search for tag "GEOMETRY:" and read the word that
       follows.                                                          */
/* --------------------------------------------------------------------- */

  fp = fopen(grid_fname,"r");
  if (fp == NULL){
    printLog ("! InputDataOpen(): grid file %s not found\n",grid_fname);
    QUIT_PLUTO(1);
  }
  success = 0;
  while(!success){
    fgets(sline,512,fp);
    sub_str = strtok(sline, delimiters);
    while (sub_str != NULL){
      if (!strcmp(sub_str,"GEOMETRY:")) {
        sub_str = strtok(NULL, delimiters);     
        success = 1;
        break;
      }
      sub_str = strtok(NULL, delimiters);     
    }  
  }
  
  if      (!strcmp(sub_str,"CARTESIAN"))   id->geom = CARTESIAN;
  else if (!strcmp(sub_str,"CYLINDRICAL")) id->geom = CYLINDRICAL;
  else if (!strcmp(sub_str,"POLAR"))       id->geom = POLAR;
  else if (!strcmp(sub_str,"SPHERICAL"))   id->geom = SPHERICAL;
  else{
    printLog ("! InputDataOpen(): unknown geometry\n");
    QUIT_PLUTO(1);
  }
    
  printLog ("  Input grid file:       %s\n", grid_fname);
  printLog ("  Input grid geometry:   %s\n", sub_str);

/* --------------------------------------------------------------------- */
/*! 2. Move file pointer to the first line of grid.out that does not
       begin with a "\c #".                                              */
/* --------------------------------------------------------------------- */

  success = 0;
  while(!success){
    fgetpos(fp, &file_pos);
    fgets(sline,512,fp);
    if (sline[0] != '#') success = 1;
  }

  fsetpos(fp, &file_pos);
  
/* --------------------------------------------------------------------- */
/*! 3. Read number of points, allocate grid arrays and store input
       grid coordinates into structure members \c id->nx1, \c id->x1,
       etc...
       In the case of a staggered direction [i] refers to the left
       interface and we increment by 1 the number of points.             */
/* --------------------------------------------------------------------- */
  
/* -- x1 direction -- */   

  fscanf (fp,"%d \n",&(id->nx1));
  if (vpos == X1FACE) {   
    id->nx1++;
    id->x1 = ARRAY_1D(id->nx1, double);
    for (i = 0; i < id->nx1-1; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x1[i] = xl;
    }
    id->x1[i] = xr;
  }else {
    id->x1 = ARRAY_1D(id->nx1, double);
    for (i = 0; i < id->nx1; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x1[i] = 0.5*(xl + xr);
    }
  }  
  
/* -- x2 direction -- */   

  fscanf (fp,"%d \n",&(id->nx2));
  if (vpos == X2FACE){
    id->nx2++;
    id->x2 = ARRAY_1D(id->nx2, double);
    for (i = 0; i < id->nx2-1; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x2[i] = xl;
    }
    id->x2[i] = xr;
  }else{
    id->x2 = ARRAY_1D(id->nx2, double);
    for (i = 0; i < id->nx2; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x2[i] = 0.5*(xl + xr);
    }
  }  

/* -- x3 direction -- */   
  
  fscanf (fp,"%d \n",&(id->nx3));
  if (vpos == X3FACE){
    id->nx3++;
    id->x3 = ARRAY_1D(id->nx3, double);
    for (i = 0; i < id->nx3-1; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x3[i] = xl;
    }
    id->x3[i] = xr;
  }else{
    id->x3 = ARRAY_1D(id->nx3, double);
    for (i = 0; i < id->nx3; i++){
      fscanf(fp,"%d  %lf %lf\n", &ip, &xl, &xr);
      id->x3[i] = 0.5*(xl + xr);
    }
  }  

  id->vpos = vpos;  
  fclose(fp);

/* -- reset grid with 1 point -- */

  if (id->nx1 == 1) id->x1[0] = 0.0;
  if (id->nx2 == 1) id->x2[0] = 0.0;
  if (id->nx3 == 1) id->x3[0] = 0.0;

  printLog ("  Input grid extension:  x1 = [%12.3e, %12.3e] (%d points)\n",
             id->x1[0], id->x1[id->nx1-1], id->nx1);
  printLog ("\t\t\t x2 = [%12.3e, %12.3e] (%d points)\n",
             id->x2[0], id->x2[id->nx2-1], id->nx2);
  printLog ("\t\t\t x3 = [%12.3e, %12.3e] (%d points)\n",
             id->x3[0], id->x3[id->nx3-1], id->nx3);
  
/* ---------------------------------------------------------- */
/*! 4. Check if binary data file exists and allocate memory
       buffer \c id->Vin                                      */
/* ---------------------------------------------------------- */

  char   ext[] = "   ";

  strcpy (id->fname, data_fname);
  fp = fopen(id->fname, "rb");
  if (fp == NULL){
    printLog ("! InputDataOpen(): file %s does not exist\n", data_fname);
    QUIT_PLUTO(1);
  }
  fclose(fp);

  id->Vin = ARRAY_3D(ID_NZ_MAX, id->nx2, id->nx1, double);

/* ---------------------------------------------------------- */
/*! 5. Set endianity (\c id->swap_endian)                     */
/* ---------------------------------------------------------- */
 
  if ( (!strcmp(endianity,"big")    &&  IsLittleEndian()) ||
       (!strcmp(endianity,"little") && !IsLittleEndian())) {
    id->swap_endian = YES;
  }else{
    id->swap_endian = NO;
  }
  
  printLog ("  Input data file:       %s (endianity: %s) \n", 
           data_fname, endianity);
  if (id->vpos == CENTER) printLog ("  Input data field pos:  CENTER\n");
  else if (id->vpos == X1FACE) printLog ("  Input data field pos:  X1FACE\n");
  else if (id->vpos == X2FACE) printLog ("  Input data field pos:  X2FACE\n");
  else if (id->vpos == X3FACE) printLog ("  Input data field pos:  X3FACE\n");
  else{
    printLog ("! InputDataOpen(): unknown field position\n");
    QUIT_PLUTO(1);
  }
  
/* ---------------------------------------------------------- */
/*! 6. Set data size (\c id->dsize) by looking at the file
       extension (\c dbl or \c flt).                          */
/* ---------------------------------------------------------- */

  str_length = strlen(data_fname);
  for (i = 0; i < 3; i++) ext[i] = data_fname[str_length-3+i];

  if (!strcmp(ext,"dbl")){
    printLog ("  Precision:             double\n");
    id->dsize = sizeof(double);
  } else if (!strcmp(ext,"flt")) {
    printLog ("  Precision:\t\t single\n");
    id->dsize = sizeof(float);
  } else {
    printLog ("! InputDataRead: unsupported data type '%s'\n",ext);
    QUIT_PLUTO(1);
  }

/* ---------------------------------------------------------- */
/*! 7. Compute offset (\c id->offset in bytes) and
       initialize \c id->klast (last read plane) to -1        */
/* ---------------------------------------------------------- */

  id->offset = (long)offset*id->dsize;
  id->klast  = -1;

  printLog ("  offset = %ld\n",id->offset);
  printLog ("\n");

  return indx;  /* The index of the id_stack[] array */
}


/* ********************************************************************* */
void InputDataClose(int indx)
/*!
 * Free memory and reset structure.
 *********************************************************************** */
{
  printLog ("  Freeing struct #%d\n",indx);
  inputData *id = id_stack + indx;
  FreeArray1D((void *)id->x1);
  FreeArray1D((void *)id->x2);
  FreeArray1D((void *)id->x3);
  FreeArray3D((void *)id->Vin);
  id->Vin = NULL;
}

/* ********************************************************************* */
void InputDataGridSize (int indx, int *size)
/*!
 * Return the size of the input data array, from the grid file. 
 *
 * \param [in] indx   input data array element (handle)
 * \param [in] size   an array of integer containing the array size [nx1,nx2,nx3]
 *
 *********************************************************************** */
{
  inputData *id = id_stack + indx;
  
  if (id->Vin == NULL){
    printLog ("! InputDataGridSize(): NULL stack\n");
    QUIT_PLUTO(1);
  }
  size[0] = id->nx1;
  size[1] = id->nx2;
  size[2] = id->nx3;
}

/* ********************************************************************* */
double InputDataInterpolate (int indx, double x1, double x2, double x3)
/*!
 * Perform bi- or tri-linear interpolation on external
 * dataset to compute vs[] at the given point {x1,x2,x3}.
 *
 * \param [in] indx   input data array element (handle)
 * \param [in] x1     coordinate point at which at interpolates are desired
 * \param [in] x2     coordinate point at which at interpolates are desired
 * \param [in] x3     coordinate point at which at interpolates are desired
 *
 * \return The interpolated value.
 *
 * The function performs the following tasks. 
 *********************************************************************** */
{
  int il = 0, jl = 0, kl = 0;
  int ih, jh, kh;
  int im, jm, km;
  int i, j, k, nv;
  float  uflt;
  double udbl;
  double xx, yy, zz, v;
  double **Vlo, **Vhi;
  inputData *id = id_stack + indx;
  static FILE *fp;

/* --------------------------------------------------------------------- */
/*! - Convert PLUTO coordinates to input grid geometry if necessary.     */
/* --------------------------------------------------------------------- */

#if GEOMETRY == CARTESIAN
  if (id->geom == GEOMETRY) {  

    /* same coordinate system: nothing to do */
     
  }else if (id->geom == CYLINDRICAL) {  
    double R, z, phi;
    R   = sqrt(x1*x1 + x2*x2);
    phi = atan2(x2,x1);
    if (phi < 0.0) phi += 2.0*CONST_PI;
    z   = x3;

    x1 = R; x2 = z; x3 = phi;
  }else if (id->geom == POLAR) {  
    double R, phi, z;
    R   = sqrt(x1*x1 + x2*x2);
    phi = atan2(x2,x1);
    if (phi < 0.0) phi += 2.0*CONST_PI;
    z   = x3;

    x1 = R; x2 = phi; x3 = z;
  }else if (id->geom == SPHERICAL){
    double r, theta, phi;
    r     = DIM_EXPAND(x1*x1, + x2*x2, + x3*x3);
    r     = sqrt(r);
    theta = acos(x3/r);
    phi   = atan2(x2,x1);
    if (phi   < 0.0) phi   += 2.0*CONST_PI;
    if (theta < 0.0) theta += 2.0*CONST_PI;
     
    x1 = r; x2 = theta; x3 = phi;
  }else{
    printLog ("! InputDataInterpolate(): invalid or unsupported coordinate transformation.\n");
    QUIT_PLUTO(1);
  }
#elif GEOMETRY == CYLINDRICAL
  if (id->geom == GEOMETRY) {  

    /* same coordinate system: nothing to do */
     
  }else if (id->geom == SPHERICAL) {  
    double r, theta, phi;
    r     = DIM_EXPAND(x1*x1, + x2*x2, + 0.0);
    r     = sqrt(r);
    theta = acos(x2/r);
    phi   = 0.0;
    if (theta < 0.0) theta += 2.0*CONST_PI;
     
    x1 = r; x2 = theta; x3 = phi;
  }else{
    printLog ("! InputDataInterpolate(): invalid or unsupported coordinate transformation.\n");
    QUIT_PLUTO(1);
  }
#elif GEOMETRY == POLAR
  if (id->geom == GEOMETRY) {  

    /* same coordinate system: nothing to do */
     
  }else if (id->geom == CARTESIAN) {  
    double x, y, z;
    x = x1*cos(x2);
    y = x1*sin(x2);
    z = x3;
     
    x1 = x; x2 = y; x3 = z;
  }else{
    printLog ("! InputDataInterpolate(): invalid or unsupported coordinate transformation.\n");
    QUIT_PLUTO(1);
  }   
#elif GEOMETRY == SPHERICAL
  if (id->geom == GEOMETRY) {  

    /* same coordinate system: nothing to do */
     
  }else if (id->geom == CARTESIAN) {  
    double x, y, z;
    x = x1*sin(x2)*cos(x3);
    y = x1*sin(x2)*sin(x3);
    z = x1*cos(x2);
     
    x1 = x; x2 = y; x3 = z;
  }else{
    printLog ("! InputDataInterpolate(): invalid or unsupported coordinate transformation.\n");
    QUIT_PLUTO(1);
  }   
#endif
/*
print ("o Interpolation required at (%f, %f, %f)\n",x1,x2,x3);
print ("o Input data extension:  [%f, %f], [%f, %f], [%f, %f]\n",
       id->x1[0], id->x1[id->nx1-1],
       id->x2[0], id->x2[id->nx2-1],
       id->x3[0], id->x3[id->nx3-1]);
*/

/* --------------------------------------------------------------------- */
/*! - Make sure point (x1,x2,x3) does not fall outside input grid range. 
      Limit to input grid edge otherwise.                                */
/* --------------------------------------------------------------------- */
  
  #if DIMENSIONS >= 1
  if (x1 < id->x1[0]) {
    WARNING(
      printLog ("! InputDataInterpolate(): x1-coord outside id range: [x1 = %f < %f]\n",
                 x1, id->x1[0]);
    )
    x1 = id->x1[0];
  } else if (x1 > id->x1[id->nx1-1]) {
    WARNING(
      printLog ("! InputDataInterpolate(): x1-coord outside id range: [x1 = %f > %f]\n",
                 x1, id->x1[id->nx1-1]);
    )
    x1 = id->x1[id->nx1-1];
  }
  #endif
  #if DIMENSIONS >= 2
  if      (x2 < id->x2[0]){
    WARNING(
      printLog ("! InputDataInterpolate(): x2-coord outside id range: [x2 = %f < %f]\n",
               x2, id->x2[0]);
    )
    x2 = id->x2[0];
  }else if (x2 > id->x2[id->nx2-1]) {
    WARNING(
      printLog ("! InputDataInterpolate(): x2-coord outside id range: [x2 = %f > %f]\n",
               x2, id->x2[id->nx2-1]);
    )
    x2 = id->x2[id->nx2-1];
  }
  #endif
  #if DIMENSIONS == 3
  if      (x3 < id->x3[0]) {
    WARNING(
      printLog ("! InputDataInterpolate(): x3-coord outside id range: [x3 = %f < %f]\n",
                 x3, id->x3[0]);
    )
    x3 = id->x3[0];
  }else if (x3 > id->x3[id->nx3-1]) {
    WARNING(
      printLog ("! InputDataInterpolate(): x3-coord outside id range: [x3 = %f > %f]\n",
                 x3, id->x3[id->nx3-1]);
    )
    x3 = id->x3[id->nx3-1];
  }
  #endif

/* --------------------------------------------------------------------- */
/*! - Use table lookup by binary search to  find the indices 
      il, jl and kl such that grid points of PLUTO fall between 
      [il, il+1], [jl, jl+1], [kl, kl+1].                                */
/* --------------------------------------------------------------------- */

  il = 0;
  ih = id->nx1 - 1;
  while (il != (ih-1)){
    im = (il+ih)/2;
    if   (x1 <= id->x1[im]) ih = im;   
    else                    il = im;
  }
  
  if (id->nx2 > 1){
    jl = 0;
    jh = id->nx2 - 1;
    while (jl != (jh-1)){
      jm = (jl+jh)/2;
      if (x2 <= id->x2[jm]) jh = jm;   
      else                  jl = jm;
    }
  }

  if (id->nx3 > 1){
    kl = 0;
    kh = id->nx3 - 1;
    while (kl != (kh - 1)){
      km = (kl+kh)/2;
      if (x3 <= id->x3[km]) kh = km;   
      else                  kl = km;
    }
  }

/* --------------------------------------------------------------------- */
/*! - Define normalized coordinates between [0,1]:
      - x[il+1] < x1[i] < x[il+1] ==> 0 < xx < 1
      - y[jl+1] < x2[j] < y[jl+1] ==> 0 < yy < 1
      - z[kl+1] < x3[k] < z[kl+1] ==> 0 < zz < 1                            */
/* --------------------------------------------------------------------- */

  xx = yy = zz = 0.0; /* initialize normalized coordinates */

  if (id->nx1 > 1) xx = (x1 - id->x1[il])/(id->x1[il+1] - id->x1[il]);  
  if (id->nx2 > 1) yy = (x2 - id->x2[jl])/(id->x2[jl+1] - id->x2[jl]);  
  if (id->nx3 > 1) zz = (x3 - id->x3[kl])/(id->x3[kl+1] - id->x3[kl]);

/* --------------------------------------------------------------------- */
/*! - Read data from disk (1 or 2 planes)                                */
/* --------------------------------------------------------------------- */

/*
if (id->vpos == X1FACE){
print ("@@ Interpolation required at %f %f %f\n",x1,x2,x3);
print ("@@ Closest input coord.      %f %f %f\n",
        id->x1[il], id->x2[jl], id->x3[kl]);
print ("@@ Coord indices:            %d %d %d\n",il,jl,kl);
print ("@@ id->klast                 %d\n",id->klast);

static long int ncall=0;

  if ( (kl >= id->klast + ID_NZ_MAX - 1) || (id->klast == -1) ){
print ("@@ kl = %d, klast = %d; ncall = %d\n",kl,id->klast,ncall);
    InputDataReadSlice(indx, kl);
ncall = 0;
  }
ncall++;
}
*/

  if ( (kl >= id->klast + ID_NZ_MAX - 1) || (kl < id->klast) || (id->klast == -1) ){
    InputDataReadSlice(indx, kl);
  }

/* --------------------------------------------------------------------- */
/*! - Perform bi- or tri-linear interpolation.                           */
/* --------------------------------------------------------------------- */

  Vlo = id->Vin[kl - id->klast];
  Vhi = id->Vin[kl - id->klast+1];

  v =   Vlo[jl][il]*(1.0 - xx)*(1.0 - yy)*(1.0 - zz)  /* [0] is kl */
      + Vlo[jl][il+1]*xx*(1.0 - yy)*(1.0 - zz);
  if (id->nx2 > 1){
    v +=   Vlo[jl+1][il]*(1.0 - xx)*yy*(1.0 - zz)
         + Vlo[jl+1][il+1]*xx*yy*(1.0 - zz);
  }
  if (id->nx3 > 1){
    v +=   Vhi[jl][il]*(1.0 - xx)*(1.0 - yy)*zz
         + Vhi[jl][il+1]*xx*(1.0 - yy)*zz;
    if (id->nx2 > 1){
      v +=   Vhi[jl+1][il]*(1.0 - xx)*yy*zz
           + Vhi[jl+1][il+1]*xx*yy*zz;
    }
  }

  return v;
}

/* ********************************************************************* */
void InputDataReadSlice(int indx, int kslice)
/*! 
 * Read ::ID_NZ_MAX slices from disk starting at the kslice vertical
 * index.
 *
 * \param [in] indx    the structure index (file handle)
 * \param [in] kslice  the starting slice
 *
 *********************************************************************** */
{
  int i,j,k, kmax;
  long int offset;
  double udbl;
  float  uflt;
  inputData *id = id_stack + indx;
  FILE *fp;

/* ----------------------------------------------------
   1. Compute offset.
      Here id->offset (in bytes) is the location of the
      variable in the file, while the second number is
      the vertical slice we want to read.
      Seek from beginning of the file.
   ---------------------------------------------------- */

  offset = id->offset + kslice*id->dsize*id->nx1*id->nx2;

  fp = fopen(id->fname,"rb");
  fseek(fp, offset, SEEK_SET);

/* -----------------------------------------------------
   2. Read binary data at specified position.
   ----------------------------------------------------- */
   
  kmax = MIN(id->nx3-kslice,ID_NZ_MAX);
  if (id->dsize == sizeof(double)){
    for (k = 0; k < kmax   ; k++){   /* Read at most kmax planes */  
    for (j = 0; j < id->nx2; j++){
    for (i = 0; i < id->nx1; i++){      
      if (fread (&udbl, id->dsize, 1, fp) != 1){  
        printLog ("! InputDataReadSlice(): error reading data (indx = %d)\n",indx);
        break;
      }
      if (id->swap_endian) SWAP_VAR(udbl);
      id->Vin[k][j][i] = udbl;
    }}}

   }else{

    for (k = 0; k < kmax   ; k++){   /* Read at most kmax planes */  
    for (j = 0; j < id->nx2; j++){
    for (i = 0; i < id->nx1; i++){
      if (fread (&uflt, id->dsize, 1, fp) != 1){
        printLog ("! InputDataReadSlice(): error reading data (indx = %d)\n",indx);
        break;
      }
      if (id->swap_endian) SWAP_VAR(uflt);
      id->Vin[k][j][i] = (double)uflt;
    }}}
  }

/* -- Update last successfully read slice -- */

  id->klast = kslice;
  fclose(fp);
/*
print ("@@@ Offset = %d\n",offset);
for (k = 0; k < kmax   ; k++){
for (j = 0; j < id->nx2; j++){
for (i = 0; i < id->nx1; i++){
  printLog ("@@@ Input value (%d %d %d) = %f\n",i,j,k,id->Vin[k][j][i]);
}}}
*/
}


