/*
 * sdfdiff - compare numerical differences between two SDF files
 * Copyright (C) 2016 SDF Development Team
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

// See: https://stackoverflow.com/a/62371749
// Windows does not define the S_ISREG and S_ISDIR macros in stat.h, so we do.
// We have to define _CRT_INTERNAL_NONSTDC_NAMES 1 before #including sys/stat.h
// in order for Microsoft's stat.h to define names like S_IFMT, S_IFREG, and S_IFDIR,
// rather than just defining  _S_IFMT, _S_IFREG, and _S_IFDIR as it normally does.
#define _CRT_INTERNAL_NONSTDC_NAMES 1
#include <sys/stat.h>
#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#include <math.h>
#include <time.h>
#include <float.h>
#include <limits.h>
#include "sdf.h"
#include "sdf_list_type.h"
#include "sdf_helper.h"
#include "stack_allocator.h"
#include "commit_info.h"

#ifdef PARALLEL
#include <mpi.h>
#endif

#define VERSION "2.6.12"

#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define ABS(a) (((a) < 0.0) ? -(a) : (a))

int metadata, debug, ignore_summary;
int exclude_variables, index_offset;
int just_id, verbose_metadata, special_format, scale_factor;
int purge_duplicate, ignore_nblocks, quiet, show_errors;
int array_blocktypes, mesh_blocktypes;
int done_header = 0;
int *blocktype_mask;
char *format_float, *format_int, *format_space;
double relerr = 1.0e-15;
double abserr = DBL_MAX;
//static char *default_float = "%9.6fE%+2.2d1p";
static char *default_float = "%27.18e";
static char *default_int   = "%" PRIi64;
static char *default_space = "    ";
static char *default_indent = "  ";
static char indent[64];
#define HEADER_LEN 512
static char header_string[HEADER_LEN];

struct id_list {
    char *id;
    struct id_list *next;
} *variable_ids, *variable_last_id;

struct range_type {
    int start, end;
} *range_list, *blocktype_list;

int nrange, nrange_max;
int nblist, nblist_max;

static char width_fmt[16];
#define SET_WIDTH_LEN(len) do { \
        snprintf(width_fmt, 16, "%%-%is", (len)); \
    } while(0)

#define SET_WIDTH(string) do { \
        int _l = strlen((string)); \
        SET_WIDTH_LEN(_l); \
    } while(0)

#define PRINTC(name,variable,fmt) do { \
        printf(indent, 1); \
        printf(width_fmt, (name)); \
        printf(" "); \
        printf(fmt, (variable)); \
        printf("\n"); \
    } while(0)

#define PRINT(name,variable,fmt) do { \
        PRINTC(name,variable,fmt); \
    } while(0)

#define PRINTAR(name,array,fmt,len) do { \
        int _i; \
        if (!(array)) break; \
        printf(indent, 1); \
        printf(width_fmt, (name)); \
        printf(" ("); \
        printf(fmt, (array)[0]); \
        for (_i = 1; _i < (len); _i++) { \
            printf(","); \
            printf(fmt, (array)[_i]); \
        } \
        printf(")\n"); \
    } while(0)

#define PRINTDAR(name,array,fmt,len) do { \
        int _i; \
        if (!(array)) break; \
        printf(indent, 1); \
        printf(width_fmt, (name)); \
        printf(" ("); \
        printf(fmt, sdf_datatype_c[(array)[0]]); \
        for (_i = 1; _i < (len); _i++) { \
            printf(","); \
            printf(fmt, sdf_datatype_c[(array)[_i]]); \
        } \
        printf(")\n"); \
    } while(0)


int close_files(sdf_file_t **handles);
static inline void print_header(void);


void usage(int err)
{
    fprintf(stderr, "usage: sdfdiff [options] <sdf_filename1> "
                    "<sdf_filename2>\n");
    fprintf(stderr, "\noptions:\n\
  -h --help            Show this usage message\n\
  -q --quiet           Do not print output. Exit with zero status if files\n\
                       are the same and non-zero if they differ.\n\
  -m --metadata        Show metadata blocks (not shown by default)\n\
  -j --just-id         Only show ID and number of differing blocks\n\
  -l --less-verbose    Print metadata less verbosely\n\
  -r --relerr          Relative error for numerical difference\n\
  -a --abserr          Absolue error for numerical difference\n\
  -v --variable=id     Find the block with id matching 'id'\n\
  -x --exclude=id      Exclude the block with id matching 'id'\n\
  -i --no-summary      Ignore the metadata summary\n\
  -b --no-nblocks      Ignore the header value for nblocks\n\
  -E --show-errors     Print error values\n\
  -I --c-indexing      Array indexing starts from 1 by default. If this flag\n\
                       is used then the indexing starts from 0.\n\
  -F --format-float=f  Use specified format for printing floating-point array\n\
                       contents.\n\
  -N --format-int=f    Use specified format for printing integer array\n\
                       contents.\n\
  -S --format-space=f  Use specified spacing between array elements.\n\
  -p --purge-duplicate Delete duplicated block IDs\n\
  -B --block-types     List of SDF block types to consider\n\
  -A --array-blocks    Only consider array block types (%i,%i,%i,%i,%i)\n\
  -M --mesh-blocks     Only consider mesh block types (%i,%i,%i,%i)\n\
  -P --print-types     Print the list of SDF blocktypes\n\
  -V --version         Print version information and exit\n\
", SDF_BLOCKTYPE_PLAIN_VARIABLE, SDF_BLOCKTYPE_POINT_VARIABLE,
   SDF_BLOCKTYPE_ARRAY,
   SDF_BLOCKTYPE_PLAIN_DERIVED, SDF_BLOCKTYPE_POINT_DERIVED,
   SDF_BLOCKTYPE_PLAIN_MESH, SDF_BLOCKTYPE_POINT_MESH,
   SDF_BLOCKTYPE_UNSTRUCTURED_MESH, SDF_BLOCKTYPE_LAGRANGIAN_MESH);

    exit(err);
}


void print_blocktypes(void)
{
    int i;

    printf("%2i Header block\n", 0);
    for (i=1; i < sdf_blocktype_len; i++) {
        printf("%2i %s\n", i, sdf_blocktype_c[i]);
    }
    exit(0);
}


int range_sort(const void *v1, const void *v2)
{
    struct range_type *a = (struct range_type *)v1;
    struct range_type *b = (struct range_type *)v2;

    return (a->start - b->start);
}


void parse_format(void)
{
    char cc;
    int len = strlen(format_float) - 1;

    scale_factor = 1;
    if (format_float[len] == 'p') {
        format_float[len] = '\0';
        for (; len > 1; len--) {
            cc = format_float[len-1];
            if (cc < '0' || cc > '9') break;
        }
        scale_factor = (int)strtol(&format_float[len], NULL, 10);
        format_float[len--] = '\0';
    }

    special_format = 0;
    if (format_float[len] == 'd')
        special_format = 1;
}


int parse_range(char *optarg, struct range_type **range_list_p, int *nrange,
                int *nrange_max)
{
    int i, range;
    size_t sz;
    char *ptr;
    struct range_type *range_list = *range_list_p;
    struct range_type *range_tmp;

    if (!((*optarg >= '0' && *optarg <= '9') || *optarg == '-'))
        return 1;

    sz = sizeof(struct range_type);
    ptr = optarg;
    range = 0;
    while (ptr < optarg + strlen(optarg) + 1) {
        if (range) {
            i = (int)strtol(ptr, &ptr, 10);
            if (i == 0)
                range_list[*nrange-1].end = INT_MAX;
            else if (i < range_list[*nrange-1].start)
                (*nrange)--;
            else
                range_list[*nrange-1].end = i;
            range = 0;
        } else {
            (*nrange)++;
            // Grow array if necessary
            if (*nrange > *nrange_max) {
                if (*nrange_max == 0) {
                    *nrange_max = 128;
                    range_list = calloc(*nrange_max, sz);
                } else {
                    i = 2 * *nrange_max;

                    range_tmp = calloc(i, sz);
                    memcpy(range_tmp, range_list, *nrange_max * sz);
                    free(range_list);
                    range_list = range_tmp;

                    *nrange_max = i;
                }
            }

            if (*ptr == '-') {
                range = 1;
                range_list[*nrange-1].end = INT_MAX;
            } else {
                i = (int)strtol(ptr, &ptr, 10);
                range_list[*nrange-1].start = i;
                range_list[*nrange-1].end = i;
                if (*ptr == '-') range = 1;
            }
        }

        ptr++;
    }

    *range_list_p = range_list;

    return 0;
}


void sort_range(struct range_type **range_list_p, int *nrange)
{
    struct range_type *range_list = *range_list_p;
    struct range_type *range_tmp;
    size_t sz;
    int i;

    if (*nrange <= 0)
        return;

    sz = sizeof(struct range_type);
    // Sanitize range list
    qsort(range_list, *nrange, sz, &range_sort);
    for (i=1; i < *nrange; ) {
        if (range_list[i].start <= range_list[i-1].end+1) {
            if (range_list[i].end > range_list[i-1].end)
                range_list[i-1].end = range_list[i].end;
            memcpy(range_list+i, range_list+i+1, (*nrange-i) * sz);
            (*nrange)--;
        } else
            i++;
    }

    // Shrink array
    range_tmp = malloc(*nrange * sz);
    memcpy(range_tmp, range_list, *nrange * sz);
    free(range_list);
    *range_list_p = range_list = range_tmp;
}


void setup_blocklist_mask(void)
{
    int i, n, blist_start = 0;

    blocktype_mask = NULL;

    if (mesh_blocktypes + array_blocktypes + nblist == 0)
        return;

    blocktype_mask = calloc(sdf_blocktype_len, sizeof(*blocktype_mask));

    if (mesh_blocktypes) {
        blocktype_mask[SDF_BLOCKTYPE_PLAIN_MESH]        = 1;
        blocktype_mask[SDF_BLOCKTYPE_POINT_MESH]        = 1;
        blocktype_mask[SDF_BLOCKTYPE_UNSTRUCTURED_MESH] = 1;
        blocktype_mask[SDF_BLOCKTYPE_LAGRANGIAN_MESH]   = 1;
    }

    if (array_blocktypes) {
        blocktype_mask[SDF_BLOCKTYPE_PLAIN_VARIABLE] = 1;
        blocktype_mask[SDF_BLOCKTYPE_POINT_VARIABLE] = 1;
        blocktype_mask[SDF_BLOCKTYPE_PLAIN_DERIVED]  = 1;
        blocktype_mask[SDF_BLOCKTYPE_POINT_DERIVED]  = 1;
        blocktype_mask[SDF_BLOCKTYPE_ARRAY]          = 1;
    }

    if (nblist == 0)
        return;

    for (i=0; i < sdf_blocktype_len; i++) {
        for (n = blist_start; n < nblist; n++) {
            if (i < blocktype_list[n].start)
                break;
            if (i <= blocktype_list[n].end) {
                blocktype_mask[i] = 1;
                break;
            }
            blist_start++;
        }
    }

    free(blocktype_list);
}


char **parse_args(int *argc, char ***argv)
{
    char *ptr, *tmp_optarg, **files = NULL;
    char **pargv = *argv;
    int c, i, err, got_include, got_exclude, len;
#ifdef WIN32
    struct __stat64 statbuf;
#else
    struct stat statbuf;
#endif
    static struct option longopts[] = {
        { "abserr",          optional_argument, NULL, 'a' },
        { "array-blocks",    no_argument,       NULL, 'A' },
        { "no-nblocks",      no_argument,       NULL, 'b' },
        { "block-types",     required_argument, NULL, 'B' },
        { "show-errors",     no_argument,       NULL, 'E' },
        { "format-float",    required_argument, NULL, 'F' },
        { "help",            no_argument,       NULL, 'h' },
        { "no-summary",      no_argument,       NULL, 'i' },
        { "c-indexing",      no_argument,       NULL, 'I' },
        { "just-id",         no_argument,       NULL, 'j' },
        { "less-verbose",    no_argument,       NULL, 'l' },
        { "metadata",        no_argument,       NULL, 'm' },
        { "mesh-blocks",     no_argument,       NULL, 'M' },
        { "format-int",      required_argument, NULL, 'N' },
        { "quiet",           no_argument,       NULL, 'q' },
        { "relerr",          optional_argument, NULL, 'r' },
        { "format-space",    required_argument, NULL, 'S' },
        { "variable",        required_argument, NULL, 'v' },
        { "exclude",         required_argument, NULL, 'x' },
        { "purge-duplicate", no_argument,       NULL, 'p' },
        { "print-types",     no_argument,       NULL, 'P' },
        { "version",         no_argument,       NULL, 'V' },
        { NULL,              0,                 NULL,  0  }
        //{ "debug",           no_argument,       NULL, 'D' },
        //{ "output",          required_argument, NULL, 'o' },
    };

    debug = index_offset = verbose_metadata = 1;
    metadata = ignore_summary = exclude_variables = 0;
    just_id = 0;
    purge_duplicate = ignore_nblocks = quiet = show_errors = 0;
    array_blocktypes = mesh_blocktypes = 0;
    variable_ids = NULL;
    variable_last_id = NULL;
    nrange_max = nrange = 0;
    nblist_max = nblist = 0;

    format_int = malloc(strlen(default_int)+1);
    memcpy(format_int, default_int, strlen(default_int)+1);

    format_float = malloc(strlen(default_float)+1);
    memcpy(format_float, default_float, strlen(default_float)+1);

    format_space = malloc(strlen(default_space)+1);
    memcpy(format_space, default_space, strlen(default_space)+1);

    got_include = got_exclude = 0;

    while ((c = getopt_long(*argc, *argv,
            "a::AbB:EF:hiIjJlmMN:qr::S:v:x:pPV", longopts, NULL)) != -1) {
        switch (c) {
        case 'a':
            tmp_optarg = optarg;
            if (!optarg && NULL != pargv[optind] && '-' != pargv[optind][0])
                tmp_optarg = pargv[optind++];
            if (tmp_optarg) {
                abserr = atof(tmp_optarg);
                relerr = DBL_MAX;
            } else {
                printf("Absolute error flag requires an argument.\n");
                printf("Default value is %g\n", abserr);
                exit(0);
            }
            break;
        case 'A':
            array_blocktypes = 1;
            break;
        case 'b':
            ignore_nblocks = 1;
            break;
        case 'B':
            parse_range(optarg, &blocktype_list, &nblist, &nblist_max);
            break;
        case 'E':
            show_errors = 1;
            break;
        case 'F':
            free(format_float);
            format_float = malloc(strlen(optarg)+1);
            memcpy(format_float, optarg, strlen(optarg)+1);
            break;
        case 'h':
            usage(0);
            break;
        case 'i':
            ignore_summary = 1;
            break;
        case 'I':
            index_offset = 0;
            break;
        case 'j':
            just_id = 1;
            break;
        case 'l':
            verbose_metadata = 0;
            break;
        case 'm':
            metadata = 1;
            break;
        case 'M':
            mesh_blocktypes = 1;
            break;
        case 'N':
            free(format_int);
            format_int = malloc(strlen(optarg)+1);
            memcpy(format_int, optarg, strlen(optarg)+1);
            break;
        case 'p':
            purge_duplicate = 1;
            break;
        case 'P':
            print_blocktypes();
            break;
        case 'q':
            quiet = 1;
            break;
        case 'r':
            tmp_optarg = optarg;
            if (!optarg && NULL != pargv[optind] && '-' != pargv[optind][0])
                tmp_optarg = pargv[optind++];
            if (tmp_optarg) {
                relerr = atof(tmp_optarg);
                abserr = DBL_MAX;
            } else {
                printf("Relative error flag requires an argument.\n");
                printf("Default value is %g\n", relerr);
                exit(0);
            }
            break;
        case 'S':
            free(format_space);
            format_space = malloc(strlen(optarg)+1);
            memcpy(format_space, optarg, strlen(optarg)+1);
            break;
        case 'V':
            printf("sdfdiff version %s\n", VERSION);
            printf("commit info: %s, %s\n", SDF_COMMIT_ID, SDF_COMMIT_DATE);
            printf("library commit info: %s, %s\n",
                   sdf_get_library_commit_id(), sdf_get_library_commit_date());
            exit(0);
            break;
        case 'v':
        case 'x':
            err = 0;
            if (c == 'v') {
                if (got_exclude) err = 1;
                got_include = 1;
            } else {
                if (got_include) err = 1;
                got_exclude = 1;
                exclude_variables = 1;
            }
            if (err) {
                fprintf(stderr, "ERROR: cannot both include and "
                        "exclude variables.\n");
                exit(1);
            }
            err = parse_range(optarg, &range_list, &nrange, &nrange_max);
            if (err) {
                if (!variable_ids) {
                    variable_last_id =
                            variable_ids = malloc(sizeof(*variable_ids));
                } else {
                    variable_last_id->next = malloc(sizeof(*variable_ids));
                    variable_last_id = variable_last_id->next;
                }
                variable_last_id->next = NULL;
                variable_last_id->id = malloc(strlen(optarg)+1);
                memcpy(variable_last_id->id, optarg, strlen(optarg)+1);
            }
            break;
        default:
            usage(1);
        }
    }

    if ((optind+2) == *argc) {
        files = calloc(2, sizeof(*files));
        for (i=0; i<2; i++) {
            files[i] = (*argv)[optind+i];
#ifdef WIN32
            err = _stat64(files[i], &statbuf);
#else
            err = stat(files[i], &statbuf);
#endif
            if (err) {
                fprintf(stderr, "Error opening file %s\n", files[i]);
                exit(1);
            }
        }
        if (S_ISDIR(statbuf.st_mode)) {
            len = strlen(files[0]);
            ptr = files[0] + len;
            for (i = 0; i < len; ++i, --ptr) {
                if (*ptr == '/') {
                    ptr++;
                    break;
                }
            }
            len = strlen(files[1]);
            for (; len > 0; --len) {
                if (files[1][len] != '/')
                    break;
            }

            files[1] = malloc(len + strlen(ptr) + 2);
            memcpy(files[1], (*argv)[optind+1], len);
            if (*(files[1]+len-1) != '/') {
                *(files[1]+len) = '/';
                len++;
            }
            memcpy(files[1]+len, ptr, strlen(ptr));
            files[1][len + strlen(ptr)] = '\0';
        }
    } else {
        files = NULL;
        fprintf(stderr, "Must specify two files\n");
        usage(1);
    }

    sort_range(&range_list, &nrange);
    sort_range(&blocktype_list, &nblist);
    setup_blocklist_mask();

    parse_format();

    if (quiet)
        done_header = 1;

    return files;
}


void free_memory(sdf_file_t **handles)
{

    if (format_int) free(format_int);
    if (format_float) free(format_float);
    if (format_space) free(format_space);
    sdf_stack_destroy(handles[0]);
    sdf_stack_destroy(handles[1]);
}


static void print_value(void *data, int datatype)
{
    int exponent;
    int64_t i64;
    double r8;

    switch (datatype) {
    case SDF_DATATYPE_INTEGER4:
        i64 = *((int32_t*)data);
        printf(format_int, i64);
        break;
    case SDF_DATATYPE_INTEGER8:
        printf(format_int, *((int64_t*)data));
        break;
    case SDF_DATATYPE_REAL4:
        if (special_format) {
            r8 = *((float*)data);
            if (r8 == 0)
                exponent = 0;
            else {
                exponent = (int)floor(log10(fabs(r8))+FLT_EPSILON) + 1;
                exponent -= scale_factor;
                r8 *= pow(10, -1.0 * exponent);
            }
            if (r8 == INFINITY)
                printf("Infinity");
            else
                printf(format_float, r8, exponent);
        } else
            printf(format_float, *((float*)data));
        break;
    case SDF_DATATYPE_REAL8:
        if (special_format) {
            r8 = *((double*)data);
            if (r8 == 0)
                exponent = 0;
            else {
                exponent = (int)floor(log10(fabs(r8))+FLT_EPSILON) + 1;
                exponent -= scale_factor;
                r8 *= pow(10, -1.0 * exponent);
            }
            if (r8 == INFINITY)
                printf("Infinity");
            else
                printf(format_float, r8, exponent);
        } else
            printf(format_float, *((double*)data));
        break;
    //case SDF_DATATYPE_REAL16:
    //    printf(format_float, (double)b->const_value);
    //    break;
    case SDF_DATATYPE_CHARACTER:
        printf("%c", *((char*)data));
        break;
    case SDF_DATATYPE_LOGICAL:
        if (*((char*)data))
            printf("T");
        else
            printf("F");
        break;
    }
}


static void print_metadata_plain_mesh(sdf_block_t *b)
{
    // Metadata is
    // - mults     REAL(r8), DIMENSION(ndims)
    // - labels    CHARACTER(id_length), DIMENSION(ndims)
    // - units     CHARACTER(id_length), DIMENSION(ndims)
    // - geometry  INTEGER(i4)
    // - minval    REAL(r8), DIMENSION(ndims)
    // - maxval    REAL(r8), DIMENSION(ndims)
    // - dims      INTEGER(i4), DIMENSION(ndims)

    SET_WIDTH("dim_labels:");
    if (verbose_metadata)
        PRINTAR("dim_mults:", b->dim_mults, format_float, b->ndims);
    PRINTAR("dim_labels:", b->dim_labels, "%s", b->ndims);
    PRINTAR("dim_units:", b->dim_units, "%s", b->ndims);
    PRINT("geometry:", sdf_geometry_c[b->geometry], "%s");
    PRINTAR("extents:", b->extents, format_float, 2*b->ndims);
    PRINTAR("dims:", b->dims, "%" PRIi64, b->ndims);
}


static void print_metadata_point_mesh(sdf_block_t *b)
{
    // Metadata is
    // - mults     REAL(r8), DIMENSION(ndims)
    // - labels    CHARACTER(id_length), DIMENSION(ndims)
    // - units     CHARACTER(id_length), DIMENSION(ndims)
    // - geometry  INTEGER(i4)
    // - minval    REAL(r8), DIMENSION(ndims)
    // - maxval    REAL(r8), DIMENSION(ndims)
    // - npoints   INTEGER(i8)
    // - speciesid CHARACTER(id_length)

    SET_WIDTH("dim_labels:");
    if (verbose_metadata)
        PRINTAR("dim_mults", b->dim_mults, format_float, b->ndims);
    PRINTAR("dim_labels", b->dim_labels, "%s", b->ndims);
    PRINTAR("dim_units", b->dim_units, "%s", b->ndims);
    PRINT("geometry:", sdf_geometry_c[b->geometry], "%s");
    PRINTAR("extents", b->extents, format_float, 2*b->ndims);
    //PRINTAR("dims", b->dims, "%" PRIi64, b->ndims);
    PRINT("nelements:", b->nelements, "%" PRIi64);
    if (b->material_id)
        PRINT("species id:", b->material_id, "%s");
}


static void print_metadata_plain_variable(sdf_block_t *b)
{
    // Metadata is
    // - mult      REAL(r8)
    // - units     CHARACTER(id_length)
    // - meshid    CHARACTER(id_length)
    // - dims      INTEGER(i4), DIMENSION(ndims)
    // - stagger   INTEGER(i4)

    SET_WIDTH("mesh id:");
    PRINT("units:", b->units, "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINTAR("dims:", b->dims, "%" PRIi64, b->ndims);
    if (verbose_metadata) {
        PRINT("mult:", b->mult, format_float);
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    }
}


static void print_metadata_point_variable(sdf_block_t *b)
{
    // Metadata is
    // - mult      REAL(r8)
    // - units     CHARACTER(id_length)
    // - meshid    CHARACTER(id_length)
    // - npoints   INTEGER(i8)
    // - speciesid CHARACTER(id_length)

    SET_WIDTH("species id:");
    PRINT("units:", b->units, "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINT("nelements:", b->nelements, "%" PRIi64);
    if (b->material_id)
        PRINT("species id:", b->material_id, "%s");
    if (verbose_metadata)
        PRINT("mult:", b->mult, format_float);
}


static void print_metadata_constant(sdf_block_t *b)
{
    // Metadata is
    // - value     TYPE_SIZE

    printf("%svalue: ", indent);
    print_value(b->const_value, b->datatype);
    printf("\n");
}


static void print_metadata_array(sdf_block_t *b)
{
    // Metadata is
    // - dims      INTEGER(i4), DIMENSION(ndims)

    SET_WIDTH("dims:");
    PRINTAR("dims:", b->dims, "%" PRIi64, b->ndims);
}


static void print_metadata_cpu_split(sdf_block_t *b)
{
    // Metadata is
    // - dims      INTEGER(i4), DIMENSION(ndims)

    SET_WIDTH("geometry:");
    PRINT("geometry:", sdf_geometry_c[b->geometry], "%s");
    PRINTAR("dims:", b->dims, "%" PRIi64, b->ndims);
}


static void print_metadata_run(sdf_block_t *b)
{
    struct run_info *run = b->data;
    time_t time;
    char *stime;
    char version[32];

    // Metadata is
    // - version   INTEGER(i4)
    // - revision  INTEGER(i4)
    // - commit_id CHARACTER(string_length)
    // - sha1sum   CHARACTER(string_length)
    // - compmac   CHARACTER(string_length)
    // - compflag  CHARACTER(string_length)
    // - defines   INTEGER(i8)
    // - compdate  INTEGER(i4)
    // - rundate   INTEGER(i4)
    // - iodate    INTEGER(i4)
    // - minor_rev INTEGER(i4)

    SET_WIDTH("compile_machine:");
    snprintf(version, 32, "%i.%i.%i",
             run->version, run->revision, run->minor_rev);
    stime = version;
    PRINT("version:", stime, "%s");
    PRINT("commit id:", run->commit_id, "%s");
    PRINT("sha1sum:", run->sha1sum, "%s");
    PRINT("compile_machine:", run->compile_machine, "%s");
    PRINT("compile_flags:", run->compile_flags, "%s");
    PRINT("defines:", run->defines, "%" PRIi64);
    time = run->compile_date; stime = ctime(&time); stime[strlen(stime)-1]='\0';
    PRINT("compile_date:", stime, "%s");
    time = run->run_date; stime = ctime(&time); stime[strlen(stime)-1] = '\0';
    PRINT("run_date:", stime, "%s");
    time = run->io_date; stime = ctime(&time); stime[strlen(stime)-1] = '\0';
    PRINT("io_date:", stime, "%s");
}


static void print_metadata_stitched(sdf_block_t *b)
{
    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - varids    ndims*CHARACTER(id_length)

    SET_WIDTH("variable ids:");
    if (verbose_metadata)
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINTAR("variable ids:", b->variable_ids, "%s", b->ndims);
}


static void print_metadata_stitched_material(sdf_block_t *b)
{
    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matnames  ndims*CHARACTER(string_length)
    // - varids    ndims*CHARACTER(id_length)

    SET_WIDTH("material names:");
    if (verbose_metadata)
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINTAR("material names:", b->material_names, "%s", b->ndims);
    PRINTAR("variable ids:", b->variable_ids, "%s", b->ndims);
}


static void print_metadata_stitched_matvar(sdf_block_t *b)
{
    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matid     CHARACTER(id_length)
    // - varids    ndims*CHARACTER(id_length)

    SET_WIDTH("variable ids:");
    if (verbose_metadata)
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINT("material id:", b->material_id, "%s");
    PRINTAR("variable ids:", b->variable_ids, "%s", b->ndims);
}


static void print_metadata_stitched_species(sdf_block_t *b)
{
    // Metadata is
    // - stagger   INTEGER(i4)
    // - meshid    CHARACTER(id_length)
    // - matid     CHARACTER(id_length)
    // - matname   CHARACTER(string_length)
    // - specnames ndims*CHARACTER(string_length)
    // - varids    ndims*CHARACTER(id_length)

    SET_WIDTH("species names:");
    if (verbose_metadata)
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    PRINT("mesh id:", b->mesh_id, "%s");
    PRINT("material id:", b->material_id, "%s");
    PRINT("material name:", b->material_name, "%s");
    PRINTAR("species names:", b->material_names, "%s", b->ndims);
    PRINTAR("variable ids:", b->variable_ids, "%s", b->ndims);
}


static void print_metadata_stitched_obstacle_group(sdf_block_t *b)
{
    // Metadata is
    // - stagger         INTEGER(i4)
    // - obstacle_id     CHARACTER(id_length)
    // - vfm_id          CHARACTER(id_length)
    // - obstacle_names  ndims*CHARACTER(string_length)

    SET_WIDTH("volume fraction id:");
    if (verbose_metadata)
        PRINT("stagger:", sdf_stagger_c[b->stagger], "%s");
    PRINT("obstacle id:", b->obstacle_id, "%s");
    PRINT("volume fraction id:", b->vfm_id, "%s");
    PRINTAR("obstacle names:", b->material_names, "%s", b->ndims);
}


static void print_metadata_station(sdf_block_t *b)
{
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

    SET_WIDTH("time_increment:");
    PRINT("nelements:", b->nelements, "%" PRIi64);
    PRINT("entry_len:", b->type_size, "%i");
    PRINT("nstations:", b->nstations, "%i");
    PRINT("nvariables:", b->nvariables, "%i");
    PRINT("step0:", b->step, "%i");
    PRINT("step_increment:", b->step_increment, "%i");
    PRINT("time0:", b->time, format_float);
    PRINT("time_increment:", b->time_increment, format_float);
    PRINTAR("station_ids:", b->station_ids, "%s", b->nstations);
    PRINTAR("station_names:", b->station_names, "%s", b->nstations);
    PRINTAR("station_nvars:", b->station_nvars, "%i", b->nstations);
    PRINTAR("station_move:", b->station_move, "%i", b->nstations);
    PRINTAR("station_x:", b->station_x, format_float, b->nstations);
    if (b->ndims > 1)
        PRINTAR("station_y:", b->station_y, format_float, b->nstations);
    if (b->ndims > 2)
        PRINTAR("station_z:", b->station_z, format_float, b->nstations);
    PRINTAR("variable_ids:", b->variable_ids, "%s", b->nvariables);
    PRINTAR("variable_names:", b->material_names, "%s", b->nvariables);
    PRINTDAR("variable_types:", b->variable_types, "%s", b->nvariables);
    PRINTAR("variable_units:", b->dim_units, "%s", b->nvariables);
    if (b->dim_mults && verbose_metadata)
        PRINTAR("variable_mults:", b->dim_mults, format_float, b->nvariables);
}


static void print_metadata_datablock(sdf_block_t *b)
{
    // Metadata is
    // - mimetype       CHARACTER(id_length)
    // - checksum_type  CHARACTER(id_length)
    // - checksum       CHARACTER(string_length)

    SET_WIDTH("checksum_type:");
    PRINT("mimetype:", b->mimetype, "%s");
    PRINT("checksum_type:", b->checksum_type, "%s");
    PRINT("checksum:", b->checksum, "%s");
    PRINTAR("species names:", b->material_names, "%s", b->ndims);
}


static void print_metadata_namevalue(sdf_block_t *b)
{
    int i, len, max;
    int32_t *i4;
    int64_t *i8;
    float *r4;
    double *r8;
    char *logical;
    char **string;

    // Metadata is
    // - names     ndims*CHARACTER(string_length)
    // - values    ndims*DATATYPE

    max = 0;
    for (i = 0; i < b->ndims; i++) {
        len = strlen(b->material_names[i]);
        if (len > max) max = len;
    }

    SET_WIDTH_LEN(max);
    switch (b->datatype) {
    case(SDF_DATATYPE_INTEGER4):
        i4 = b->data;
        for (i = 0; i < b->ndims; i++) {
            PRINT(b->material_names[i], i4[i], "%i");
        }
        break;
    case(SDF_DATATYPE_INTEGER8):
        i8 = b->data;
        for (i = 0; i < b->ndims; i++) {
            PRINT(b->material_names[i], i8[i], "%" PRIi64);
        }
        break;
    case(SDF_DATATYPE_REAL4):
        r4 = b->data;
        for (i = 0; i < b->ndims; i++) {
            PRINT(b->material_names[i], r4[i], format_float);
        }
        break;
    case(SDF_DATATYPE_REAL8):
        r8 = b->data;
        for (i = 0; i < b->ndims; i++) {
            PRINT(b->material_names[i], r8[i], format_float);
        }
        break;
    case(SDF_DATATYPE_LOGICAL):
        logical = b->data;
        for (i = 0; i < b->ndims; i++) {
            printf(indent, 1);
            printf(width_fmt, b->material_names[i]);
            if (logical[i])
                printf("True");
            else
                printf("False");
            printf("\n");
        }
        break;
    case(SDF_DATATYPE_CHARACTER):
        string = b->data;
        for (i = 0; i < b->ndims; i++) {
            PRINT(b->material_names[i], string[i], "%s");
        }
        break;
    }
}


static void print_metadata_id(sdf_block_t *b, int inum, int nblocks)
{
    int digit = 0;
    static const int fmtlen = 64;
    // char fmt[fmtlen]; doesn't work with Microsoft Visual C, so:
    char fmt[64];

    while (nblocks) {
        nblocks /= 10;
        digit++;
    }

    snprintf(fmt, fmtlen, "\nBlock %%%ii", digit);
    printf(fmt, inum);
    printf(", ID: %s", b->id);
}


static void print_metadata(sdf_block_t *b, int inum, int nblocks)
{
    print_header();
    print_metadata_id(b, inum, nblocks);
    printf(" - not in second file\n");
    if (just_id) return;

    sprintf(indent, default_indent, 1);

    if (verbose_metadata)
        SET_WIDTH("block_location:");
    else
        SET_WIDTH("blocktype:");

    PRINT("name:", b->name, "%s");
    PRINT("blocktype:", sdf_blocktype_c[b->blocktype], "%s");
    PRINT("datatype:", sdf_datatype_c[b->datatype], "%s");

    if (verbose_metadata) {
        PRINT("ndims:", b->ndims, "%i");
        PRINT("data_length:", b->data_length, "%" PRIi64);
        PRINT("info_length:", b->info_length, "%i");
        PRINT("data_location:", b->data_location, "%" PRIi64);
        PRINT("block_location:", b->block_start, "%" PRIi64);
        PRINT("next_block:", b->next_block_location, "%" PRIi64);
    }

    sprintf(indent+strlen(indent), default_indent, 1);

    switch (b->blocktype) {
    case SDF_BLOCKTYPE_PLAIN_MESH:
    case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
        print_metadata_plain_mesh(b);
        break;
    case SDF_BLOCKTYPE_POINT_MESH:
        print_metadata_point_mesh(b);
        break;
    case SDF_BLOCKTYPE_PLAIN_VARIABLE:
    case SDF_BLOCKTYPE_PLAIN_DERIVED:
        print_metadata_plain_variable(b);
        break;
    case SDF_BLOCKTYPE_POINT_VARIABLE:
    case SDF_BLOCKTYPE_POINT_DERIVED:
        print_metadata_point_variable(b);
        break;
    case SDF_BLOCKTYPE_CONSTANT:
        print_metadata_constant(b);
        break;
    case SDF_BLOCKTYPE_ARRAY:
        print_metadata_array(b);
        break;
    case SDF_BLOCKTYPE_CPU_SPLIT:
        print_metadata_cpu_split(b);
        break;
    case SDF_BLOCKTYPE_RUN_INFO:
        print_metadata_run(b);
        break;
    case SDF_BLOCKTYPE_STITCHED:
    case SDF_BLOCKTYPE_CONTIGUOUS:
    case SDF_BLOCKTYPE_STITCHED_TENSOR:
    case SDF_BLOCKTYPE_CONTIGUOUS_TENSOR:
        print_metadata_stitched(b);
        break;
    case SDF_BLOCKTYPE_STITCHED_MATERIAL:
    case SDF_BLOCKTYPE_CONTIGUOUS_MATERIAL:
        print_metadata_stitched_material(b);
        break;
    case SDF_BLOCKTYPE_STITCHED_MATVAR:
    case SDF_BLOCKTYPE_CONTIGUOUS_MATVAR:
        print_metadata_stitched_matvar(b);
        break;
    case SDF_BLOCKTYPE_STITCHED_SPECIES:
    case SDF_BLOCKTYPE_CONTIGUOUS_SPECIES:
        print_metadata_stitched_species(b);
        break;
    case SDF_BLOCKTYPE_STITCHED_OBSTACLE_GROUP:
        print_metadata_stitched_obstacle_group(b);
        break;
    case SDF_BLOCKTYPE_STATION:
    case SDF_BLOCKTYPE_STATION_DERIVED:
        print_metadata_station(b);
        break;
    case SDF_BLOCKTYPE_DATABLOCK:
        print_metadata_datablock(b);
        break;
    case SDF_BLOCKTYPE_NAMEVALUE:
        print_metadata_namevalue(b);
        break;
    }

    printf("\n");
}


static void print_data(sdf_block_t *b)
{
    if (!b->done_data)
        fprintf(stderr, "Data not read.\n");
    else
        fwrite(b->data, 1, b->data_length, stdout);
}


void get_index_str(sdf_block_t *b, int64_t n, int *idx, int *fac, char **fmt,
                   char *str)
{
    int64_t rem, idx0;
    int i, len;

    *str = '\0';
    if (b->ndims < 1)
        return;

    switch (b->blocktype) {
    case SDF_BLOCKTYPE_PLAIN_DERIVED:
    case SDF_BLOCKTYPE_PLAIN_VARIABLE:
    case SDF_BLOCKTYPE_POINT_DERIVED:
    case SDF_BLOCKTYPE_POINT_VARIABLE:
    case SDF_BLOCKTYPE_ARRAY:
    case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
        break;
    case SDF_BLOCKTYPE_PLAIN_MESH:
    case SDF_BLOCKTYPE_POINT_MESH:
        i = idx[0];
        str[0] = '(';
        sprintf(str+1, fmt[i], n + index_offset);
        len = strlen(str);
        str[len] = ')';
        str[len+1] = '\0';
        return;
    default:
        return;
    }

    rem = n;
    for (i = b->ndims-1; i >= 0; i--) {
        idx0 = idx[i] = rem / fac[i];
        rem -= idx0 * fac[i];
    }

    str[0] = '(';
    str[1] = '\0';
    for (i = 0; i < b->ndims; i++) {
        len = strlen(str);
        sprintf(str+len, fmt[i], idx[i] + index_offset);
    }
    len = strlen(str);
    str[len] = ')';
    str[len+1] = '\0';
}


void set_header_string(sdf_file_t **handles)
{
    int len;
    char *name;
    struct stat st;
    struct tm *tm;
    static const int idxlen = 64;
    // char prestr[idxlen]; doesn't work with Microsoft Visual C, so:
    char prestr[64];

    if (done_header)
        return;

    name = handles[0]->filename;
    stat(name, &st);
    tm = localtime(&st.st_mtime);
    //strftime(prestr, idxlen, "%Y-%m-%d %H:%M:%S.%N %z", tm);
    strftime(prestr, idxlen, "%Y-%m-%d %H:%M:%S.000000000 %z", tm);
    snprintf(header_string, HEADER_LEN, "--- %s\t%s\n", name, prestr);

    name = handles[1]->filename;
    stat(name, &st);
    tm = localtime(&st.st_mtime);
    strftime(prestr, idxlen, "%Y-%m-%d %H:%M:%S.000000000 %z", tm);
    len = strlen(header_string);
    snprintf(header_string+len, HEADER_LEN-len, "+++ %s\t%s\n", name, prestr);
}


static inline void print_header(void)
{
    if (done_header)
        return;

    printf("%s", header_string);
    done_header = 1;
}


#define PRINT_DIFF(v1, v2, format) do { \
    get_index_str(b, n, idx, fac, fmt, idxstr); \
    printf("-%s%s: ", prestr, idxstr); \
    printf(format, (v1)); \
    printf("\n"); \
    printf("+%s%s: ", prestr, idxstr); \
    printf(format, (v2)); \
    printf("\n"); \
    if (show_errors) \
        printf(" Error absolute %25.17e, relative %25.17e\n", \
               abserr_val, relerr_val); \
} while(0)


#define DIFF(v1, v2, pv1, pv2, format) do { \
    val1 = (v1); \
    val2 = (v2); \
    denom = MIN(ABS(val1), ABS(val2)); \
    abserr_val = ABS(val1 - val2); \
    if (denom < DBL_MIN) { \
        if (abserr_val < DBL_MIN) \
            relerr_val = 0; \
        else \
            relerr_val = 1; \
    } else \
        relerr_val = abserr_val / denom; \
    if (relerr_val >= relerr || abserr_val >= abserr) { \
        /* If we got here then the numbers differ */ \
        print_header(); \
        gotdiff = 1; \
        if (!quiet) { \
            if (!gotblock) { \
                gotblock = 1; \
                print_metadata_id(b, inum, handles[0]->nblocks); \
                printf("\n"); \
            } \
            if (!just_id) { \
                PRINT_DIFF(pv1, pv2, format); \
            } \
        } \
        if (relerr_val > relerr_max) relerr_max = relerr_val; \
        if (abserr_val > abserr_max) abserr_max = abserr_val; \
    } \
} while(0)


