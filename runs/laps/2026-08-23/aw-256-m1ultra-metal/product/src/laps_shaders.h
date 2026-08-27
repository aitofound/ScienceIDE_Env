// SPDX-License-Identifier: GPL-2.0
// Metal Shading Language source for the LAPS port, as a string.
//
// It is a string because the target has Command Line Tools and no Xcode, so
// there is no offline .metal -> .air -> .metallib compiler on the machine.
// Metal.framework compiles from source at run time (newLibraryWithSource:) and
// needs no Xcode to do it, so the shaders travel inside the executable and are
// built on first run.  Nothing is read from disk and nothing is downloaded.
//
// -------------------------------------------------------------------------
// THE PROBLEM THIS FILE SOLVES
//
// Metal Shading Language HAS NO double.  Not a slow double - none: the runtime
// compiler rejects a kernel that declares one.  LAPS is graded pointwise at
// 1e-10 against a binary64 incumbent, and fp32 carries 24 bits, which lands
// around 1e-7 on this state and 1e-8 to 1e-9 on the measured f32 saboteur.
// So the arithmetic is built out of pairs of floats: a value is (hi, lo) with
// hi + lo evaluated exactly, giving ~48 bits of significand against binary64's
// 53.  Roughly 32x coarser per operation, which is the trade the target file
// describes and the one this port takes.
//
// The primitives are the classical error-free transformations: Dekker's
// two-sum for addition, Knuth's two-product built on fma for multiplication,
// Newton steps from an fp32 seed for reciprocal and square root.  They are
// EXACT identities in IEEE-754 binary32 round-to-nearest and they are the
// first thing a fast-math pass destroys - (a+b)-a is not b, and a compiler
// that believes it is will delete the error term.  The host therefore compiles
// this source with fastMathEnabled = NO, and that is not a tuning flag: with
// fast math on, every kernel below silently degrades to fp32.
//
// float2 is eight bytes, exactly like a double, so this costs no bandwidth
// over an fp64 port - only ALU.
//
// -------------------------------------------------------------------------
// LAYOUT
//
//   ds        float2   (hi, lo)                one real value
//   cds       float4   (re.hi, re.lo, im.hi, im.lo)   one complex value
//
//   real field    ds  [ncell]     ix fastest, then iy, then iz
//   spectral      cds [nspec]     ikx fastest, ikx in [0, nx/2]
//
// The FFT is Stockham autosort, mixed radix, done as separate 1D transforms
// along x, y and z with the 1/nx, 1/ny, 1/nz scalings applied per axis - which
// is what LAPS itself does, for its own reasons, and reproducing it here costs
// nothing and removes a difference.
#pragma once

static const char* kLapsMetalSource = R"MSL(
#include <metal_stdlib>
using namespace metal;

typedef float2 ds;     // hi, lo
typedef float4 cds;    // re.hi, re.lo, im.hi, im.lo

// --- error-free transformations -----------------------------------------
// Every one of these is an identity in round-to-nearest binary32 and a lie
// under fast math.  The host compiles with fastMathEnabled = NO.

inline float2 two_sum(float a, float b) {
    float s = a + b;
    float bb = s - a;
    float e = (a - (s - bb)) + (b - bb);
    return float2(s, e);
}

// Valid only when |a| >= |b|, which every caller below guarantees.
inline float2 quick_two_sum(float a, float b) {
    float s = a + b;
    float e = b - (s - a);
    return float2(s, e);
}

inline float2 two_prod(float a, float b) {
    float p = a * b;
    float e = fma(a, b, -p);
    return float2(p, e);
}

inline ds ds_set(float a) { return ds(a, 0.0f); }

inline ds ds_add(ds a, ds b) {
    float2 s = two_sum(a.x, b.x);
    float2 t = two_sum(a.y, b.y);
    s.y += t.x;
    s = quick_two_sum(s.x, s.y);
    s.y += t.y;
    s = quick_two_sum(s.x, s.y);
    return s;
}

inline ds ds_neg(ds a) { return ds(-a.x, -a.y); }
inline ds ds_sub(ds a, ds b) { return ds_add(a, ds_neg(b)); }

