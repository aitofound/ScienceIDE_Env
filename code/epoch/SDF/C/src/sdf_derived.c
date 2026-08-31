/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2011-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sdf_vector_type.h>
#include <sdf_list_type.h>
#include <sdf.h>
#include <sdf_helper.h>
#include "sdf_control.h"
#include "sdf_input.h"
#include "stack_allocator.h"

#define SDF_SET_ENTRY_STRINGLEN(value, strvalue, length) do { \
        if (!(value)) value = malloc(h->string_length+1); \
        strncpy((value), (strvalue), (length)); \
        value[h->string_length-1] = '\0'; \
    } while (0)

#define SDF_SET_ENTRY_ID(value, strvalue) do { \
        SDF_SET_ENTRY_STRINGLEN(value, strvalue, h->id_length); \
        SDF_DPRNT(#value ": %s\n", (value)); \
    } while (0)

#define SDF_SET_ENTRY_STRING(value, strvalue) do { \
        SDF_SET_ENTRY_STRINGLEN(value, strvalue, h->string_length); \
        SDF_DPRNT(#value ": %s\n", (value)); \
    } while (0)

#define IJK(i,j,k) ((i) + nx * ((j) + ny * (k)))
#define IJK1(i,j,k) ((i) + (nx+1) * ((j) + (ny+1) * (k)))
#define IJK2(i,j,k) ((i)+1 + (nx+2) * ((j)+1 + (ny+2) * ((k)+1)))

#define APPEND_BLOCK(b) do { \
        (b)->next = calloc(1, sizeof(sdf_block_t)); \
        (b)->next->prev = (b); \
        (b) = (b)->next; \
        (b)->next = NULL; \
    } while (0)


static char *strcat_alloc(char *base, char *sfx)
{
    int len1 = strlen(base);
    int len2 = strlen(sfx);
    char *str = malloc(len1+len2+2);
    memcpy(str, base, len1);
    str[len1] = '/';
    memcpy(str+len1+1, sfx, len2); \
    str[len1+len2+1] = '\0';
    return str;
}



void sdf_unique_id(sdf_file_t *h, char *str)
{
    sdf_block_t *b, *next;
    int i, pos, len, sublen, unique;
    int max_length = h->id_length;

    len = strlen(str);
    if (len >= max_length - 1) str[max_length-1] = '\0';

    for (i=0; i < 99; i++) {
        next = h->blocklist;

        unique = 1;
        while (next) {
            b = next;
            next = b->next;

            if (b->id == str) continue;

            sublen = strlen(b->id) + 1;

            if (len == sublen && memcmp(b->id, str, len) == 0) {
                unique = 0;
                break;
            }
        }

        if (unique) break;

        pos = len - 2;
        if (pos == max_length) pos--;
        if (i == 9 && pos == max_length - 1) pos--;

        sprintf(str + pos, "%d", i+1);
        len = strlen(str);
    }
}



void sdf_unique_name(sdf_file_t *h, char *str)
{
    sdf_block_t *b, *next;
    int i, pos, len, sublen, unique;
    int max_length = h->string_length;

    len = strlen(str);
    if (len >= max_length - 1) str[max_length-1] = '\0';

    for (i=0; i < 99; i++) {
        next = h->blocklist;

        unique = 1;
        while (next) {
            b = next;
            next = b->next;

            if (b->name == str) continue;

            sublen = strlen(b->name) + 1;

            if (len == sublen && memcmp(b->name, str, len) == 0) {
                unique = 0;
                break;
            }
        }

        if (unique) break;

        pos = len - 2;
        if (pos == max_length) pos--;
        if (i == 9 && pos == max_length - 1) pos--;

        sprintf(str + pos, "%d", i+1);
        len = strlen(str);
    }
}