#define LDIFF(v1, v2, pv1, pv2, format) do { \
    i1 = (v1); \
    i2 = (v2); \
    if (i1 != i2) { \
        /* If we got here then the numbers differ */ \
        abserr_val = abserr_max = relerr_val = relerr_max = 1.0; \
        print_header(); \
        gotdiff = 1; \
        if (!quiet) { \
            if (!gotblock) { \
                gotblock = 1; \
                print_metadata_id(b, inum, handles[0]->nblocks); \
                printf("\n"); \
            } \
            if (!just_id) { \
                PRINT_DIFF(pv1, pv2, format); \
            } \
        } \
    } \
} while(0)


#define DIFF_PREAMBLE() do { \
    switch (b->datatype) { \
    case(SDF_DATATYPE_INTEGER4): \
    case(SDF_DATATYPE_INTEGER8): \
    case(SDF_DATATYPE_REAL4): \
    case(SDF_DATATYPE_REAL8): \
    case(SDF_DATATYPE_LOGICAL): \
        break; \
    default: \
        return gotdiff; \
    } \
\
    sdf_helper_read_data(handles[0], b1); \
    sdf_helper_read_data(handles[1], b2); \
\
    gotblock = 0; \
    abserr_max = relerr_max = 0.0; \
} while(0)