inline ds ds_mul(ds a, ds b) {
    float2 p = two_prod(a.x, b.x);
    // a.x*b.y + a.y*b.x is the whole of the cross term that matters; a.y*b.y
    // is below the representable range of the result and is dropped, as every
    // double-single multiply does.
    p.y += fma(a.x, b.y, a.y * b.x);
    return quick_two_sum(p.x, p.y);
}

inline ds ds_div(ds a, ds b) {
    float q1 = a.x / b.x;
    ds r = ds_sub(a, ds_mul(b, ds_set(q1)));
    float q2 = r.x / b.x;
    r = ds_sub(r, ds_mul(b, ds_set(q2)));
    float q3 = r.x / b.x;
    ds q = quick_two_sum(q1, q2);
    return ds_add(q, ds_set(q3));
}

inline ds ds_sqrt(ds a) {
    if (a.x <= 0.0f) return ds(0.0f, 0.0f);
    // One Newton step on y = sqrt(a) from an fp32 seed doubles the correct
    // digits, which takes 24 bits to the ~48 the pair can hold.
    float xn = rsqrt(a.x);
    float yn = a.x * xn;
    ds y = ds_set(yn);
    ds diff = ds_sub(a, ds_mul(y, y));
    ds corr = ds_mul(diff, ds_set(0.5f * xn));
    return ds_add(y, corr);
}

inline ds ds_abs(ds a) { return a.x < 0.0f ? ds_neg(a) : a; }
inline bool ds_lt(ds a, ds b) { return (a.x < b.x) || (a.x == b.x && a.y < b.y); }
inline ds ds_min(ds a, ds b) { return ds_lt(a, b) ? a : b; }
inline ds ds_max(ds a, ds b) { return ds_lt(a, b) ? b : a; }

// --- complex -------------------------------------------------------------
inline ds  cre(cds z) { return ds(z.x, z.y); }
inline ds  cim(cds z) { return ds(z.z, z.w); }
inline cds cmake(ds re, ds im) { return cds(re.x, re.y, im.x, im.y); }
inline cds czero() { return cds(0.0f, 0.0f, 0.0f, 0.0f); }

inline cds cadd(cds a, cds b) { return cmake(ds_add(cre(a), cre(b)), ds_add(cim(a), cim(b))); }
inline cds csub(cds a, cds b) { return cmake(ds_sub(cre(a), cre(b)), ds_sub(cim(a), cim(b))); }

inline cds cmul(cds a, cds b) {
    ds ar = cre(a), ai = cim(a), br = cre(b), bi = cim(b);
    return cmake(ds_sub(ds_mul(ar, br), ds_mul(ai, bi)),
                 ds_add(ds_mul(ar, bi), ds_mul(ai, br)));
}

inline cds cmul_i(cds a) {            // multiply by i
    return cmake(ds_neg(cim(a)), cre(a));
}
inline cds cconj(cds a) { return cmake(cre(a), ds_neg(cim(a))); }

// --- parameter blocks ----------------------------------------------------
// Scalars only, all four bytes wide, so the host struct and this one cannot
// disagree about padding.
struct Geom {
    uint ncell, nspec;
    uint nx, ny, nz, nkx;
    uint two_d, pad;
};

struct FftP {
    uint n;        // transform length along this axis
    uint r;        // radix of this pass
    uint Ns;       // product of the radices already applied
    uint nrow;     // how many transforms
    uint A, SA, SB;// row base = (b % A) * SA + (b / A) * SB
    uint stride;   // element stride within one transform
    int  sign;     // -1 forward, +1 inverse
    uint total;    // nrow * (n / r), the thread count
};

struct ScatterP {
    uint nspec, nkx, ny;
    int  nterm, var0, axis0, var1, axis1;
    float sign0, sign1;
};

struct RktP {
    uint n;                       // 8 * nspec
    float cc_hi, cc_lo, dd_hi, dd_lo;
};

struct FluxP {
    uint ncell;
    uint m0, nm;
    float gamma_hi, gamma_lo;
};

struct CflP {
    uint ncell;
    uint two_d, z_radial;
    float gamma_hi, gamma_lo;
    float dx_hi, dx_lo, dy_hi, dy_lo, dz_hi, dz_lo;
    float rs_hi, rs_lo;
};

