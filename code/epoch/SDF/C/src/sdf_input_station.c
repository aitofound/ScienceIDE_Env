/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2013-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sdf.h>
#include "sdf_input.h"
#include "sdf_input_station.h"
#include "sdf_control.h"

#ifndef PARALLEL
# ifdef _WIN32
#  include <io.h>
# else
#  include <unistd.h>
#  include <sys/mman.h>
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


int sdf_read_station_info(sdf_file_t *h)
{
    sdf_block_t *b;
    char use_mult[5];
    char *use_mult_ptr = use_mult;

    // Metadata is
    // - nelements INTEGER(i8)
    // - entry_len INTEGER(i4)
    // - nstations INTEGER(i4)
    // - nvars     INTEGER(i4)
    // - step0     INTEGER(i4)
    // - step_inc  INTEGER(i4)
    // - time0     REAL(r8)
    // - time_inc  REAL(r8)
    // - use_mult  CHARACTER(1)
    // - padding   CHARACTER(3)
    // - statids   CHARACTER(id_length), DIMENSION(nstations)
    // - statnames CHARACTER(string_length), DIMENSION(nstations)
    // - statnvars INTEGER(i4), DIMENSION(nstations)
    // - statmove  INTEGER(i4), DIMENSION(nstations)
    // - statx0    REAL(r8), DIMENSION(nstations*ndims)
    // - varids    CHARACTER(id_length), DIMENSION(nvars)
    // - varnames  CHARACTER(string_length), DIMENSION(nvars)
    // - vartypes  INTEGER(i4), DIMENSION(nvars)
    // - varunits  CHARACTER(id_length), DIMENSION(nvars)
    // - varmults  REAL(r8), DIMENSION(use_mult*nvars)

    SDF_COMMON_MESH_INFO();

    SDF_READ_ENTRY_INT8(b->nelements);
    SDF_READ_ENTRY_INT4(b->type_size);
    SDF_READ_ENTRY_INT4(b->nstations);
    SDF_READ_ENTRY_INT4(b->nvariables);
    SDF_READ_ENTRY_INT4(b->step);
    SDF_READ_ENTRY_INT4(b->step_increment);
    SDF_READ_ENTRY_REAL8(b->time);
    SDF_READ_ENTRY_REAL8(b->time_increment);
    SDF_READ_ENTRY_STRINGLEN(use_mult_ptr,4);

    SDF_READ_ENTRY_ARRAY_ID(b->station_ids, b->nstations);
    SDF_READ_ENTRY_ARRAY_STRING(b->station_names, b->nstations);
    SDF_READ_ENTRY_ARRAY_INT4(b->station_nvars, b->nstations);
    SDF_READ_ENTRY_ARRAY_INT4(b->station_move, b->nstations);
    SDF_READ_ENTRY_ARRAY_REAL8(b->station_x, b->nstations);
    if (b->ndims > 1)
        SDF_READ_ENTRY_ARRAY_REAL8(b->station_y, b->nstations);
    if (b->ndims > 2)
        SDF_READ_ENTRY_ARRAY_REAL8(b->station_z, b->nstations);

    SDF_READ_ENTRY_ARRAY_ID(b->variable_ids, b->nvariables);
    SDF_READ_ENTRY_ARRAY_STRING(b->material_names, b->nvariables);
    SDF_READ_ENTRY_ARRAY_INT4(b->variable_types, b->nvariables);
    SDF_READ_ENTRY_ARRAY_ID(b->dim_units, b->nvariables);
    if (use_mult[0])
        SDF_READ_ENTRY_ARRAY_REAL8(b->dim_mults, b->nvariables);

    b->stagger = SDF_STAGGER_VERTEX;

    b->nstation_ids = b->nstation_names = b->nstations;
    b->ndim_units = b->nvariable_ids = b->nmaterial_names = b->nvariables;
    return 0;
}



