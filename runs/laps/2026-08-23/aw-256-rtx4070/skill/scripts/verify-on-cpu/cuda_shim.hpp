// SPDX-License-Identifier: GPL-2.0
// A CPU stand-in for the slice of CUDA and cuFFT that backend_cuda.cuh uses.
//
// WHY THIS EXISTS.  The rtx4070 cells are graded on a machine this port was
// developed nowhere near, and a GPU port that has only ever been COMPILED is a
// guess.  This header lets the CUDA backend - the same file, the same kernel
// bodies, the same host orchestration, the same indexing - be built and RUN by
// a host compiler and checked against the incumbent.  What it does not test is
// the two things it replaces: real device scheduling, and cuFFT's arithmetic
// as against FFTW's.  Everything else, including every index calculation and
// every launch geometry, is exercised.
//
// It is a test harness, not a deliverable.  No cell ships it, and the CUDA
// product never compiles it: `__CUDACC__` selects the real headers.
//
// One deliberate restriction: a block is one thread here (kBlock is 1 in this
// build).  Every kernel in backend_cuda.cuh is a grid-stride loop, which
// covers the whole range for any launch geometry, and the one kernel that
// cooperates within a block - the CFL minimum - degenerates correctly, since
// its shared-memory tree does no passes when blockDim.x is 1 and it then
// publishes its own thread's minimum.  So the emulation is faithful for
// exactly the geometries this backend uses, and would not be for a kernel that
// assumed a wider block.
#pragma once

#if defined(__CUDACC__)
#error "cuda_shim.hpp is the host stand-in; it must not be compiled by nvcc"
#endif

#include <fftw3.h>

#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// --- language ------------------------------------------------------------
#define __global__
#define __device__
#define __host__
#define __shared__ static
#define __syncthreads() ((void)0)

struct dim3 { unsigned x = 1, y = 1, z = 1; };
struct double2 { double x, y; };

namespace laps_emu {
inline dim3& blockIdx_()  { static dim3 v; return v; }
inline dim3& threadIdx_() { static dim3 v; return v; }
inline dim3& blockDim_()  { static dim3 v; return v; }
inline dim3& gridDim_()   { static dim3 v; return v; }

template <class F>
struct Launcher {
    int nb, nt;
    F f;
    Launcher(int nb_, int nt_, F f_) : nb(nb_), nt(nt_), f(f_) {}
    template <class... A>
    void operator()(A... args) {
        gridDim_().x = static_cast<unsigned>(nb);
        blockDim_().x = static_cast<unsigned>(nt);
        for (int b = 0; b < nb; ++b) {
            blockIdx_().x = static_cast<unsigned>(b);
            for (int t = 0; t < nt; ++t) {
                threadIdx_().x = static_cast<unsigned>(t);
                f(args...);
            }
        }
    }
};
}  // namespace laps_emu

#define blockIdx  (laps_emu::blockIdx_())
#define threadIdx (laps_emu::threadIdx_())
#define blockDim  (laps_emu::blockDim_())
#define gridDim   (laps_emu::gridDim_())

#define LAPS_LAUNCH(kernel, nb, nt) laps_emu::Launcher(nb, nt, kernel)

// --- runtime API ---------------------------------------------------------
typedef int cudaError_t;
enum { cudaSuccess = 0, cudaErrorMemoryAllocation = 2 };
enum cudaMemcpyKind {
    cudaMemcpyHostToDevice, cudaMemcpyDeviceToHost, cudaMemcpyDeviceToDevice
};

struct cudaDeviceProp {
    char name[64];
    int major, minor;
    size_t totalGlobalMem;
};

inline const char* cudaGetErrorString(cudaError_t e) {
    return e == cudaSuccess ? "no error" : "emulated CUDA error";
}
inline cudaError_t cudaGetLastError() { return cudaSuccess; }
inline cudaError_t cudaDeviceSynchronize() { return cudaSuccess; }
inline cudaError_t cudaGetDeviceCount(int* n) { *n = 1; return cudaSuccess; }
inline cudaError_t cudaSetDevice(int) { return cudaSuccess; }

inline cudaError_t cudaGetDeviceProperties(cudaDeviceProp* p, int) {
    std::snprintf(p->name, sizeof p->name, "%s", "emulated device (cuda_shim.hpp)");
    p->major = 8;
    p->minor = 9;
    p->totalGlobalMem = size_t(12) << 30;
    return cudaSuccess;
}

namespace laps_emu {
inline size_t& emu_used() { static size_t v = 0; return v; }
inline std::map<void*, size_t>& emu_blocks() { static std::map<void*, size_t> m; return m; }
// A budget the size of the target's card, so pick_batch() is exercised against
// the memory the rtx4070 actually has rather than against this host's.
inline size_t emu_total() {
    if (const char* s = std::getenv("LAPS_EMU_VRAM_MB"))
        return size_t(std::strtoull(s, nullptr, 10)) << 20;
    return size_t(11500) << 20;
}
}  // namespace laps_emu

inline cudaError_t cudaMalloc(void** p, size_t bytes) {
    if (laps_emu::emu_used() + bytes > laps_emu::emu_total())
        return cudaErrorMemoryAllocation;
    void* q = std::malloc(bytes ? bytes : 1);
    if (!q) return cudaErrorMemoryAllocation;
    laps_emu::emu_used() += bytes;
    laps_emu::emu_blocks()[q] = bytes;
    *p = q;
    return cudaSuccess;
}