static sdf_block_t *sdf_callback_boundary_mesh_ob(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *grid = sdf_find_block_by_id(h, b->subblock->mesh_id);
    sdf_block_t *current_block = h->current_block;

    int i, j, k, ii, jj, kk, i0, j0, k0, i1, j1, k1;
    int imin, imax, jmin, jmax, kmin, kmax;
    int nx, ny, nz;
    int gotmat, gotobst, nelements;
    int *vertex_index, *obdata, *ijk;
    float *vertex;

    vector_t *faces = vector_new();
    vector_t *boundary = vector_new();
    vector_t *vertijk = vector_new();

    h->current_block = grid;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = b->subblock;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = current_block;

    obdata = (int *)b->subblock->data;
    nx = b->subblock->local_dims[0] - 2;
    ny = b->subblock->local_dims[1] - 2;
    nz = b->subblock->local_dims[2] - 2;

#ifdef PARALLEL
    imin = (b->subblock->proc_min[0] < 0) ? 0  : nx;
    imax = (b->subblock->proc_max[0] < 0) ? nx : 0;
    jmin = (b->subblock->proc_min[1] < 0) ? 0  : ny;
    jmax = (b->subblock->proc_max[1] < 0) ? ny : 0;
    kmin = (b->subblock->proc_min[2] < 0) ? 0  : nz;
    kmax = (b->subblock->proc_max[2] < 0) ? nz : 0;
#else
    imin = 0; imax = nx;
    jmin = 0; jmax = ny;
    kmin = 0; kmax = nz;
#endif

    switch(b->nm) {
        case 1: // x_min
            imax = 0;
            jmin = ny; jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 2: // x_max
            imin = nx;
            jmin = ny; jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 3: // y_min
            imin = nx; imax = 0;
            jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 4: // y_max
            imin = nx; imax = 0;
            jmin = ny;
            kmin = nz; kmax = 0;
            break;
        case 5: // z_min
            imin = nx; imax = 0;
            jmin = ny; jmax = 0;
            kmax = 0;
            break;
        case 6: // z_max
            imin = nx; imax = 0;
            jmin = ny; jmax = 0;
            kmin = nz;
            break;
    }

    nelements = 0;
    vertex_index = (int*)malloc((nx+1)*(ny+1)*(nz+1)*sizeof(int));

    ii = 0;
    for (i = imin; i <= imax; i += nx) {
        k0 = 0;
        for (k = 0; k <= nz; k++) {
            k1 = (k == nz) ? k : nz-1;
            j0 = 0;
            for (j = 0; j <= ny; j++) {
                j1 = (j == ny) ? ny-1 : j;

                gotmat = gotobst = 0;

                for (kk = k0; kk <= k1; kk++) {
                for (jj = j0; jj <= j1; jj++) {
                    if (!obdata[IJK2(ii,jj,kk)])
                        gotmat = 1;
                }}

                if (gotmat) {
                    vertex_index[IJK1(i,j,k)] = nelements++;
                    vector_push_back(vertijk, i);
                    vector_push_back(vertijk, j);
                    vector_push_back(vertijk, k);
                }

                j0 = j;
            }
            k0 = k;
        }
        ii = nx-1;
    }

    jj = 0;
    for (j = jmin; j <= jmax; j += ny) {
        k0 = 0;
        for (k = 0; k <= nz; k++) {
            k1 = (k == nz) ? k : nz-1;
            i0 = 0;
            for (i = 0; i <= nx; i++) {
                i1 = (i == nx) ? nx-1 : i;

                gotmat = gotobst = 0;

                for (kk = k0; kk <= k1; kk++) {
                for (ii = i0; ii <= i1; ii++) {
                    if (!obdata[IJK2(ii,jj,kk)])
                        gotmat = 1;
                }}

                if (gotmat) {
                    vertex_index[IJK1(i,j,k)] = nelements++;
                    vector_push_back(vertijk, i);
                    vector_push_back(vertijk, j);
                    vector_push_back(vertijk, k);
                }

                i0 = i;
            }
            k0 = k;
        }
        jj = ny-1;
    }

    kk = 0;
    for (k = kmin; k <= kmax; k += nz) {
        j0 = 0;
        for (j = 0; j <= ny; j++) {
            j1 = (j == ny) ? j : ny-1;
            i0 = 0;
            for (i = 0; i <= nx; i++) {
                i1 = (i == nx) ? nx-1 : i;

                gotmat = gotobst = 0;

                for (jj = j0; jj <= j1; jj++) {
                for (ii = i0; ii <= i1; ii++) {
                    if (!obdata[IJK2(ii,jj,kk)])
                        gotmat = 1;
                }}

                if (gotmat) {
                    vertex_index[IJK1(i,j,k)] = nelements++;
                    vector_push_back(vertijk, i);
                    vector_push_back(vertijk, j);
                    vector_push_back(vertijk, k);
                }

                i0 = i;
            }
            j0 = j;
        }
        kk = nz-1;
    }

    b->dims[0] = b->nelements = nelements;
    if (b->data) free(b->data);
    b->data = malloc(b->ndims * nelements * sizeof(float));
    vertex = (float*)b->data;
    ijk = vertijk->data;

    if (grid->datatype_out == SDF_DATATYPE_REAL8) {
        double *x = grid->grids[0];
        double *y = grid->grids[1];
        double *z = grid->grids[2];
        for (i = 0; i < nelements; i++) {
            *vertex++ = x[*ijk++];
            *vertex++ = y[*ijk++];
            *vertex++ = z[*ijk++];
        }
    } else {
        float *x = grid->grids[0];
        float *y = grid->grids[1];
        float *z = grid->grids[2];
        for (i = 0; i < nelements; i++) {
            *vertex++ = x[*ijk++];
            *vertex++ = y[*ijk++];
            *vertex++ = z[*ijk++];
        }
    }

    vector_free(vertijk);

    // Scan faces in x-direction
    ii = 0;
    for (i = imin; i <= imax; i += nx) {
        for (k = 0; k < nz; k++) {
        for (j = 0; j < ny; j++) {
            if (!obdata[IJK2(ii,j,k)]) {
                vector_push_back(faces, vertex_index[IJK1(i,j  ,k  )]);
                vector_push_back(faces, vertex_index[IJK1(i,j+1,k  )]);
                vector_push_back(faces, vertex_index[IJK1(i,j+1,k+1)]);
                vector_push_back(faces, vertex_index[IJK1(i,j  ,k+1)]);
                vector_push_back(boundary, IJK(ii,j,k));
            }
        }}
        ii = nx - 1;
    }

    // Scan faces in y-direction
    jj = 0;
    for (j = jmin; j <= jmax; j += ny) {
        for (k = 0; k < nz; k++) {
        for (i = 0; i < nx; i++) {
            if (!obdata[IJK2(i,jj,k)]) {
                vector_push_back(faces, vertex_index[IJK1(i  ,j,k  )]);
                vector_push_back(faces, vertex_index[IJK1(i+1,j,k  )]);
                vector_push_back(faces, vertex_index[IJK1(i+1,j,k+1)]);
                vector_push_back(faces, vertex_index[IJK1(i  ,j,k+1)]);
                vector_push_back(boundary, IJK(i,jj,k));
            }
        }}
        jj = ny - 1;
    }

    // Scan faces in z-direction
    kk = 0;
    for (k = kmin; k <= kmax; k += nz) {
        for (j = 0; j < ny; j++) {
        for (i = 0; i < nx; i++) {
            if (!obdata[IJK2(i,j,kk)]) {
                vector_push_back(faces, vertex_index[IJK1(i  ,j  ,k)]);
                vector_push_back(faces, vertex_index[IJK1(i+1,j  ,k)]);
                vector_push_back(faces, vertex_index[IJK1(i+1,j+1,k)]);
                vector_push_back(faces, vertex_index[IJK1(i  ,j+1,k)]);
                vector_push_back(boundary, IJK(i,j,kk));
            }
        }}
        kk = nz - 1;
    }

    free(vertex_index);

    if (b->boundary_cells) free(b->boundary_cells);

    b->nfaces = boundary->size;
    vector_truncate(boundary);
    b->boundary_cells = boundary->data;
    free(boundary);

    vector_truncate(faces);
    b->node_list = faces->data;
    free(faces);

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_boundary_mesh(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *grid = sdf_find_block_by_id(h, b->subblock->mesh_id);
    sdf_block_t *current_block = h->current_block;

    int i, j, k, ii, jj, kk;
    int imin, imax, jmin, jmax, kmin, kmax;
    int nx, ny, nz;
    int nelements;
    int *vertex_index, *ijk;
    float *vertex;

    vector_t *faces = vector_new();
    vector_t *boundary = vector_new();
    vector_t *vertijk = vector_new();

    h->current_block = grid;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = b->subblock;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = current_block;

    nx = b->subblock->local_dims[0] - 2;
    ny = b->subblock->local_dims[1] - 2;
    nz = b->subblock->local_dims[2] - 2;

#ifdef PARALLEL
    imin = (b->subblock->proc_min[0] < 0) ? 0  : nx;
    imax = (b->subblock->proc_max[0] < 0) ? nx : 0;
    jmin = (b->subblock->proc_min[1] < 0) ? 0  : ny;
    jmax = (b->subblock->proc_max[1] < 0) ? ny : 0;
    kmin = (b->subblock->proc_min[2] < 0) ? 0  : nz;
    kmax = (b->subblock->proc_max[2] < 0) ? nz : 0;
#else
    imin = 0; imax = nx;
    jmin = 0; jmax = ny;
    kmin = 0; kmax = nz;
#endif

    switch(b->nm) {
        case 1: // x_min
            imax = 0;
            jmin = ny; jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 2: // x_max
            imin = nx;
            jmin = ny; jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 3: // y_min
            imin = nx; imax = 0;
            jmax = 0;
            kmin = nz; kmax = 0;
            break;
        case 4: // y_max
            imin = nx; imax = 0;
            jmin = ny;
            kmin = nz; kmax = 0;
            break;
        case 5: // z_min
            imin = nx; imax = 0;
            jmin = ny; jmax = 0;
            kmax = 0;
            break;
        case 6: // z_max
            imin = nx; imax = 0;
            jmin = ny; jmax = 0;
            kmin = nz;
            break;
    }

    nelements = 0;
    vertex_index = (int*)malloc((nx+1)*(ny+1)*(nz+1)*sizeof(int));

    for (i = imin; i <= imax; i += nx) {
        for (k = 0; k <= nz; k++) {
        for (j = 0; j <= ny; j++) {
            vertex_index[IJK1(i,j,k)] = nelements++;
            vector_push_back(vertijk, i);
            vector_push_back(vertijk, j);
            vector_push_back(vertijk, k);
        }}
    }

    for (j = jmin; j <= jmax; j += ny) {
        for (k = 0; k <= nz; k++) {
        for (i = 0; i <= nx; i++) {
            vertex_index[IJK1(i,j,k)] = nelements++;
            vector_push_back(vertijk, i);
            vector_push_back(vertijk, j);
            vector_push_back(vertijk, k);
        }}
    }

    for (k = kmin; k <= kmax; k += nz) {
        for (j = 0; j <= ny; j++) {
        for (i = 0; i <= nx; i++) {
            vertex_index[IJK1(i,j,k)] = nelements++;
            vector_push_back(vertijk, i);
            vector_push_back(vertijk, j);
            vector_push_back(vertijk, k);
        }}
    }

    b->dims[0] = b->nelements = nelements;
    if (b->data) free(b->data);
    b->data = malloc(b->ndims * nelements * sizeof(float));
    vertex = (float*)b->data;
    ijk = vertijk->data;

    if (grid->datatype_out == SDF_DATATYPE_REAL8) {
        double *x = grid->grids[0];
        double *y = grid->grids[1];
        double *z = grid->grids[2];
        for (i = 0; i < nelements; i++) {
            *vertex++ = x[*ijk++];
            *vertex++ = y[*ijk++];
            *vertex++ = z[*ijk++];
        }
    } else {
        float *x = grid->grids[0];
        float *y = grid->grids[1];
        float *z = grid->grids[2];
        for (i = 0; i < nelements; i++) {
            *vertex++ = x[*ijk++];
            *vertex++ = y[*ijk++];
            *vertex++ = z[*ijk++];
        }
    }

    vector_free(vertijk);

    // Scan faces in x-direction
    ii = 0;
    for (i = imin; i <= imax; i += nx) {
        for (k = 0; k < nz; k++) {
        for (j = 0; j < ny; j++) {
            vector_push_back(faces, vertex_index[IJK1(i,j  ,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i,j+1,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i,j+1,k+1)]);
            vector_push_back(faces, vertex_index[IJK1(i,j  ,k+1)]);
            vector_push_back(boundary, IJK(ii,j,k));
        }}
        ii = nx - 1;
    }

    // Scan faces in y-direction
    jj = 0;
    for (j = jmin; j <= jmax; j += ny) {
        for (k = 0; k < nz; k++) {
        for (i = 0; i < nx; i++) {
            vector_push_back(faces, vertex_index[IJK1(i  ,j,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j,k+1)]);
            vector_push_back(faces, vertex_index[IJK1(i  ,j,k+1)]);
            vector_push_back(boundary, IJK(i,jj,k));
        }}
        jj = ny - 1;
    }

    // Scan faces in z-direction
    kk = 0;
    for (k = kmin; k <= kmax; k += nz) {
        for (j = 0; j < ny; j++) {
        for (i = 0; i < nx; i++) {
            vector_push_back(faces, vertex_index[IJK1(i  ,j  ,k)]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j  ,k)]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j+1,k)]);
            vector_push_back(faces, vertex_index[IJK1(i  ,j+1,k)]);
            vector_push_back(boundary, IJK(i,j,kk));
        }}
        kk = nz - 1;
    }

    free(vertex_index);

    if (b->boundary_cells) free(b->boundary_cells);

    b->nfaces = boundary->size;
    vector_truncate(boundary);
    b->boundary_cells = boundary->data;
    free(boundary);

    vector_truncate(faces);
    b->node_list = faces->data;
    free(faces);

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_surface_mesh(sdf_file_t *h, sdf_block_t *b)
{
    int grp = b->nm + 1;
    sdf_block_t *grid = sdf_find_block_by_id(h, b->subblock->mesh_id);
    sdf_block_t *current_block = h->current_block;

    int i, j, k, i1, j1, k1;
    int nx, ny, nz;
    int gotmat, gotobst, nelements, face, which, cell, ocell;
    int *vertex_index, *obdata;
    float *vertex;

    vector_t *faces = vector_new();
    vector_t *boundary = vector_new();

    h->current_block = grid;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = b->subblock;
    sdf_stack_alloc(h, h->current_block);
    sdf_read_data(h);
    h->current_block = current_block;

    obdata = (int *)b->subblock->data;
    nx = b->subblock->local_dims[0] - 2;
    ny = b->subblock->local_dims[1] - 2;
    nz = b->subblock->local_dims[2] - 2;

    nelements = 0;
    vertex_index = (int*)malloc((nx+1)*(ny+1)*(nz+1)*sizeof(int));

    for (k = 0; k <= nz; k++) {
    for (j = 0; j <= ny; j++) {
    for (i = 0; i <= nx; i++) {
        vertex_index[IJK1(i,j,k)] = -1;
        gotmat = gotobst = 0;
        for (k1 = k-1; k1 <= k; k1++) {
        for (j1 = j-1; j1 <= j; j1++) {
        for (i1 = i-1; i1 <= i; i1++) {
            if (obdata[IJK2(i1,j1,k1)] == grp)
                gotobst = 1;
            else if (obdata[IJK2(i1,j1,k1)] == 0)
                gotmat = 1;
        }}}

        if (gotmat && gotobst)
            vertex_index[IJK1(i,j,k)] = nelements++;
    }}}

    b->dims[0] = b->nelements = nelements;
    if (b->data) free(b->data);
    b->data = malloc(b->ndims * nelements * sizeof(float));
    vertex = (float*)b->data;

    if (grid->datatype_out == SDF_DATATYPE_REAL8) {
        double *x = grid->grids[0];
        double *y = grid->grids[1];
        double *z = grid->grids[2];
        for (k = 0; k <= nz; k++) {
        for (j = 0; j <= ny; j++) {
        for (i = 0; i <= nx; i++) {
            if (vertex_index[IJK1(i,j,k)] != -1) {
                *vertex++ = x[i];
                *vertex++ = y[j];
                *vertex++ = z[k];
            }
        }}}
    } else {
        float *x = grid->grids[0];
        float *y = grid->grids[1];
        float *z = grid->grids[2];
        for (k = 0; k <= nz; k++) {
        for (j = 0; j <= ny; j++) {
        for (i = 0; i <= nx; i++) {
            if (vertex_index[IJK1(i,j,k)] != -1) {
                *vertex++ = x[i];
                *vertex++ = y[j];
                *vertex++ = z[k];
            }
        }}}
    }

    // Scan faces in x-direction
    for (k = 0; k <  nz; k++) {
    for (j = 0; j <  ny; j++) {
    for (i = 0; i <= nx; i++) {
        ocell = obdata[IJK2(i-1,j,k)];
        cell  = obdata[IJK2(i  ,j,k)];
        which = cell / grp;

        if (i == 0)
            face = (ocell == grp && cell == 0);
        else if (i == nx)
            face = (ocell == 0 && cell == grp);
        else
            face = (ocell == grp && cell == 0)
                    || (ocell == 0 && cell == grp);

        if (face) {
            vector_push_back(faces, vertex_index[IJK1(i,j  ,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i,j+1,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i,j+1,k+1)]);
            vector_push_back(faces, vertex_index[IJK1(i,j  ,k+1)]);
            vector_push_back(boundary, IJK(i-which,j,k));
        }
    }}}

    // Scan faces in y-direction
    for (k = 0; k <  nz; k++) {
    for (i = 0; i <  nx; i++) {
    for (j = 0; j <= ny; j++) {
        ocell = obdata[IJK2(i,j-1,k)];
        cell  = obdata[IJK2(i,j  ,k)];
        which = cell / grp;

        if (j == 0)
            face = (ocell == grp && cell == 0);
        else if (j == ny)
            face = (ocell == 0 && cell == grp);
        else
            face = (ocell == grp && cell == 0)
                    || (ocell == 0 && cell == grp);

        if (face) {
            vector_push_back(faces, vertex_index[IJK1(i  ,j,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j,k  )]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j,k+1)]);
            vector_push_back(faces, vertex_index[IJK1(i  ,j,k+1)]);
            vector_push_back(boundary, IJK(i,j-which,k));
        }
    }}}

    // Scan faces in z-direction
    for (j = 0; j <  ny; j++) {
    for (i = 0; i <  nx; i++) {
    for (k = 0; k <= nz; k++) {
        ocell = obdata[IJK2(i,j,k-1)];
        cell  = obdata[IJK2(i,j,k  )];
        which = cell / grp;

        if (k == 0)
            face = (ocell == grp && cell == 0);
        else if (k == nz)
            face = (ocell == 0 && cell == grp);
        else
            face = (ocell == grp && cell == 0)
                    || (ocell == 0 && cell == grp);

        if (face) {
            vector_push_back(faces, vertex_index[IJK1(i  ,j  ,k)]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j  ,k)]);
            vector_push_back(faces, vertex_index[IJK1(i+1,j+1,k)]);
            vector_push_back(faces, vertex_index[IJK1(i  ,j+1,k)]);
            vector_push_back(boundary, IJK(i,j,k-which));
        }
    }}}

    free(vertex_index);

    if (b->boundary_cells) free(b->boundary_cells);

    b->nfaces = boundary->size;
    vector_truncate(boundary);
    b->boundary_cells = boundary->data;
    free(boundary);

    vector_truncate(faces);
    b->node_list = faces->data;
    free(faces);

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_surface(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *mesh = sdf_find_block_by_id(h, b->mesh_id);
    sdf_block_t *current_block = h->current_block;
    int i, idx, sz;
    int *indexes = mesh->boundary_cells;
    char *ptr, *dptr;

    if (!b->subblock->data) {
        h->current_block = b->subblock;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }

    b->nelements_local = mesh->nfaces;
    b->datatype_out = b->subblock->datatype_out;
    sz = SDF_TYPE_SIZES[b->datatype_out];
    if (b->data) free(b->data);
    b->data = malloc(b->nelements_local * sz);
    ptr = b->data;
    dptr = b->subblock->data;
    for (i=0; i < b->nelements_local; i++) {
        idx = *indexes++;
        memcpy(ptr, dptr + idx * sz, sz);
        ptr += sz;
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_average(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *sb = b->subblock;
    sdf_block_t *current_block = h->current_block;
    int sz, i, j, k, is, js, ks;
    int nx, ny, nz, nxc, nyc, nzc, xs, ys, zs;

    ny = nz = nyc = nzc = 1;
    nx = sb->local_dims[0];
    nxc = b->local_dims[0];
    if (b->ndims > 1) {
        ny = sb->local_dims[1];
        nyc = b->local_dims[1];
    }
    if (b->ndims > 2) {
        nz = sb->local_dims[2];
        nzc = b->local_dims[2];
    }
    b->datatype_out = b->subblock->datatype_out;

    if (!sb->done_data) {
        h->current_block = sb;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }

    if (b->data) free(b->data);
    sz = SDF_TYPE_SIZES[b->datatype_out];
    b->data = malloc(b->nelements_local * sz);

    xs = sb->stagger&SDF_STAGGER_FACE_X ? 1 : 0;
    ys = sb->stagger&SDF_STAGGER_FACE_Y ? 1 : 0;
    zs = sb->stagger&SDF_STAGGER_FACE_Z ? 1 : 0;

#define IJKc(i,j,k) ((i) + nxc * ((j) + nyc * (k)))
    if (xs + ys + zs == 1) {
        if (b->datatype_out == SDF_DATATYPE_REAL8) {
            double *dat = sb->data;
            double *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.5 * (dat[IJK(i,j,k)] + dat[IJK(is,js,ks)]);
            }}}
        } else {
            float *dat = sb->data;
            float *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.5 * (dat[IJK(i,j,k)] + dat[IJK(is,js,ks)]);
            }}}
        }
    } else if (xs == 0) {
        if (b->datatype_out == SDF_DATATYPE_REAL8) {
            double *dat = sb->data;
            double *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0; i < nxc; i++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(i ,js,k )] +
                                           dat[IJK(i ,j ,ks)] +
                                           dat[IJK(i ,js,ks)]);
            }}}
        } else {
            float *dat = sb->data;
            float *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0; i < nxc; i++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(i ,js,k )] +
                                           dat[IJK(i ,j ,ks)] +
                                           dat[IJK(i ,js,ks)]);
            }}}
        }
    } else if (ys == 0) {
        if (b->datatype_out == SDF_DATATYPE_REAL8) {
            double *dat = sb->data;
            double *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0; j < nyc; j++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(is,j ,k )] +
                                           dat[IJK(i ,j ,ks)] +
                                           dat[IJK(is,j ,ks)]);
            }}}
        } else {
            float *dat = sb->data;
            float *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0; j < nyc; j++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(is,j ,k )] +
                                           dat[IJK(i ,j ,ks)] +
                                           dat[IJK(is,j ,ks)]);
            }}}
        }
    } else if (zs == 0) {
        if (b->datatype_out == SDF_DATATYPE_REAL8) {
            double *dat = sb->data;
            double *res = b->data;
            for (k=0; k < nzc; k++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(is,j ,k )] +
                                           dat[IJK(i ,js,k )] +
                                           dat[IJK(is,js,k )]);
            }}}
        } else {
            float *dat = sb->data;
            float *res = b->data;
            for (k=0; k < nzc; k++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.25 * (dat[IJK(i ,j ,k )] +
                                           dat[IJK(is,j ,k )] +
                                           dat[IJK(i ,js,k )] +
                                           dat[IJK(is,js,k )]);
            }}}
        }
    } else {
        if (b->datatype_out == SDF_DATATYPE_REAL8) {
            double *dat = sb->data;
            double *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.125 * (dat[IJK(i ,j ,k )] +
                                            dat[IJK(is,j ,k )] +
                                            dat[IJK(i ,js,k )] +
                                            dat[IJK(is,js,k )] +
                                            dat[IJK(i ,j ,ks)] +
                                            dat[IJK(is,j ,ks)] +
                                            dat[IJK(i ,js,ks)] +
                                            dat[IJK(is,js,ks)]);
            }}}
        } else {
            float *dat = sb->data;
            float *res = b->data;
            for (k=0, ks=zs; k < nzc; k++, ks++) {
            for (j=0, js=ys; j < nyc; j++, js++) {
            for (i=0, is=xs; i < nxc; i++, is++) {
                res[IJKc(i,j,k)] = 0.125 * (dat[IJK(i ,j ,k )] +
                                            dat[IJK(is,j ,k )] +
                                            dat[IJK(i ,js,k )] +
                                            dat[IJK(is,js,k )] +
                                            dat[IJK(i ,j ,ks)] +
                                            dat[IJK(is,j ,ks)] +
                                            dat[IJK(i ,js,ks)] +
                                            dat[IJK(is,js,ks)]);
            }}}
        }
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_grid_component(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *mesh = sdf_find_block_by_id(h, b->mesh_id);
    sdf_block_t *current_block = h->current_block;

    if (!b->grids) {
        b->ngrids = 1;
        b->grids = calloc(b->ngrids, sizeof(float*));
    }

    if (!mesh->done_data) {
        h->current_block = mesh;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }

    if (!h->mmap && b->data)
        free(b->data);
    b->data = b->grids[0] = mesh->grids[b->nm];
    b->datatype_out = mesh->datatype_out;
    if (b->blocktype == SDF_BLOCKTYPE_POINT_DERIVED)
        b->local_dims[0] = b->nelements_local = mesh->nelements_local;
    else
        b->local_dims[0] = b->nelements_local = mesh->local_dims[b->nm];
    return b;
}



