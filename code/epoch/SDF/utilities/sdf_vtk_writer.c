#include <stdio.h>
#include <stdlib.h>
#include "sdf.h"
#include "sdf_helper.h"

#define MAX(a,b) (((a) > (b)) ? (a) : (b))

/* Needs to be added to sdf.h */
int sdf_free_block_data(sdf_file_t *h, sdf_block_t *b);

struct block_list;

struct block_list {
    sdf_block_t *block;
    struct block_list *next;
};


int sdf_write_vtk_grid(sdf_file_t *h, sdf_block_t *grid, char *filename)
{
    sdf_block_t *b, *next;
    FILE *fd = fopen(filename, "w");
    int array_offset = 0;
    int nx, ny, nz, sz, v_size, c_size, i, len;
    double *xptr, *yptr, *zptr;
    double zero = 0.0;
    sdf_block_t **cell_blocks, **vertex_blocks;
    int ncell_blocks = 0, nvertex_blocks = 0;
    char *vtk_type_strings[3] = {"RectilinearGrid", "StructuredGrid",
                                 "UnstructuredGrid"};
    int vtk_type;

    switch (grid->blocktype) {
    case SDF_BLOCKTYPE_PLAIN_MESH:
        vtk_type = 0;
        break;
    case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
        vtk_type = 1;
        break;
    case SDF_BLOCKTYPE_POINT_MESH:
    case SDF_BLOCKTYPE_UNSTRUCTURED_MESH:
    case SDF_BLOCKTYPE_STATION:
        vtk_type = 2;
        break;
    }

    cell_blocks = malloc(h->nblocks * sizeof(*cell_blocks));
    vertex_blocks = malloc(h->nblocks * sizeof(*vertex_blocks));

    len = strlen(grid->id);

    /* Find arrays associated with the grid */
    next = h->blocklist;
    while (next) {
        h->current_block = b = next;
        next = b->next;
        if (b->blocktype != SDF_BLOCKTYPE_PLAIN_VARIABLE
                && b->blocktype != SDF_BLOCKTYPE_POINT_VARIABLE)
            continue;

        if (strlen(b->mesh_id) != len || memcmp(b->mesh_id, grid->id, len+1))
            continue;

        if (b->datatype != SDF_DATATYPE_REAL8) {
            printf("Datatype not yet supported. %s ignored.\n", b->id);
            continue;
        }

        switch (b->stagger) {
        case SDF_STAGGER_VERTEX:
            vertex_blocks[nvertex_blocks++] = b;
            break;
        case SDF_STAGGER_CELL_CENTRE:
            cell_blocks[ncell_blocks++] = b;
            break;
        default:
            printf("Stagger not yet supported. %s ignored.\n", b->id);
            break;
        }
    }

    nx = ny = nz = 0;
    if (grid->ndims > 0) nx = grid->dims[0] - 1;
    if (grid->ndims > 1) ny = grid->dims[1] - 1;
    if (grid->ndims > 2) nz = grid->dims[2] - 1;

    c_size = MAX(nx,1) * MAX(ny,1) * MAX(nz,1) * sizeof(double);
    v_size = (nx + 1) * (ny + 1) * (nz + 1) * sizeof(double);

    /* Header */
    fprintf(fd, "<?xml version=\"1.0\"?>\n\
<VTKFile type=\"%s\" version=\"0.1\" byte_order=\"LittleEndian\">\n\
  <%s WholeExtent=\"0 %i 0 %i 0 %i\">\n\
    <FieldData>\n\
      <Array type=\"String\" Name=\"MeshName\" NumberOfTuples=\"1\" "
      "format=\"appended\" offset=\"%i\"/>\n\
      <DataArray type=\"Int32\" Name=\"CYCLE\" NumberOfTuples=\"1\">\n\
        %i\n\
      </DataArray>\n\
      <DataArray type=\"Float64\" Name=\"TIME\" NumberOfTuples=\"1\">\n\
        %g\n\
      </DataArray>\n\
    </FieldData>\n",
    vtk_type_strings[vtk_type], vtk_type_strings[vtk_type], nx, ny, nz,
    array_offset, h->step, h->time);

    array_offset += sizeof(int) + strlen(grid->id) + 1;


    if (vtk_type == 2) {
        fprintf(fd, "    <Piece NumberOfPoints=\"%i\" NumberOfCells=\"0\">\n",
                    nx);
        fprintf(fd, "      <Cells>\n        <DataArray type=\"Int32\" "
                    "Name=\"connectivity\"/>\n      </Cells>\n");
    } else
        fprintf(fd, "    <Piece Extent=\"0 %i 0 %i 0 %i\">\n", nx, ny, nz);

    /* PointData */
    fprintf(fd, "      <PointData>\n");
    for (i = 0; i < nvertex_blocks; i++) {
        b = vertex_blocks[i];
        fprintf(fd, "        <DataArray Name=\"%s\"\n"
                    "                   type=\"Float64\" "
                    "format=\"appended\" offset=\"%i\"/>\n", b->id,
                    array_offset);
        array_offset += sizeof(int) + v_size;
    }
    fprintf(fd, "      </PointData>\n");

    /* CellData */
    fprintf(fd, "      <CellData>\n");
    for (i = 0; i < ncell_blocks; i++) {
        b = cell_blocks[i];
        fprintf(fd, "        <DataArray Name=\"%s\"\n"
                    "                   type=\"Float64\" "
                    "format=\"appended\" offset=\"%i\"/>\n", b->id,
                    array_offset);
        array_offset += sizeof(int) + c_size;
    }
    fprintf(fd, "      </CellData>\n");

    if (vtk_type == 0) {
        /* Coordinates */
        fprintf(fd, "      <Coordinates Name=\"%s\">\n", grid->id);

        fprintf(fd, "        <DataArray Name=\"x\"\n"
                    "                   type=\"Float64\" ");
        if (grid->ndims > 0) {
            fprintf(fd, "format=\"appended\" offset=\"%i\"/>\n", array_offset);
            array_offset += sizeof(int) + grid->dims[0] * sizeof(double);
        } else
            fprintf(fd, ">0</DataArray>\n");

        fprintf(fd, "        <DataArray Name=\"y\"\n"
                    "                   type=\"Float64\" ");
        if (grid->ndims > 1) {
            fprintf(fd, "format=\"appended\" offset=\"%i\"/>\n", array_offset);
            array_offset += sizeof(int) + grid->dims[1] * sizeof(double);
        } else
            fprintf(fd, ">0</DataArray>\n");

        fprintf(fd, "        <DataArray Name=\"z\"\n"
                    "                   type=\"Float64\" ");
        if (grid->ndims > 2) {
            fprintf(fd, "format=\"appended\" offset=\"%i\"/>\n", array_offset);
            array_offset += sizeof(int) + grid->dims[2] * sizeof(double);
        } else
            fprintf(fd, ">0</DataArray>\n");

        fprintf(fd, "      </Coordinates>\n");
    } else {
        /* Points */
        sz = (nx + 1) * (ny + 1);
        fprintf(fd, "      <Points>\n");
        fprintf(fd, "        <DataArray Name=\"%s\" NumberOfComponents=\"3\"\n"
                    "                   type=\"Float64\" "
                    "format=\"appended\" offset=\"%i\"/>\n", grid->id,
                    array_offset);
        fprintf(fd, "      </Points>\n");

        array_offset += grid->nelements * 3 * sizeof(double) + sizeof(int);
    }

    fprintf(fd, "    </Piece>\n  </%s>\n", vtk_type_strings[vtk_type]);

    /* Data */
    fprintf(fd, "  <AppendedData encoding=\"raw\">\n    _");

    /* MeshName */
    sz = strlen(grid->id) + 1;
    fwrite(&sz, sizeof(sz), 1, fd);
    fwrite(grid->id, 1, sz, fd);

    /* PointData */
    for (i = 0; i < nvertex_blocks; i++) {
        b = vertex_blocks[i];
        sdf_helper_read_data(h, b);
        sz = v_size;
        fwrite(&sz, sizeof(sz), 1, fd);
        fwrite(b->data, 1, sz, fd);
        sdf_free_block_data(h, b);
    }

    /* CellData */
    for (i = 0; i < ncell_blocks; i++) {
        b = cell_blocks[i];
        sdf_helper_read_data(h, b);
        sz = c_size;
        fwrite(&sz, sizeof(sz), 1, fd);
        fwrite(b->data, 1, sz, fd);
        sdf_free_block_data(h, b);
    }

    /* Grid */

    sdf_helper_read_data(h, grid);

    if (vtk_type == 0) {
        /* Coordinates */
        if (grid->ndims > 0) {
            sz = grid->dims[0] * sizeof(double);
            fwrite(&sz, sizeof(sz), 1, fd);
            fwrite(grid->grids[0], 1, sz, fd);
        }

        if (grid->ndims > 1) {
            sz = grid->dims[1] * sizeof(double);
            fwrite(&sz, sizeof(sz), 1, fd);
            fwrite(grid->grids[1], 1, sz, fd);
        }

        if (grid->ndims > 2) {
            sz = grid->dims[2] * sizeof(double);
            fwrite(&sz, sizeof(sz), 1, fd);
            fwrite(grid->grids[2], 1, sz, fd);
        }
    } else {
        /* Points */
        sz = grid->nelements * 3 * sizeof(double);
        fwrite(&sz, sizeof(sz), 1, fd);

        sdf_helper_read_data(h, grid);

        xptr = yptr = zptr = NULL;
        if (grid->ndims > 0) xptr = grid->grids[0];
        if (grid->ndims > 1) yptr = grid->grids[1];
        if (grid->ndims > 2) zptr = grid->grids[2];

        for (i=0; i < grid->nelements; i++) {
            if (xptr)
                fwrite(xptr++, sizeof(double), 1, fd);
            else
                fwrite(&zero, sizeof(double), 1, fd);

            if (yptr)
                fwrite(yptr++, sizeof(double), 1, fd);
            else
                fwrite(&zero, sizeof(double), 1, fd);

            if (zptr)
                fwrite(zptr++, sizeof(double), 1, fd);
            else
                fwrite(&zero, sizeof(double), 1, fd);
        }
    }

    sdf_free_block_data(h, b);

    fprintf(fd, "\n  </AppendedData>\n");

    /* Footer */
    fprintf(fd, "</VTKFile>\n");

    fclose(fd);

    free(cell_blocks);
    free(vertex_blocks);

    return 0;
}