inline cudaError_t cudaFree(void* p) {
    auto it = laps_emu::emu_blocks().find(p);
    if (it != laps_emu::emu_blocks().end()) {
        laps_emu::emu_used() -= it->second;
        laps_emu::emu_blocks().erase(it);
    }
    std::free(p);
    return cudaSuccess;
}

inline cudaError_t cudaMemGetInfo(size_t* freeb, size_t* total) {
    *total = laps_emu::emu_total();
    *freeb = *total - laps_emu::emu_used();
    return cudaSuccess;
}

inline cudaError_t cudaMemcpy(void* d, const void* s, size_t n, cudaMemcpyKind) {
    std::memcpy(d, s, n);
    return cudaSuccess;
}
inline cudaError_t cudaMemcpyAsync(void* d, const void* s, size_t n, cudaMemcpyKind) {
    std::memmove(d, s, n);
    return cudaSuccess;
}
inline cudaError_t cudaMemset(void* d, int v, size_t n) {
    std::memset(d, v, n);
    return cudaSuccess;
}
inline cudaError_t cudaMemsetAsync(void* d, int v, size_t n) {
    std::memset(d, v, n);
    return cudaSuccess;
}

// --- cuFFT ---------------------------------------------------------------
typedef int cufftResult;
enum { CUFFT_SUCCESS = 0 };
enum cufftType { CUFFT_D2Z = 0x6a, CUFFT_Z2D = 0x6c };
typedef double cufftDoubleReal;
typedef double2 cufftDoubleComplex;
typedef int cufftHandle;

namespace laps_emu {
struct Plan {
    int rank = 0;
    int dims[3] = {1, 1, 1};
    cufftType type = CUFFT_D2Z;
    fftw_plan plan = nullptr;
};
inline std::map<int, Plan>& plans() { static std::map<int, Plan> m; return m; }
inline int& next_plan() { static int v = 1; return v; }

inline cufftResult make(cufftHandle* h, int rank, const int* dims, cufftType t) {
    Plan p;
    p.rank = rank;
    for (int i = 0; i < rank; ++i) p.dims[i] = dims[i];
    p.type = t;
    long long n = 1, nk = 1;
    for (int i = 0; i < rank; ++i) n *= dims[i];
    for (int i = 0; i < rank - 1; ++i) nk *= dims[i];
    nk *= dims[rank - 1] / 2 + 1;
    // Planned on scratch and executed new-array, exactly as the CPU backend
    // does, so the two builds go through the same FFTW code path.
    static std::vector<double> rs;
    static std::vector<double> cs;
    if ((long long)rs.size() < n) rs.assign(n, 0.0);
    if ((long long)cs.size() < 2 * nk) cs.assign(2 * nk, 0.0);
    p.plan = (t == CUFFT_D2Z)
                 ? fftw_plan_dft_r2c(rank, p.dims, rs.data(),
                                     reinterpret_cast<fftw_complex*>(cs.data()),
                                     FFTW_ESTIMATE | FFTW_UNALIGNED)
                 : fftw_plan_dft_c2r(rank, p.dims,
                                     reinterpret_cast<fftw_complex*>(cs.data()),
                                     rs.data(), FFTW_ESTIMATE | FFTW_UNALIGNED);
    if (!p.plan) return 1;
    *h = next_plan()++;
    plans()[*h] = p;
    return CUFFT_SUCCESS;
}
}  // namespace laps_emu

inline cufftResult cufftPlan2d(cufftHandle* h, int n0, int n1, cufftType t) {
    const int d[2] = {n0, n1};
    return laps_emu::make(h, 2, d, t);
}
inline cufftResult cufftPlan3d(cufftHandle* h, int n0, int n1, int n2, cufftType t) {
    const int d[3] = {n0, n1, n2};
    return laps_emu::make(h, 3, d, t);
}
inline cufftResult cufftDestroy(cufftHandle h) {
    auto it = laps_emu::plans().find(h);
    if (it != laps_emu::plans().end()) {
        fftw_destroy_plan(it->second.plan);
        laps_emu::plans().erase(it);
    }
    return CUFFT_SUCCESS;
}
inline cufftResult cufftExecD2Z(cufftHandle h, cufftDoubleReal* in,
                                cufftDoubleComplex* out) {
    auto& p = laps_emu::plans().at(h);
    fftw_execute_dft_r2c(p.plan, in, reinterpret_cast<fftw_complex*>(out));
    return CUFFT_SUCCESS;
}
inline cufftResult cufftExecZ2D(cufftHandle h, cufftDoubleComplex* in,
                                cufftDoubleReal* out) {
    auto& p = laps_emu::plans().at(h);
    // Real cuFFT is free to clobber the input of an out-of-place Z2D, and the
    // backend copies into scratch for exactly that reason; FFTW's c2r clobbers
    // it too, so the stand-in shares the hazard rather than hiding it.
    fftw_execute_dft_c2r(p.plan, reinterpret_cast<fftw_complex*>(in), out);
    return CUFFT_SUCCESS;
}
