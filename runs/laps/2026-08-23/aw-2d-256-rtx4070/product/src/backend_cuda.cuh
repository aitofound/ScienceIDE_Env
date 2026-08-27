// SPDX-License-Identifier: GPL-2.0
// CUDA backend: one device, cuFFT, everything else hand-written.
//
// WHAT MOVED TO THE DEVICE AND WHY.  LAPS spends its time in two places: the
// batched 3D transforms, and the all-to-all transposes that feed them under
// the 2D pencil decomposition.  On one device the second cost disappears - the
// whole grid is addressable, so there is nothing to transpose and nothing to
// exchange - and the first becomes a cuFFT call.  What is left is pointwise:
// the flux tensor, the spectral derivative, the RK update, the dealiasing
// mask, the primitive recovery, and a minimum reduction for the timestep.
// Every one of those is a kernel here, and each calls the SAME arithmetic the
// CPU build calls, out of laps_core.hpp, so the two cannot drift.
//
// The composition LAPS performs - r2c along x scaled by 1/nx, then complex
// FFTs along y and z scaled by 1/ny and 1/nz - IS a 3D r2c transform scaled by
// 1/(nx ny nz).  Upstream splits it because it must; a single device need not,
// and does not.  The same transform, not the same bits: a different
// factorisation reassociates additions, and the package's own floor says two
// correct f64 builds of the SAME source already differ by 1e-17.
//
// LAYOUT.  Every multi-field array is one allocation with a field stride, so a
// kernel takes a pointer and an index rather than an array of pointers:
//     u      [8][ncell]   conserved state, x fastest - the file's own order
//     prim   [4][ncell]   ux, uy, uz, p
//     uuF    [8][nspec]   double2, kx fastest, kx in [0, nx/2]
//     fnl    [8][nspec]   the right-hand side of this stage
//     fnl_rk [8][nspec]   the previous stage's, which RK3 carries
//
// MEMORY.  At 256^3 the state above is 4.9 GB and one flux slab is 134 MB, on
// a card with 12 GB.  The eighteen flux components are therefore transformed
// in batches whose size is chosen at startup from what the device actually
// has, smallest batch 1 - see pick_batch().  Nothing about the batch changes
// the answer: each component is scattered into fnl as soon as it is
// transformed, and addition is what it is.
#pragma once

#if defined(__CUDACC__)
#include <cuda_runtime.h>
#include <cufft.h>
// The real thing.  Everything below is written once and compiled twice: nvcc
// takes this branch, and a host compiler takes cuda_shim.hpp, which supplies
// the same names over FFTW so the kernels can be RUN and checked against the
// incumbent on a machine with no NVIDIA device.  See cuda_shim.hpp.
#define LAPS_LAUNCH(kernel, nb, nt) kernel<<<nb, nt>>>
static constexpr int kBlockSize = 256;
#else
#include "cuda_shim.hpp"
static constexpr int kBlockSize = 1;
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "laps_core.hpp"
#include "laps_init.hpp"
#include "laps_io.hpp"
#include "laps_params.hpp"

namespace laps {

#define LAPS_CUDA(call)                                                        \
    do {                                                                       \
        cudaError_t err_ = (call);                                             \
        if (err_ != cudaSuccess)                                               \
            throw std::runtime_error(std::string("cuda: ") + #call + ": " +    \
                                     cudaGetErrorString(err_));                \
    } while (0)

#define LAPS_CUFFT(call)                                                       \
    do {                                                                       \
        cufftResult r_ = (call);                                               \
        if (r_ != CUFFT_SUCCESS)                                               \
            throw std::runtime_error(std::string("cufft: ") + #call +          \
                                     " returned " + std::to_string((int)r_));  \
    } while (0)

// Geometry a kernel needs, small enough to pass by value.
struct DevGeom {
    long long ncell, nspec;
    int nx, ny, nz, nkx;
    int two_d;
};

__global__ void k_calc_flux(DevGeom g, const double* __restrict__ u,
                            const double* __restrict__ pr,
                            double* __restrict__ flux, int m0, int nm) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         i < g.ncell; i += stride) {
        double uu[8], pp[4];
#pragma unroll
        for (int v = 0; v < 8; ++v) uu[v] = u[(long long)v * g.ncell + i];
#pragma unroll
        for (int v = 0; v < 4; ++v) pp[v] = pr[(long long)v * g.ncell + i];
        const FluxCell f = flux_of(uu, pp);
        for (int m = 0; m < nm; ++m) flux[(long long)m * g.ncell + i] = f.f[m0 + m];
    }
}