struct HermP { uint nx, ny, nz, nkx, ncell, nspec; };

// =========================================================================
// FFT
//
// Stockham autosort: at each pass the input is read with stride n/r inside
// the transform and written with stride Ns, so the output comes out in
// natural order with no bit reversal.  Ns starts at 1 and is multiplied by
// the radix each pass.
//
// The twiddles are NOT computed here.  cos and sin in fp32 would cap the
// whole transform at 24 bits however carefully the butterflies are done, so
// the host builds the table W[k] = exp(-2 pi i k / n) in binary64 and hands it
// over as double-single.  The twiddle a pass needs is
//     exp(sign * 2 pi i * r * (j mod Ns) / (Ns * R))
// and Ns*R divides n at every pass, so that is W at index
//     r * (j mod Ns) * (n / (Ns * R))
// conjugated when the sign is +1.  One table per axis, no runtime
// trigonometry anywhere.
// =========================================================================

inline uint fft_expand(uint idxL, uint N1, uint N2) {
    return (idxL / N1) * N1 * N2 + (idxL % N1);
}

inline cds twiddle(const device cds* W, uint n, uint Ns, uint R, uint jmod, uint r, int sign) {
    uint idx = (r * jmod * (n / (Ns * R))) % n;
    cds w = W[idx];
    return sign < 0 ? w : cconj(w);
}

kernel void k_fft_r2(device const cds* in [[buffer(0)]],
                     device cds* out [[buffer(1)]],
                     device const cds* W [[buffer(2)]],
                     constant FftP& p [[buffer(3)]],
                     uint tid [[thread_position_in_grid]]) {
    if (tid >= p.total) return;
    const uint per = p.n / 2u;
    const uint b = tid / per;
    const uint j = tid % per;
    const uint base = (b % p.A) * p.SA + (b / p.A) * p.SB;
    const uint jm = j % p.Ns;

    cds v0 = in[base + (j) * p.stride];
    cds v1 = in[base + (j + per) * p.stride];
    v1 = cmul(v1, twiddle(W, p.n, p.Ns, 2u, jm, 1u, p.sign));

    cds o0 = cadd(v0, v1);
    cds o1 = csub(v0, v1);

    const uint idxD = fft_expand(j, p.Ns, 2u);
    out[base + (idxD) * p.stride] = o0;
    out[base + (idxD + p.Ns) * p.stride] = o1;
}

kernel void k_fft_r4(device const cds* in [[buffer(0)]],
                     device cds* out [[buffer(1)]],
                     device const cds* W [[buffer(2)]],
                     constant FftP& p [[buffer(3)]],
                     uint tid [[thread_position_in_grid]]) {
    if (tid >= p.total) return;
    const uint per = p.n / 4u;
    const uint b = tid / per;
    const uint j = tid % per;
    const uint base = (b % p.A) * p.SA + (b / p.A) * p.SB;
    const uint jm = j % p.Ns;

    cds v[4];
    for (uint r = 0; r < 4u; ++r) {
        cds x = in[base + (j + r * per) * p.stride];
        v[r] = (r == 0u) ? x : cmul(x, twiddle(W, p.n, p.Ns, 4u, jm, r, p.sign));
    }

    cds t0 = cadd(v[0], v[2]);
    cds t1 = csub(v[0], v[2]);
    cds t2 = cadd(v[1], v[3]);
    cds t3 = csub(v[1], v[3]);
    cds it3 = cmul_i(t3);

    cds o0 = cadd(t0, t2);
    cds o2 = csub(t0, t2);
    cds o1 = (p.sign < 0) ? csub(t1, it3) : cadd(t1, it3);
    cds o3 = (p.sign < 0) ? cadd(t1, it3) : csub(t1, it3);

    const uint idxD = fft_expand(j, p.Ns, 4u);
    out[base + (idxD) * p.stride] = o0;
    out[base + (idxD + p.Ns) * p.stride] = o1;
    out[base + (idxD + 2u * p.Ns) * p.stride] = o2;
    out[base + (idxD + 3u * p.Ns) * p.stride] = o3;
}

