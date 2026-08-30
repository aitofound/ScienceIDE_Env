/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#ifndef _SDF_OUTPUT_H_
#define _SDF_OUTPUT_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int sdf_write_bytes(sdf_file_t *h, void *buf, int buflen);
int sdf_write_at(sdf_file_t *h, int64_t offset, void *buf, int buflen);
int sdf_flush(sdf_file_t *h);
int64_t sdf_write_new_summary(sdf_file_t *h);
int sdf_write_meta(sdf_file_t *h);

#ifdef __cplusplus
}
#endif

#endif