__global__ void k_scale(long long n2, double* __restrict__ a, double s) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x; i < n2;
         i += stride)
        a[i] *= s;
}

// fnl[var] += sign * (i k_axis) * F, for one or two targets.  This is
// calc_rhs read by column instead of by row; see laps_core.hpp flux_drive().
__global__ void k_scatter(DevGeom g, const double2* __restrict__ c,
                          double2* __restrict__ fnl,
                          const double* __restrict__ kx,
                          const double* __restrict__ ky,
                          const double* __restrict__ kz, int nterm, int var0,
                          int axis0, double sign0, int var1, int axis1,
                          double sign1) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long s = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         s < g.nspec; s += stride) {
        const int i = (int)(s % g.nkx);
        const long long t = s / g.nkx;
        const int j = (int)(t % g.ny);
        const int k = (int)(t / g.ny);
        const double kv[3] = {kx[i], ky[j], kz[k]};
        const double2 f = c[s];

        double2* o = fnl + (long long)var0 * g.nspec + s;
        double a = sign0 * kv[axis0];
        o->x -= a * f.y;
        o->y += a * f.x;

        if (nterm > 1) {
            double2* o1 = fnl + (long long)var1 * g.nspec + s;
            double a1 = sign1 * kv[axis1];
            o1->x -= a1 * f.y;
            o1->y += a1 * f.x;
        }
    }
}

// rktmod.f90 rkt: u <- u + cc f + dd f_prev, then f_prev <- f, over all eight
// variables at once - the update is elementwise and the variable index is only
// an offset.
__global__ void k_rkt(long long n8, double* __restrict__ uuF,
                      double* __restrict__ fnl, double* __restrict__ fnl_rk,
                      double cc, double dd) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x; i < n8;
         i += stride) {
        const double f = fnl[i];
        uuF[i] += cc * f + dd * fnl_rk[i];
        fnl_rk[i] = f;
    }
}

__global__ void k_implicit(DevGeom g, double2* __restrict__ uuF,
                           const double* __restrict__ kx,
                           const double* __restrict__ ky,
                           const double* __restrict__ kz, int v0, int v1,
                           double coef) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long s = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         s < g.nspec; s += stride) {
        const int i = (int)(s % g.nkx);
        const long long t = s / g.nkx;
        const int j = (int)(t % g.ny);
        const int k = (int)(t / g.ny);
        const double k2 = kx[i] * kx[i] + ky[j] * ky[j] + kz[k] * kz[k];
        const double f = 1.0 / (coef * k2 + 1.0);
        for (int v = v0; v <= v1; ++v) {
            double2* o = uuF + (long long)v * g.nspec + s;
            o->x *= f;
            o->y *= f;
        }
    }
}

// dealiasing.f90.  The mask is recomputed from the mode index rather than read
// from a table: at 256^3 a table is 68 MB of bandwidth per stage to say
// something two multiplies can say.
__global__ void k_dealias(DevGeom g, double2* __restrict__ uuF,
                          const double* __restrict__ kxu,
                          const double* __restrict__ kyu,
                          const double* __restrict__ kzu, int option, double Lx,
                          double Ly, double Lz, double afx, double afy,
                          double afz) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long s = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         s < g.nspec; s += stride) {
        const int i = (int)(s % g.nkx);
        const long long t = s / g.nkx;
        const int j = (int)(t % g.ny);
        const int k = (int)(t / g.ny);
        double w;
        if (option == 1) {
            w = dealias_keep_sphere(kxu[i], kyu[j], g.two_d ? 0.0 : kzu[k], Lx, Ly,
                                    Lz, g.nx, g.ny, g.nz, g.two_d != 0)
                    ? 1.0
                    : 0.0;
        } else if (option == 2) {
            w = dealias_filter_1d(kxu[i], Lx, g.nx, afx) *
                dealias_filter_1d(kyu[j], Ly, g.ny, afy);
            if (!g.two_d) w *= dealias_filter_1d(kzu[k], Lz, g.nz, afz);
        } else {
            return;
        }
        for (int v = 0; v < 8; ++v) {
            double2* o = uuF + (long long)v * g.nspec + s;
            o->x *= w;
            o->y *= w;
        }
    }
}

