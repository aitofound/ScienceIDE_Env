/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2012-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#include <string.h>
#include <sdf.h>
#include "sdf_util.h"

/**
 * @defgroup utility
 * @brief General helper routines
 * @{
 */

sdf_block_t *sdf_find_block_by_id(sdf_file_t *h, const char *id)
{
    sdf_block_t *b;

    if (!h || !h->blocklist || !id)
        return NULL;

    HASH_FIND(hh1, h->hashed_blocks_by_id, id, strlen(id), b);
    return b;
}


sdf_block_t *sdf_find_block_by_name(sdf_file_t *h, const char *name)
{
    sdf_block_t *b;

    if (!h || !h->blocklist || !name)
        return NULL;

    HASH_FIND(hh2, h->hashed_blocks_by_name, name, strlen(name), b);
    return b;
}


void sdf_delete_hash_block(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *bb = NULL;

    HASH_FIND(hh1, h->hashed_blocks_by_id, b->id, strlen(b->id), bb);
    if (bb != NULL && b == bb)
        HASH_DELETE(hh1, h->hashed_blocks_by_id, b);

    HASH_FIND(hh2, h->hashed_blocks_by_name, b->name, strlen(b->name), bb);
    if (bb != NULL && b == bb)
        HASH_DELETE(hh2, h->hashed_blocks_by_name, b);
}


void sdf_hash_block(sdf_file_t *h, sdf_block_t *b)
{
    sdf_block_t *bb = NULL;

    HASH_FIND(hh1, h->hashed_blocks_by_id, b->id, strlen(b->id), bb);
    if (bb != NULL && b != bb) {
        HASH_DELETE(hh1, h->hashed_blocks_by_id, bb);
        bb = NULL;
    }
    if (!bb)
        HASH_ADD_KEYPTR(hh1, h->hashed_blocks_by_id, b->id, strlen(b->id), b);

    HASH_FIND(hh2, h->hashed_blocks_by_name, b->name, strlen(b->name), bb);
    if (bb != NULL && b != bb) {
        HASH_DELETE(hh2, h->hashed_blocks_by_name, bb);
        bb = NULL;
    }
    if (!bb)
        HASH_ADD_KEYPTR(hh2, h->hashed_blocks_by_name, b->name,
                strlen(b->name), b);
}


void sdf_hash_block_list(sdf_file_t *h)
{
    sdf_block_t *b;

    b = h->blocklist;
    while (b) {
        sdf_hash_block(h, b);
        b = b->next;
    }
}
/**@}*/
