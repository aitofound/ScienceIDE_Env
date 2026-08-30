/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2010-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sdf.h>
#include "sdf_input.h"
#include "sdf_input_point.h"
#include "sdf_control.h"

#ifndef PARALLEL
# ifdef _WIN32
#  include <io.h>
#  define FSEEKO _fseeki64
# else
#  include <unistd.h>
#  include <sys/mman.h>
#  define FSEEKO fseeko
# endif
#endif

//#define SDF_COMMON_MESH_LENGTH (4 + 8 + h->id_length + 4 * b->ndims)

#define SDF_COMMON_MESH_INFO() do { \
    if (!h->current_block || !h->current_block->done_header) { \
        if (h->rank == h->rank_master) { \
            fprintf(stderr, "*** ERROR ***\n"); \
            fprintf(stderr, "SDF block header has not been read." \
                    " Ignoring call.\n"); \
        } \
        return 1; \
    } \
    b = h->current_block; \
    if (b->done_info) return 0; \
    h->current_location = b->block_start + h->block_header_length; \
    b->done_info = 1; } while(0)


static int sdf_point_factor(sdf_file_t *h, int64_t *nelements_local);


int sdf_read_point_mesh_info(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    // Metadata is
    // - mults     REAL(r8), DIMENSION(ndims)
    // - labels    CHARACTER(id_length), DIMENSION(ndims)
    // - units     CHARACTER(id_length), DIMENSION(ndims)
    // - geometry  INTEGER(i4)
    // - minval    REAL(r8), DIMENSION(ndims)
    // - maxval    REAL(r8), DIMENSION(ndims)
    // - npoints   INTEGER(i8)
    // - speciesid CHARACTER(id_length)

    SDF_COMMON_MESH_INFO();

    SDF_READ_ENTRY_ARRAY_REAL8(b->dim_mults, b->ndims);

    SDF_READ_ENTRY_ARRAY_ID(b->dim_labels, b->ndims);

    SDF_READ_ENTRY_ARRAY_ID(b->dim_units, b->ndims);

    SDF_READ_ENTRY_INT4(b->geometry);

    SDF_READ_ENTRY_ARRAY_REAL8(b->extents, 2*b->ndims);

    SDF_READ_ENTRY_INT8(b->nelements);
    for (i = 0; i < b->ndims; i++) b->dims[i] = b->nelements;

    // species_id field added in version 1.3
    if (1000 * h->file_version + h->file_revision > 1002)
        SDF_READ_ENTRY_ID(b->material_id);

    b->stagger = SDF_STAGGER_VERTEX;

    b->ndim_labels = b->ndim_units = b->ndims;

    return 0;
}



int sdf_read_point_variable_info(sdf_file_t *h)
{
    sdf_block_t *b;
    int i;

    // Metadata is
    // - mult      REAL(r8)
    // - units     CHARACTER(id_length)
    // - meshid    CHARACTER(id_length)
    // - npoints   INTEGER(i8)
    // - speciesid CHARACTER(id_length)

    SDF_COMMON_MESH_INFO();

    SDF_READ_ENTRY_REAL8(b->mult);

    SDF_READ_ENTRY_ID(b->units);

    SDF_READ_ENTRY_ID(b->mesh_id);

    SDF_READ_ENTRY_INT8(b->nelements);
    for (i = 0; i < b->ndims; i++) b->dims[i] = b->nelements;

    // species_id field added in version 1.3
    if (1000 * h->file_version + h->file_revision > 1002)
        SDF_READ_ENTRY_ID(b->material_id);

    return 0;
}



static int sdf_create_1d_distribution(sdf_file_t *h, int64_t global, int local,
        int start)
{
#ifdef PARALLEL
    sdf_block_t *b = h->current_block;
    int size, blocklengths[1];
    MPI_Aint displacements[1];
    MPI_Datatype typ;

    MPI_Type_size(b->mpitype, &size);

    blocklengths[0] = global;
    displacements[0] = start * size;

    MPI_Type_create_hindexed(1, blocklengths, displacements, b->mpitype, &typ);

    b->distribution = 0;
    MPI_Type_create_resized(typ, 0, displacements[0], &b->distribution);
    MPI_Type_commit(&b->distribution);
#endif
    return 0;
}



static int sdf_free_distribution(sdf_file_t *h)
{
#ifdef PARALLEL
    sdf_block_t *b = h->current_block;

    MPI_Type_free(&b->distribution);
#endif
    return 0;
}



