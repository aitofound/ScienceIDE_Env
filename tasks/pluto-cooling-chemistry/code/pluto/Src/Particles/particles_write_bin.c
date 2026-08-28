/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Writer for particles binary data in .dbl, .flt and 
        ASCII in .tab (only for serial version) format .
 
 \authors A. Mignone (andrea.mignone@unito.it)\n
          B. Vaidya (bvaidya@unito.it)\n
          D. Mukherjee

 \date    Oct 31, 2020
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

#if PARTICLES != PARTICLES_LP
/* ********************************************************************* */
void Particles_WriteBinary(particleNode *PHeadRef, double dt_particles,
                           Output *output, char *filename)
/*! 
 *  Write particle data in single or double precision binary format.
 *  The binary file structure is:
 *
 *  <Header (ASCII) section>
 *   .
 *   .
 *   .
 *  {field1, field2, ...}_p
 *   .
 *   .
 *   . 
 *
 * All fields are mapped from the particle structure into an array
 * and thus converted to either single or double precision, depending
 * on the function that is being called.
 * Fields can be scalars or array: for each particle nelem indicates
 * the number of elements written for each particle.
 * The field order does not have to match the structure member order but it
 * must be consistent with the sequence declared in Particles_SetOutput()
 * function.
 * Individual fields may be excluded by calling SetOutputVar() 
 * from ChangeOutputVar ().
 *
 *  \param [in]  PheadRef      Pointer to the Head Node of Particle List.
 *  \param [in]  dt_particles  Particle time step
 *  \param [in]  output        Pointer to output structure
 *  \param [in]  filename      File name of particle data: particles.nnnn.flt,
 *                             where nnnn is the data file number.
 ************************************************************************* */
{
  char     fheader[1024];
  size_t   size;
  int      dir, nv, k, nfields;
  int     *dump_var = output->dump_var;
  int      nvar     = output->nvar;
  long int nelem;
  long int i, offset, nparticles_glob, proc_npart[g_nprocs+1];
  void    *arr;
  float   *farr;
  double  *darr, gamma;
  particleNode * CurNode;
  
/* --------------------------------------------------------
   0. Allocate memory for required fields
   -------------------------------------------------------- */

  nfields = 0; /* Count how many fields are written to disk */
  nelem   = 0; /* The number of elements to be written (some fields may
                  be arrays). */
  for (nv = 0; nv < nvar; nv++) {
    if (dump_var[nv]) {
      nfields++;
      nelem += output->field_dim[nv];
    }
  }
  darr = ARRAY_1D(nelem*p_nparticles, double);
  farr = ARRAY_1D(nelem*p_nparticles, float);
  
/* --------------------------------------------------------
   1. Loop over particles and map struct. members to array
   -------------------------------------------------------- */
    
  i  = 0;  /* Array index */

  PARTICLES_LOOP(CurNode, PHeadRef){

  /* ------------------------------------------------------
     1b. Map structure members to array.
         Important: field order should match the order 
         given in Particles_SetOutput().
         Here nv scan *all* fields (nv <= nfields)
     ------------------------------------------------------ */

    nv = 0;
    #if PARTICLES == PARTICLES_CR
    if (dump_var[nv++]) darr[i++] = CurNode->p.id;
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[IDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[JDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[KDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[IDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[JDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[KDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.mass;
    if (dump_var[nv++]) darr[i++] = CurNode->p.tinj;
    if (dump_var[nv++]) darr[i++] = CurNode->p.color;
    #endif

    #if PARTICLES == PARTICLES_DUST
    if (dump_var[nv++]) darr[i++] = CurNode->p.id;
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[IDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[JDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.coord[KDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[IDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[JDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.speed[KDIR];
    if (dump_var[nv++]) darr[i++] = CurNode->p.mass;
    if (dump_var[nv++]) darr[i++] = CurNode->p.tau_s;
    if (dump_var[nv++]) darr[i++] = CurNode->p.tinj;
    if (dump_var[nv++]) darr[i++] = CurNode->p.color;
    #endif

  } /* End PARTICLES_LOOP() */
  
/* --------------------------------------------------------
   2. Compute the total number of particles and gather
      each proc. particle number into the proc_npart[1:]
      array. This serves to compute the file offset. 
   -------------------------------------------------------- */
    
  offset = 0L;
#ifdef PARALLEL
  MPI_Allreduce(&p_nparticles, &nparticles_glob, 1,
                MPI_LONG, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allgather(&p_nparticles, 1, MPI_LONG, &(proc_npart[1]), 1,
                MPI_LONG, MPI_COMM_WORLD);

  /* Compute individual processor offset (in particle units) */
    
  for(i = 0; i < prank; i++) offset += proc_npart[i+1];
    
  MPI_File fhw;
  MPI_File_open(MPI_COMM_WORLD, filename,
                MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fhw);
  MPI_File_close(&fhw);
#else
  nparticles_glob = p_nparticles;
#endif
    
/* --------------------------------------------------------
   3. Write file header section.
   -------------------------------------------------------- */
  
  sprintf(fheader,"# PLUTO %s binary particle data file\n", PLUTO_VERSION);
  sprintf(fheader+strlen(fheader),"# dimensions     %d\n",DIMENSIONS);
  sprintf(fheader+strlen(fheader),"# dt_particles   %12.6e\n",dt_particles);  
  if (IsLittleEndian())
      sprintf(fheader+strlen(fheader),"# endianity      little\n");
  else
      sprintf(fheader+strlen(fheader),"# endianity      big\n");

  sprintf(fheader+strlen(fheader),"# nparticles     %ld\n",nparticles_glob);  
  sprintf(fheader+strlen(fheader),"# idCounter      %ld\n",p_idCounter);
  sprintf(fheader+strlen(fheader),"# particletype   %d\n",PARTICLES);
  if (output->type == PARTICLES_FLT_OUTPUT){  
    sprintf(fheader+strlen(fheader),"# precision      float\n");    
  }else if (output->type == PARTICLES_DBL_OUTPUT){  
    sprintf(fheader+strlen(fheader),"# precision      double\n");    
  }
  
  sprintf(fheader+strlen(fheader),"# time           %12.6e\n",g_time);  
  sprintf(fheader+strlen(fheader),"# stepNumber     %ld\n",g_stepNumber);
  sprintf(fheader+strlen(fheader),"# nfields        %d\n",nfields);
  sprintf(fheader+strlen(fheader),"# field_names    ");
  for (i = 0; i < nvar; i++){
    if (dump_var[i]) {
      sprintf(fheader+strlen(fheader),"%s", output->var_name[i]);
      sprintf(fheader+strlen(fheader),"  ");
    }  
  }
  sprintf(fheader+strlen(fheader),"\n");

  sprintf(fheader+strlen(fheader),"# field_dim      ");
  for (i = 0; i < nvar; i++){
    if (dump_var[i]) {
      sprintf(fheader+strlen(fheader),"%d",output->field_dim[i]);
      sprintf(fheader+strlen(fheader),"  ");
    }  
  }
  sprintf(fheader+strlen(fheader),"\n");

  FileWriteHeader(fheader, filename, -1);
    
/* --------------------------------------------------------
   4. Write data 
   -------------------------------------------------------- */
 
  if (output->type == PARTICLES_DBL_OUTPUT) size = sizeof(double);
  if (output->type == PARTICLES_FLT_OUTPUT) size = sizeof(float);

  offset *= nelem*size; /* Convert offset to bytes */
  if (output->type == PARTICLES_FLT_OUTPUT){
    for (i = 0; i < nelem*p_nparticles; i++) farr[i] = (float)darr[i];
    arr = (void *) farr;
  }else if (output->type == PARTICLES_DBL_OUTPUT){
    arr = (void *) darr;
  }
/*
if (output->type == PARTICLES_DBL_OUTPUT){
  i = 0;
  for (k = 0; k < p_nparticles; k++){
    for (nv = 0; nv < nfields; nv++){
      if (darr[i] != darr[i]){
        printLog ("nan found\n");
        QUIT_PLUTO(1);
      }
      printLog ("np = %d, darr = %f\n",k,darr[i]);
      i++;
    }   
  }
}
*/
  FileWriteArray(arr, offset, nelem*p_nparticles, size, filename);

/* --------------------------------------------------------
   5. Write data 
   -------------------------------------------------------- */

  FreeArray1D(darr);   
  FreeArray1D(farr);   
}
#endif /* PARTICLES != PARTICLES_LP */

/* ********************************************************************* */
void Particles_WriteTab(particleNode* PHeadRef, char filename[128])
/*
 * Write particle coordinates, ids and speeds in Tab ASCII format 
 * only for *serial* version. 
 *
 *  \param [in]  PheadRef    Pointer to the Head Node of Particle List.
 *  \param [in]  filename    File name of particle data: particles.nnnn.tab,
 *                           where nnnn is the data file number.
 *********************************************************************** */
{
#ifdef PARALLEL
  print("! WARNING: Particle Data in Tabulated ASCII format is only written \
            with serial version");
#else
  int i;
  FILE *stream;
  particleNode* CurNode;
  
  stream = fopen(filename, "w");
  fprintf(stream, "# Nparticles: %ld\n", p_nparticles);
  fprintf(stream, "# Step:       %ld\n", g_stepNumber);
  fprintf(stream, "# Time:       %f\n",  g_time);
  
  CurNode = PHeadRef;
  while(CurNode != NULL) {
    fprintf(stream, "%d", CurNode->p.id);
    
    for (i = 0; i < 3; ++i) {
      fprintf(stream, "  %lf", CurNode->p.coord[i]);
    }
    for (i = 0; i < 3; ++i) {
      fprintf(stream, "  %lf", CurNode->p.speed[i]);
    }
    fprintf(stream, "\n");

    CurNode = CurNode->next;
  }
  fclose(stream);
#endif
}
