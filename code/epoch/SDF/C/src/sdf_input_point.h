/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

#ifndef _SDF_INPUT_POINT_H_
#define _SDF_INPUT_POINT_H_

#ifdef __cplusplus
extern "C" {
#endif

int sdf_read_point_mesh(sdf_file_t *h);
int sdf_read_point_mesh_info(sdf_file_t *h);
int sdf_read_point_variable(sdf_file_t *h);
int sdf_read_point_variable_info(sdf_file_t *h);

#ifdef __cplusplus
}
#endif

#endif