#define DIFF_PRINT_MAX_ERROR() do { \
    if (!quiet && gotblock) { \
        print_header(); \
        if (!gotblock) { \
            print_metadata_id(b, inum, handles[0]->nblocks); \
            printf("\n"); \
        } \
        printf("Max error absolute %25.17e, relative %25.17e\n", \
               abserr_max, relerr_max); \
    } \
} while(0)


int diff_plain(sdf_file_t **handles, sdf_block_t *b1, sdf_block_t *b2, int inum)
{
    int32_t *i4_1, *i4_2;
    int64_t *i8_1, *i8_2;
    float *r4_1, *r4_2;
    double *r8_1, *r8_2;
    double val1, val2;
    int64_t ival1, ival2;
    int i1, i2;
    char clogical[2] = {'F', 'T'};
    char *l_1, *l_2;
    int gotblock;
    int gotdiff = 0;
    static const int fmtlen = 32;
    static const int idxlen = 64;
    // char idxstr[idxlen]; doesn't work with Microsoft Visual C, so:
    char idxstr[64];
    char prestr[64];
    sdf_block_t *b = b1;
    double relerr_max, relerr_val, abserr_max, abserr_val, denom;
    int64_t n = 0;
    int *idx = NULL, *fac = NULL;
    char **fmt = NULL;
    int i, rem, left, digit, len;

    DIFF_PREAMBLE();

    /* Get index format */

    idx = malloc(b->ndims * sizeof(*idx));
    fac = malloc(b->ndims * sizeof(*fac));
    fmt = malloc(b->ndims * sizeof(*fmt));

    snprintf(prestr, idxlen, "%s[", b->id);

    rem = 1;
    for (i = 0; i < b->ndims; i++) {
        left = b->local_dims[i];
        fac[i] = rem;
        rem *= left;
        digit = 0;
        while (left) {
            left /= 10;
            digit++;
        }
        if (!digit) digit = 1;
        fmt[i] = malloc(fmtlen * sizeof(**fmt));
        if (i == 0)
            snprintf(fmt[i], fmtlen, "%%%ii", digit);
        else
            snprintf(fmt[i], fmtlen, ",%%%ii", digit);
        len = strlen(prestr);
        snprintf(prestr+len, idxlen-len, fmt[i], b->dims[i]);
    }
    len = strlen(prestr);
    snprintf(prestr+len, idxlen-len, "] ");

    switch (b->datatype) {
    case(SDF_DATATYPE_INTEGER4):
        i4_1 = b1->data;
        i4_2 = b2->data;
        for (n = 0; n < b->nelements_local; n++) {
            ival1 = i4_1[n];
            ival2 = i4_2[n];
            DIFF(ival1, ival2, ival1, ival2, format_int);
        }
        break;
    case(SDF_DATATYPE_INTEGER8):
        i8_1 = b1->data;
        i8_2 = b2->data;
        for (n = 0; n < b->nelements_local; n++) {
            ival1 = i8_1[n];
            ival2 = i8_2[n];
            DIFF(ival1, ival2, ival1, ival2, format_int);
        }
        break;
    case(SDF_DATATYPE_REAL4):
        r4_1 = b1->data;
        r4_2 = b2->data;
        for (n = 0; n < b->nelements_local; n++) {
            DIFF(r4_1[n], r4_2[n], val1, val2, format_float);
        }
        break;
    case(SDF_DATATYPE_REAL8):
        r8_1 = b1->data;
        r8_2 = b2->data;
        for (n = 0; n < b->nelements_local; n++) {
            DIFF(r8_1[n], r8_2[n], val1, val2, format_float);
        }
        break;
    case(SDF_DATATYPE_LOGICAL):
        l_1 = b1->data;
        l_2 = b2->data;
        for (n = 0; n < b->nelements_local; n++) {
            LDIFF(l_1[n], l_2[n], clogical[i1], clogical[i2], "%c");
        }
        break;
    }

    DIFF_PRINT_MAX_ERROR();

    for (i = 0; i < b->ndims; i++) free(fmt[i]);
    free(fmt);
    free(idx);
    free(fac);

    return gotdiff;
}


