/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2013-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#ifdef __APPLE__
#  define _DARWIN_C_SOURCE
#  include <mach-o/dyld.h>
#elif defined(_WIN32) || defined(__CYGWIN__)
#  include <windows.h>
#else // Linux
#  define _GNU_SOURCE
#  include <link.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sdf_extension.h>
#include <sdf_derived.h>
#include <sdf.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  define LIBLOAD(fname) LoadLibraryA((fname))
#  define LIBFUNC(lib, fn) GetProcAddress((lib), (fn))
#  define LIBCLOSE(lib) FreeLibrary((lib))
#else
#  include <unistd.h>
#  include <dlfcn.h>
#  define LIBLOAD(fname) dlopen((fname), RTLD_LAZY)
#  define LIBFUNC(lib, fn) dlsym((lib), (fn))
#  define LIBCLOSE(lib) dlclose((lib))
#endif

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif
#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

static void *sdf_global_extension = NULL;
static void *sdf_global_extension_dlhandle = NULL;
static int   sdf_global_extension_failed = 0;
static int   sdf_global_extension_refcount = 0;
static char *sdf_global_extension_path = NULL;
static char *info_string = NULL;
static char *full_info_string = NULL;

int sdf_purge_duplicates(sdf_file_t *h);


static const char *dlpath(void *handle)
{
    const char *path = NULL;

#ifdef __APPLE__
    for (int32_t i = _dyld_image_count(); i >= 0 ; i--) {

        bool found = FALSE;
        const char *probe_path = _dyld_get_image_name(i);
        void *probe_handle = dlopen(probe_path,
                                    RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);

        if (handle == probe_handle) {
            found = TRUE;
            path = probe_path;
        }

        LIBCLOSE(probe_handle);

        if (found)
            break;
    }
#elif defined(_WIN32) || defined(__CYGWIN__)
    char fname[512];
    DWORD ret;
    ret = GetModuleFileNameA(handle, fname, sizeof(fname));
    path = strdup(fname);
#else // Linux
    struct link_map *map;
    dlinfo(handle, RTLD_DI_LINKMAP, &map);

    if (map)
        path = map->l_name;
#endif

    return path;
}


void *sdf_extension_load(sdf_file_t *h)
{
    sdf_extension_create_t *sdf_extension_create;
    void *p;
#if defined(_WIN32) || defined(__CYGWIN__)
    char *libname1 = "libsdf_extension.dll";
    char *libname2 = "sdf_extension.dll";
    char *sep = ";,";
    char *join = "\\";
#else
    char *libname1 = "sdf_extension.so";
    char *libname2 = "libsdf_extension.so";
    char *sep = ":;,";
    char *join = "/";
#endif
    char *path_env, *pathname, *path;
    char error_buffer[256];
    int len;
    struct stat sb;

    h->sdf_extension_version  = SDF_EXTENSION_VERSION;
    h->sdf_extension_revision = SDF_EXTENSION_REVISION;

    if (sdf_global_extension_failed) {
        h->error_message = "sdf_extension_load: failed to load extension.";
        return NULL;
    }

    sdf_global_extension_refcount++;

    if (sdf_global_extension) return sdf_global_extension;

    /*
     * SDF_EXTENSION_PATH is a string separated by colon, semicolon or comma
     * Each substring is examined in turn. If it is a file then we attempt to
     * load it as a dynamic library. If it is a directory then we attempt to
     * load a file named "sdf_extension.so" or "libsdf_extension.so" in that
     * directory. The routine exits once a valid library is found.
     */
    path_env = getenv("SDF_EXTENSION_PATH");
    if (path_env) {
        len = strlen(path_env) + strlen(libname1) + strlen(libname2) + 2;
        pathname = malloc(len);
        for (path = strtok(path_env, sep); path; path = strtok(NULL, sep)) {
            stat(path, &sb);
            if (S_ISDIR(sb.st_mode)) {
                snprintf(pathname, len, "%s%s%s", path, join, libname1);
                sdf_global_extension_dlhandle = LIBLOAD(pathname);
                if (!sdf_global_extension_dlhandle) {
                    snprintf(pathname, len, "%s%s%s", path, join, libname2);
                    sdf_global_extension_dlhandle = LIBLOAD(pathname);
                }
            } else if (S_ISREG(sb.st_mode)) {
                sdf_global_extension_dlhandle = LIBLOAD(path);
            }
            if (sdf_global_extension_dlhandle) break;
        }
        free(pathname);
    }

    if (!sdf_global_extension_dlhandle) {
        sdf_global_extension_dlhandle = LIBLOAD(libname1);
        if (!sdf_global_extension_dlhandle)
            sdf_global_extension_dlhandle = LIBLOAD(libname2);
    }

    if (!sdf_global_extension_dlhandle) {
        sdf_global_extension_failed = 1;
#if defined(_WIN32) || defined(__CYGWIN__)
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,                 /* message source */
                      GetLastError(),       /* error number */
                      0,                    /* default language */
                      error_buffer,         /* destination */
                      sizeof(error_buffer), /* size of destination */
                      NULL);                /* no inserts */
        // strndup not available
        h->error_message = strdup(error_buffer);
#else
        h->error_message = dlerror();
#endif
        sdf_global_extension_refcount--;
        return NULL;
    }

    // Weird pointer copying required by ISO C
    p = LIBFUNC(sdf_global_extension_dlhandle, "sdf_extension_create");
    memcpy(&sdf_extension_create, &p, sizeof(p));

    sdf_global_extension = sdf_extension_create(h);

    sdf_global_extension_path = strdup(dlpath(sdf_global_extension_dlhandle));

    return sdf_global_extension;
}


