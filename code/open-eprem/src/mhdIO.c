/* The Earth-Moon-Mars Radiation Environment Module (EMMREM) software is */
/* free software; you can redistribute and/or modify the EMMREM sotware */
/* or any part of the EMMREM software under the terms of the GNU General */
/* Public License (GPL) as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any later */
/* version. Software that uses any portion of the EMMREM software must */
/* also be released under the GNU GPL license (version 2 of the GNU GPL */
/* license or a later version). A copy of this GNU General Public License */
/* may be obtained by writing to the Free Software Foundation, Inc., 59 */
/* Temple Place, Suite 330, Boston MA 02111-1307 USA or by viewing the */
/* license online at http://www.gnu.org/copyleft/gpl.html. */

#include <math.h>
#include <hdf5.h>
#include <hdf5_hl.h>

#ifdef HAVE_HDF4
#include <mfhdf.h>
#endif

#include <unistd.h>
#include "readMHD.h"
#include "mpiInit.h"
#include "global.h"
#include "configuration.h"
#include "error.h"
#include "simCore.h"
#include "flow.h"
#include "observerOutput.h"
#include "timers.h"

#ifndef HAVE_HDF4
#define H4_MAX_VAR_DIMS 32
typedef void *VOIDP;
#endif

/* This function reads MHD mesh dimensions. Switch HDF4 or HDF5 based on
   hdf5_input flag.
*/
void mhdReadMeshDimensions(char *fname, char *dsetname, int dsetnumber, int32_t *DimMax)
{

    if (hdf5_input == 1)
    {                                            // Read HDF5 files
        hid_t file_id, dataset_id, dataspace_id; // Data Handles
        hsize_t dim_sizes[H4_MAX_VAR_DIMS];
        int status;
        file_id = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT); // Open the file
        dataset_id = H5Dopen2(file_id, dsetname, H5P_DEFAULT); // Open the dataset
        dataspace_id = H5Dget_space(dataset_id);
        status = H5Sget_simple_extent_dims(dataspace_id, dim_sizes, NULL);
        DimMax[0] = dim_sizes[0];

        /* Close the file. */
        status = H5Dclose(dataset_id); // Close the dataset
        ERR(status);
        status = H5Fclose(file_id);
        ERR(status);
    }
    else
    { // Read HDF4 files
#ifdef HAVE_HDF4
        int32_t sd_id, sds_id;
        int32_t rank, data_type, n_attrs;
        int32_t dim_sizes[H4_MAX_VAR_DIMS];
        intn status;
        char name[H4_MAX_NC_NAME];

        sd_id = SDstart(fname, DFACC_READ);
        sds_id = SDselect(sd_id, dsetnumber);
        status = SDgetinfo(sds_id, name, &rank, dim_sizes, &data_type, &n_attrs);
        DimMax[0] = dim_sizes[0];

        status = SDendaccess(sds_id); // close dataset
        ERR(status);
        status = SDend(sd_id);
        ERR(status);
#endif
    }
} /* END mhdReadMeshDimensions */


/* This function reads MHD mesh coordinates. Switch HDF4 or HDF5 based on
   hdf5_input flag.
*/
void mhdReadMesh(char *fname, char *dsetname, int dsetnumber, float *Dim[])
{
    if (hdf5_input == 1)
    {
        hid_t file_id, dataset_id; // Data Handles
        int status;

        file_id = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT); // Open the file
        dataset_id = H5Dopen2(file_id, dsetname, H5P_DEFAULT); // Open the dataset

        status = H5Dread(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, (VOIDP)*Dim);

        /* Close the file. */
        status = H5Dclose(dataset_id); // Close the dataset
        ERR(status);
        status = H5Fclose(file_id);    // Close the file
        ERR(status);
    }
    else
    { // Read HDF4 file
#ifdef HAVE_HDF4
        int32_t sd_id, sds_id;
        int32_t rank, data_type, n_attrs;
        int32_t dim_sizes[H4_MAX_VAR_DIMS];
        int32_t start[1] = {0};
        char name[H4_MAX_NC_NAME];
        intn status;

        sd_id = SDstart(fname, DFACC_READ);
        sds_id = SDselect(sd_id, dsetnumber);
        status = SDgetinfo(sds_id, name, &rank, dim_sizes, &data_type, &n_attrs);
        status = SDreaddata(sds_id, start, NULL, dim_sizes, (VOIDP)*Dim);
        status = SDendaccess(sds_id); // Close dataset
        ERR(status);
        status = SDend(sd_id); // Close the file
        ERR(status);
#endif
    }
} /* END mhdReadMesh */


/* This function reads MHD 3D data. Switch HDF4 or HDF5 based on hdf5_input
   flag.
*/
void mhdReadDatafromFile(char *fname, float *buf[])
{
    if (hdf5_input == 1)
    { // Read from HDF5 file

        hid_t file_id, dataset_id, data_type; // Data Handles
        H5T_class_t data_class;
        int status;

        file_id = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT); // Open the file
        dataset_id = H5Dopen2(file_id, "Data", H5P_DEFAULT);   // Open the dataset: "data"
        data_type = H5Dget_type(dataset_id);
        data_class = H5Tget_native_type(data_type, H5T_DIR_DEFAULT);
        status = H5Dread(dataset_id, data_class, H5S_ALL, H5S_ALL, H5P_DEFAULT, (VOIDP)*buf);
        ERR(status);
        // Close the dataset
        status = H5Dclose(dataset_id);
        ERR(status);
        // Close the file.
        status = H5Fclose(file_id);
        ERR(status);
    }
    else
    { // Read from HDF4 file
#ifdef HAVE_HDF4
        int32_t sd_id, sds_id;
        int32_t rank, data_type, n_attrs;
        int32_t dim_sizes[H4_MAX_VAR_DIMS];
        int32_t start[3] = {0, 0, 0};
        intn status;
        char name[H4_MAX_NC_NAME];

        sd_id = SDstart(fname, DFACC_READ);                                       // Open the file
        sds_id = SDselect(sd_id, 3);                                             // Open the dataset: 3 is the default datasetnumber
        status = SDgetinfo(sds_id, name, &rank, dim_sizes, &data_type, &n_attrs); // Get the dimensions
        status = SDreaddata(sds_id, start, NULL, dim_sizes, (VOIDP)*buf);         // Read in data
        ERR(status);
        status = SDendaccess(sds_id); // Close the dataset
        ERR(status);
        status = SDend(sd_id); // Close the file
        ERR(status);
#endif
    }
} /* END mhdReadDatafromfile */


/* Checks the file extension H5(default) or HDF4. */
void mhdDatafile_type()
{

  char fileNames[7][MAX_STRING_SIZE];

  if (config.mhdDigits == 3)
  {
    sprintf(fileNames[0], "%sbp%03d", config.mhdDirectory, 1);
  }
  else
  {
    sprintf(fileNames[0], "%sbp%06d", config.mhdDirectory, 1);
  }

    if (access(strcat(fileNames[0],".h5"), F_OK) == 0) {
        printf("HDF5 mhd datafiles detected \n");
        hdf5_input =1;
        strncpy(file_extension,".h5",strlen(".h5")+1);
    } else if (access(strcat(fileNames[0],".hdf"), F_OK) == 0){
        printf("HDF4 mhd datafiles detected \n");
        hdf5_input =0;
        strncpy(file_extension,".hdf",strlen(".hdf")+1);
    }
    else{
        printf("No valid mhd datafiles detected \n");
        exit(0);
    }
} /* END mhdDatafile_type */