static int sdf_helper_read_array(sdf_file_t *h, void **var_in, size_t count)
{
    sdf_block_t *b = h->current_block;
    char **var = (char **)var_in;
    size_t sz, length;
#ifndef PARALLEL
    size_t mlen, mstart, moff;
#endif

    sz = SDF_TYPE_SIZES[b->datatype];
    length = count * sz;

#if !defined(PARALLEL) && !defined(_WIN32)
    if (h->mmap) {
        mlen = sysconf(_SC_PAGESIZE);
        mstart = mlen * (h->current_location / mlen);
        moff = h->current_location - mstart;
        b->mmap_len = mlen = length + moff;
        b->mmap = mmap(NULL, mlen, PROT_READ, MAP_SHARED, h->fd, mstart);
        if (*var) free(*var);
        *var = moff + b->mmap;
        return 0;
    }
#endif

    if (*var) free(*var);
    *var = malloc(length);
#ifdef PARALLEL
    MPI_File_set_view(h->filehandle, h->current_location, b->mpitype,
            b->distribution, "native", MPI_INFO_NULL);
    MPI_File_read_all(h->filehandle, *var, count, b->mpitype,
            MPI_STATUS_IGNORE);
    MPI_File_set_view(h->filehandle, 0, MPI_BYTE, MPI_BYTE, "native",
            MPI_INFO_NULL);
#else
    FSEEKO(h->filehandle, h->current_location, SEEK_SET);
    if (!fread(*var, sz, count, h->filehandle))
        return 1;
#endif

    return 0;
}



int sdf_read_point_mesh(sdf_file_t *h)
{
    sdf_block_t *b = h->current_block;
    int n;

    if (b->done_data) return 0;
    if (!b->done_info) sdf_read_point_mesh_info(h);

    sdf_point_factor(h, &b->local_dims[0]);
    for (n = 1; n < 3; n++)
        b->local_dims[n] = b->local_dims[0];
    if (!b->array_starts)
        b->nelements_local = b->local_dims[0];

    h->current_location = b->data_location;

    if (!b->grids) {
        b->ngrids = b->ndims;
        b->grids = calloc(b->ngrids, sizeof(float*));
    }

    if (h->print) {
        h->indent = 0;
        SDF_DPRNT("\n");
        SDF_DPRNT("b->name: %s ", b->name);
        for (n=0; n < b->ndims; n++)
            SDF_DPRNT("%" PRIi64 " ",b->nelements_local);
        SDF_DPRNT("\n");
        h->indent = 2;
    }
    for (n = 0; n < 3; n++) {
        if (b->ndims > n) {
            if (b->array_starts)
                h->current_location
                        += b->array_starts[n] * SDF_TYPE_SIZES[b->datatype];
#ifdef PARALLEL
            sdf_create_1d_distribution(h, b->dims[0], b->nelements_local,
                    b->starts[0]);
#endif
            sdf_helper_read_array(h, &b->grids[n], b->nelements_local);
            sdf_free_distribution(h);
            sdf_convert_array_to_float(h, &b->grids[n], b->nelements_local);
            if (h->use_random)
                sdf_randomize_array(h, &b->grids[n], b->nelements_local);
            if (h->print) {
                SDF_DPRNT("%s: ", b->dim_labels[n]);
                SDF_DPRNTar(b->grids[n], b->nelements_local);
            }
            h->current_location += SDF_TYPE_SIZES[b->datatype] * b->dims[0];
            if (b->array_starts)
                h->current_location
                        -= b->array_starts[n] * SDF_TYPE_SIZES[b->datatype];
        }
    }

    b->done_data = 1;

    return 0;
}



static int sdf_point_factor(sdf_file_t *h, int64_t *nelements_local)
{
    sdf_block_t *b = h->current_block;
#ifdef PARALLEL
    int64_t npoint_min, split_big;

    npoint_min = b->dims[0] / h->ncpus;
    split_big = b->dims[0] - h->ncpus * npoint_min;

    if (h->rank >= split_big) {
        b->starts[0] = split_big * (npoint_min + 1)
              + (h->rank - split_big) * npoint_min;
        *nelements_local = npoint_min;
    } else {
        b->starts[0] = h->rank * (npoint_min + 1);
        *nelements_local = npoint_min + 1;
    }
#else
    *nelements_local = b->dims[0];
#endif

    return 0;
}



int sdf_read_point_variable(sdf_file_t *h)
{
    sdf_block_t *b = h->current_block;
    int n;

    if (b->done_data) return 0;
    if (!b->done_info) sdf_read_blocklist(h);

    sdf_point_factor(h, &b->local_dims[0]);
    if (!b->array_starts)
        b->nelements_local = b->local_dims[0];

    h->current_location = b->data_location;
    if (b->array_starts)
        h->current_location += b->array_starts[0] * SDF_TYPE_SIZES[b->datatype];

#ifdef PARALLEL
    sdf_create_1d_distribution(h, b->dims[0], b->nelements_local,
            b->starts[0]);
#endif
    sdf_helper_read_array(h, &b->data, b->nelements_local);
    sdf_free_distribution(h);
    sdf_convert_array_to_float(h, &b->data, b->nelements_local);
    if (h->use_random) sdf_randomize_array(h, &b->data, b->nelements_local);
    h->current_location = h->current_location
            + SDF_TYPE_SIZES[b->datatype] * b->dims[0];

    if (h->print) {
        h->indent = 0;
        SDF_DPRNT("\n");
        SDF_DPRNT("b->name: %s ", b->name);
        for (n=0; n < b->ndims; n++)
            SDF_DPRNT("%" PRIi64 " ",b->nelements_local);
        SDF_DPRNT("\n  ");
        SDF_DPRNTar(b->data, b->nelements_local);
    }

    b->done_data = 1;

    return 0;
}
