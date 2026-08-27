// SPDX-License-Identifier: GPL-2.0
// Initial condition, in the two forms upstream ships.
//
// `ifield` and `ipert` are indices into two `select case` blocks that DO NOT
// agree between src_compressible and src_compressible/2D - ifield=5 is "a jet
// along x" in the 3D tree and "uniform field plus a jet in uz" in the 2D one.
// Both are reproduced here under the variant the product was built for.
//
// Cases that draw from Fortran's RANDOM_NUMBER are refused rather than
// approximated: their trajectory is a property of gfortran's PRNG stream, and
// a port that guessed at it would differ from the incumbent everywhere rather
// than fail loudly.  No deck in checks/ selects one.
#pragma once

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "laps_params.hpp"

namespace laps {

// Real-space field storage: uu[var][ix + nx*(iy + ny*iz)], eight variables.
// The index order is Fortran's, so a variable's slab is exactly the byte range
// the snapshot file wants and the writer needs no transpose.
struct RealState {
    std::vector<double> u[8];       // conserved after init_conserved()
    std::vector<double> prim[4];    // ux, uy, uz, p
    void allocate(int64_t n) {
        for (auto& a : u) a.assign(n, 0.0);
        for (auto& a : prim) a.assign(n, 0.0);
    }
};

inline void background_fields_3d(const Params& p, RealState& s) {
    const int64_t nx = p.nx, ny = p.ny, nz = p.nz;
    auto at = [&](int64_t i, int64_t j, int64_t k) { return i + nx * (j + ny * k); };

    for (int64_t n = 0; n < p.ncell(); ++n) {
        s.u[0][n] = p.rho0;
        s.u[7][n] = p.rho0 * p.T0;      // pressure at this stage
    }
    const double ninf = 1.0;

    switch (p.ifield) {
    case 1: {   // double current sheet B0 = B0x(z), density balances pressure
        const double zlow = 0.25 * p.Lz, zup = 0.75 * p.Lz, zcent = 0.5 * p.Lz;
        for (int64_t k = 0; k < nz; ++k) {
            const double z = p.zgrid[k];
            double bx, rho;
            if (z < zcent) {
                bx = p.B0 * std::tanh((z - zlow) / p.a0);
                rho = ninf + p.B0 * p.B0 / 2.0 / p.T0 /
                             (std::cosh((z - zlow) / p.a0) * std::cosh((z - zlow) / p.a0));
            } else {
                bx = -p.B0 * std::tanh((z - zup) / p.a0);
                rho = ninf + p.B0 * p.B0 / 2.0 / p.T0 /
                             (std::cosh((z - zup) / p.a0) * std::cosh((z - zup) / p.a0));
            }
            for (int64_t j = 0; j < ny; ++j)
                for (int64_t i = 0; i < nx; ++i) {
                    s.u[4][at(i, j, k)] = bx;
                    s.u[0][at(i, j, k)] = rho;
                    s.u[7][at(i, j, k)] = rho * p.T0;
                }
        }
        break;
    }
    case 2: {   // double Harris shear layer in Ux(y) and rho(y)
        const double ylow = 0.25 * p.Ly, yup = 0.75 * p.Ly, ycent = 0.5 * p.Ly;
        const double a0 = p.Ly * 0.5 * p.shear_width;
        for (int64_t j = 0; j < ny; ++j) {
            const double y = p.ygrid[j];
            double ux, rho;
            if (y < ycent) {
                ux = 0.5 * (p.U_fast - p.U_slow) * std::tanh((y - ylow) / a0) +
                     0.5 * (p.U_fast + p.U_slow);
                rho = 0.5 * (p.n_fast - p.n_slow) * std::tanh((y - ylow) / a0) +
                      0.5 * (p.n_fast + p.n_slow);
            } else {
                ux = -0.5 * (p.U_fast - p.U_slow) * std::tanh((y - yup) / a0) +
                     0.5 * (p.U_fast + p.U_slow);
                rho = -0.5 * (p.n_fast - p.n_slow) * std::tanh((y - yup) / a0) +
                      0.5 * (p.n_fast + p.n_slow);
            }
            for (int64_t k = 0; k < nz; ++k)
                for (int64_t i = 0; i < nx; ++i) {
                    s.u[1][at(i, j, k)] = ux;
                    s.u[0][at(i, j, k)] = rho;
                }
        }
        for (int64_t n = 0; n < p.ncell(); ++n) {
            s.u[7][n] = p.press0;
            s.u[4][n] = p.Bx0;
            s.u[5][n] = p.By0;
        }
        break;
    }
    case 3:     // uniform background field
        for (int64_t n = 0; n < p.ncell(); ++n) {
            s.u[4][n] = p.Bx0;
            s.u[5][n] = p.By0;
            s.u[6][n] = p.Bz0;
            s.u[7][n] = p.press0;
        }
        break;
    case 4: {   // double current sheet B0 = B0x(y), temperature balances pressure
        const double ylow = 0.25 * p.Ly, yup = 0.75 * p.Ly, ycent = 0.5 * p.Ly;
        for (int64_t j = 0; j < ny; ++j) {
            const double y = p.ygrid[j];
            const double bx = (y < ycent) ? p.B0 * std::tanh((y - ylow) / p.a0)
                                          : -p.B0 * std::tanh((y - yup) / p.a0);
            for (int64_t k = 0; k < nz; ++k)
                for (int64_t i = 0; i < nx; ++i) {
                    s.u[4][at(i, j, k)] = bx;
                    s.u[7][at(i, j, k)] = p.press0 - 0.5 * bx * bx;
                    s.u[0][at(i, j, k)] = ninf;
                }
        }
        break;
    }
    case 5: {   // uniform field plus a jet in ux
        for (int64_t n = 0; n < p.ncell(); ++n) {
            s.u[4][n] = p.Bx0;
            s.u[5][n] = p.By0;
            s.u[6][n] = p.Bz0;
            s.u[7][n] = p.press0;
        }
        for (int64_t k = 0; k < nz; ++k)
            for (int64_t j = 0; j < ny; ++j) {
                const double r = std::sqrt((p.zgrid[k] - p.Lz / 2) * (p.zgrid[k] - p.Lz / 2) +
                                           (p.ygrid[j] - p.Ly / 2) * (p.ygrid[j] - p.Ly / 2));
                const double v = p.Vjet / 2 * (1 - std::tanh((r - 1) / 0.1));
                for (int64_t i = 0; i < nx; ++i) s.u[1][at(i, j, k)] = v;
            }
        break;
    }
    default:
        break;      // upstream's `case default: continue` - uniform rho and P
    }
}

inline void perturbation_3d(const Params& p, RealState& s) {
    const int64_t nx = p.nx, ny = p.ny, nz = p.nz;
    auto at = [&](int64_t i, int64_t j, int64_t k) { return i + nx * (j + ny * k); };

    switch (p.ipert) {
    case 0:
        break;
    case 1: {   // circularly polarised Alfven wave along x
        const double k0 = 2 * Params::pi / p.Lx * static_cast<double>(p.wave_number_jet);
        for (int64_t i = 0; i < nx; ++i) {
            const double sn = std::sin(k0 * p.xgrid[i]);
            const double cs = std::cos(k0 * p.xgrid[i]);
            for (int64_t k = 0; k < nz; ++k)
                for (int64_t j = 0; j < ny; ++j) {
                    const int64_t n = at(i, j, k);
                    const double vA = p.db0 / std::sqrt(s.u[0][n]);
                    s.u[6][n] -= p.db0 * sn;
                    s.u[3][n] += vA * sn;
                    s.u[1][n] += vA * cs * p.sin_cor_ang;
                    s.u[4][n] -= p.db0 * cs * p.sin_cor_ang;
                    s.u[2][n] += vA * cs * p.cos_cor_ang;
                    s.u[5][n] -= p.db0 * cs * p.cos_cor_ang;
                }
        }
        break;
    }
    default:
        throw std::runtime_error(
            "deck selects ipert=" + std::to_string(p.ipert) +
            ", which upstream builds from Fortran RANDOM_NUMBER or from a "
            "setup this port does not implement; see skill/references/diary.md");
    }
}

inline void background_fields_2d(const Params& p, RealState& s) {
    const int64_t nx = p.nx, ny = p.ny;
    auto at = [&](int64_t i, int64_t j) { return i + nx * j; };

    for (int64_t n = 0; n < p.ncell(); ++n) {
        s.u[0][n] = p.rho0;
        s.u[7][n] = p.rho0 * p.T0;
    }
    const double ninf = 1.0;

    switch (p.ifield) {
    case 0:     // uniform background field
    case 5: {   // uniform background field, plus a jet in uz for case 5
        for (int64_t n = 0; n < p.ncell(); ++n) {
            s.u[4][n] = p.Bx0;
            s.u[5][n] = p.By0;
            s.u[6][n] = p.Bz0;
            s.u[7][n] = p.press0;
        }
        if (p.ifield == 5) {
            for (int64_t j = 0; j < ny; ++j)
                for (int64_t i = 0; i < nx; ++i) {
                    const double r =
                        std::sqrt((p.xgrid[i] - p.Lx / 2) * (p.xgrid[i] - p.Lx / 2) +
                                  (p.ygrid[j] - p.Ly / 2) * (p.ygrid[j] - p.Ly / 2));
                    s.u[3][at(i, j)] = p.Vjet / 2 * (1 - std::tanh((r - 1) / 0.1));
                }
        }
        break;
    }
    case 6: {   // double current sheet B0 = B0x(y)
        const double ylow = 0.25 * p.Ly, yup = 0.75 * p.Ly, ycent = 0.5 * p.Ly;
        for (int64_t j = 0; j < ny; ++j) {
            const double y = p.ygrid[j];
            const double bx = (y < ycent) ? p.B0 * std::tanh((y - ylow) / p.a0)
                                          : -p.B0 * std::tanh((y - yup) / p.a0);
            for (int64_t i = 0; i < nx; ++i) {
                s.u[4][at(i, j)] = bx;
                s.u[7][at(i, j)] = p.press0 - 0.5 * bx * bx;
                s.u[0][at(i, j)] = ninf;
            }
        }
        break;
    }
    default:
        throw std::runtime_error(
            "deck selects ifield=" + std::to_string(p.ifield) +
            " for the 2D solver, which this port does not implement; "
            "see skill/references/diary.md");
    }
}

inline void perturbation_2d(const Params& p, RealState& s) {
    const int64_t nx = p.nx, ny = p.ny;
    auto at = [&](int64_t i, int64_t j) { return i + nx * j; };

    switch (p.ipert) {
    case 0:
        break;
    case 1: {   // monochromatic outward Alfven wave
        const double k0 = 2 * Params::pi / p.Lx;
        for (int64_t i = 0; i < nx; ++i) {
            const double sn = std::sin(k0 * p.xgrid[i]);
            const double cs = std::cos(k0 * p.xgrid[i]);
            for (int64_t j = 0; j < ny; ++j) {
                const int64_t n = at(i, j);
                const double rs = std::sqrt(s.u[0][n]);
                s.u[6][n] -= p.db0 * rs * sn;
                s.u[3][n] += p.db0 * sn;
                s.u[1][n] += p.db0 * cs * p.sin_cor_ang;
                s.u[4][n] -= p.db0 * rs * cs * p.sin_cor_ang;
                s.u[2][n] += p.db0 * cs * p.cos_cor_ang;
                s.u[5][n] -= p.db0 * rs * cs * p.cos_cor_ang;
            }
        }
        break;
    }
    case 2: {   // Gaussian pulse in uz and Bz
        for (int64_t j = 0; j < ny; ++j)
            for (int64_t i = 0; i < nx; ++i) {
                const int64_t n = at(i, j);
                const double ex = (p.xgrid[i] - 0.5 * p.Lx) / (0.1 * p.Lx);
                const double ey = (p.ygrid[j] - 0.5 * p.Ly) / (0.1 * p.Ly);
                const double g = std::exp(-(ex * ex) - (ey * ey));
                s.u[3][n] += p.db0 * g;
                s.u[6][n] -= p.db0 / s.u[0][n] * g;
            }
        break;
    }
    default:
        throw std::runtime_error(
            "deck selects ipert=" + std::to_string(p.ipert) +
            " for the 2D solver, which upstream builds from Fortran "
            "RANDOM_NUMBER or from a setup this port does not implement; "
            "see skill/references/diary.md");
    }
}

// mhdinit.f90 initial_calc_conserve_variable: the arrays have been filled with
// PRIMITIVE variables in the conserved slots; swap them into conserved form
// and keep the primitives beside them.
inline void init_conserved(const Params& p, RealState& s) {
    const double gm1 = p.adiabatic_index - 1;
    for (int64_t n = 0; n < p.ncell(); ++n) {
        const double ux = s.u[1][n], uy = s.u[2][n], uz = s.u[3][n];
        const double pr = s.u[7][n];
        const double rho = s.u[0][n];
        const double bx = s.u[4][n], by = s.u[5][n], bz = s.u[6][n];
        s.prim[0][n] = ux; s.prim[1][n] = uy; s.prim[2][n] = uz; s.prim[3][n] = pr;
        s.u[1][n] = rho * ux;
        s.u[2][n] = rho * uy;
        s.u[3][n] = rho * uz;
        s.u[7][n] = pr / gm1 + 0.5 * (rho * (ux * ux + uy * uy + uz * uz) +
                                      bx * bx + by * by + bz * bz);
    }
}

inline void build_initial_condition(const Params& p, RealState& s) {
    s.allocate(p.ncell());
    if (p.two_d()) {
        background_fields_2d(p, s);
        perturbation_2d(p, s);
    } else {
        background_fields_3d(p, s);
        perturbation_3d(p, s);
    }
    init_conserved(p, s);
}

}  // namespace laps