__global__ void k_prim(DevGeom g, const double* __restrict__ u,
                       double* __restrict__ pr, double gamma) {
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         i < g.ncell; i += stride) {
        double uu[8], pp[4];
#pragma unroll
        for (int v = 0; v < 8; ++v) uu[v] = u[(long long)v * g.ncell + i];
        prim_of(uu, gamma, pp);
#pragma unroll
        for (int v = 0; v < 4; ++v) pr[(long long)v * g.ncell + i] = pp[v];
    }
}

// mhd.f90 vardt.  A minimum is exact and order-independent, so this reduces
// however it likes; the host takes the last min over the block results.
__global__ void k_cfl(DevGeom g, const double* __restrict__ u,
                      const double* __restrict__ pr, double gamma, double dx,
                      double dy, double dz, double rs, int z_radial,
                      double* __restrict__ out) {
    __shared__ double sh[kBlockSize];
    double m = HUGE_VAL;
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x;
         i < g.ncell; i += stride) {
        double uu[8], pp[4];
#pragma unroll
        for (int v = 0; v < 8; ++v) uu[v] = u[(long long)v * g.ncell + i];
#pragma unroll
        for (int v = 0; v < 4; ++v) pp[v] = pr[(long long)v * g.ncell + i];
        const CflSpeeds c = cfl_speeds(uu, pp, gamma);
        double d;
        if (g.two_d) {
            const double dtx = z_radial ? dx / c.cx * rs : dx / c.cx;
            d = fmin(dtx, dy / c.cy * rs);
        } else {
            d = fmin(fmin(dx / c.cx, dy / c.cy * rs), dz / c.cz * rs);
        }
        m = fmin(m, d);
    }
    sh[threadIdx.x] = m;
    __syncthreads();
    for (unsigned s = blockDim.x / 2; s > 0; s >>= 1) {
        if (threadIdx.x < s) sh[threadIdx.x] = fmin(sh[threadIdx.x], sh[threadIdx.x + s]);
        __syncthreads();
    }
    if (threadIdx.x == 0) out[blockIdx.x] = sh[0];
}

__global__ void k_finite(DevGeom g, const double* __restrict__ u,
                         int* __restrict__ bad) {
    const long long n8 = g.ncell * 8;
    const long long stride = (long long)blockDim.x * gridDim.x;
    for (long long i = (long long)blockIdx.x * blockDim.x + threadIdx.x; i < n8;
         i += stride)
        if (!(fabs(u[i]) <= DBL_MAX)) *bad = 1;   // false for NaN and for inf
}

class CudaBackend {
  public:
    CudaBackend(const Params& p, RealState& s, OutputWriter& w)
        : p_(p), w_(w), n_(p.ncell()), ns_(p.nspec()) {
        int ndev = 0;
        LAPS_CUDA(cudaGetDeviceCount(&ndev));
        if (ndev < 1) throw std::runtime_error("no CUDA device is visible to this container");
        LAPS_CUDA(cudaSetDevice(0));
        cudaDeviceProp prop{};
        LAPS_CUDA(cudaGetDeviceProperties(&prop, 0));
        std::fprintf(stderr, "laps-cuda: %s, sm_%d%d, %.1f GiB\n", prop.name,
                     prop.major, prop.minor,
                     (double)prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));

        g_.ncell = n_;
        g_.nspec = ns_;
        g_.nx = (int)p_.nx; g_.ny = (int)p_.ny; g_.nz = (int)p_.nz;
        g_.nkx = (int)p_.nkx();
        g_.two_d = p_.two_d() ? 1 : 0;

        // cuFFT's D2Z on dims (nz, ny, nx) writes (nz, ny, nx/2+1) with the
        // last axis fastest, which IS the Fortran layout uu_fourier has, so no
        // permutation is needed anywhere in this file.
        if (p_.two_d()) {
            LAPS_CUFFT(cufftPlan2d(&fwd_, (int)p_.ny, (int)p_.nx, CUFFT_D2Z));
            LAPS_CUFFT(cufftPlan2d(&bwd_, (int)p_.ny, (int)p_.nx, CUFFT_Z2D));
        } else {
            LAPS_CUFFT(cufftPlan3d(&fwd_, (int)p_.nz, (int)p_.ny, (int)p_.nx, CUFFT_D2Z));
            LAPS_CUFFT(cufftPlan3d(&bwd_, (int)p_.nz, (int)p_.ny, (int)p_.nx, CUFFT_Z2D));
        }