// Any radix up to eight, by direct DFT against the same table.  Slower per
// point than the two specialisations above and used only for the odd factors
// a non-power-of-two grid brings; a deck that asks for one still runs.
kernel void k_fft_gen(device const cds* in [[buffer(0)]],
                      device cds* out [[buffer(1)]],
                      device const cds* W [[buffer(2)]],
                      constant FftP& p [[buffer(3)]],
                      uint tid [[thread_position_in_grid]]) {
    if (tid >= p.total) return;
    const uint R = p.r;
    const uint per = p.n / R;
    const uint b = tid / per;
    const uint j = tid % per;
    const uint base = (b % p.A) * p.SA + (b / p.A) * p.SB;
    const uint jm = j % p.Ns;

    cds v[8];
    for (uint r = 0; r < R; ++r) {
        cds x = in[base + (j + r * per) * p.stride];
        v[r] = (r == 0u) ? x : cmul(x, twiddle(W, p.n, p.Ns, R, jm, r, p.sign));
    }

    const uint idxD = fft_expand(j, p.Ns, R);
    for (uint k = 0; k < R; ++k) {
        cds acc = v[0];
        for (uint m = 1; m < R; ++m) {
            uint idx = ((k * m) % R) * (p.n / R);
            cds w = W[idx % p.n];
            if (p.sign > 0) w = cconj(w);
            acc = cadd(acc, cmul(v[m], w));
        }
        out[base + (idxD + k * p.Ns) * p.stride] = acc;
    }
}

// =========================================================================
// transform plumbing
// =========================================================================

kernel void k_real_to_cplx(device const ds* re [[buffer(0)]],
                           device cds* out [[buffer(1)]],
                           constant Geom& g [[buffer(2)]],
                           uint tid [[thread_position_in_grid]]) {
    if (tid >= g.ncell) return;
    ds a = re[tid];
    out[tid] = cds(a.x, a.y, 0.0f, 0.0f);
}

// Keep bins 0 .. nx/2 of each x-row and divide by nx, which is what LAPS does
// immediately after its r2c transform.
kernel void k_extract_half(device const cds* full [[buffer(0)]],
                           device cds* half_ [[buffer(1)]],
                           constant Geom& g [[buffer(2)]],
                           constant float2& invn [[buffer(3)]],
                           uint tid [[thread_position_in_grid]]) {
    if (tid >= g.nspec) return;
    uint i = tid % g.nkx;
    uint t = tid / g.nkx;
    uint j = t % g.ny;
    uint k = t / g.ny;
    cds z = full[i + g.nx * (j + g.ny * k)];
    half_[tid] = cmake(ds_mul(cre(z), invn), ds_mul(cim(z), invn));
}

// Rebuild the full x-row from the stored half by Hermitian symmetry.  Bin 0
// and, for even nx, bin nx/2 are real for a real signal, and the imaginary
// part stored there is roundoff; FFTW's c2r discards it, so this discards it
// too.  On these decks the dealiasing rule has already zeroed the Nyquist bin
// before any inverse transform runs, so the choice is not observable - it is
// made explicitly so that it is not accidental.
kernel void k_expand_herm(device const cds* half_ [[buffer(0)]],
                          device cds* full [[buffer(1)]],
                          constant Geom& g [[buffer(2)]],
                          uint tid [[thread_position_in_grid]]) {
    if (tid >= g.ncell) return;
    uint i = tid % g.nx;
    uint t = tid / g.nx;
    uint j = t % g.ny;
    uint k = t / g.ny;
    const uint nyq = g.nx / 2u;
    cds z;
    if (i <= nyq) {
        z = half_[i + g.nkx * (j + g.ny * k)];
        if (i == 0u || (i == nyq && (g.nx % 2u) == 0u)) z = cmake(cre(z), ds(0.0f, 0.0f));
    } else {
        z = cconj(half_[(g.nx - i) + g.nkx * (j + g.ny * k)]);
    }
    full[tid] = z;
}

kernel void k_take_real(device const cds* full [[buffer(0)]],
                        device ds* re [[buffer(1)]],
                        constant Geom& g [[buffer(2)]],
                        uint tid [[thread_position_in_grid]]) {
    if (tid >= g.ncell) return;
    re[tid] = cre(full[tid]);
}

