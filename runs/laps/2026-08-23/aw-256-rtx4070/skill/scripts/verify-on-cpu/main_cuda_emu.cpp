// SPDX-License-Identifier: GPL-2.0
// Host build of the CUDA product, over the stand-in in cuda_shim.hpp.
//
// Same main, same backend, same kernels; only cudaMalloc, the launch geometry
// and cuFFT are replaced.  This is how the rtx4070 cells were checked against
// the incumbent from a machine with no NVIDIA device - see
// skill/references/diary.md, which says plainly what that does and does not
// establish.
#include "main_cuda.cu"