        alloc(&d_u_, sizeof(double) * 8 * n_);
        alloc(&d_prim_, sizeof(double) * 4 * n_);
        alloc(&d_uuF_, sizeof(double2) * 8 * ns_);
        alloc(&d_fnl_, sizeof(double2) * 8 * ns_);
        alloc(&d_fnlrk_, sizeof(double2) * 8 * ns_);
        alloc(&d_cbuf_, sizeof(double2) * ns_);
        alloc(&d_kx_, sizeof(double) * p_.nx);
        alloc(&d_ky_, sizeof(double) * p_.ny);
        alloc(&d_kz_, sizeof(double) * std::max<long>(p_.nz, 1));
        alloc(&d_kxu_, sizeof(double) * p_.nx);
        alloc(&d_kyu_, sizeof(double) * p_.ny);
        alloc(&d_kzu_, sizeof(double) * std::max<long>(p_.nz, 1));
        alloc(&d_blockmin_, sizeof(double) * kRedBlocks);
        alloc(&d_bad_, sizeof(int));

        nbatch_ = pick_batch();
        alloc(&d_flux_, sizeof(double) * nbatch_ * n_);
        std::fprintf(stderr, "laps-cuda: flux batch %d of 18\n", nbatch_);

        upload_wavenumbers();
        LAPS_CUDA(cudaMemset(d_fnlrk_, 0, sizeof(double2) * 8 * ns_));

        // Upload the initial condition, then take its transform, which is what
        // mhd.f90 does once before the loop.
        for (int v = 0; v < 8; ++v)
            LAPS_CUDA(cudaMemcpy(d_u_ + (long long)v * n_, s.u[v].data(),
                                 sizeof(double) * n_, cudaMemcpyHostToDevice));
        for (int v = 0; v < 4; ++v)
            LAPS_CUDA(cudaMemcpy(d_prim_ + (long long)v * n_, s.prim[v].data(),
                                 sizeof(double) * n_, cudaMemcpyHostToDevice));
        for (int v = 0; v < 8; ++v)
            forward(d_u_ + (long long)v * n_, d_uuF_ + (long long)v * ns_);

        // cuFFT documents that an out-of-place transform preserves its input
        // EXCEPT for complex-to-real, and every other forward in this file
        // reads a flux slab that is dead the moment it has been transformed.
        // This one is different: it reads the live conserved state, which the
        // first snapshot and the first CFL reduction both need afterwards. The
        // host still has that state, so it is simply written back - which costs
        // one upload once and does not depend on reading the fine print
        // correctly.
        for (int v = 0; v < 8; ++v)
            LAPS_CUDA(cudaMemcpy(d_u_ + (long long)v * n_, s.u[v].data(),
                                 sizeof(double) * n_, cudaMemcpyHostToDevice));