kernel void k_cscale(device cds* a [[buffer(0)]],
                     constant uint& n [[buffer(1)]],
                     constant float2& s [[buffer(2)]],
                     uint tid [[thread_position_in_grid]]) {
    if (tid >= n) return;
    cds z = a[tid];
    a[tid] = cmake(ds_mul(cre(z), s), ds_mul(cim(z), s));
}

kernel void k_czero(device cds* a [[buffer(0)]],
                    constant uint& n [[buffer(1)]],
                    uint tid [[thread_position_in_grid]]) {
    if (tid >= n) return;
    a[tid] = czero();
}

kernel void k_ccopy(device const cds* src [[buffer(0)]],
                    device cds* dst [[buffer(1)]],
                    constant uint& n [[buffer(2)]],
                    uint tid [[thread_position_in_grid]]) {
    if (tid >= n) return;
    dst[tid] = src[tid];
}

// =========================================================================
// the solver
// =========================================================================

// mhdrhs.f90 calc_flux.  Slot order: u = rho, rho*ux, rho*uy, rho*uz,
// Bx, By, Bz, e;  pr = ux, uy, uz, p.
kernel void k_flux(device const ds* u [[buffer(0)]],
                   device const ds* pr [[buffer(1)]],
                   device ds* flux [[buffer(2)]],
                   constant FluxP& p [[buffer(3)]],
                   uint tid [[thread_position_in_grid]]) {
    if (tid >= p.ncell) return;
    const uint n = p.ncell;
    ds u1 = u[1u * n + tid], u2 = u[2u * n + tid], u3 = u[3u * n + tid];
    ds bx = u[4u * n + tid], by = u[5u * n + tid], bz = u[6u * n + tid];
    ds e  = u[7u * n + tid];
    ds ux = pr[0u * n + tid], uy = pr[1u * n + tid], uz = pr[2u * n + tid];
    ds pp = pr[3u * n + tid];

    ds b2 = ds_add(ds_add(ds_mul(bx, bx), ds_mul(by, by)), ds_mul(bz, bz));
    ds ptot = ds_add(pp, ds_mul(ds_set(0.5f), b2));
    ds udotb = ds_add(ds_add(ds_mul(ux, bx), ds_mul(uy, by)), ds_mul(uz, bz));
    ds eptot = ds_add(e, ptot);

    ds f[18];
    f[0] = u1; f[1] = u2; f[2] = u3;

    f[3] = ds_add(ds_sub(ds_mul(u1, ux), ds_mul(bx, bx)), ptot);
    f[4] = ds_sub(ds_mul(u2, ux), ds_mul(by, bx));
    f[5] = ds_sub(ds_mul(u3, ux), ds_mul(bz, bx));

    f[6] = ds_sub(ds_mul(u1, uy), ds_mul(bx, by));
    f[7] = ds_add(ds_sub(ds_mul(u2, uy), ds_mul(by, by)), ptot);
    f[8] = ds_sub(ds_mul(u3, uy), ds_mul(bz, by));

    f[9]  = ds_sub(ds_mul(u1, uz), ds_mul(bx, bz));
    f[10] = ds_sub(ds_mul(u2, uz), ds_mul(by, bz));
    f[11] = ds_add(ds_sub(ds_mul(u3, uz), ds_mul(bz, bz)), ptot);

    f[12] = ds_sub(ds_mul(uz, by), ds_mul(uy, bz));
    f[13] = ds_sub(ds_mul(ux, bz), ds_mul(uz, bx));
    f[14] = ds_sub(ds_mul(uy, bx), ds_mul(ux, by));

    f[15] = ds_sub(ds_mul(eptot, ux), ds_mul(udotb, bx));
    f[16] = ds_sub(ds_mul(eptot, uy), ds_mul(udotb, by));
    f[17] = ds_sub(ds_mul(eptot, uz), ds_mul(udotb, bz));

    for (uint m = 0; m < p.nm; ++m) flux[m * n + tid] = f[p.m0 + m];
}