int diff_mesh(sdf_file_t **handles, sdf_block_t *b1, sdf_block_t *b2, int inum)
{
    int32_t *i4_1, *i4_2;
    int64_t *i8_1, *i8_2;
    float *r4_1, *r4_2;
    double *r8_1, *r8_2;
    double val1, val2;
    int64_t ival1, ival2;
    int gotblock;
    static int gotdiff = 0;
    static const int fmtlen = 32;
    static const int idxlen = 64;
    // char idxstr[idxlen]; doesn't work with Microsoft Visual C, so:
    char idxstr[64];
    char *prestr;
    char **prestr_dim;
    sdf_block_t *b = b1;
    double relerr_max, relerr_val, abserr_max, abserr_val, denom;
    int64_t n = 0;
    int *idx = NULL, *fac = NULL;
    char **fmt = NULL;
    int i, rem, left, digit, len;

    DIFF_PREAMBLE();

    /* Get index format */

    idx = malloc(b->ndims * sizeof(*idx));
    fac = malloc(b->ndims * sizeof(*fac));
    fmt = malloc(b->ndims * sizeof(*fmt));
    prestr_dim = malloc(b->ndims * sizeof(*prestr_dim));

    for (i = 0; i < b->ndims; i++) {
        fac[i] = 1;
        left = b->local_dims[i] + index_offset - 1;
        digit = 0;
        while (left) {
            left /= 10;
            digit++;
        }
        if (!digit) digit = 1;
        fmt[i] = malloc(fmtlen * sizeof(**fmt));
        snprintf(fmt[i], fmtlen, "%%%ii", digit);

        rem = strlen(b->dim_labels[i]) + strlen(fmt[i]) + 4;
        prestr_dim[i] = malloc(rem * sizeof(**prestr_dim));
        snprintf(prestr_dim[i], rem, "%s[", b->dim_labels[i]);
        len = strlen(prestr_dim[i]);
        left = rem - len;
        snprintf(prestr_dim[i]+len, rem-len, fmt[i], b->dims[i]);
        len = strlen(prestr_dim[i]);
        snprintf(prestr_dim[i]+len, rem-len, "] ");
    }
    idxstr[0] = '\0';

    switch (b->datatype) {
    case(SDF_DATATYPE_INTEGER4):
        for (i = 0; i < b->ndims; i++) {
            prestr = prestr_dim[i];
            idx[0] = i;
            i4_1 = ((int32_t**)b1->grids)[i];
            i4_2 = ((int32_t**)b2->grids)[i];
            for (n = 0; n < b->dims[i]; n++) {
                ival1 = i4_1[n];
                ival2 = i4_2[n];
                DIFF(ival1, ival2, ival1, ival2, format_int);
            }
        }
        break;
    case(SDF_DATATYPE_INTEGER8):
        for (i = 0; i < b->ndims; i++) {
            prestr = prestr_dim[i];
            idx[0] = i;
            i8_1 = ((int64_t**)b1->grids)[i];
            i8_2 = ((int64_t**)b2->grids)[i];
            for (n = 0; n < b->dims[i]; n++) {
                ival1 = i8_1[n];
                ival2 = i8_2[n];
                DIFF(ival1, ival2, ival1, ival2, format_int);
            }
        }
        break;
    case(SDF_DATATYPE_REAL4):
        for (i = 0; i < b->ndims; i++) {
            prestr = prestr_dim[i];
            idx[0] = i;
            r4_1 = ((float**)b1->grids)[i];
            r4_2 = ((float**)b2->grids)[i];
            for (n = 0; n < b->dims[i]; n++) {
                DIFF(r4_1[n], r4_2[n], val1, val2, format_float);
            }
        }
        break;
    case(SDF_DATATYPE_REAL8):
        for (i = 0; i < b->ndims; i++) {
            prestr = prestr_dim[i];
            idx[0] = i;
            r8_1 = ((double**)b1->grids)[i];
            r8_2 = ((double**)b2->grids)[i];
            for (n = 0; n < b->dims[i]; n++) {
                DIFF(r8_1[n], r8_2[n], val1, val2, format_float);
            }
        }
        break;
    }

    DIFF_PRINT_MAX_ERROR();

    for (i = 0; i < b->ndims; i++) free(fmt[i]);
    free(fmt);
    free(idx);
    free(fac);

    return gotdiff;
}


