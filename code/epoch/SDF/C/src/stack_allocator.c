/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <sdf.h>
#include "stack_allocator.h"

// ****************************************************************************
//  Memory management stack
// ****************************************************************************

struct stack;

struct stack {
    sdf_block_t *block;
    struct stack *next;
};

struct stack_handle {
    struct stack *head, *tail;
    int64_t memory_size;
};

#define MAX_MEMORY 2147483648 // 2GB


void stack_alloc(stack_handle_t *sh, sdf_block_t *b)
{
    int i;
    uint64_t sz;
    struct stack *tail;

    if (b->done_data || b->dont_own_data) return;

    if (b->blocktype == SDF_BLOCKTYPE_PLAIN_MESH
            || b->blocktype == SDF_BLOCKTYPE_POINT_MESH) {
        b->ngrids = 3; //b->ndims;
        sz = b->ngrids * sizeof(*b->grids);
        b->grids = calloc(1, sz);
        sh->memory_size += sz;
        for (i = 0; i < b->ndims; i++) {
            sz = b->local_dims[i] * SDF_TYPE_SIZES[b->datatype_out];
            b->grids[i] = calloc(1, sz);
            sh->memory_size += sz;
        }
        for (i = b->ndims; i < b->ngrids; i++) {
            sz = SDF_TYPE_SIZES[b->datatype_out];
            b->grids[i] = calloc(1, sz);
            sh->memory_size += sz;
        }
    } else if (b->blocktype == SDF_BLOCKTYPE_LAGRANGIAN_MESH) {
        b->ngrids = 3; //b->ndims;
        sz = b->ngrids * sizeof(*b->grids);
        b->grids = calloc(1, sz);
        sh->memory_size += sz;
        for (i = 0; i < b->ndims; i++) {
            sz = b->nelements_local * SDF_TYPE_SIZES[b->datatype_out];
            b->grids[i] = calloc(1, sz);
            sh->memory_size += sz;
        }
        for (i = b->ndims; i < b->ngrids; i++) {
            sz = SDF_TYPE_SIZES[b->datatype_out];
            b->grids[i] = calloc(1, sz);
            sh->memory_size += sz;
        }
    } else {
        sz = b->nelements_local * SDF_TYPE_SIZES[b->datatype_out];
        b->data = calloc(1, sz);
        sh->memory_size += sz;
    }
    sh->tail->next = tail = (struct stack*)malloc(sizeof(struct stack));
    tail->block = b;
    tail->next = NULL;
    sh->tail = tail;
}


static void stack_free_data_or_grid(stack_handle_t *sh, sdf_block_t *b)
{
    int i, nmin;

    if (b->dont_allocate == 0) {
        if (b->grids) {
            nmin = 0;
            if (b->mmap) nmin = b->ndims;

            for (i = nmin; i < b->ngrids; i++) {
                if (b->grids[i])
                    free(b->grids[i]);
                sh->memory_size
                        -= b->local_dims[i] * SDF_TYPE_SIZES[b->datatype_out];
                sh->memory_size -= sizeof(*b->grids);
            }
            if (b->grids) free(b->grids);
        } else if (!b->mmap) {
            if (b->data) free(b->data);
            sh->memory_size
                    -= b->nelements_local * SDF_TYPE_SIZES[b->datatype_out];
        }
    } else if (b->grids)
        free(b->grids);
    b->grids = NULL;
    b->data = NULL;
    b->done_data = 0;
}


void stack_free_block(stack_handle_t *sh, sdf_block_t *b)
{
    struct stack *old_stack_entry = sh->head;
    struct stack *stack_entry = sh->head;

    while (stack_entry) {
        if (stack_entry->block == b) {
            stack_free_data_or_grid(sh, b);
            old_stack_entry->next = stack_entry->next;
            if (stack_entry == sh->tail) sh->tail = old_stack_entry;
            free(stack_entry);
            return;
        }
        old_stack_entry = stack_entry;
        stack_entry = stack_entry->next;
    }
}


void stack_push_to_bottom(stack_handle_t *sh, sdf_block_t *b)
{
    struct stack *old_stack_entry = sh->head;
    struct stack *stack_entry = sh->head;

    while (stack_entry) {
        if (stack_entry->block == b) {
            old_stack_entry->next = stack_entry->next;
            sh->tail->next = stack_entry;
            sh->tail = stack_entry;
            sh->tail->next = NULL;
            return;
        }
        old_stack_entry = stack_entry;
        stack_entry = stack_entry->next;
    }
}


void stack_freeup_memory(stack_handle_t *sh)
{
    sdf_block_t *b;
    struct stack *head;

    if (sh->memory_size < MAX_MEMORY) return;

    while (sh->head->next) {
        head = sh->head;
        sh->head = sh->head->next;
        free(head);
        b = sh->head->block;
        sh->head->block = NULL;
        stack_free_data_or_grid(sh, b);
        if (sh->memory_size < MAX_MEMORY) break;
    }
}


void stack_free(stack_handle_t *sh)
{
    sdf_block_t *b;
    struct stack *head;

    while (sh->head->next) {
        head = sh->head;
        sh->head = sh->head->next;
        free(head);
        b = sh->head->block;
        sh->head->block = NULL;
        stack_free_data_or_grid(sh, b);
    }
    sh->memory_size = 0;
}


void stack_destroy(stack_handle_t *sh)
{
    stack_free(sh);
    if (sh->head) {
        free(sh->head);
        sh->head = sh->tail = NULL;
    }
    free(sh);
}


stack_handle_t *stack_init(void)
{
    stack_handle_t *sh = (stack_handle_t*)calloc(1, sizeof(*sh));
    sh->head = sh->tail = (struct stack*)calloc(1, sizeof(struct stack));
    return sh;
}


void sdf_stack_alloc(sdf_file_t *h, sdf_block_t *b)
{
    stack_alloc(h->stack_handle, b);
}


void sdf_stack_free_block(sdf_file_t *h, sdf_block_t *b)
{
    stack_free_block(h->stack_handle, b);
}


void sdf_stack_push_to_bottom(sdf_file_t *h, sdf_block_t *b)
{
    stack_push_to_bottom(h->stack_handle, b);
}


void sdf_stack_freeup_memory(sdf_file_t *h)
{
    stack_freeup_memory(h->stack_handle);
}


void sdf_stack_free(sdf_file_t *h)
{
    stack_free(h->stack_handle);
}


void sdf_stack_destroy(sdf_file_t *h)
{
    stack_destroy(h->stack_handle);
}


void sdf_stack_init(sdf_file_t *h)
{
    if (!h) {
        fprintf(stderr, "Error in sdf_stack_init: invalid file handle\n");
        return;
    }

    if (h->stack_handle) {
        fprintf(stderr, "Error in sdf_stack_init: already initialised\n");
        return;
    }

    h->stack_handle = stack_init();
}