// mhdrhs.f90 update_uu_prim_from_uu.
kernel void k_prim(device const ds* u [[buffer(0)]],
                   device ds* pr [[buffer(1)]],
                   constant FluxP& p [[buffer(2)]],
                   uint tid [[thread_position_in_grid]]) {
    if (tid >= p.ncell) return;
    const uint n = p.ncell;
    ds rho = u[tid];
    ds u1 = u[1u * n + tid], u2 = u[2u * n + tid], u3 = u[3u * n + tid];
    ds bx = u[4u * n + tid], by = u[5u * n + tid], bz = u[6u * n + tid];
    ds e  = u[7u * n + tid];
    ds gm1 = ds_sub(ds(p.gamma_hi, p.gamma_lo), ds_set(1.0f));

    ds ux = ds_div(u1, rho);
    ds uy = ds_div(u2, rho);
    ds uz = ds_div(u3, rho);
    ds s = ds_add(ds_add(ds_mul(u1, ux), ds_mul(u2, uy)), ds_mul(u3, uz));
    s = ds_add(s, ds_add(ds_add(ds_mul(bx, bx), ds_mul(by, by)), ds_mul(bz, bz)));
    ds pp = ds_mul(ds_sub(e, ds_mul(ds_set(0.5f), s)), gm1);

    pr[0u * n + tid] = ux;
    pr[1u * n + tid] = uy;
    pr[2u * n + tid] = uz;
    pr[3u * n + tid] = pp;
}

// fnl[var] += sign * (i k_axis) * F.  calc_rhs, read by column.
kernel void k_scatter(device const cds* c [[buffer(0)]],
                      device cds* fnl [[buffer(1)]],
                      device const ds* kx [[buffer(2)]],
                      device const ds* ky [[buffer(3)]],
                      device const ds* kz [[buffer(4)]],
                      constant ScatterP& p [[buffer(5)]],
                      uint tid [[thread_position_in_grid]]) {
    if (tid >= p.nspec) return;
    uint i = tid % p.nkx;
    uint t = tid / p.nkx;
    uint j = t % p.ny;
    uint k = t / p.ny;

    cds f = c[tid];
    ds kv0 = (p.axis0 == 0) ? kx[i] : ((p.axis0 == 1) ? ky[j] : kz[k]);
    ds a0 = ds_mul(ds_set(p.sign0), kv0);
    uint o0 = uint(p.var0) * p.nspec + tid;
    cds z0 = fnl[o0];
    // (a i) * (fr + i fi) = -a fi + i a fr
    fnl[o0] = cmake(ds_sub(cre(z0), ds_mul(a0, cim(f))),
                    ds_add(cim(z0), ds_mul(a0, cre(f))));

    if (p.nterm > 1) {
        ds kv1 = (p.axis1 == 0) ? kx[i] : ((p.axis1 == 1) ? ky[j] : kz[k]);
        ds a1 = ds_mul(ds_set(p.sign1), kv1);
        uint o1 = uint(p.var1) * p.nspec + tid;
        cds z1 = fnl[o1];
        fnl[o1] = cmake(ds_sub(cre(z1), ds_mul(a1, cim(f))),
                        ds_add(cim(z1), ds_mul(a1, cre(f))));
    }
}

// rktmod.f90 rkt, over all eight variables at once.
kernel void k_rkt(device cds* uuF [[buffer(0)]],
                  device cds* fnl [[buffer(1)]],
                  device cds* fnl_rk [[buffer(2)]],
                  constant RktP& p [[buffer(3)]],
                  uint tid [[thread_position_in_grid]]) {
    if (tid >= p.n) return;
    ds cc = ds(p.cc_hi, p.cc_lo);
    ds dd = ds(p.dd_hi, p.dd_lo);
    cds f = fnl[tid];
    cds r = fnl_rk[tid];
    cds u = uuF[tid];
    ds re = ds_add(cre(u), ds_add(ds_mul(cc, cre(f)), ds_mul(dd, cre(r))));
    ds im = ds_add(cim(u), ds_add(ds_mul(cc, cim(f)), ds_mul(dd, cim(r))));
    uuF[tid] = cmake(re, im);
    fnl_rk[tid] = f;
}

// dealiasing.f90.  The mask is decided on the HOST in binary64 and uploaded,
// because the rule is a float comparison - `radius > 1/3` - and a mode that
// sits on the boundary must fall the same side of it as the incumbent's.
// Deciding it in double-single would be a different comparison.
kernel void k_dealias_mask(device cds* uuF [[buffer(0)]],
                           device const uchar* mask [[buffer(1)]],
                           constant Geom& g [[buffer(2)]],
                           uint tid [[thread_position_in_grid]]) {
    if (tid >= g.nspec) return;
    if (mask[tid] != 0) return;
    for (uint v = 0; v < 8u; ++v) uuF[v * g.nspec + tid] = czero();
}

