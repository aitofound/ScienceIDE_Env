/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#ifndef _SDF_INPUT_CARTESIAN_H_
#define _SDF_INPUT_CARTESIAN_H_

#ifdef __cplusplus
extern "C" {
#endif

int sdf_read_plain_mesh(sdf_file_t *h);
int sdf_read_plain_mesh_info(sdf_file_t *h);
int sdf_read_lagran_mesh(sdf_file_t *h);
int sdf_read_plain_variable(sdf_file_t *h);
int sdf_read_plain_variable_info(sdf_file_t *h);
int64_t sdf_helper_read_array(sdf_file_t *h, void **var_in, int dim);

#ifdef __cplusplus
}
#endif

#endif