char *sdf_extension_path(void)
{
    return sdf_global_extension_path;
}


void sdf_extension_unload(void)
{
    sdf_extension_destroy_t *sdf_extension_destroy;
    void *p;

    if (!sdf_global_extension_dlhandle) return;

    if (sdf_global_extension) {
        sdf_global_extension_refcount--;
        if (sdf_global_extension_refcount > 0) return;
        // Weird pointer copying required by ISO C
        p = LIBFUNC(sdf_global_extension_dlhandle, "sdf_extension_destroy");
        memcpy(&sdf_extension_destroy, &p, sizeof(p));

        sdf_extension_destroy(sdf_global_extension);
    }

#ifndef VALGRIND
    LIBCLOSE(sdf_global_extension_dlhandle);
    sdf_global_extension_dlhandle = NULL;
#endif

    sdf_extension_destroy = NULL;
    sdf_global_extension = NULL;
    sdf_global_extension_failed = 0;

    if (sdf_global_extension_path) free(sdf_global_extension_path);
    if (info_string) free(info_string);
    if (full_info_string) free(full_info_string);
    sdf_global_extension_path = NULL;
    info_string = NULL;
    full_info_string = NULL;

    return;
}


void sdf_extension_free_data(sdf_file_t *h)
{
    sdf_extension_free_t *sdf_extension_free;
    void *p;

    if (!sdf_global_extension_dlhandle || !h) return;

    if (sdf_global_extension) {
        // Weird pointer copying required by ISO C
        p = LIBFUNC(sdf_global_extension_dlhandle, "sdf_extension_free");
        if (!p)
            return;
        memcpy(&sdf_extension_free, &p, sizeof(p));

        sdf_extension_free(h);
    }

    return;
}