int diff_constant(sdf_file_t **handles, sdf_block_t *b1, sdf_block_t *b2,
                  int inum)
{
    int32_t i4_1, i4_2;
    int64_t i8_1, i8_2;
    float r4_1, r4_2;
    double r8_1, r8_2;
    double val1, val2;
    int64_t ival1, ival2;
    int i1, i2;
    char clogical[2] = {'F', 'T'};
    int gotblock;
    static int gotdiff = 0;
    char idxstr[1] = {'\0'};
    char *prestr;
    sdf_block_t *b = b1;
    double relerr_max, relerr_val, abserr_max, abserr_val, denom;
    int64_t n = 0;
    int *idx = NULL, *fac = NULL;
    char **fmt = NULL;

    DIFF_PREAMBLE();

    prestr = b->id;

    switch (b->datatype) {
    case SDF_DATATYPE_INTEGER4:
        memcpy(&i4_1, b1->const_value, sizeof(i4_1));
        memcpy(&i4_2, b2->const_value, sizeof(i4_2));
        ival1 = i4_1;
        ival2 = i4_2;
        DIFF(ival1, ival2, ival1, ival2, format_int);
        break;
    case SDF_DATATYPE_INTEGER8:
        memcpy(&i8_1, b1->const_value, sizeof(i8_1));
        memcpy(&i8_2, b2->const_value, sizeof(i8_2));
        ival1 = i8_1;
        ival2 = i8_2;
        DIFF(ival1, ival2, ival1, ival2, format_int);
        break;
    case SDF_DATATYPE_REAL4:
        memcpy(&r4_1, b1->const_value, sizeof(r4_1));
        memcpy(&r4_2, b2->const_value, sizeof(r4_2));
        DIFF(r4_1, r4_2, val1, val2, format_float);
        break;
    case SDF_DATATYPE_REAL8:
        memcpy(&r8_1, b1->const_value, sizeof(r8_1));
        memcpy(&r8_2, b2->const_value, sizeof(r8_2));
        DIFF(r8_1, r8_2, val1, val2, format_float);
        break;
    case SDF_DATATYPE_LOGICAL:
        LDIFF(*b1->const_value, *b2->const_value,
              clogical[i1], clogical[i2], "%c");
        break;
    }

    DIFF_PRINT_MAX_ERROR();

    return gotdiff;
}