static sdf_block_t *sdf_callback_cartesian_grid(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *mesh = b->subblock;
    long long i, j, k;
    double *x8p, *y8p, *z8p, *rr8p, *theta8p, *phi8p, *zz8p;
    float *x4p, *y4p, *z4p, *rr4p, *theta4p, *phi4p, *zz4p;
    double rr, theta, phi, zz, sin_theta, cos_theta, sin_phi, cos_phi;
    double sin_theta_cos_phi, sin_theta_sin_phi;

    b->datatype_out = b->subblock->datatype_out;
    memcpy(b->local_dims, mesh->local_dims,
           b->ndims * sizeof(b->local_dims[0]));
    b->nelements_local = 1;
    for (i = 0; i < b->ndims; i++)
        b->nelements_local *= b->local_dims[i];

    sdf_stack_alloc(h, b);

    if (!mesh->done_data)
        sdf_helper_read_data(h, mesh);

    if (b->datatype_out == SDF_DATATYPE_REAL8) {
        x8p = (double*)b->grids[0];
        y8p = (double*)b->grids[1];
        z8p = (double*)b->grids[2];

        if (b->geometry == SDF_GEOMETRY_CYLINDRICAL) {
            zz8p = (double*)mesh->grids[2];
            for (k=0; k < b->local_dims[2]; k++) {
                theta8p = (double*)mesh->grids[1];
                zz = *zz8p;
                for (j=0; j < b->local_dims[1]; j++) {
                    rr8p = (double*)mesh->grids[0];
                    theta = *theta8p;
                    cos_theta = cos(theta);
                    sin_theta = sin(theta);
                    for (i=0; i < b->local_dims[0]; i++) {
                        rr = *rr8p;

                        *x8p = rr * cos_theta;
                        *y8p = rr * sin_theta;
                        *z8p = zz;

                        rr8p++;
                        x8p++;
                        y8p++;
                        z8p++;
                    }
                    theta8p++;
                }
                zz8p++;
            }
        } else {
            phi8p = (double*)mesh->grids[2];
            for (k=0; k < b->local_dims[2]; k++) {
                theta8p = (double*)mesh->grids[1];
                phi = *phi8p;
                cos_phi = cos(phi);
                sin_phi = sin(phi);
                for (j=0; j < b->local_dims[1]; j++) {
                    rr8p = (double*)mesh->grids[0];
                    theta = *theta8p;
                    cos_theta = cos(theta);
                    sin_theta = sin(theta);
                    sin_theta_cos_phi = sin_theta * cos_phi;
                    sin_theta_sin_phi = sin_theta * sin_phi;
                    for (i=0; i < b->local_dims[0]; i++) {
                        rr = *rr8p;

                        *x8p = rr * sin_theta_cos_phi;
                        *y8p = rr * sin_theta_sin_phi;
                        *z8p = rr * cos_theta;

                        rr8p++;
                        x8p++;
                        y8p++;
                        z8p++;
                    }
                    theta8p++;
                }
                phi8p++;
            }
        }
    } else {
        x4p = (float*)b->grids[0];
        y4p = (float*)b->grids[1];
        z4p = (float*)b->grids[2];

        if (b->geometry == SDF_GEOMETRY_CYLINDRICAL) {
            zz4p = (float*)mesh->grids[2];
            for (k=0; k < b->local_dims[2]; k++) {
                theta4p = (float*)mesh->grids[1];
                zz = *zz4p;
                for (j=0; j < b->local_dims[1]; j++) {
                    rr4p = (float*)mesh->grids[0];
                    theta = *theta4p;
                    cos_theta = cos(theta);
                    sin_theta = sin(theta);
                    for (i=0; i < b->local_dims[0]; i++) {
                        rr = *rr4p;

                        *x4p = rr * cos_theta;
                        *y4p = rr * sin_theta;
                        *z4p = zz;

                        rr4p++;
                        x4p++;
                        y4p++;
                        z4p++;
                    }
                    theta4p++;
                }
                zz4p++;
            }
        } else {
            phi4p = (float*)mesh->grids[2];
            for (k=0; k < b->local_dims[2]; k++) {
                theta4p = (float*)mesh->grids[1];
                phi = *phi4p;
                cos_phi = cos(phi);
                sin_phi = sin(phi);
                for (j=0; j < b->local_dims[1]; j++) {
                    rr4p = (float*)mesh->grids[0];
                    theta = *theta4p;
                    cos_theta = cos(theta);
                    sin_theta = sin(theta);
                    sin_theta_cos_phi = sin_theta * cos_phi;
                    sin_theta_sin_phi = sin_theta * sin_phi;
                    for (i=0; i < b->local_dims[0]; i++) {
                        rr = *rr4p;

                        *x4p = rr * sin_theta_cos_phi;
                        *y4p = rr * sin_theta_sin_phi;
                        *z4p = rr * cos_theta;

                        rr4p++;
                        x4p++;
                        y4p++;
                        z4p++;
                    }
                    theta4p++;
                }
                phi4p++;
            }
        }
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_face_grid(sdf_file_t *h, sdf_block_t *b)
{
    int i, n, sz;
    sdf_block_t *old = b->subblock;

    if (b->done_data) return b;

    if (!old->grids)
        sdf_helper_read_data(h, old);

    memcpy(b->local_dims, old->local_dims, 3 * sizeof(*b->local_dims));

    if (!b->grids) {
        b->ngrids = 3;
        b->grids = calloc(b->ngrids, sizeof(float*));
    }

    if (old->blocktype == SDF_BLOCKTYPE_LAGRANGIAN_MESH) {
        int i0, i1, ii0, ii1, nm, *ii, f0, f1;
        int64_t *nn, irem, j;

        b->nelements = 1;
        b->nelements_local = 1;
        for (i = 0; i < b->ndims; i++) {
            b->dims[i] = old->dims[i];
            b->local_dims[i] = old->local_dims[i];
            if (i == b->stagger) {
                b->dims[i]++;
                b->local_dims[i]++;
            }
            b->nelements *= b->dims[i];
            b->nelements_local *= b->local_dims[i];
        }

        sz = b->nelements_local * SDF_TYPE_SIZES[b->datatype_out];
        for (i = 0; i < b->ndims; i++) {
            if (b->grids[i])
                free(b->grids[i]);
            b->grids[i] = malloc(sz);
        }

        nn = malloc(b->ndims * sizeof(nn[0]));
        ii = malloc(b->ndims * sizeof(ii[0]));

        nn[0] = 1;
        for (i = 1; i < b->ndims; i++)
            nn[i] = nn[i-1] * b->local_dims[i-1];

        for (i = 0; i < b->nelements_local; i++) {
            j = b->ndims - 1;
            irem = i;
            for (n = 0; n < b->ndims; n++) {
                ii[j] = irem / nn[j];
                irem -= ii[j] * nn[j];
                j--;
            }

            i0 = i1 = 0;
            f0 = f1 = nm = 1;
            for (n = 0; n < b->ndims; n++) {
                ii0 = ii1 = ii[n];
                if (n == b->stagger) {
                    ii0--;
                    if (ii0 < 0) {
                        ii0 = 1;
                        f0 = -1;
                        f1 = 3;
                    }
                    if (ii1 > old->local_dims[n] - 1) {
                        ii1 = old->local_dims[n] - 2;
                        f0 = 3;
                        f1 = -1;
                    }
                }
                i0 += ii0 * nm;
                i1 += ii1 * nm;
                nm *= old->local_dims[n];
            }

            if (b->datatype_out == SDF_DATATYPE_REAL8) {
                double x0, x1, *x;

                for (n = 0; n < b->ndims; n++) {
                    x = old->grids[n];
                    x0 = f0 * x[i0];
                    x1 = f1 * x[i1];
                    x = b->grids[n];
                    x[i] = 0.5 * (x0 + x1);
                }
            } else if (b->datatype_out == SDF_DATATYPE_REAL4) {
                float x0, x1, *x;

                for (n = 0; n < b->ndims; n++) {
                    x = old->grids[n];
                    x0 = f0 * x[i0];
                    x1 = f1 * x[i1];
                    x = b->grids[n];
                    x[i] = 0.5 * (x0 + x1);
                }
            }
        }

        free(nn);
        free(ii);

        b->done_data = 1;

        return b;
    }

    n = b->stagger;
    b->local_dims[n] += 1;
    b->nelements = 0;
    b->nelements_local = 0;
    for (i = 0; i < b->ndims; i++) {
        b->nelements += b->dims[i];
        b->nelements_local += b->local_dims[i];

        sz = b->local_dims[i] * SDF_TYPE_SIZES[b->datatype_out];
        if (b->grids[i])
            free(b->grids[i]);
        b->grids[i] = malloc(sz);
        if (i != n)
            memcpy(b->grids[i], old->grids[i], sz);
    }

    if (b->datatype_out == SDF_DATATYPE_REAL8) {
        double *oldx_ptr = (double*)old->grids[n];
        double *newx_ptr = (double*)b->grids[n];
        double x0, x1;
        x0 = *oldx_ptr++;
        x1 = *oldx_ptr;
        *newx_ptr++ = 0.5 * (3 * x0 - x1);

        for (i = 0; i < old->local_dims[n]-1; i++) {
            *newx_ptr++ = 0.5 * (x0 + *oldx_ptr);
            x0 = *oldx_ptr++;
        }

        x0 = *(oldx_ptr-1);
        x1 = *(oldx_ptr-2);
        *newx_ptr = 0.5 * (3 * x0 - x1);
    } else {
        float *oldx_ptr = (float*)old->grids[n];
        float *newx_ptr = (float*)b->grids[n];
        float x0, x1;
        x0 = *oldx_ptr++;
        x1 = *oldx_ptr;
        *newx_ptr++ = 0.5 * (3 * x0 - x1);

        for (i = 0; i < old->local_dims[n]-1; i++) {
            *newx_ptr++ = 0.5 * (x0 + *oldx_ptr);
            x0 = *oldx_ptr++;
        }

        x0 = *(oldx_ptr-1);
        x1 = *(oldx_ptr-2);
        *newx_ptr = 0.5 * (3 * x0 - x1);
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_cpu_mesh(sdf_file_t *h, sdf_block_t *b)
{
    int i, n, sz, np, nx;
    int i0, i1, idx;
    int *index;
    char *x, *xmesh;
    sdf_block_t *split = b->subblock;
    sdf_block_t *mesh = b->subblock2;
    sdf_block_t *current_block = h->current_block;
#ifdef PARALLEL
    void *buf;
#endif

    if (b->done_data) return b;

    if (!split->done_data) {
        h->current_block = split;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }

    if (!mesh->done_data) {
        h->current_block = mesh;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }
#ifdef PARALLEL
    memcpy(b->cpu_split, mesh->cpu_split, SDF_MAXDIMS * sizeof(*b->cpu_split));
#endif

    b->datatype_out = mesh->datatype_out;
    if (!b->grids) {
        b->ngrids = 3;
        b->grids = calloc(b->ngrids, sizeof(float*));
    }
    sz = SDF_TYPE_SIZES[b->datatype_out];

    if (split->geometry == 1) {
        index = split->data;
        np = 0;

        b->nelements_local = 1;
        for (n=0; n < b->ndims; n++) {
            xmesh = mesh->grids[n];

            nx = b->local_dims[n] = b->dims[n];
            x = calloc(nx, sz);
#ifdef PARALLEL
            i0 = mesh->starts[n];
#else
            i0 = 0;
#endif
            i1 = mesh->local_dims[n] - 1;
            for (i = 1; i < nx-1; i++) {
                idx = index[np++] - i0;
                if (idx > i1) break;
                if (idx >= 0)
                    memcpy(x+sz*i, xmesh+sz*idx, sz);
            }
            if (i0 == 0)
                memcpy(x, xmesh, sz);
            idx = mesh->local_dims[n] - 1;
            if (mesh->dims[n] == mesh->local_dims[n] + i0)
                memcpy(x+sz*(nx-1), xmesh+sz*idx, sz);

#ifdef PARALLEL
            buf = malloc(nx*sz);
            MPI_Reduce(x, buf, nx*sz, MPI_BYTE, MPI_BOR, 0, h->comm);
            free(x);
            x = buf;

            if (h->rank)
                free(x);
            else
#endif
                if (b->grids[n]) free(b->grids[n]);
                b->grids[n] = x;
                b->nelements_local *= nx;
        }
        for (n=b->ndims; n < 3; n++) {
            b->local_dims[n] = 1;
            if (!b->grids[n])
                b->grids[n] = calloc(1, sz);
        }

#ifdef PARALLEL
        if (h->rank) {
            for (n=0; n < 3; n++) {
                b->local_dims[n] = 1;
                if (!b->grids[n])
                    b->grids[n] = calloc(1, sz);
            }
            b->nelements_local = 1;
        }
#endif
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_current_cpu_mesh(sdf_file_t *h, sdf_block_t *b)
{
    int n, nx, sz, idx, i0 = 0, pmax = 1;
    char *x;
    sdf_block_t *mesh = b->subblock;
    sdf_block_t *current_block = h->current_block;
#ifdef PARALLEL
    int rem = h->rank;
    char *buf;
#endif

    if (b->done_data) return b;

    if (!mesh->done_data) {
        h->current_block = mesh;
        sdf_stack_alloc(h, h->current_block);
        sdf_read_data(h);
        h->current_block = current_block;
    }
#ifdef PARALLEL
    memcpy(b->cpu_split, mesh->cpu_split, SDF_MAXDIMS * sizeof(*b->cpu_split));
#endif

    b->datatype_out = mesh->datatype_out;
    if (!b->grids) {
        b->ngrids = 3;
        b->grids = calloc(b->ngrids, sizeof(float*));
    }
    sz = SDF_TYPE_SIZES[b->datatype_out];

    b->nelements_local = 0;
    for (n = 0; n < b->ndims; n++) {
#ifdef PARALLEL
        nx = mesh->cpu_split[n];
        i0 = rem%nx;
        rem = rem / nx;
        b->local_dims[n] = ++nx;
        pmax = (mesh->proc_max[n] == MPI_PROC_NULL);
#else
        nx = b->local_dims[n] = 2;
#endif
        idx = mesh->ngb[2*n];
        x = calloc(nx, sz);
        memcpy(x+sz*i0, (char*)mesh->grids[n] + sz*idx, sz);
        if (pmax) {
            idx = mesh->local_dims[n] - 1;
            memcpy(x+sz*(i0+1), (char*)mesh->grids[n]+sz*idx, sz);
        }
        b->nelements_local += nx;
#ifdef PARALLEL
        buf = malloc(nx*sz);
        MPI_Reduce(x, buf, nx*sz, MPI_BYTE, MPI_BOR, 0, h->comm);
        free(x);
        x = buf;

        if (h->rank)
            free(x);
        else
#endif
        if (b->grids[n]) free(b->grids[n]);
        b->grids[n] = x;
    }

    if (h->rank) {
        b->nelements_local = 1;
        nx = 0;
    } else
        nx = b->ndims;

    for (n = nx; n < 3; n++) {
        b->local_dims[n] = 1;
        if (!b->grids[n])
            b->grids[n] = calloc(1, sz);
    }

    b->subblock->nelements_local = b->nelements_local;
    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_cpu_data(sdf_file_t *h, sdf_block_t *b)
{
    int n, *var = b->data;

    if (b->done_data) return b;

    for (n=0; n < b->nelements_local; n++) var[n] = n;

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_station_time(sdf_file_t *h, sdf_block_t *b)
{
    int i, n, idx, sz, data_offset0, varoffset;
    float *r4, dt4, time4;
    double *r8, dt8, time8;
    char *ptr;
    sdf_block_t *block, *current_block, *mesh = b->subblock;

    if (b->done_data) return b;

    if (mesh) {
        if (!mesh->done_data) {
            current_block = h->current_block;
            h->current_block = mesh;
            sdf_stack_alloc(h, h->current_block);
            sdf_read_data(h);
            h->current_block = current_block;
        }

        if (b->datatype_out == SDF_DATATYPE_REAL4)
            b->data = (float*)mesh->data + b->offset;
        else
            b->data = (double*)mesh->data + b->offset;

        if (!b->grids) {
            b->ngrids = 1;
            b->grids = calloc(b->ngrids, sizeof(float*));
        }
        b->grids[0] = b->data;

        b->done_data = 1;

        return b;
    }

    sz = SDF_TYPE_SIZES[b->datatype_out];
    if (!b->data) b->data = malloc(b->nelements_local * sz);

    if (b->grids) {
        for (i=0; i < b->ngrids; i++)
            if (b->grids[i])
                free(b->grids[i]);
        free(b->grids);
    }
    b->ngrids = 1;
    b->grids = calloc(b->ngrids, sizeof(float*));
    b->grids[0] = b->data;

    // Find first station block
    block = h->blocklist;
    for (i = 0; i < h->nblocks; i++) {
        if (block->blocktype == SDF_BLOCKTYPE_STATION) break;
        block = block->next;
    }

    // If there is a fixed time increment then generate the data, otherwise
    // read it from file.
    if (block->time_increment > 0.0) {
        if (b->datatype_out == SDF_DATATYPE_REAL4) {
            r4 = (float*)b->data;
            dt4 = block->time_increment;
            time4 = block->time;
            for (n = 0; n < b->nelements; n++) {
                *r4++ = time4;
                time4 += dt4;
            }
        } else {
            r8 = (double*)b->data;
            dt8 = block->time_increment;
            time8 = block->time;
            for (n = 0; n < b->nelements; n++) {
                *r8++ = time8;
                time8 += dt8;
            }
        }
    } else {
        ptr = b->data;
        idx = i;
        data_offset0 = 0;
        varoffset = 0;
        if (block->step_increment <= 0) {
            data_offset0 += SDF_TYPE_SIZES[block->variable_types[varoffset]];
            varoffset++;
        }

        for (i = idx; i < h->nblocks; i++) {
            if (block->blocktype == SDF_BLOCKTYPE_STATION) {
                h->current_location = block->data_location + data_offset0;

                for (n = 0; n < block->nelements; n++) {
                    sdf_seek(h);
                    sdf_read_bytes(h, ptr, sz);
                    ptr += sz;
                    h->current_location += block->type_size;
                }
            }
            block = block->next;
        }
    }

    b->done_data = 1;

    return b;
}



static sdf_block_t *sdf_callback_station(sdf_file_t *h, sdf_block_t *b)
{
    int i, j, k, sz, len, vidx;
    int data_offset;
    list_t *station_blocks;
    char *varid, *ptr = b->data;
    sdf_block_t *block;

    if (b->done_data) return b;

    // Build list of all station blocks in the file
    list_init(&station_blocks);
    block = h->blocklist;
    for (i = 0; i < h->nblocks; i++) {
        if (block->blocktype == SDF_BLOCKTYPE_STATION)
            list_append(station_blocks, block);
        block = block->next;
    }

    len = strlen(b->station_id);
    varid = &b->id[len+1];
    sz = SDF_TYPE_SIZES[b->datatype_out];
    if (!b->data) b->data = malloc(b->nelements_local * sz);
    ptr = b->data;

    // Find station blocks containing this station id.
    block = list_start(station_blocks);

    for (i = 0; i < station_blocks->count; i++) {
        for (j = 0; j < block->nstations; j++) {
            if (strncmp(block->station_ids[j], b->station_id, h->id_length))
                continue;
            if (block->station_move[j] < 0) continue;

            // Found station block. Now find the variable.
            vidx = 0;
            for (k = 0; k < j; k++) vidx += block->station_nvars[k];

            for (k = 0; k < block->station_nvars[j]; k++, vidx++) {
                if (strncmp(block->variable_ids[vidx], varid,
                        h->id_length) == 0) break;
            }

            // Now read the data.
            data_offset = 0;
            for (k = 0; k < vidx; k++)
                data_offset += SDF_TYPE_SIZES[block->variable_types[k]];

            h->current_location = block->data_location + data_offset;
            for (k = 0; k < block->nelements; k++) {
                sdf_seek(h);
                sdf_read_bytes(h, ptr, sz);
                ptr += sz;
                h->current_location += block->type_size;
            }
        }
        block = list_next(station_blocks);
    }

    list_destroy(&station_blocks);

    b->done_data = 1;

    return b;
}



static void add_surface_variables(sdf_file_t *h, sdf_block_t *surf,
        sdf_block_t **append_ptr, sdf_block_t **append_tail_ptr,
        int *nappend_ptr)
{
    sdf_block_t *b, *next, *append, *append_tail;
    int nappend = *nappend_ptr;
    size_t len1, len2;
    char *str, *name1, *name2;

    append = *append_ptr;
    append_tail = *append_tail_ptr;

    next = h->blocklist;
    while (next) {
        b = next;
        next = b->next;

        if ((b->blocktype != SDF_BLOCKTYPE_PLAIN_VARIABLE
                && b->blocktype != SDF_BLOCKTYPE_PLAIN_DERIVED)
                || b->dont_display || b->stagger != SDF_STAGGER_CELL_CENTRE)
            continue;

        APPEND_BLOCK(append);
        nappend++;
        append_tail = append;

        name1 = surf->id;
        name2 = b->id;
        len1 = strlen(name1);
        len2 = strlen(name2);
        str = (char*)malloc(len1 + len2 + 2);
        memcpy(str, name1, len1);
        str[len1] = '/';
        memcpy(str+len1+1, name2, len2);
        str[len1+len2+1] = '\0';
        sdf_unique_id(h, str);
        append->id = str;

        name1 = surf->name;
        name2 = b->name;
        len1 = strlen(name1);
        len2 = strlen(name2);
        str = (char*)malloc(len1 + len2 + 2);
        memcpy(str, name1, len1);
        str[len1] = '/';
        memcpy(str+len1+1, name2, len2);
        str[len1+len2+1] = '\0';
        append->name = str;

        append->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
        append->derived = 1;
        append->datatype = b->datatype;
        append->datatype_out = b->datatype_out;
        append->ndims = b->ndims;
        append->mult = b->mult;
        append->stagger = b->stagger;
        append->subblock = b;
        memcpy(append->dims, b->dims, 3 * sizeof(b->dims[0]));

        SDF_SET_ENTRY_ID(append->units, b->units);
        SDF_SET_ENTRY_ID(append->mesh_id, surf->id);
        append->done_header = 1;
        // Hack to prevent storage being allocated for this variable.
        append->dont_allocate = 1;
        append->populate_data = sdf_callback_surface;

        sdf_hash_block(h, append);
    }

    *nappend_ptr = nappend;
    *append_tail_ptr = append_tail;
    *append_ptr = append;
}



static void add_station_variables(sdf_file_t *h, sdf_block_t **append,
        sdf_block_t **append_tail, int *nappend)
{
    sdf_block_t *b, *sb, *global_mesh, *mesh, *station = *append;
    char *meshid = "global_station/time";
    char *meshname = "Station/Time";
    char *str;
    int varoffset, mesh_datatype, var, i, j, n, is, nelements, nsofar;
    int *nelements_array;
    list_t *station_blocks;

    varoffset = 0;
    mesh_datatype = SDF_DATATYPE_REAL8;
    if (station->step_increment <= 0) varoffset++;

    if (station->time_increment <= 0.0) {
        varoffset++;
        mesh_datatype = station->variable_types[0];
    }

    // Build list of all station blocks in the file
    list_init(&station_blocks);
    b = h->blocklist;
    for (i = 0; i < h->nblocks; i++) {
        if (b->blocktype == SDF_BLOCKTYPE_STATION)
            list_append(station_blocks, b);
        b = b->next;
    }

    // Build list of nelements for each station block in the file
    nelements_array = calloc(station_blocks->count, sizeof(*nelements_array));
    b = list_start(station_blocks);
    for (i = 0; i < station_blocks->count; i++) {
        nelements_array[i] = b->nelements;
        b = list_next(station_blocks);
    }

    // Reverse step over the nelements_array and fill with a cumulative
    // sum. nelements_array will then contain the number of time entries
    // for the current station block and all which come after it in the file.
    nelements = 0;
    for (i = station_blocks->count-1; i >= 0; i--) {
        nelements += nelements_array[i];
        nelements_array[i] = nelements;
    }

    /* Add global time mesh */
    APPEND_BLOCK(*append);
    (*nappend)++;
    *append_tail = *append;
    global_mesh = mesh = *append;

    SDF_SET_ENTRY_ID(mesh->id, meshid);
    SDF_SET_ENTRY_ID(mesh->units, "s");
    SDF_SET_ENTRY_STRING(mesh->name, meshname);
    mesh->blocktype = SDF_BLOCKTYPE_PLAIN_MESH;
    mesh->derived = 1;
    mesh->ndim_units = mesh->ndim_labels = mesh->ndims = 1;
    mesh->dim_units = calloc(mesh->ndims, sizeof(char*));
    mesh->dim_labels = calloc(mesh->ndims, sizeof(char*));
    SDF_SET_ENTRY_ID(mesh->dim_units[0], "s");
    SDF_SET_ENTRY_ID(mesh->dim_labels[0], "Time");
    mesh->populate_data = sdf_callback_station_time;
    mesh->nelements_local = mesh->nelements = station->nelements;
    mesh->local_dims[0] = mesh->dims[0] = mesh->nelements;
    mesh->datatype = mesh->datatype_out = mesh_datatype;
    mesh->option = station->ndims;

    sdf_hash_block(h, mesh);

    /* Add per-station-block time meshes */
    nsofar = 0;
    b = list_start(station_blocks);
    for (i = 0; i < station_blocks->count; i++) {
        APPEND_BLOCK(*append);
        (*nappend)++;
        *append_tail = *append;
        mesh = *append;

        str = strcat_alloc(meshid, b->id);
        sdf_unique_id(h, str);
        mesh->id = str;
        mesh->name = strcat_alloc(meshname, b->name);
        SDF_SET_ENTRY_ID(mesh->units, "s");
        mesh->blocktype = SDF_BLOCKTYPE_PLAIN_MESH;
        mesh->derived = 1;
        mesh->ndim_units = mesh->ndim_labels = mesh->ndims = 1;
        mesh->dim_units = calloc(mesh->ndims, sizeof(char*));
        mesh->dim_labels = calloc(mesh->ndims, sizeof(char*));
        SDF_SET_ENTRY_ID(mesh->dim_units[0], "s");
        SDF_SET_ENTRY_ID(mesh->dim_labels[0], "Time");
        mesh->populate_data = sdf_callback_station_time;
        mesh->nelements_local = mesh->nelements = nelements_array[i];
        mesh->local_dims[0] = mesh->dims[0] = mesh->nelements;
        mesh->datatype = mesh->datatype_out = mesh_datatype;
        mesh->subblock = global_mesh;
        mesh->offset = nsofar;
        mesh->dont_own_data = 1;
        mesh->option = b->ndims;

        nsofar += b->nelements;
        b = list_next(station_blocks);

        sdf_hash_block(h, mesh);
    }

    free(nelements_array);

    var = varoffset;
    for (i = 0; i < station->nstations; i++) {
        // Sum nelements for all station blocks containing this station
        nelements = 0;
        sb = list_start(station_blocks);
        for (is = 0; is < station_blocks->count; is++) {
            for (n = 0; n < sb->nstations; n++) {
                if (strncmp(sb->station_ids[n], station->station_ids[i],
                        h->id_length) == 0) {
                    if (sb->station_move[n] >= 0)
                        nelements += sb->nelements;
                    break;
                }
            }
            sb = list_next(station_blocks);
        }
        for (j = 0; j < station->station_nvars[i]; j++) {
            APPEND_BLOCK(*append);
            (*nappend)++;
            *append_tail = *append;
            b = *append;

            str = strcat_alloc(station->station_ids[i],
                    station->variable_ids[var]);
            sdf_unique_id(h, str);
            b->id = str;
            b->name = strcat_alloc(station->station_names[i],
                    station->material_names[var]);
            b->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
            b->derived = 1;
            SDF_SET_ENTRY_ID(b->units, station->dim_units[var]);
            SDF_SET_ENTRY_ID(b->station_id, station->station_ids[i]);
            b->ndims = 1;
            b->populate_data = sdf_callback_station;
            b->datatype = b->datatype_out = station->variable_types[var];
            b->nelements_local = b->nelements = nelements;
            b->local_dims[0] = b->dims[0] = b->nelements;
            b->option = station->ndims;
            var++;

            // Find the first station block which contains this station id
            nsofar = 0;
            sb = list_start(station_blocks);
            for (is = 0; is < station_blocks->count; is++) {
                for (n = 0; n < sb->nstations; n++) {
                    if (strncmp(sb->station_ids[n], b->station_id,
                            h->id_length) != 0) continue;
                    if (sb->station_move[n] < 0) continue;
                    str = strcat_alloc(meshid, sb->id);
                    sdf_unique_id(h, str);
                    b->mesh_id = str;
                    break;
                }
                if (b->mesh_id) break;
                nsofar += sb->nelements;
                sb = list_next(station_blocks);
            }
            b->offset = nsofar;

            sdf_hash_block(h, b);
        }
    }

    list_destroy(&station_blocks);
/*
    APPEND_BLOCK(*append);
    (*nappend)++;
    *append_tail = *append;
    b = *append;

    b->blocktype == SDF_BLOCKTYPE_STATION_DERIVED;
    SDF_SET_ENTRY_ID(b->id, "stat");
*/
}



/* hash: compute hash value of string */
static unsigned int hash(char *str)
{
   unsigned int h;
   unsigned char *p;

   h = 0;
   for (p = (unsigned char*)str; *p != '\0'; p++)
      h = 37 * h + *p;
   return h; // or, h % ARRAY_SIZE;
}



static void add_global_station(sdf_file_t *h, sdf_block_t **append,
        sdf_block_t **append_tail, int *nappend)
{
    sdf_block_t *b, *next, *new;
    int nextra, nstat_total, nstat_max = 2;
    int i, j, m, n, varoffset, nelements;
    int nidx; // variable index for new block
    int bidx; // variable index for current station block
    int *extra_id;
    unsigned int *hash_list, hash_entry;
    char found, *found_id, *ctmp;
    list_t *station_blocks;

    // Create the new derived block and add it to the list
    APPEND_BLOCK(*append);
    (*nappend)++;
    *append_tail = *append;
    new = *append;

    new->blocktype = SDF_BLOCKTYPE_STATION_DERIVED;
    new->derived = 1;
    SDF_SET_ENTRY_ID(new->id, "global_stations");
    SDF_SET_ENTRY_STRING(new->name, "Global Stations");
    sdf_hash_block(h, new);

    nelements = 0;
    extra_id = calloc(nstat_max, sizeof(*extra_id));
    found_id = malloc(sizeof(*found_id));
    list_init(&station_blocks);

    // Build list of globally unique station_ids and for each
    // station block, assign the station indexes into this global array
    next = h->blocklist;
    for (i = 0; i < h->nblocks; i++) {
        h->current_block = b = next;
        next = b->next;
        if (b->blocktype != SDF_BLOCKTYPE_STATION) continue;

        // Sanity check. List of station_ids must be unique
        // First build list of hashes for speed of comparison
        hash_list = malloc(b->nstations * sizeof(unsigned int));
        for (n = 0; n < b->nstations; n++) {
            hash_entry = hash(b->station_ids[n]);
            hash_list[n] = hash_entry;
            for (m = 0; m < n; m++) {
                if (hash_entry == hash_list[m]) {
                    fprintf(stderr, "*** ERROR ***\n");
                    fprintf(stderr, "Duplicate station id found: "
                            "%s in block %s\n", b->station_ids[n], b->id);
                    exit(1);
                }
            }
        }
        free(hash_list);

        nelements += b->nelements;
        list_append(station_blocks, b);

        if (b->nstations > nstat_max) {
            nstat_max = b->nstations * 11 / 10 + 2;
            free(extra_id);
            extra_id = calloc(nstat_max, sizeof(*extra_id));
        }

        b->station_index = calloc(b->nstations, sizeof(*b->station_index));
        memset(found_id, 0, new->nstations);
        nextra = 0;
        for (n = 0; n < b->nstations; n++) {
            found = 0;
            for (m = 0; m < new->nstations; m++) {
                if (found_id[m]) continue;
                if (strncmp(b->station_ids[n], new->station_ids[m],
                        h->id_length)) continue;
                b->station_index[n] = m;
                found = 1;
                found_id[m] = 1;
                break;
            }
            if (!found) {
                extra_id[nextra] = n;
                b->station_index[n] = new->nstations + n;
                nextra++;
            }
        }

        if (nextra == 0) continue;

        nstat_total = new->nstations + nextra;

        if (new->nstations == 0) {
            new->station_ids = calloc(nstat_total, sizeof(char*));
            new->nstation_ids = nstat_total;
        } else {
            ctmp = malloc(new->nstations);
            memcpy(ctmp, new->station_ids, new->nstations * sizeof(char*));
            free(new->station_ids);
            new->station_ids = calloc(nstat_total, sizeof(char*));
            new->nstation_ids = nstat_total;
            memcpy(new->station_ids, ctmp, new->nstations * sizeof(char*));
            free(ctmp);
        }

        free(found_id);
        found_id = malloc(nstat_total * sizeof(char));

        m = new->nstations;
        for (n = 0; n < nextra; n++) {
            SDF_SET_ENTRY_ID(new->station_ids[m], b->station_ids[extra_id[n]]);
            m++;
        }

        new->nstations = nstat_total;
    }


    // Populate each station_id with its associated data
    if (new->nstations > 0) {
        // Assign the station block variables
        b = list_start(station_blocks);
        new->step = b->step;
        new->step_increment = b->step_increment;
        new->time = b->time;
        new->time_increment = b->time_increment;
        new->use_mult = b->use_mult;
        new->ndims = b->ndims;

        varoffset = 0;
        if (new->step_increment <= 0) varoffset++;
        if (new->time_increment <= 0.0) varoffset++;
        new->nvariables = varoffset;
        new->nelements_local = new->nelements = nelements;
        new->local_dims[0] = new->nelements_local;
        new->nstation_names = new->nstations;

        // Allocate the stations
        new->station_names =
            calloc(new->nstations, sizeof(*new->station_names));
        new->station_nvars =
            calloc(new->nstations, sizeof(*new->station_nvars));
        new->station_move =
            calloc(new->nstations, sizeof(*new->station_move));
        new->station_index =
            calloc(new->nstations, sizeof(*new->station_index));
        new->station_x =
            calloc(new->nstations, sizeof(*new->station_x));
        if (new->ndims > 1)
            new->station_y = calloc(new->nstations, sizeof(*new->station_y));
        if (new->ndims > 2)
            new->station_z = calloc(new->nstations, sizeof(*new->station_z));

        // Assign the stations
        nstat_total = new->nstations;
        memset(found_id, 0, nstat_total);
        next = list_start(station_blocks);
        for (i = 0; i < station_blocks->count; i++) {
            b = next;
            next = list_next(station_blocks);
            new->station_index[i] = i;
            for (n = 0; n < b->nstations; n++) {
                m = b->station_index[n];
                if (found_id[m]) continue;
                nstat_total--;
                found_id[m] = 1;
                SDF_SET_ENTRY_STRING(new->station_names[m],
                        b->station_names[n]);
                new->station_nvars[m] = b->station_nvars[n];
                new->station_move[m] = b->station_move[n];
                new->station_x[m] = b->station_x[n];
                if (new->ndims > 1) new->station_y[m] = b->station_y[n];
                if (new->ndims > 2) new->station_z[m] = b->station_z[n];
                new->nvariables += new->station_nvars[m];
                if (!nstat_total) break;
            }
            if (!nstat_total) break;
        }

        // Allocate the variables
        new->nvariable_ids = new->ndim_units =
                new->nmaterial_names = new->nvariables;

        new->variable_ids =
                calloc(new->nvariables, sizeof(*new->variable_ids));
        new->dim_units =
                calloc(new->nvariables, sizeof(*new->dim_units));
        new->material_names =
                calloc(new->nvariables, sizeof(*new->material_names));
        new->variable_types =
                calloc(new->nvariables, sizeof(*new->variable_types));
        if (new->use_mult)
            new->dim_mults = calloc(new->nvariables, sizeof(*new->dim_mults));

        // Assign starting variables (time, step)
        b = list_start(station_blocks);
        i = 0;
        nidx = 0;
        for (i = 0; i < varoffset; i++) {
            SDF_SET_ENTRY_ID(new->variable_ids[nidx], b->variable_ids[i]);
            SDF_SET_ENTRY_ID(new->dim_units[nidx], b->dim_units[i]);
            SDF_SET_ENTRY_STRING(new->material_names[nidx],
                    b->material_names[i]);
            new->variable_types[nidx] = b->variable_types[i];
            if (new->use_mult)
                new->dim_mults[nidx] = b->dim_mults[i];
            nidx++;
        }

        // Assign the remaining variables
        nstat_total = new->nstations;
        memset(found_id, 0, nstat_total);
        next = list_start(station_blocks);
        for (i = 0; i < station_blocks->count; i++) {
            b = next;
            next = list_next(station_blocks);
            for (n = 0; n < b->nstations; n++) {
                m = b->station_index[n];
                if (found_id[m]) continue;
                nstat_total--;
                found_id[m] = 1;
                bidx = varoffset;
                for (j = 0; j < n; j++)
                    bidx += b->station_nvars[j];
                for (j = 0; j < b->station_nvars[n]; j++) {
                    SDF_SET_ENTRY_ID(new->variable_ids[nidx],
                            b->variable_ids[bidx]);
                    SDF_SET_ENTRY_ID(new->dim_units[nidx], b->dim_units[bidx]);
                    SDF_SET_ENTRY_STRING(new->material_names[nidx],
                            b->material_names[bidx]);
                    new->variable_types[nidx] = b->variable_types[bidx];
                    if (new->use_mult)
                        new->dim_mults[nidx] = b->dim_mults[bidx];
                    bidx++; nidx++;
                }
                if (!nstat_total) break;
            }
            if (!nstat_total) break;
        }
    }

    list_destroy(&station_blocks);
    free(extra_id);
    free(found_id);

    add_station_variables(h, append, append_tail, nappend);
}



/** @ingroup derived */
int sdf_add_derived_blocks(sdf_file_t *h)
{
    sdf_block_t *b, *next, *append, *append_head, *append_tail;
    sdf_block_t *mesh, *first_mesh = NULL;
    sdf_block_t *current_block = h->current_block;
    int i, n, nd, nappend = 0;
    size_t len1, len2;
    char *str, *name1, *name2;
    char *grid_ids[] = { "x", "y", "z" };
    char *grid_labels[] = { "X", "Y", "Z" };

    append = append_head = calloc(1, sizeof(sdf_block_t));

    next = h->blocklist;
    while (next) {
        b = next;
        next = b->next;

        if (b->blocktype == SDF_BLOCKTYPE_POINT_MESH
                || b->blocktype == SDF_BLOCKTYPE_PLAIN_MESH) {
            if (!first_mesh && b->blocktype == SDF_BLOCKTYPE_PLAIN_MESH)
                first_mesh = b;

            if (b->blocktype == SDF_BLOCKTYPE_PLAIN_MESH
                    && (b->geometry == SDF_GEOMETRY_SPHERICAL
                    ||  b->geometry == SDF_GEOMETRY_CYLINDRICAL)) {
                // Add an unstructured cartesian grid corresponding to these
                // coordinates
                APPEND_BLOCK(append);
                nappend++;
                append_tail = append;

                // Rename original block so that we can use the original name
                // for plotting

                sdf_delete_hash_block(h, b);

                str = strcat_alloc(b->id, "orig");
                sdf_unique_id(h, str);
                append->id = b->id;
                b->id = str;
                mesh = b;

                str = strcat_alloc(b->name, "Orig");
                sdf_unique_name(h, str);
                append->name = b->name;
                b->name = str;

                sdf_hash_block(h, b);

                nd = 3;
                append->geometry = SDF_GEOMETRY_CARTESIAN;
                append->blocktype = SDF_BLOCKTYPE_LAGRANGIAN_MESH;
                append->derived = 1;
                append->datatype = b->datatype;
                append->datatype_out = b->datatype_out;
                append->ndims = nd;
                append->mult = b->mult;
                append->stagger = b->stagger;
                append->subblock = b;
                memcpy(append->dims, b->dims, nd * sizeof(b->dims[0]));

                append->dont_display = 0;

                len1 = 2 * nd * sizeof(double*);
                append->extents = malloc(len1);

                append->extents[0] = -b->extents[3];
                append->extents[1] = -b->extents[3];
                append->extents[2] = -b->extents[3];
                append->extents[3] =  b->extents[3];
                append->extents[4] =  b->extents[3];
                append->extents[5] =  b->extents[3];

                if (b->geometry == SDF_GEOMETRY_CYLINDRICAL) {
                    append->extents[4] = b->extents[4];
                    append->extents[5] = b->extents[5];
                }

                append->ndim_labels = append->ndim_units = nd;
                append->dim_labels = calloc(nd, sizeof(char*));
                append->dim_units = calloc(nd, sizeof(char*));
                for (n = 0; n < nd; n++) {
                    SDF_SET_ENTRY_STRING(append->dim_labels[n],
                            grid_labels[n]);
                    SDF_SET_ENTRY_STRING(append->dim_units[n],
                            mesh->dim_units[0]);
                }

                append->use_mult = b->use_mult;
                if (b->use_mult) {
                    append->dim_mults = calloc(nd, sizeof(*b->dim_mults));
                    memcpy(append->dim_mults,
                           b->dim_mults, nd * sizeof(b->dim_mults[0]));
                }

                append->n_ids = 1;
                append->variable_ids = calloc(append->n_ids, sizeof(char*));
                append->nvariable_ids = append->n_ids;
                SDF_SET_ENTRY_ID(append->variable_ids[0], b->id);
                append->must_read = calloc(append->n_ids, sizeof(char*));
                append->must_read[0] = 1;
                append->populate_data = sdf_callback_cartesian_grid;
                append->done_header = 1;
                // Hack to prevent storage being allocated for this variable.
                append->dont_allocate = 0;

                sdf_hash_block(h, append);
            }

            for (i = 0; i < b->ndims; i++) {
                // Add 1d arrays for each coordinate dimension of the
                // particles. (ie. all the x's, all the y's, all the z's).
                // These are required in order to perform scatter plots.
                APPEND_BLOCK(append);
                nappend++;
                append_tail = append;

                if (b->blocktype == SDF_BLOCKTYPE_POINT_MESH) {
                    name1 = b->id;
                    name2 = grid_ids[i];
                    len1 = strlen(name1);
                    len2 = strlen(name2);
                    str = (char*)malloc(len1 + len2 + 2);
                    memcpy(str, name1, len1);
                    str[len1] = '/';
                    memcpy(str+len1+1, name2, len2);
                    str[len1+len2+1] = '\0';
                    sdf_unique_id(h, str);
                    append->id = str;

                    append->blocktype = SDF_BLOCKTYPE_POINT_DERIVED;
                    append->derived = 1;
                    name2 = b->dim_labels[i];
                } else {
                    str = NULL;
                    SDF_SET_ENTRY_ID(str, grid_ids[i]);
                    sdf_unique_id(h, str);
                    append->id = str;

                    append->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
                    append->derived = 1;
                    name2 = grid_ids[i];
                    append->dont_display = 1;
                }

                name1 = b->name;
                len1 = strlen(name1);
                len2 = strlen(name2);
                str = (char*)malloc(len1 + len2 + 2);
                memcpy(str, name1, len1);
                str[len1] = '/';
                memcpy(str+len1+1, name2, len2);
                str[len1+len2+1] = '\0';
                append->name = str;

                SDF_SET_ENTRY_ID(append->units, b->dim_units[i]);
                SDF_SET_ENTRY_ID(append->mesh_id, b->id);
                append->nm = i;
                append->ndims = 1;
                append->n_ids = 1;
                append->variable_ids = calloc(append->n_ids, sizeof(char*));
                append->nvariable_ids = append->n_ids;
                SDF_SET_ENTRY_ID(append->variable_ids[0], b->id);
                append->must_read = calloc(append->n_ids, sizeof(char*));
                append->must_read[0] = 1;
                append->populate_data = sdf_callback_grid_component;
                append->done_header = 1;
                // Hack to prevent storage being allocated for this variable.
                append->dont_allocate = 1;

                sdf_hash_block(h, append);
            }
        } else if (b->blocktype == SDF_BLOCKTYPE_CPU_SPLIT) {
            // Find first grid mesh
            mesh = h->blocklist;
            while (mesh) {
                if (mesh->blocktype == SDF_BLOCKTYPE_PLAIN_MESH) break;
                mesh = mesh->next;
            }
            if (!mesh) break;

            // First add CPU mesh
            APPEND_BLOCK(append);
            nappend++;
            append_tail = append;

            len1 = strlen(b->id);
            str = malloc(len1+6);
            memcpy(str, "grid_", 5);
            memcpy(str+5, b->id, len1+1);
            sdf_unique_id(h, str);
            append->id = str;

            len1 = strlen(b->name);
            str = malloc(len1+6);
            memcpy(str, "Grid/", 5);
            memcpy(str+5, b->name, len1+1);
            append->name = str;

            append->subblock = b;
            append->subblock2 = mesh;
            append->populate_data = sdf_callback_cpu_mesh;
            append->blocktype = SDF_BLOCKTYPE_PLAIN_MESH;
            append->no_internal_ghost = 1;
            append->datatype_out = mesh->datatype_out;

            append->ndims = b->ndims;
            for (i = 0; i < b->ndims; i++) append->dims[i] = b->dims[i] + 2;
            for (i = b->ndims; i < 3; i++) append->dims[i] = 1;

            nd = mesh->ndims;
            len1 = 2 * nd * sizeof(double*);
            append->extents = malloc(len1);
            memcpy(append->extents, mesh->extents, len1);
            append->ndim_labels = append->ndim_units = nd;
            append->dim_labels = calloc(nd, sizeof(char*));
            append->dim_units = calloc(nd, sizeof(char*));
            for (n = 0; n < nd; n++) {
                SDF_SET_ENTRY_STRING(append->dim_labels[n],
                        mesh->dim_labels[n]);
                SDF_SET_ENTRY_STRING(append->dim_units[n],
                        mesh->dim_units[n]);
            }
            mesh = append;

            sdf_hash_block(h, append);

            // Now add CPU data block
            APPEND_BLOCK(append);
            nappend++;
            append_tail = append;

            // Rename original block so that we can use the original name
            // for plotting

            sdf_delete_hash_block(h, b);

            str = b->id;
            append->id = str;
            len1 = strlen(str);
            str = malloc(len1+7);
            memcpy(str, append->id, len1);
            memcpy(str+len1, "_orig", 6);
            sdf_unique_id(h, str);
            b->id = str;

            str = b->name;
            append->name = str;
            len1 = strlen(str);
            str = malloc(len1+7);
            memcpy(str, append->name, len1);
            memcpy(str+len1, "_orig", 6);
            b->name = str;

            sdf_hash_block(h, b);

            append->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
            append->derived = 1;
            append->no_internal_ghost = 1;

            SDF_SET_ENTRY_ID(append->mesh_id, mesh->id);
            SDF_SET_ENTRY_ID(append->units, "CPU");
            append->ndims = b->ndims;
            append->nelements_local = 1;
            for (i=0; i < b->ndims; i++) {
                append->dims[i] = b->dims[i] + 1;
                append->local_dims[i] = append->dims[i];
                append->nelements_local *= append->local_dims[i];
            }
            for (i=b->ndims; i < 3; i++)
                append->local_dims[i] = append->dims[i] = 1;
            append->n_ids = 2;
            append->variable_ids = calloc(append->n_ids, sizeof(char*));
            append->nvariable_ids = append->n_ids;
            SDF_SET_ENTRY_ID(append->variable_ids[0], b->id);
            SDF_SET_ENTRY_ID(append->variable_ids[1], mesh->id);
            append->must_read = calloc(append->n_ids, sizeof(char*));
            append->must_read[0] = 1;
            append->must_read[1] = 1;
            append->populate_data = sdf_callback_cpu_data;
            append->datatype = append->datatype_out = SDF_DATATYPE_INTEGER4;

            sdf_hash_block(h, append);
        }
    }

    if (first_mesh) {
        // First add CPU mesh
        APPEND_BLOCK(append);
        nappend++;
        append_tail = append;

        SDF_SET_ENTRY_ID(append->id, "grid_cpus_current");
        SDF_SET_ENTRY_STRING(append->name, "Grid/CPUs/Current rank");

        append->subblock = first_mesh;
        append->populate_data = sdf_callback_current_cpu_mesh;
        append->blocktype = SDF_BLOCKTYPE_PLAIN_MESH;
        append->no_internal_ghost = 1;

        nd = append->ndims = first_mesh->ndims;

        len1 = 2 * nd * sizeof(double*);
        append->extents = malloc(len1);
        memcpy(append->extents, first_mesh->extents, len1);
        append->ndim_labels = append->ndim_units = nd;
        append->dim_labels = calloc(nd, sizeof(char*));
        append->dim_units = calloc(nd, sizeof(char*));
        for (n = 0; n < nd; n++) {
            SDF_SET_ENTRY_STRING(append->dim_labels[n],
                    first_mesh->dim_labels[n]);
            SDF_SET_ENTRY_STRING(append->dim_units[n],
                    first_mesh->dim_units[n]);
#ifdef PARALLEL
            append->dims[n] = append->local_dims[n] = first_mesh->cpu_split[n];
#else
            append->dims[n] = append->local_dims[n] = 2;
#endif
        }
        mesh = append;

        sdf_hash_block(h, append);

        // Now add CPU data block
        APPEND_BLOCK(append);
        nappend++;
        append_tail = append;

        SDF_SET_ENTRY_ID(append->id, "cpus_current");
        SDF_SET_ENTRY_ID(append->name, "CPUs/Current rank");

        append->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
        append->derived = 1;
        append->no_internal_ghost = 1;

        SDF_SET_ENTRY_ID(append->units, "CPU");
        SDF_SET_ENTRY_ID(append->mesh_id, mesh->id);
        append->ndims = mesh->ndims;
        append->subblock = mesh;
        append->populate_data = sdf_callback_cpu_data;
        append->datatype = append->datatype_out = SDF_DATATYPE_INTEGER4;

        mesh->subblock2 = append;

        sdf_hash_block(h, append);
    }

    if (h->station_file)
        add_global_station(h, &append, &append_tail, &nappend);

    if (nappend) {
        h->tail->next = append_head->next;
        h->tail->next->prev = h->tail;
        h->tail = append_tail;
        h->nblocks += nappend;
    }

    free(append_head);

    h->current_block = current_block;

    return 0;
}



/** @ingroup derived */
int sdf_add_derived_blocks_final(sdf_file_t *h)
{
    sdf_block_t *b, *next, *append, *append_head, *append_tail;
    sdf_block_t *mesh, *old_mesh, *vfm, *obst = NULL;
    sdf_block_t *first_obst = NULL, *first_mat = NULL, *first_mesh_var = NULL;
    sdf_block_t *current_block = h->current_block;
    sdf_block_t *gb, *sb;
    int i, n, stagger, dont_add_grid, nd, nappend = 0;
    size_t len1, len2;
    char *str, *name1, *name2;
    char *boundary_names[] =
        { "", "_x_min", "_x_max", "_y_min", "_y_max", "_z_min", "_z_max" };
    char *face_grid_ids[] = { "xface", "yface", "zface" };
    sdf_block_t *(*callback)(sdf_file_t *, sdf_block_t *);

    append = append_head = calloc(1, sizeof(sdf_block_t));

    next = h->blocklist;
    while (next) {
        b = next;
        next = b->next;

        if (b->blocktype == SDF_BLOCKTYPE_STITCHED_OBSTACLE_GROUP) {
            obst = sdf_find_block_by_id(h, b->obstacle_id);
            if (!obst) continue;

            vfm = sdf_find_block_by_id(h, b->vfm_id);
            if (!vfm) continue;

            if (!first_obst && obst->ndims == 3) first_obst = obst;
            vfm->subblock = b;
            b->subblock = obst;
            mesh = sdf_find_block_by_id(h, obst->mesh_id);

            if (obst->ng == 0) {
                obst->ng = 1;
                obst->dont_display = 1;
                obst->nelements_local = 1;
                for (i = 0; i < obst->ndims; i++) {
                    obst->dims[i] += 2 * obst->ng;
                    obst->local_dims[i] += 2 * obst->ng;
                    obst->nelements_local *= obst->local_dims[i];
                }
            }

            if (obst->ndims != 3) continue;

            for (i = 0; i < b->ndims; i++) {
                APPEND_BLOCK(append);
                nappend++;
                append_tail = append;

                append->blocktype = SDF_BLOCKTYPE_UNSTRUCTURED_MESH;
                append->ndims = obst->ndims;
                append->subblock = obst;
                append->nm = i;
                append->populate_data = sdf_callback_surface_mesh;

                name1 = "obmsh";
                name2 = b->material_names[i];
                len1 = strlen(name1);
                len2 = strlen(name2);
                str = (char*)malloc(len1 + len2 + 1);
                memcpy(str, name1, len1);
                memcpy(str+len1, name2, len2);
                str[len1+len2] = '\0';
                sdf_unique_id(h, str);
                append->id = str;

                name1 = "Surface Values/";
                name2 = b->material_names[i];
                len1 = strlen(name1);
                len2 = strlen(name2);
                str = (char*)malloc(len1 + len2 + 1);
                memcpy(str, name1, len1);
                memcpy(str+len1, name2, len2);
                str[len1+len2] = '\0';
                append->name = str;

                sdf_hash_block(h, append);

                add_surface_variables(h, append, &append, &append_tail,
                        &nappend);
            }
        } else if (b->blocktype == SDF_BLOCKTYPE_PLAIN_VARIABLE
                || b->blocktype == SDF_BLOCKTYPE_PLAIN_DERIVED) {

            if (!first_mesh_var) {
                mesh = sdf_find_block_by_id(h, b->mesh_id);
                if (mesh && b->ndims == 3) first_mesh_var = b;
            }
            if (!first_mat) {
                vfm = sdf_find_block_by_id(h, b->vfm_id);
                if (vfm && b->ndims == 3) first_mat = b;
            }

            // Add grids for face-staggered variables
            for (i = 0; i < b->ndims; i++) {
                stagger = 1<<i;
                if (b->stagger == stagger) {
                    old_mesh = sdf_find_block_by_id(h, b->mesh_id);
                    if (!old_mesh) break;
                    // For now, only add the mesh if variables are the correct
                    // dimensions
                    dont_add_grid = 0;
                    for (n = 0; n < b->ndims; n++) {
                        nd = old_mesh->dims[n];
                        if (n == i) nd += b->ng + 1;
                        if (b->dims[n]+1 != nd) dont_add_grid++;
                    }
                    if (dont_add_grid) continue;

                    // Add cell-centred variable
                    APPEND_BLOCK(append);
                    nappend++;
                    append_tail = append;

                    str = strcat_alloc(b->id, "centred");
                    sdf_unique_id(h, str);

                    SDF_SET_ENTRY_ID(append->id, str);
                    free(str);

                    str = strcat_alloc(b->name, "centred");
                    SDF_SET_ENTRY_STRING(append->name, str);
                    free(str);

                    append->blocktype = SDF_BLOCKTYPE_PLAIN_DERIVED;
                    append->derived = 1;
                    append->no_internal_ghost = 1;

                    SDF_SET_ENTRY_ID(append->units, b->units);
                    SDF_SET_ENTRY_ID(append->mesh_id, b->mesh_id);
                    append->ndims = b->ndims;
                    append->subblock = b;
                    for (n = 0; n < b->ndims; n++) {
                        append->dims[n] = b->dims[n];
                        append->local_dims[n] = b->local_dims[n];
                    }
                    for (n=b->ndims; n < 3; n++) {
                        append->dims[n] = 1;
                        append->local_dims[n] = 1;
                    }
                    append->dims[i]--;
                    append->local_dims[i]--;
                    append->nelements = 1;
                    append->nelements_local = 1;
                    for (n = 0; n < b->ndims; n++) {
                        append->nelements *= b->dims[n];
                        append->nelements_local *= b->local_dims[n];
                    }

                    append->n_ids = 1;
                    append->variable_ids = calloc(append->n_ids, sizeof(char*));
                    append->nvariable_ids = append->n_ids;
                    SDF_SET_ENTRY_ID(append->variable_ids[0], b->id);
                    append->must_read = calloc(append->n_ids, sizeof(char*));
                    append->must_read[0] = 1;
                    append->populate_data = sdf_callback_average;
                    append->datatype = append->datatype_out = b->datatype;

                    sdf_hash_block(h, append);

                    name1 = b->mesh_id;
                    name2 = face_grid_ids[i];
                    len1 = strlen(name1);
                    len2 = strlen(name2);
                    str = (char*)malloc(len1 + len2 + 2);
                    memcpy(str, name1, len1);
                    str[len1] = '/';
                    memcpy(str+len1+1, name2, len2);
                    str[len1+len2+1] = '\0';
                    free(b->mesh_id);
                    sdf_unique_id(h, str);
                    b->mesh_id = str;

                    // Add new face grid if it doesn't exist
                    mesh = sdf_find_block_by_id(h, b->mesh_id);
                    if (!mesh) {
                        APPEND_BLOCK(append);
                        nappend++;
                        append_tail = append->prev;

                        memcpy(append, old_mesh, sizeof(sdf_block_t));
                        append->next = NULL;
                        append->prev = append_tail;
                        append_tail = append;

                        append->n_ids = 0;
                        append->must_read = NULL;
                        append->variable_ids = NULL;

                        str = (char*)malloc(len1 + len2 + 2);
                        memcpy(str, b->mesh_id, len1+len2+2);
                        sdf_unique_id(h, str);
                        append->id = str;

                        name1 = old_mesh->name;
                        len1 = strlen(name1);
                        str = (char*)malloc(len1 + len2 + 2);
                        memcpy(str, name1, len1);
                        str[len1] = '/';
                        memcpy(str+len1+1, name2, len2);
                        str[len1+len2+1] = '\0';
                        append->name = str;

                        append->stagger = i;
                        append->subblock = old_mesh;
                        append->populate_data = sdf_callback_face_grid;

                        append->dims[i] += 1;
                        append->dim_mults = NULL;
                        append->extents = NULL;

                        nd = old_mesh->ndims;
                        len1 = 2 * nd * sizeof(double*);
                        append->extents = malloc(len1);
                        memcpy(append->extents, old_mesh->extents, len1);
                        append->ndim_labels = append->ndim_units = nd;
                        append->dim_labels = calloc(nd, sizeof(char*));
                        append->dim_units = calloc(nd, sizeof(char*));
                        for (n = 0; n < nd; n++) {
                            SDF_SET_ENTRY_STRING(append->dim_labels[n],
                                    old_mesh->dim_labels[n]);
                            SDF_SET_ENTRY_STRING(append->dim_units[n],
                                    old_mesh->dim_units[n]);
                        }

                        sdf_hash_block(h, append);
                    }
                }
            }
        } else if (b->blocktype == SDF_BLOCKTYPE_STITCHED && b->stagger > 10) {
            // Hide individual rays from the VisIt menu
            for (i = 0 ; i < b->ndims ; i++) {
                gb = sdf_find_block_by_id(h, b->variable_ids[i]);
                if (!gb) continue;
                gb->dont_display = 1;
                for (n = 0 ; n < gb->ndims ; n++) {
                    sb = sdf_find_block_by_id(h, gb->variable_ids[n]);
                    sb->dont_display = 1;
                }
            }

        }
    }

    if (first_obst) {
        obst = first_obst;
        callback = sdf_callback_boundary_mesh_ob;
    } else if (first_mat) {
        obst = first_mat;
        callback = sdf_callback_boundary_mesh;
    } else if (first_mesh_var) {
        obst = first_mesh_var;
        callback = sdf_callback_boundary_mesh;
    } else {
        obst = NULL;
    }

    if (obst) {
        for (i = 0; i < 7; i++) {
            APPEND_BLOCK(append);
            nappend++;
            append_tail = append;

            append->blocktype = SDF_BLOCKTYPE_UNSTRUCTURED_MESH;
            append->ndims = obst->ndims;
            append->subblock = obst;
            append->nm = i;
            append->populate_data = callback;

            name1 = "boundary";
            name2 = boundary_names[i];
            len1 = strlen(name1);
            len2 = strlen(name2);
            str = (char*)malloc(len1 + len2 + 1);
            memcpy(str, name1, len1);
            memcpy(str+len1, name2, len2);
            str[len1+len2] = '\0';
            sdf_unique_id(h, str);
            append->id = str;

            name1 = "Surface Values/boundary";
            name2 = boundary_names[i];
            len1 = strlen(name1);
            len2 = strlen(name2);
            str = (char*)malloc(len1 + len2 + 1);
            memcpy(str, name1, len1);
            memcpy(str+len1, name2, len2);
            str[len1+len2] = '\0';
            append->name = str;

            sdf_hash_block(h, append);

            add_surface_variables(h, append, &append, &append_tail,
                    &nappend);
        }
    }

    if (nappend) {
        h->tail->next = append_head->next;
        h->tail->next->prev = h->tail;
        h->tail = append_tail;
        h->nblocks += nappend;
    }

    free(append_head);

    h->current_block = current_block;

    return 0;
}