int sdf_read_blocklist_all(sdf_file_t *h)
{
    sdf_extension_t *ext;
    char **preload;
    sdf_block_t *b, *next, *cur, *block;
    int i;

    // Retrieve the extended interface library from the plugin manager
    sdf_extension_load(h);
    ext = sdf_global_extension;

    if (h->blocklist) {
        if (ext) ext->timestate_update(ext, h);
        return 0;
    }

    sdf_read_blocklist(h);

    // Append derived data to the blocklist using built-in library.
    sdf_add_derived_blocks(h);

    if (ext) {
        preload = ext->preload(ext, h);
        // For each entry in the preload array, try to find the block
        // and populate its data.
        if (preload) {
            int n = 0;
            cur = h->current_block;
            while(preload[n]) {
                b = sdf_find_block_by_id(h, preload[n]);
                if (b && !b->data) {
                    h->current_block = b;
                    sdf_read_data(h);
                }
                free(preload[n]);
                n++;
            }
            free(preload);
            h->current_block = cur;
        }

        // Append derived data to the blocklist using the extension library.
        ext->read_blocklist(ext, h);
    }

    // Append additional derived data for blocks added by the extension.
    sdf_add_derived_blocks_final(h);
    sdf_purge_duplicates(h);

    // Fill in dimensions for derived blocks
    next = h->last_block_in_file;
    while (next) {
        b = next;
        next = b->next;

        if (b->blocktype != SDF_BLOCKTYPE_PLAIN_DERIVED
                && b->blocktype == SDF_BLOCKTYPE_POINT_DERIVED) continue;

        if (b->ndims > 0 || !b->mesh_id) continue;

        block = sdf_find_block_by_id(h, b->mesh_id);
        b->ndims = block->ndims;
        memcpy(b->local_dims, block->local_dims,
               b->ndims * sizeof(*b->local_dims));

        if (b->blocktype == SDF_BLOCKTYPE_POINT_DERIVED) {
            b->nelements_local = block->dims[0];
        } else {
            b->nelements_local = 1;
            for (i = 0; i < b->ndims; i++) {
                if (b->stagger == SDF_STAGGER_CELL_CENTRE && !b->station_id)
                    b->local_dims[i]--;
                b->nelements_local *= b->local_dims[i];
            }
        }

        if (!b->datatype_out)
            b->datatype_out = block->datatype_out;
    }

    return 0;
}


static char *get_info_string(sdf_file_t *h)
{
    sdf_extension_t *ext;

    if (info_string) return info_string;

    // Retrieve the extended interface library from the plugin manager
    sdf_extension_load(h);
    ext = sdf_global_extension;
    info_string = NULL;

    if (ext) {
        int ilen, plen, rlen, len;
        char *ptr, *oldinfo;
        static char const *pstr = "Extension path: ";

        oldinfo = ext->get_info(ext);
        ilen = strlen(oldinfo);
        plen = strlen(pstr);
        rlen = strlen(sdf_global_extension_path);

        len = ilen + plen + rlen + 2;
        ptr = info_string = calloc(1, len);

        memcpy(ptr, pstr, ilen);
        ptr += plen;
        memcpy(ptr, sdf_global_extension_path, rlen);
        ptr += rlen;
        *ptr = '\n';
        ptr++;
        memcpy(ptr, oldinfo, ilen);
        ptr += ilen;
    } else {
        if (sdf_global_extension_failed) {
            info_string = strdup(h->error_message);
        }
    }

    return info_string;
}


char *sdf_extension_get_info_string(sdf_file_t *h, char const *prefix)
{
    static char *old_prefix = NULL;
    char *info_base;

    if (full_info_string && old_prefix == prefix) return full_info_string;

    info_base = get_info_string(h);
    old_prefix = (char*)prefix;

    if (prefix) {
        int ilen, plen, rlen, len, i, count = 1;
        char *c, *ptr;

        if (full_info_string && full_info_string != info_base)
            free(full_info_string);

        ilen = strlen(info_base);
        plen = strlen(prefix);

        for (i=0, c=info_base; i < ilen; i++, c++) {
            if (*c == '\n')
                count++;
        }
        len = ilen + count * plen + 1;
        ptr = full_info_string = calloc(1, len);

        memcpy(ptr, prefix, plen);
        ptr += plen;
        for (i=0, c=info_base, rlen=0; i < ilen; i++, c++, rlen++) {
            if (*c == '\n') {
                rlen++;
                memcpy(ptr, info_base, rlen);
                ptr += rlen;
                info_base += rlen;
                rlen = -1;
                memcpy(ptr, prefix, plen);
                ptr += plen;
            }
        }
        memcpy(ptr, info_base, rlen);
    } else {
        full_info_string = strdup(info_base);
    }

    return full_info_string;
}


void sdf_extension_print_version(sdf_file_t *h)
{
    char *info = sdf_extension_get_info_string(h, NULL);

    if (info) {
        if (sdf_global_extension_failed)
            printf("No extension loaded\n");
        printf("%s\n", info);
    }
}