int diff_namevalue(sdf_file_t **handles, sdf_block_t *b1, sdf_block_t *b2,
                   int inum)
{
    int32_t *i4_1, *i4_2;
    int64_t *i8_1, *i8_2;
    float *r4_1, *r4_2;
    double *r8_1, *r8_2;
    double val1, val2;
    int64_t ival1, ival2;
    int i1, i2;
    char clogical[2] = {'F', 'T'};
    char *l_1, *l_2;
    int gotblock;
    static int gotdiff = 0;
    char idxstr[1] = {'\0'};
    char *prestr;
    sdf_block_t *b = b1;
    double relerr_max, relerr_val, abserr_max, abserr_val, denom;
    int64_t n = 0;
    int *idx = NULL, *fac = NULL;
    char **fmt = NULL;

    DIFF_PREAMBLE();

    switch (b->datatype) {
    case(SDF_DATATYPE_INTEGER4):
        i4_1 = b1->data;
        i4_2 = b2->data;
        for (n = 0; n < b->ndims; n++) {
            prestr = b->material_names[n];
            ival1 = i4_1[n];
            ival2 = i4_2[n];
            DIFF(ival1, ival2, ival1, ival2, format_int);
        }
        break;
    case(SDF_DATATYPE_INTEGER8):
        i8_1 = b1->data;
        i8_2 = b2->data;
        for (n = 0; n < b->ndims; n++) {
            prestr = b->material_names[n];
            ival1 = i8_1[n];
            ival2 = i8_2[n];
            DIFF(ival1, ival2, ival1, ival2, format_int);
        }
        break;
    case(SDF_DATATYPE_REAL4):
        r4_1 = b1->data;
        r4_2 = b2->data;
        for (n = 0; n < b->ndims; n++) {
            prestr = b->material_names[n];
            DIFF(r4_1[n], r4_2[n], val1, val2, format_float);
        }
        break;
    case(SDF_DATATYPE_REAL8):
        r8_1 = b1->data;
        r8_2 = b2->data;
        for (n = 0; n < b->ndims; n++) {
            prestr = b->material_names[n];
            DIFF(r8_1[n], r8_2[n], val1, val2, format_float);
        }
        break;
    case(SDF_DATATYPE_LOGICAL):
        l_1 = b1->data;
        l_2 = b2->data;
        for (n = 0; n < b->ndims; n++) {
            prestr = b->material_names[n];
            LDIFF(l_1[n], l_2[n], clogical[i1], clogical[i2], "%c");
        }
        break;
    }

    DIFF_PRINT_MAX_ERROR();

    return gotdiff;
}