        host_slab_.resize(n_);
        build_skip();
        // The host copy is not carried forward; the device holds the state.
        for (auto& a : s.u) { a.clear(); a.shrink_to_fit(); }
        for (auto& a : s.prim) { a.clear(); a.shrink_to_fit(); }
        check("initialisation");
    }

    ~CudaBackend() {
        if (fwd_) cufftDestroy(fwd_);
        if (bwd_) cufftDestroy(bwd_);
        for (void* p : owned_) cudaFree(p);
    }

    CudaBackend(const CudaBackend&) = delete;
    CudaBackend& operator=(const CudaBackend&) = delete;

    // --- the interface TimeLoop drives -----------------------------------
    void step(const RK3& rk) {
        for (int irk = 0; irk < 3; ++irk) {
            LAPS_CUDA(cudaMemsetAsync(d_fnl_, 0, sizeof(double2) * 8 * ns_));
            for (int m0 = 0; m0 < 18; m0 += nbatch_) {
                const int nm = std::min<int>(nbatch_, 18 - m0);
                LAPS_LAUNCH(k_calc_flux, grid(n_), kBlock)(g_, d_u_, d_prim_, d_flux_, m0, nm);
                for (int b = 0; b < nm; ++b) {
                    const int m = m0 + b;
                    if (skip_flux_[m]) continue;
                    forward(d_flux_ + (long long)b * n_, d_cbuf_);
                    const FluxDrive d = flux_drive(m);
                    LAPS_LAUNCH(k_scatter, grid(ns_), kBlock)(
                        g_, d_cbuf_, d_fnl_, d_kx_, d_ky_, d_kz_, d.n, d.t[0].var,
                        d.t[0].axis, d.t[0].sign,
                        d.n > 1 ? d.t[1].var : 0, d.n > 1 ? d.t[1].axis : 0,
                        d.n > 1 ? d.t[1].sign : 0.0);
                }
            }

            LAPS_LAUNCH(k_rkt, grid(16 * ns_), kBlock)(16 * ns_, (double*)d_uuF_,
                                              (double*)d_fnl_, (double*)d_fnlrk_,
                                              rk.cc[irk], rk.dd[irk]);
            if (p_.if_visc && !p_.if_visc_exp)
                LAPS_LAUNCH(k_implicit, grid(ns_), kBlock)(g_, d_uuF_, d_kx_, d_ky_, d_kz_, 1,
                                                  3, rk.ts[irk] * p_.viscosity);
            if (p_.if_resis && !p_.if_resis_exp)
                LAPS_LAUNCH(k_implicit, grid(ns_), kBlock)(g_, d_uuF_, d_kx_, d_ky_, d_kz_, 4,
                                                  6, rk.ts[irk] * p_.resistivity);

            if (p_.dealias_option == 1 || p_.dealias_option == 2)
                LAPS_LAUNCH(k_dealias, grid(ns_), kBlock)(g_, d_uuF_, d_kxu_, d_kyu_, d_kzu_,
                                                 (int)p_.dealias_option, p_.Lx, p_.Ly,
                                                 p_.Lz, p_.afx, p_.afy, p_.afz);

            for (int v = 0; v < 8; ++v)
                backward(d_uuF_ + (long long)v * ns_, d_u_ + (long long)v * n_);

            LAPS_LAUNCH(k_prim, grid(n_), kBlock)(g_, d_u_, d_prim_, p_.adiabatic_index);
        }
        check("step");
    }

    double cfl_dt() {
        const double rs = p_.radius / p_.radius0;
        LAPS_LAUNCH(k_cfl, kRedBlocks, kBlock)(
            g_, d_u_, d_prim_, p_.adiabatic_index, p_.dx, p_.dy, p_.dz, rs,
            p_.if_z_radial ? 1 : 0, d_blockmin_);
        std::vector<double> h(kRedBlocks);
        LAPS_CUDA(cudaMemcpy(h.data(), d_blockmin_, sizeof(double) * kRedBlocks,
                             cudaMemcpyDeviceToHost));
        double m = std::numeric_limits<double>::infinity();
        for (double x : h) m = std::min(m, x);
        check("cfl_dt");
        return m;
    }

    void reset_rk_register() {
        LAPS_CUDA(cudaMemset(d_fnlrk_, 0, sizeof(double2) * 8 * ns_));
    }

    void snapshot(long iout, double time) {
        w_.frame_begin(iout, time);
        for (int v = 0; v < 8; ++v) {
            const int src = w_.slab_source(v);
            const double* p = src < 8 ? d_u_ + (long long)src * n_
                                      : d_prim_ + (long long)(src - 8) * n_;
            LAPS_CUDA(cudaMemcpy(host_slab_.data(), p, sizeof(double) * n_,
                                 cudaMemcpyDeviceToHost));
            w_.frame_slab(host_slab_.data());
        }
        w_.frame_end();
    }

    bool state_is_finite() {
        LAPS_CUDA(cudaMemset(d_bad_, 0, sizeof(int)));
        LAPS_LAUNCH(k_finite, grid(8 * n_), kBlock)(g_, d_u_, d_bad_);
        int bad = 0;
        LAPS_CUDA(cudaMemcpy(&bad, d_bad_, sizeof(int), cudaMemcpyDeviceToHost));
        return bad == 0;
    }

    // The grader times this from outside the container, so the process must
    // not return while the device still has work in flight.
    void finish() { LAPS_CUDA(cudaDeviceSynchronize()); }

  private:
    static constexpr int kBlock = kBlockSize;
    static constexpr int kRedBlocks = 1024;

    static int grid(long long n) {
        long long b = (n + kBlock - 1) / kBlock;
        return (int)std::min<long long>(b, 65535);
    }

    template <class T>
    void alloc(T** p, size_t bytes) {
        void* q = nullptr;
        cudaError_t e = cudaMalloc(&q, bytes);
        if (e != cudaSuccess)
            throw std::runtime_error("cuda: out of device memory asking for " +
                                     std::to_string(bytes / (1024 * 1024)) +
                                     " MiB: " + cudaGetErrorString(e));
        owned_.push_back(q);
        *p = (T*)q;
    }

    // How many flux components can be resident at once.  Asked of the device
    // rather than assumed, because the answer is a property of the card and
    // the grid, and the deck sets the grid.
    int pick_batch() {
        size_t freeb = 0, totalb = 0;
        LAPS_CUDA(cudaMemGetInfo(&freeb, &totalb));
        const size_t slab = sizeof(double) * (size_t)n_;
        // Leave a margin for the cuFFT work area, which is allocated lazily on
        // the first execute and is of order one transform.
        const size_t margin = 3 * slab + (64u << 20);
        size_t room = freeb > margin ? freeb - margin : 0;
        int b = (int)std::min<size_t>(18, slab ? room / slab : 1);
        if (b < 1) b = 1;
        return b;
    }

    void build_skip() {
        for (int m = 0; m < 18; ++m) {
            skip_flux_[m] = false;
            if (!p_.two_d()) continue;
            const FluxDrive d = flux_drive(m);
            bool all_z = true;
            for (int t = 0; t < d.n; ++t)
                if (d.t[t].axis != 2) all_z = false;
            skip_flux_[m] = all_z;   // kz is identically zero in the 2D tree
        }
    }

    void upload_wavenumbers() {
        // Two sets: the ones calc_rhs uses, which carry the expanding-box
        // factor radius0/radius on y and z, and the raw ones dealiasing uses,
        // which do not.  With the box off they are the same numbers, and the
        // deck can turn the box on.
        std::vector<double> kx(p_.kx), ky(p_.ky), kz(p_.nz, 0.0);
        if (!p_.two_d()) kz = p_.kz;
        std::vector<double> kxu = kx, kyu = ky, kzu = kz;
        const double sc = p_.kscale();
        for (auto& v : ky) v *= sc;
        for (auto& v : kz) v *= sc;
        if (p_.two_d()) std::fill(kz.begin(), kz.end(), 0.0);

        auto up = [&](double* d, const std::vector<double>& h) {
            LAPS_CUDA(cudaMemcpy(d, h.data(), sizeof(double) * h.size(),
                                 cudaMemcpyHostToDevice));
        };
        up(d_kx_, kx); up(d_ky_, ky); up(d_kz_, kz);
        up(d_kxu_, kxu); up(d_kyu_, kyu); up(d_kzu_, kzu);
    }

    void forward(const double* in, double2* out) {
        LAPS_CUFFT(cufftExecD2Z(fwd_, (cufftDoubleReal*)in, (cufftDoubleComplex*)out));
        LAPS_LAUNCH(k_scale, grid(2 * ns_), kBlock)(2 * ns_, (double*)out,
                                           1.0 / (double)n_);
    }

    void backward(const double2* in, double* out) {
        // cuFFT's out-of-place Z2D may overwrite its input, and uu_fourier is
        // live state, so it gets a scratch copy - as the CPU build does.
        LAPS_CUDA(cudaMemcpyAsync(d_cbuf_, in, sizeof(double2) * ns_,
                                  cudaMemcpyDeviceToDevice));
        LAPS_CUFFT(cufftExecZ2D(bwd_, (cufftDoubleComplex*)d_cbuf_,
                                (cufftDoubleReal*)out));
    }

    void check(const char* where) {
        cudaError_t e = cudaGetLastError();
        if (e != cudaSuccess)
            throw std::runtime_error(std::string("cuda: ") + where + ": " +
                                     cudaGetErrorString(e));
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess)
            throw std::runtime_error(std::string("cuda: ") + where +
                                     " (synchronise): " + cudaGetErrorString(e));
    }

    const Params& p_;
    OutputWriter& w_;
    long long n_, ns_;
    DevGeom g_{};
    int nbatch_ = 1;
    bool skip_flux_[18] = {};

    double *d_u_ = nullptr, *d_prim_ = nullptr, *d_flux_ = nullptr;
    double2 *d_uuF_ = nullptr, *d_fnl_ = nullptr, *d_fnlrk_ = nullptr, *d_cbuf_ = nullptr;
    double *d_kx_ = nullptr, *d_ky_ = nullptr, *d_kz_ = nullptr;
    double *d_kxu_ = nullptr, *d_kyu_ = nullptr, *d_kzu_ = nullptr;
    double* d_blockmin_ = nullptr;
    int* d_bad_ = nullptr;
    std::vector<void*> owned_;
    std::vector<double> host_slab_;
    cufftHandle fwd_ = 0, bwd_ = 0;
};

}  // namespace laps