// dealias_option 2: a separable smoothing filter, its three factors built on
// the host in binary64 and multiplied here in the order upstream multiplies
// them.
kernel void k_dealias_filter(device cds* uuF [[buffer(0)]],
                             device const ds* fx [[buffer(1)]],
                             device const ds* fy [[buffer(2)]],
                             device const ds* fz [[buffer(3)]],
                             constant Geom& g [[buffer(4)]],
                             uint tid [[thread_position_in_grid]]) {
    if (tid >= g.nspec) return;
    uint i = tid % g.nkx;
    uint t = tid / g.nkx;
    uint j = t % g.ny;
    uint k = t / g.ny;
    ds w = ds_mul(fx[i], fy[j]);
    if (g.two_d == 0u) w = ds_mul(w, fz[k]);
    for (uint v = 0; v < 8u; ++v) {
        uint o = v * g.nspec + tid;
        cds z = uuF[o];
        uuF[o] = cmake(ds_mul(cre(z), w), ds_mul(cim(z), w));
    }
}

// The implicit viscous and resistive divisions of rktmod.f90.  Dead on every
// deck in checks/, which are ideal MHD; present so a patched deck still gets
// the incumbent's scheme.
kernel void k_implicit(device cds* uuF [[buffer(0)]],
                       device const ds* kx [[buffer(1)]],
                       device const ds* ky [[buffer(2)]],
                       device const ds* kz [[buffer(3)]],
                       constant Geom& g [[buffer(4)]],
                       constant float4& vc [[buffer(5)]],
                       uint tid [[thread_position_in_grid]]) {
    // vc = (v0, v1, coef_hi, coef_lo)
    if (tid >= g.nspec) return;
    uint i = tid % g.nkx;
    uint t = tid / g.nkx;
    uint j = t % g.ny;
    uint k = t / g.ny;
    ds k2 = ds_add(ds_add(ds_mul(kx[i], kx[i]), ds_mul(ky[j], ky[j])),
                   ds_mul(kz[k], kz[k]));
    ds den = ds_add(ds_mul(ds(vc.z, vc.w), k2), ds_set(1.0f));
    uint v0 = uint(vc.x), v1 = uint(vc.y);
    for (uint v = v0; v <= v1; ++v) {
        uint o = v * g.nspec + tid;
        cds z = uuF[o];
        uuF[o] = cmake(ds_div(cre(z), den), ds_div(cim(z), den));
    }
}

