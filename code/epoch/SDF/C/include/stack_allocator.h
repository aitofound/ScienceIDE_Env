/*
 * SDF (Self-Describing Format) Software Library
 * Copyright (c) 2014-2016, SDF Development Team
 *
 * Distributed under the terms of the BSD 3-clause License.
 * See the LICENSE file for details.
 */

/**
   @internal
   @file stack_allocator.h

   @brief Declarations for the SDF stack allocator helper routines.
   @details Routines for stack-based memory management.
   @author Dr Keith Bennett
   @date 15/02/2014
*/

#ifndef _STACK_ALLOCATOR_H_
#define _STACK_ALLOCATOR_H_
#include <sdf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stack_handle stack_handle_t;

void stack_alloc(stack_handle_t *sh, sdf_block_t *b);
void stack_free_block(stack_handle_t *sh, sdf_block_t *b);
void stack_push_to_bottom(stack_handle_t *sh, sdf_block_t *b);
void stack_freeup_memory(stack_handle_t *sh);
void stack_free(stack_handle_t *sh);
void stack_destroy(stack_handle_t *sh);
stack_handle_t *stack_init(void);

void sdf_stack_alloc(sdf_file_t *h, sdf_block_t *b);
void sdf_stack_free_block(sdf_file_t *h, sdf_block_t *b);
void sdf_stack_push_to_bottom(sdf_file_t *h, sdf_block_t *b);
void sdf_stack_freeup_memory(sdf_file_t *h);
void sdf_stack_free(sdf_file_t *h);
void sdf_stack_destroy(sdf_file_t *h);
void sdf_stack_init(sdf_file_t *h);

#ifdef __cplusplus
}
#endif

#endif