int diff_block(sdf_file_t **handles, sdf_block_t *b1, sdf_block_t *b2, int inum)
{
    int i, gotdiff = 0;

    /* Sanity check */
    if (b1->blocktype != b2->blocktype) gotdiff = 1;
    if (b1->datatype != b2->datatype) gotdiff = 1;
    if (b1->ndims != b2->ndims) gotdiff = 1;

    switch (b1->blocktype) {
    case SDF_BLOCKTYPE_PLAIN_DERIVED:
    case SDF_BLOCKTYPE_PLAIN_VARIABLE:
    case SDF_BLOCKTYPE_POINT_DERIVED:
    case SDF_BLOCKTYPE_POINT_VARIABLE:
    case SDF_BLOCKTYPE_ARRAY:
    case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
    case SDF_BLOCKTYPE_PLAIN_MESH:
    case SDF_BLOCKTYPE_POINT_MESH:
        for (i = 0; i < b1->ndims; i++) {
            if (b1->dims[i] == b2->dims[i])
                continue;
            gotdiff = 1;
        }
        break;
    default:
        break;
    }

    if (gotdiff && !quiet) {
        print_header();
        print_metadata_id(b1, inum, handles[0]->nblocks);
        printf(" - mismatched\n");
        return gotdiff;
    }

    switch (b1->blocktype) {
    case SDF_BLOCKTYPE_PLAIN_DERIVED:
    case SDF_BLOCKTYPE_PLAIN_VARIABLE:
    case SDF_BLOCKTYPE_POINT_DERIVED:
    case SDF_BLOCKTYPE_POINT_VARIABLE:
    case SDF_BLOCKTYPE_ARRAY:
        return diff_plain(handles, b1, b2, inum);
        break;
    case SDF_BLOCKTYPE_CONSTANT:
        return diff_constant(handles, b1, b2, inum);
        break;
    case SDF_BLOCKTYPE_NAMEVALUE:
        return diff_namevalue(handles, b1, b2, inum);
        break;
    case SDF_BLOCKTYPE_PLAIN_MESH:
    case SDF_BLOCKTYPE_POINT_MESH:
    case SDF_BLOCKTYPE_LAGRANGIAN_MESH:
        return diff_mesh(handles, b1, b2, inum);
        break;
    default:
        return 0;
    }
}