// mhd.f90 vardt, one cell, then a per-thread minimum the host finishes.  A
// minimum is exact and order-independent, so no part of this is a reduction
// hazard.
kernel void k_cfl(device const ds* u [[buffer(0)]],
                  device const ds* pr [[buffer(1)]],
                  device ds* out [[buffer(2)]],
                  constant CflP& p [[buffer(3)]],
                  uint tid [[thread_position_in_grid]],
                  uint nthreads [[threads_per_grid]]) {
    const uint n = p.ncell;
    ds gamma = ds(p.gamma_hi, p.gamma_lo);
    ds dx = ds(p.dx_hi, p.dx_lo), dy = ds(p.dy_hi, p.dy_lo), dz = ds(p.dz_hi, p.dz_lo);
    ds rs_exp = ds(p.rs_hi, p.rs_lo);
    ds best = ds(3.4e38f, 0.0f);
    ds two = ds_set(2.0f);
    ds four = ds_set(4.0f);
    ds sqrt2 = ds_sqrt(two);

    for (uint i = tid; i < n; i += nthreads) {
        ds rho = u[i];
        ds bx = u[4u * n + i], by = u[5u * n + i], bz = u[6u * n + i];
        ds ux = pr[0u * n + i], uy = pr[1u * n + i], uz = pr[2u * n + i];
        ds pp = pr[3u * n + i];

        ds csound2 = ds_div(ds_mul(gamma, pp), rho);
        ds rr = ds_sqrt(rho);
        ds cax = ds_div(bx, rr), cay = ds_div(by, rr), caz = ds_div(bz, rr);
        ds ca2 = ds_add(ds_add(ds_mul(cax, cax), ds_mul(cay, cay)), ds_mul(caz, caz));
        ds cms2 = ds_add(csound2, ca2);
        ds cms4 = ds_mul(cms2, cms2);
        ds fc = ds_mul(four, csound2);

        ds d0 = ds_sub(cms4, ds_mul(fc, ds_mul(cax, cax)));
        ds d1 = ds_sub(cms4, ds_mul(fc, ds_mul(cay, cay)));
        ds d2 = ds_sub(cms4, ds_mul(fc, ds_mul(caz, caz)));
        ds zero = ds_set(0.0f);
        d0 = ds_sqrt(ds_max(d0, zero));
        d1 = ds_sqrt(ds_max(d1, zero));
        d2 = ds_sqrt(ds_max(d2, zero));

        ds cfx = ds_div(ds_sqrt(ds_add(cms2, d0)), sqrt2);
        ds cfy = ds_div(ds_sqrt(ds_add(cms2, d1)), sqrt2);
        ds cfz = ds_div(ds_sqrt(ds_add(cms2, d2)), sqrt2);
        ds csx = ds_div(ds_sqrt(ds_max(ds_sub(cms2, d0), zero)), sqrt2);
        ds csy = ds_div(ds_sqrt(ds_max(ds_sub(cms2, d1), zero)), sqrt2);
        ds csz = ds_div(ds_sqrt(ds_max(ds_sub(cms2, d2), zero)), sqrt2);

        // The seven-way max upstream writes out in full.
        ds cmx = ds_abs(ux);
        cmx = ds_max(cmx, ds_abs(ds_add(ux, cfx))); cmx = ds_max(cmx, ds_abs(ds_sub(ux, cfx)));
        cmx = ds_max(cmx, ds_abs(ds_add(ux, csx))); cmx = ds_max(cmx, ds_abs(ds_sub(ux, csx)));
        cmx = ds_max(cmx, ds_abs(ds_add(ux, cax))); cmx = ds_max(cmx, ds_abs(ds_sub(ux, cax)));

        ds cmy = ds_abs(uy);
        cmy = ds_max(cmy, ds_abs(ds_add(uy, cfy))); cmy = ds_max(cmy, ds_abs(ds_sub(uy, cfy)));
        cmy = ds_max(cmy, ds_abs(ds_add(uy, csy))); cmy = ds_max(cmy, ds_abs(ds_sub(uy, csy)));
        cmy = ds_max(cmy, ds_abs(ds_add(uy, cay))); cmy = ds_max(cmy, ds_abs(ds_sub(uy, cay)));

        ds cmz = ds_abs(uz);
        cmz = ds_max(cmz, ds_abs(ds_add(uz, cfz))); cmz = ds_max(cmz, ds_abs(ds_sub(uz, cfz)));
        cmz = ds_max(cmz, ds_abs(ds_add(uz, csz))); cmz = ds_max(cmz, ds_abs(ds_sub(uz, csz)));
        cmz = ds_max(cmz, ds_abs(ds_add(uz, caz))); cmz = ds_max(cmz, ds_abs(ds_sub(uz, caz)));

        ds dtx = ds_div(dx, cmx);
        if (p.two_d != 0u && p.z_radial != 0u) dtx = ds_mul(dtx, rs_exp);
        ds dty = ds_mul(ds_div(dy, cmy), rs_exp);
        ds d = ds_min(dtx, dty);
        if (p.two_d == 0u) d = ds_min(d, ds_mul(ds_div(dz, cmz), rs_exp));
        best = ds_min(best, d);
    }
    out[tid] = best;
}

kernel void k_finite(device const ds* u [[buffer(0)]],
                     device atomic_uint* bad [[buffer(1)]],
                     constant uint& n8 [[buffer(2)]],
                     uint tid [[thread_position_in_grid]]) {
    if (tid >= n8) return;
    float x = u[tid].x;
    if (!(fabs(x) <= 3.4028235e38f)) atomic_store_explicit(bad, 1u, memory_order_relaxed);
}
)MSL";