void sdf_write_vtm_header(FILE *fd)
{
    /* Header */
    fprintf(fd, "<?xml version=\"1.0\"?>\n"
                "<VTKFile type=\"vtkMultiBlockDataSet\" version=\"1.0\" "
                "byte_order=\"LittleEndian\">\n"
                "  <vtkMultiBlockDataSet>\n");
}


int sdf_write_vtk_file(sdf_file_t *h, char *stem)
{
    sdf_block_t *b, *next;
    FILE *fd;
    char *filename;
    int file_index = 0;
    int result = 0;
    int len, vtk_type;
    char *vtk_suffixes[3] = {"vtr", "vts", "vtu"};

    len = strlen(stem) + 512;
    filename = malloc(len);
    snprintf(filename, len, "%s.vtm", stem);

    /* Grids */

    next = h->blocklist;
    while (next) {
        h->current_block = b = next;
        next = b->next;

        switch (b->blocktype) {
        case SDF_BLOCKTYPE_PLAIN_MESH:
            vtk_type = 0;
            break;
        case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
            vtk_type = 1;
            break;
        case SDF_BLOCKTYPE_POINT_MESH:
            vtk_type = 2;
            break;
        case SDF_BLOCKTYPE_UNSTRUCTURED_MESH:
        case SDF_BLOCKTYPE_STATION:
            printf("Grid type not yet supported. %s ignored.\n", b->id);
            continue;
        default:
            continue;
        }

        if (b->datatype != SDF_DATATYPE_REAL8) {
            printf("Datatype not yet supported. %s ignored.\n", b->id);
            continue;
        }

        if (file_index == 0) {
            fd = fopen(filename, "w");
            sdf_write_vtm_header(fd);
        }
        snprintf(filename, len, "%s_%i.%s", stem, file_index,
                 vtk_suffixes[vtk_type]);
        fprintf(fd, "    <DataSet index=\"%i\" file=\"%s\"/>\n", file_index,
                filename);
        sdf_write_vtk_grid(h, b, filename);
        file_index++;
    }

    if (file_index == 0) {
        result = 1;
        goto cleanup;
    }

    /* Footer */
    fprintf(fd, "  </vtkMultiBlockDataSet>\n</VTKFile>\n");

cleanup:
    if (file_index != 0) fclose(fd);
    free(filename);

    return result;
}