int main(int argc, char **argv)
{
    char **files = NULL;
    int i, n, block, err, found, idx, range_start;
    //int nelements_max;
    sdf_file_t *h, *h2, **handles;
    sdf_block_t *b, *b2, *next;
    //sdf_block_t *mesh, *mesh0;
    //list_t *station_blocks;
    comm_t comm;
    int gotdiff = 0;

    files = parse_args(&argc, &argv);

#ifdef PARALLEL
    MPI_Init(&argc, &argv);
    MPI_Comm_dup(MPI_COMM_WORLD, &comm);
#else
    comm = 0;
#endif

    handles = calloc(2, sizeof(*handles));
    for (i=0; i<2; i++) {
        h = handles[i] = sdf_open(files[i], comm, SDF_READ, 0);
        if (!h) {
            fprintf(stderr, "Error opening file %s\n", files[i]);
            return 1;
        }

        h->print = debug;
        if (ignore_summary) h->use_summary = 0;
        if (ignore_nblocks) h->ignore_nblocks = 1;
        sdf_stack_init(h);

        sdf_read_header(h);
        h->current_block = NULL;

        // If nblocks is negative then the file is corrupt
        if (h->nblocks < 0) {
            block = (-h->nblocks) / 64;
            err = -h->nblocks - 64 * block;
            fprintf(stderr, "Error code %s found at block %i\n",
                    sdf_error_codes_c[err], block);
        }

        h->purge_duplicated_ids = purge_duplicate;

        sdf_read_blocklist(h);
    }
    free(files);

    h  = handles[0];
    h2 = handles[1];

    //list_init(&station_blocks);

    set_header_string(handles);

    range_start = 0;
    //nelements_max = 0;
    //mesh0 = NULL;
    found = 1;
    next = h->blocklist;
    for (i = 0, idx = 1; next; i++, idx++) {
        h->current_block = b = next;
        next = b->next;

        if (nrange > 0 || variable_ids) found = 0;

        for (n = range_start; n < nrange; n++) {
            if (idx < range_list[n].start)
                break;
            if (idx <= range_list[n].end) {
                found = 1;
                break;
            }
            range_start++;
        }

        if (found == 0 && variable_ids) {
            variable_last_id = variable_ids;
            while (variable_last_id) {
                if (!memcmp(b->id, variable_last_id->id,
                        strlen(variable_last_id->id)+1)) {
                    found = 1;
                    break;
                }
                variable_last_id = variable_last_id->next;
            }
        }

        if (exclude_variables) {
            if (found) continue;
        } else {
            if (!found) continue;
        }

        /* Only consider blocks in the blocktype mask */
        if (blocktype_mask && blocktype_mask[b->blocktype] == 0)
            continue;

        b2 = sdf_find_block_by_id(h2, b->id);
        if (!b2) {
            if (metadata && !quiet)
                print_metadata(b, idx, h->nblocks);
            else if (!quiet) {
                print_header();
                print_metadata_id(b, idx, handles[0]->nblocks);
                printf(" - not in second file\n");
            }
            continue;
        }

        gotdiff += diff_block(handles, b, b2, idx);
/*
        switch (b->blocktype) {
        case SDF_BLOCKTYPE_PLAIN_DERIVED:
            sdf_helper_read_data(h, b);
            if (b->station_id) {
                mesh = sdf_find_block_by_id(h, b->mesh_id);
                if (!mesh) continue;
                if (mesh->nelements > nelements_max) {
                    nelements_max = mesh->nelements;
                    mesh0 = mesh;
                }

                if (!mesh->done_data)
                    sdf_helper_read_data(h, mesh);

                list_append(station_blocks, b);
            } else
                pretty_print(h, b, idx);
            break;
        case SDF_BLOCKTYPE_PLAIN_VARIABLE:
        case SDF_BLOCKTYPE_PLAIN_MESH:
        case SDF_BLOCKTYPE_POINT_VARIABLE:
        case SDF_BLOCKTYPE_POINT_MESH:
            sdf_helper_read_data(h, b);
            pretty_print(h, b, idx);
            break;
        case SDF_BLOCKTYPE_DATABLOCK:
            sdf_helper_read_data(h, b);
            print_data(b);
            break;
        default:
            printf("Unsupported blocktype %s\n",
                   sdf_blocktype_c[b->blocktype]);
        }
*/
    }

/*
    if (mesh0 && (variable_ids || nrange > 0)) {
        nelements_max = 0;
        b = list_start(station_blocks);
        for (i = 0; i < station_blocks->count; i++) {
            idx = b->offset + b->nelements_local - mesh0->offset;
            if (idx > nelements_max)
                nelements_max = idx;
            b = list_next(station_blocks);
        }

        for (n = 0; n < nelements_max; n++) {
            print_value_element(mesh0->data, mesh0->datatype, n);

            b = list_start(station_blocks);
            for (i = 0; i < station_blocks->count; i++) {
                idx = n + mesh0->offset - b->offset;
                printf(format_space,1);
                if (idx >= 0 && idx < b->nelements_local)
                    print_value_element(b->data, b->datatype_out, n);
                else
                    print_value(zero, b->datatype_out);
                b = list_next(station_blocks);
            }

            printf("\n");
        }
    }

    list_destroy(&station_blocks);
*/
    if (range_list) free(range_list);
    if (blocktype_mask) free(blocktype_mask);

    close_files(handles);

    return gotdiff;
}


int close_files(sdf_file_t **handles)
{
    free_memory(handles);
    sdf_close(handles[0]);
    sdf_close(handles[1]);
    free(handles);
#ifdef PARALLEL
    MPI_Finalize();
#endif

    return 0;
}
