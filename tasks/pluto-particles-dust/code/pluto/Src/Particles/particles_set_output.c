/* ///////////////////////////////////////////////////////////////////// */
/*!
 \file
 \brief Set particles output data attributes.
 
 \authors A. Mignone (andrea.mignone@unito.it)\n
 
 \date    Aug 27, 2020
 */
/* ///////////////////////////////////////////////////////////////////// */
#include "pluto.h"

static Output *all_outputs;

/* ********************************************************************* */
void Particles_SetOutput (Data *d, Runtime *runtime)
/*!
 *  Set default attributes (variable names, pointers to data structures, 
 *  filename extensions, etc...) of the particle output structures.
 *
 * \param [in] d        pointer to Data structure
 * \param [in] runtime  pointer to Runtime structure
 *
 *********************************************************************** */
{
  int nv, i, k;
  char vname[64];
  Output *output;

  all_outputs = runtime->output;

/* --------------------------------------------------------
   1. Loop on output types 
   -------------------------------------------------------- */

  for (k = 0; k < MAX_OUTPUT_TYPES; k++){

    output = runtime->output + k;

    for (i = 0; i < MAX_OUTPUT_VARS; i++) output->field_dim[i] = 1;

  /* -- 1a. Exclude fluid output types -- */

    if (! (output->type == PARTICLES_DBL_OUTPUT ||
           output->type == PARTICLES_FLT_OUTPUT ||
           output->type == PARTICLES_VTK_OUTPUT ||
           output->type == PARTICLES_TAB_OUTPUT ||
           output->type == PARTICLES_HDF5_OUTPUT)) continue;

  /* ------------------------------------------------------
     1b. Allocate memory for field names.
         Note that, for LP particles with spectra we must
         use a large number in order to fit spectral
         information.
     ------------------------------------------------------ */

    #if (PARTICLES == PARTICLES_LP) && (PARTICLES_LP_SPECTRA == YES)
    output->var_name = ARRAY_2D(1024, 32, char);
    #else
    output->var_name = ARRAY_2D(16, 32, char);
    #endif

    strcpy(output->dir, runtime->output_dir); /* output directory is the same     */
                                              /* for all outputs (easy to change) */
    output->nfile    = -1;

  /* -- 1c. Set particles filename extensions -- */

    if (output->type == PARTICLES_DBL_OUTPUT) strcpy (output->ext,"dbl");
    if (output->type == PARTICLES_FLT_OUTPUT) strcpy (output->ext,"flt");
    if (output->type == PARTICLES_VTK_OUTPUT) strcpy (output->ext,"vtk");
    if (output->type == PARTICLES_TAB_OUTPUT) strcpy (output->ext,"tab");

  /* ------------------------------------------------------
     1d. Set default field names (all of them).
         Important: the order must be preserved when
         mapping strucure members to array!
     ------------------------------------------------------ */

    i = 0;
    #if PARTICLES == PARTICLES_CR
    strcpy(output->var_name[i++], "id");
    strcpy(output->var_name[i++], "x1");  
    strcpy(output->var_name[i++], "x2");  
    strcpy(output->var_name[i++], "x3");
    strcpy(output->var_name[i++], "vx1");  
    strcpy(output->var_name[i++], "vx2");  
    strcpy(output->var_name[i++], "vx3");
    strcpy(output->var_name[i++], "mass");
    strcpy(output->var_name[i++], "tinj");
    strcpy(output->var_name[i++], "color");
    strcpy(output->var_name[i++], "energy");
    #endif

    #if PARTICLES == PARTICLES_DUST
    strcpy(output->var_name[i++], "id");
    strcpy(output->var_name[i++], "x1");  
    strcpy(output->var_name[i++], "x2");  
    strcpy(output->var_name[i++], "x3");
    strcpy(output->var_name[i++], "vx1");  
    strcpy(output->var_name[i++], "vx2");  
    strcpy(output->var_name[i++], "vx3");
    strcpy(output->var_name[i++], "mass");
    strcpy(output->var_name[i++], "tau_s");
    strcpy(output->var_name[i++], "tinj");
    strcpy(output->var_name[i++], "color");
    strcpy(output->var_name[i++], "energy");
    #endif

    #if PARTICLES == PARTICLES_LP
    strcpy(output->var_name[i++], "id");
    strcpy(output->var_name[i++], "x1");  
    strcpy(output->var_name[i++], "x2");  
    strcpy(output->var_name[i++], "x3");
    strcpy(output->var_name[i++], "vx1");  
    strcpy(output->var_name[i++], "vx2");  
    strcpy(output->var_name[i++], "vx3");
    strcpy(output->var_name[i++], "tinj");
    strcpy(output->var_name[i],   "color");
    output->field_dim[i++] = PARTICLES_LP_NCOLORS;
    #if PARTICLES_LP_SPECTRA == YES
    strcpy(output->var_name[i++], "density");
    strcpy(output->var_name[i++], "pressure");
    strcpy(output->var_name[i++], "bx1");  
    strcpy(output->var_name[i++], "bx2");  
    strcpy(output->var_name[i++], "bx3");  
    strcpy(output->var_name[i++], "nmicro");
    strcpy(output->var_name[i++], "cmp_ratio");
    strcpy(output->var_name[i++], "shkflag");
    strcpy(output->var_name[i++], "shk_gradp");
  
    strcpy(output->var_name[i++], "cr");
    
    strcpy(output->var_name[i], "vL");
    output->field_dim[i++] = NFLX;

    strcpy(output->var_name[i], "vR");
    output->field_dim[i++] = NFLX;

    strcpy(output->var_name[i], "eng");
    output->field_dim[i++] = PARTICLES_LP_NEBINS + 1;

    strcpy(output->var_name[i], "chi");
    output->field_dim[i++] = PARTICLES_LP_NEBINS;

/*
    for(nv = 0; nv < NFLX; nv++) sprintf (output->var_name[i++],"vL%02d",nv);
    for(nv = 0; nv < NFLX; nv++) sprintf (output->var_name[i++],"vR%02d",nv);

    for(nv = 0; nv < PARTICLES_LP_NEBINS; nv++) {
      sprintf (output->var_name[i++],"specE%04d",nv);
    }
    for(nv = 0; nv < PARTICLES_LP_NEBINS; nv++) {
      sprintf (output->var_name[i++],"pdens%04d",nv);
    }
*/
    #endif
    #endif  

    output->nvar = i;
    
  /* -- Initialize dump_var to YES for all fields -- */  
    for (nv = output->nvar; nv--; ) output->dump_var[nv] = YES;
  }

/* --------------------------------------------------------
   2. Unset fields [default]
   -------------------------------------------------------- */

  #if (PARTICLES == PARTICLES_CR) || (PARTICLES == PARTICLES_DUST)
  SetOutputVar ("mass",   PARTICLES_FLT_OUTPUT, NO); 
  SetOutputVar ("energy", PARTICLES_FLT_OUTPUT, NO);
  #endif

  #if (PARTICLES == PARTICLES_LP) && (PARTICLES_LP_SPECTRA == YES)
  SetOutputVar("nmicro",    PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("cmp_ratio", PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("shkflag",   PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("shk_gradp", PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("cmp_ratio", PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("cr",        PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("vL",        PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("vR",        PARTICLES_FLT_OUTPUT, NO);

  SetOutputVar("pressure",  PARTICLES_DBL_OUTPUT, NO);
  SetOutputVar("bx1",       PARTICLES_DBL_OUTPUT, NO);
  SetOutputVar("bx2",       PARTICLES_DBL_OUTPUT, NO);
  SetOutputVar("bx3",       PARTICLES_DBL_OUTPUT, NO);

  SetOutputVar("pressure",  PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("bx1",       PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("bx2",       PARTICLES_FLT_OUTPUT, NO);
  SetOutputVar("bx3",       PARTICLES_FLT_OUTPUT, NO);
  #endif

  SetOutputVar ("energy", PARTICLES_DBL_OUTPUT, NO); 


//for (k = 0; k < MAX_OUTPUT_TYPES; k++){ 
//  output = runtime->output + k;
//  if (output->type == PARTICLES_FLT_OUTPUT) for (nv = 0; nv < output->nvar; nv++){
//    printLog (".flt, field  #%d, dump = %c\n",nv, output->dump_var[nv] == YES ? 'Y':'N');
//  }    
//}

}
