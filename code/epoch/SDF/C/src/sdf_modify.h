/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#ifndef _SDF_MODIFY_H_
#define _SDF_MODIFY_H_

#ifdef __cplusplus
extern "C" {
#endif

sdf_block_t *sdf_find_block_by_id(sdf_file_t *h, const char *id);
sdf_block_t *sdf_find_block_by_name(sdf_file_t *h, const char *name);

#ifdef __cplusplus
}
#endif

#endif
