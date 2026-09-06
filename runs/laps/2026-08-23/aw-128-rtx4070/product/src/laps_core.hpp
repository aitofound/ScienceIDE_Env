// SPDX-License-Identifier: GPL-2.0
// The scheme, stated once, in the form every backend implements.
//
// This header holds no arrays and no FFT.  It holds the parts of LAPS that are
// pure arithmetic on a single cell or a single mode, so that the CPU, CUDA and
// Metal builds cannot drift from each other or from the incumbent: the flux
// tensor, the primitive recovery, the CFL speed, the RK3 coefficients, the
// dealiasing mask, and the table that says which flux component drives which
// right-hand side.
//
// The scheme is pinned by the task: same Runge-Kutta integrator, same
// dealiasing rule, same CFL condition.  Nothing here is a choice.
#pragma once

#include <cmath>
#include <cstdint>

// Every function in this header is compiled into the CUDA and Metal device
// code as well as the host build, so that the three backends run the SAME
// arithmetic rather than three transcriptions of it.  Under nvcc that means
// __host__ __device__; under a plain host compiler it means nothing.
#if defined(__CUDACC__)
#define LAPS_HD __host__ __device__
#else
#define LAPS_HD
#endif

namespace laps {

// --- RK3 -----------------------------------------------------------------
// rktmod.f90.  A three-stage low-storage scheme carrying one extra register
// (fnl_rk): u <- u + cc[i]*f(u) + dd[i]*f_prev, f_prev <- f(u).
struct RK3 {
    double cc[3], dd[3], ts[3];
    LAPS_HD void init(double dt) {
        cc[0] = (8.0 / 15.0) * dt;  dd[0] = 0.0;
        cc[1] = (5.0 / 12.0) * dt;  dd[1] = (-17.0 / 60.0) * dt;
        cc[2] = 0.75 * dt;          dd[2] = (-5.0 / 12.0) * dt;
        ts[0] = (8.0 / 15.0) * dt;
        ts[1] = (2.0 / 15.0) * dt;
        ts[2] = (1.0 / 3.0) * dt;
    }
};

// --- the flux tensor -----------------------------------------------------
// mhdrhs.f90 calc_flux, one cell.  `u` is the conserved state and `pr` the
// primitives, both in the slot order the rest of the port uses.
//   u  : rho, rho*ux, rho*uy, rho*uz, Bx, By, Bz, e
//   pr : ux, uy, uz, p
// `out[m]` is written for m in [0,18) with the stride the caller supplies.
struct FluxCell {
    double f[18];
};

LAPS_HD inline FluxCell flux_of(const double u[8], const double pr[4]) {
    FluxCell o;
    const double p = pr[3];
    const double bx = u[4], by = u[5], bz = u[6];
    const double ux = pr[0], uy = pr[1], uz = pr[2];
    const double ptot = p + 0.5 * (bx * bx + by * by + bz * bz);
    const double udotb = ux * bx + uy * by + uz * bz;

    o.f[0] = u[1];  o.f[1] = u[2];  o.f[2] = u[3];

    o.f[3] = u[1] * ux - bx * bx + ptot;
    o.f[4] = u[2] * ux - by * bx;
    o.f[5] = u[3] * ux - bz * bx;

    o.f[6] = u[1] * uy - bx * by;
    o.f[7] = u[2] * uy - by * by + ptot;
    o.f[8] = u[3] * uy - bz * by;

    o.f[9]  = u[1] * uz - bx * bz;
    o.f[10] = u[2] * uz - by * bz;
    o.f[11] = u[3] * uz - bz * bz + ptot;

    o.f[12] = uz * by - uy * bz;
    o.f[13] = ux * bz - uz * bx;
    o.f[14] = uy * bx - ux * by;

    o.f[15] = (u[7] + ptot) * ux - udotb * bx;
    o.f[16] = (u[7] + ptot) * uy - udotb * by;
    o.f[17] = (u[7] + ptot) * uz - udotb * bz;
    return o;
}

// --- which flux drives which right-hand side ------------------------------
// calc_rhs writes, for every mode,
//     fnl[0]   = -( ikx F0  + iky F1  + ikz F2  )
//     fnl[1..3]= -( ikx F3  + iky F4  + ikz F5  ) and the y,z rows
//     fnl[4]   =    ikz F13 - iky F14
//     fnl[5]   =    ikx F14 - ikz F12
//     fnl[6]   =    iky F12 - ikx F13
//     fnl[7]   = -( ikx F15 + iky F16 + ikz F17 )
// Read the other way round, each flux component contributes to at most two
// right-hand sides with a coefficient of the form (sign) * i * k_axis.  That
// transposed form is what lets a backend transform one flux component and
// scatter it immediately, instead of holding all eighteen spectra at once -
// which at 256^3 is 2.4 GB that a 12 GB card would rather spend elsewhere.
struct FluxTerm { int var; int axis; double sign; };   // axis 0=x 1=y 2=z
struct FluxDrive { int n; FluxTerm t[2]; };

LAPS_HD inline FluxDrive flux_drive(int m) {
    switch (m) {
    case 0:  return {1, {{0, 0, -1}, {}}};
    case 1:  return {1, {{0, 1, -1}, {}}};
    case 2:  return {1, {{0, 2, -1}, {}}};
    case 3:  return {1, {{1, 0, -1}, {}}};
    case 4:  return {1, {{1, 1, -1}, {}}};
    case 5:  return {1, {{1, 2, -1}, {}}};
    case 6:  return {1, {{2, 0, -1}, {}}};
    case 7:  return {1, {{2, 1, -1}, {}}};
    case 8:  return {1, {{2, 2, -1}, {}}};
    case 9:  return {1, {{3, 0, -1}, {}}};
    case 10: return {1, {{3, 1, -1}, {}}};
    case 11: return {1, {{3, 2, -1}, {}}};
    case 12: return {2, {{5, 2, -1}, {6, 1, +1}}};
    case 13: return {2, {{4, 2, +1}, {6, 0, -1}}};
    case 14: return {2, {{4, 1, -1}, {5, 0, +1}}};
    case 15: return {1, {{7, 0, -1}, {}}};
    case 16: return {1, {{7, 1, -1}, {}}};
    default: return {1, {{7, 2, -1}, {}}};
    }
}

// --- primitive recovery ---------------------------------------------------
// mhdrhs.f90 update_uu_prim_from_uu.
LAPS_HD inline void prim_of(const double u[8], double gamma, double pr[4]) {
    const double inv_rho = 1.0 / u[0];
    pr[0] = u[1] * inv_rho;
    pr[1] = u[2] * inv_rho;
    pr[2] = u[3] * inv_rho;
    pr[3] = (u[7] - 0.5 * (u[1] * pr[0] + u[2] * pr[1] + u[3] * pr[2] +
                           u[4] * u[4] + u[5] * u[5] + u[6] * u[6])) * (gamma - 1);
}

LAPS_HD inline double fmax_(double a, double b) { return a > b ? a : b; }

// max(|u+cf|, |u+cs|, |u+ca|, |u-cf|, |u-cs|, |u-ca|, |u|) - upstream's exact
// seven-way max.  cf >= cs >= 0 and cf >= |ca|, so it reduces to |u| + cf; the
// long form is kept because "reduces to" is an argument and this is a check.
LAPS_HD inline double cmax_(double u, double cf, double cs, double ca) {
    double m = std::fabs(u);
    const double c[3] = {cf, cs, ca};
    for (int i = 0; i < 3; ++i) {
        m = fmax_(m, std::fabs(u + c[i]));
        m = fmax_(m, std::fabs(u - c[i]));
    }
    return m;
}

// --- the CFL timestep ------------------------------------------------------
// mhd.f90 vardt, one cell: the largest signal speed along each axis, from the
// fast, slow and Alfven characteristics plus the flow itself, then dx over it.
// Upstream takes the MINIMUM over cells and ranks and multiplies by cfl, and a
// minimum is exact and order-independent, so a port may reduce however it
// likes without moving the answer.
struct CflSpeeds { double cx, cy, cz; };

LAPS_HD inline CflSpeeds cfl_speeds(const double u[8], const double pr[4], double gamma) {
    const double rho = u[0];
    const double csound2 = gamma * pr[3] / rho;
    const double rs = std::sqrt(rho);
    const double cax = u[4] / rs, cay = u[5] / rs, caz = u[6] / rs;
    const double calfven2 = cax * cax + cay * cay + caz * caz;
    const double cms2 = csound2 + calfven2;

    const double dx2 = std::sqrt(fmax_(cms2 * cms2 - 4 * csound2 * cax * cax, 0.0));
    const double dy2 = std::sqrt(fmax_(cms2 * cms2 - 4 * csound2 * cay * cay, 0.0));
    const double dz2 = std::sqrt(fmax_(cms2 * cms2 - 4 * csound2 * caz * caz, 0.0));

    const double r2 = std::sqrt(2.0);
    const double cfx = std::sqrt(cms2 + dx2) / r2;
    const double cfy = std::sqrt(cms2 + dy2) / r2;
    const double cfz = std::sqrt(cms2 + dz2) / r2;
    const double csx = std::sqrt(fmax_(cms2 - dx2, 0.0)) / r2;
    const double csy = std::sqrt(fmax_(cms2 - dy2, 0.0)) / r2;
    const double csz = std::sqrt(fmax_(cms2 - dz2, 0.0)) / r2;

    CflSpeeds o;
    o.cx = cmax_(pr[0], cfx, csx, cax);
    o.cy = cmax_(pr[1], cfy, csy, cay);
    o.cz = cmax_(pr[2], cfz, csz, caz);
    return o;
}

// --- dealiasing ------------------------------------------------------------
// dealiasing.f90.  Option 1 is the two-thirds rule on a SPHERE in normalised
// wavenumber: a mode survives when sqrt((kx Lx/2 pi nx)^2 + ...) <= 1/3, and
// kx Lx / (2 pi nx) is just (signed mode index)/nx.  Option 2 is a compact
// smoothing filter, separable in the three axes.
LAPS_HD inline bool dealias_keep_sphere(double kx, double ky, double kz,
                                double Lx, double Ly, double Lz,
                                long nx, long ny, long nz, bool two_d) {
    const double twopi = 6.283185307179586;
    const double ax = kx * Lx / (twopi * static_cast<double>(nx));
    const double ay = ky * Ly / (twopi * static_cast<double>(ny));
    const double az = two_d ? 0.0 : kz * Lz / (twopi * static_cast<double>(nz));
    const double r = std::sqrt(ax * ax + ay * ay + az * az);
    return !(r > 1.0 / 3.0);
}

LAPS_HD inline double dealias_filter_1d(double k, double L, long n, double af) {
    const double aj = (5.0 + 6.0 * af) / 8.0;
    const double bj = (1.0 + 2.0 * af) / 2.0;
    const double cj = -(1.0 - 2 * af) / 8.0;
    const double w = k * L / static_cast<double>(n);
    return (aj + bj * std::cos(w) + cj * std::cos(2 * w)) / (1 + 2 * af * std::cos(w));
}

}  // namespace laps