int sdf_read_station_timehis(sdf_file_t *h, long *stat, int nstat,
        char **var_names, int nvars, double t0, double t1, char **timehis,
        int *size, int *offset, int *nrows, int *row_size)
{
    /* Read station time histories for specified stations and variables
     * from a single station block.
     * Unknown variables are ignored.
     * The list of specified stations must be in ascending numerical
     * order and all valid
     */

    sdf_block_t *b = h->current_block;
    char *data, *t_raw, *trow, *v;
    int i, j, s, ii, jj, ss;
    long buff_size, pos, start_pos, end_pos, start, end, mid;
    double t;
#ifndef PARALLEL
    size_t mlen, mstart, moff;
#endif

    if (!b->done_info) sdf_read_station_info(h);

    /* Record 'Time' as the first variable.
     * Sanity check first. */
    if (strcmp(b->variable_ids[0], "time") != 0) {
        fprintf(stderr, "SDF C Library, internal error: "
                "First variable in a station block is not 'time'.\n");
        return 1;
    }
    /* Check validity of station numbers */
    for (i=0; i < nstat; i++) {
        if (stat[i] < 0 || stat[i] >= b->nstations) {
            fprintf(stderr, "SDF C Library, function sdf_read_station_timehis, "
                    "internal error: Requested station number lies "
                    "outside valid range.\n");
            return 1;
        }
        if (i > 0 && stat[i-1] >= stat[i]) {
            fprintf(stderr, "SDF C Library, function sdf_read_station_timehis, "
                    "internal error: Requested list of stations is not "
                    "a strictly increasing list.\n");
            return 1;
        }
    }

    offset[0] = 0;
    size[0] = SDF_TYPE_SIZES[b->variable_types[0]];

    buff_size = SDF_TYPE_SIZES[b->variable_types[0]];
    *row_size = SDF_TYPE_SIZES[b->variable_types[0]];
    ss = 0;
    ii = 1;
    jj = 1;
    for (s=0; s < b->nstations; s++) {
        for (i=0; i < b->station_nvars[s]; i++) {
            if (s == stat[ss]) {
                for (j=0; j < nvars; j++) {
                    if (!strcmp(b->material_names[ii+i], var_names[j])
                            || !strcmp(b->variable_ids[ii+i], var_names[j])) {
                        offset[jj+j] = buff_size;
                        size[jj+j] = SDF_TYPE_SIZES[b->variable_types[ii+i]];
                        *row_size += SDF_TYPE_SIZES[b->variable_types[ii+i]];
                    }
                }
            }
            buff_size += SDF_TYPE_SIZES[b->variable_types[ii+i]];
        }
        if (s == stat[ss]) {
            ss++;
            jj += nvars;
        }
        ii += i;
    }

#if !defined(PARALLEL) && !defined(_WIN32)
    if (h->mmap) {
        mlen = sysconf(_SC_PAGESIZE);
        mstart = mlen * (b->data_location / mlen);
        moff = b->data_location - mstart;
        b->mmap_len = mlen = b->data_length + moff;
        b->mmap = mmap(NULL, mlen, PROT_READ, MAP_SHARED, h->fd, mstart);
        data = moff + b->mmap;
    } else
#endif
        t_raw = malloc(SDF_TYPE_SIZES[b->variable_types[i]]);

    /* Find the start and end time steps
     * Bisect until t[start] <= t0 < t[start+1]
     */
    start = 0;
    end = b->nelements;
    for (; start+1 < end;) {
        /* mid = (start + end)/2 can overflow
         * See http://googleresearch.blogspot.co.uk/2006/06/extra-extra-read-all-about-it-nearly.html
         */
        mid = ((unsigned int)start + (unsigned int)end) >> 1;
        if (h->mmap)
            t_raw = data + mid * buff_size;
        else {
            sdf_seek_set(h, b->data_location + mid * buff_size);
            sdf_read_bytes(h, t_raw, SDF_TYPE_SIZES[b->variable_types[0]]);
        }
        switch(b->variable_types[0]) {
        case SDF_DATATYPE_REAL4:
            t = (double)(*(float *)t_raw);
            break;
        case SDF_DATATYPE_REAL8:
            t = *((double *)t_raw);
            break;
        }
        if (t <= t0)
            start = mid;
        else
            end = mid;
    }
    start_pos = start;

    start = 0;
    end = b->nelements;
    for (; start+1 < end;) {
        /* mid = (start + end)/2 can overflow
         * See http://googleresearch.blogspot.co.uk/2006/06/extra-extra-read-all-about-it-nearly.html
         */
        mid = ((unsigned int)start + (unsigned int)end) >> 1;
        if (h->mmap)
            t_raw = data + mid * buff_size;
        else {
            sdf_seek_set(h, b->data_location + mid * buff_size);
            sdf_read_bytes(h, t_raw, SDF_TYPE_SIZES[b->variable_types[0]]);
        }
        switch(b->variable_types[0]) {
        case SDF_DATATYPE_REAL4:
            t = (double)(*(float *)t_raw);
            break;
        case SDF_DATATYPE_REAL8:
            t = *((double *)t_raw);
            break;
        }
        if (t <= t1)
            start = mid;
        else
            end = mid;
    }
    end_pos = start;

    *nrows = end_pos - start_pos + 1;
    if (*nrows <= 0) {
        if (!h->mmap)
            free(t_raw);
        return 1;
    }

    *timehis = (char *)malloc(*nrows * *row_size);
    if (h->mmap)
        data = data + start_pos * buff_size;
    else
        data = malloc(buff_size);

    trow = *timehis;
    for (pos=start_pos; pos <= end_pos; pos++) {
        if (!h->mmap) {
            sdf_seek_set(h, b->data_location + pos*buff_size);
            sdf_read_bytes(h, data, buff_size);
        }
        v = trow;
        for (i=0; i < nstat*nvars+1; i++) {
            memcpy(v, data + offset[i], size[i]);
            if (i < nstat*nvars)
                v += (end_pos - pos + 1) * size[i]
                        + (pos - start_pos) * size[i+1];
        }
        trow += size[0];
        if (h->mmap)
            data += buff_size;
    }

    sdf_seek_set(h, b->data_location);
    h->current_location = b->data_location;

    if (!h->mmap)
        free(data);

    /* Now redefine offset to point to the columns within timhis
     * rather than the raw data block
     */
    offset[0] = 0;
    for (i=1; i <= nstat*nvars; i++)
        offset[i] = offset[i-1] + size[i-1];

    return 0;
}
