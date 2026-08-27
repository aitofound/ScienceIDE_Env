// SPDX-License-Identifier: GPL-2.0
// The deck, resolved against the upstream defaults, plus the grid and the
// wavenumbers it implies.
//
// Two solver variants exist upstream and they are different programs, not two
// settings of one: src_compressible and src_compressible/2D disagree about
// what ifield and ipert mean, about how often the CFL timestep is recomputed,
// and about which namelist groups exist.  Which one a check builds is part of
// the check (rubric.solver.source_dir), so it is a property of the product,
// declared at build time - never sniffed from the deck's physics.
#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "laps_deck.hpp"

namespace laps {

enum class Variant { D3, D2 };

struct Params {
    Variant variant = Variant::D3;

    // &genr
    double tmax = 0.0, dtout = 0.0, dtrms = 0.0;
    bool output_primitive = true;
    bool if_restart = false;
    long n_start = 0;

    // &numerical
    double cfl = 0.5;
    long dealias_option = 2;
    double afx = 0.495, afy = 0.495, afz = 0.495;
    bool if_resis_exp = false, if_visc_exp = false, if_conserve_background = false;
    bool if_limit_dt_increase = false;   // 2D only

    // &grid
    long nx = 128, ny = 128, nz = 64;
    double Lx = 0, Ly = 0, Lz = 0;

    // &field
    long ifield = 0;
    double B0 = 1.0, a0 = 0.05, shear_width = 0.0, press0 = 0.0;
    double U_fast = 0, U_slow = 0, n_fast = 0, n_slow = 0;
    double Bx0 = 0, By0 = 0, Bz0 = 0;
    double jet_width = 1.0, Vjet = 0.3;
    double current_sheet_width = 0.075;   // 2D only

    // &pert
    long ipert = 0;
    double db0 = 0.0, dv0 = 0.0, drho0 = 0.0;
    long wave_number_jet = 1;
    long nmode = 16;

    // &phys
    double adiabatic_index = 5.0 / 3.0;
    bool if_resis = false, if_visc = false;
    double resistivity = 0.0, viscosity = 0.0;

    // &AEB
    bool if_AEB = false, if_corotating = false, if_z_radial = false;
    double radius0 = 30.0, Ur0 = 1.167, corotating_angle = 0.0;

    // &Hall
    bool if_Hall = false;
    double ion_inertial_length = 0.0;

    // Fixed upstream constants (mhdinit.f90).  pi is the literal upstream
    // spells out, and with -fdefault-real-8 that literal is a binary64
    // constant, so this is the same number bit for bit.
    static constexpr double pi = 3.141592653589793;
    double T0 = 1.0, rho0 = 1.0;

    // --- derived ---------------------------------------------------------
    double dx = 0, dy = 0, dz = 0;
    double radius = 30.0, Ur = 0.0, tau_exp = 0.0;
    double cos_cor_ang = 1.0, sin_cor_ang = 0.0;

    std::vector<double> xgrid, ygrid, zgrid;
    std::vector<double> kx, ky, kz;

    long nkx() const { return nx / 2 + 1; }
    int64_t ncell() const { return static_cast<int64_t>(nx) * ny * nz; }
    int64_t nspec() const { return static_cast<int64_t>(nkx()) * ny * nz; }
    bool two_d() const { return variant == Variant::D2; }

    // How many steps between CFL recomputations.  3D recomputes every step;
    // 2D recomputes on istep % 20 == 0 only (mhd.f90, dstep_calcdt).  This is
    // not a tuning knob: it changes the step sequence, so it is pinned to the
    // variant the check builds.
    long dstep_calcdt() const { return two_d() ? 20 : 1; }

    void from_deck(const Deck& d) {
        tmax   = d.real("tmax", tmax);
        dtout  = d.real("dtout", dtout);
        dtrms  = d.real("dtrms", dtrms);
        output_primitive = d.logical("output_primitive", output_primitive);
        if_restart = d.logical("if_restart", if_restart);
        n_start = d.integer("n_start", n_start);

        cfl = d.real("cfl", cfl);
        dealias_option = d.integer("dealias_option", dealias_option);
        afx = d.real("afx", afx); afy = d.real("afy", afy); afz = d.real("afz", afz);
        if_resis_exp = d.logical("if_resis_exp", if_resis_exp);
        if_visc_exp  = d.logical("if_visc_exp", if_visc_exp);
        if_conserve_background = d.logical("if_conserve_background", if_conserve_background);
        if_limit_dt_increase = d.logical("if_limit_dt_increase", if_limit_dt_increase);

        nx = d.integer("nx", nx);
        ny = d.integer("ny", ny);
        Lx = d.real("Lx", Lx);
        Ly = d.real("Ly", Ly);
        if (two_d()) {
            nz = 1;
            Lz = 0.0;
        } else {
            nz = d.integer("nz", nz);
            Lz = d.real("Lz", Lz);
        }

        ifield = d.integer("ifield", ifield);
        B0 = d.real("B0", B0);
        a0 = d.real("a0", a0);
        shear_width = d.real("shear_width", shear_width);
        press0 = d.real("press0", press0);
        U_fast = d.real("U_fast", U_fast);
        U_slow = d.real("U_slow", U_slow);
        n_fast = d.real("n_fast", n_fast);
        n_slow = d.real("n_slow", n_slow);
        Bx0 = d.real("Bx0", Bx0);
        By0 = d.real("By0", By0);
        Bz0 = d.real("Bz0", Bz0);
        jet_width = d.real("jet_width", jet_width);
        Vjet = d.real("Vjet", Vjet);
        current_sheet_width = d.real("current_sheet_width", current_sheet_width);

        ipert = d.integer("ipert", ipert);
        db0 = d.real("db0", db0);
        dv0 = d.real("dv0", dv0);
        drho0 = d.real("drho0", drho0);
        wave_number_jet = d.integer("wave_number_jet", wave_number_jet);
        nmode = d.integer("nmode", nmode);

        adiabatic_index = d.real("adiabatic_index", adiabatic_index);
        if_resis = d.logical("if_resis", if_resis);
        if_visc  = d.logical("if_visc", if_visc);
        resistivity = d.real("resistivity", resistivity);
        viscosity = d.real("viscosity", viscosity);

        if_AEB = d.logical("if_AEB", if_AEB);
        if_corotating = d.logical("if_corotating", if_corotating);
        if_z_radial = d.logical("if_z_radial", if_z_radial);
        radius0 = d.real("radius0", radius0);
        Ur0 = d.real("Ur0", Ur0);
        corotating_angle = d.real("corotating_angle", corotating_angle);

        if_Hall = d.logical("if_Hall", if_Hall);
        ion_inertial_length = d.real("ion_inertial_length", ion_inertial_length);

        // mhd.f90: `if (.not. if_AEB) Ur0 = 0.0`, then AEB_initialize.
        if (!if_AEB) Ur0 = 0.0;
        if (!if_corotating) corotating_angle = 0.0;
        cos_cor_ang = std::cos(corotating_angle);
        sin_cor_ang = std::sin(corotating_angle);
        radius = radius0;
        Ur = Ur0;
        tau_exp = radius / Ur;              // +inf when Ur == 0, exactly as upstream

        build_grid();
    }

    void build_grid() {
        dx = Lx / static_cast<double>(nx);
        dy = Ly / static_cast<double>(ny);
        dz = two_d() ? 0.0 : Lz / static_cast<double>(nz);

        xgrid.resize(nx); kx.resize(nx);
        for (long i = 0; i < nx; ++i) {
            xgrid[i] = static_cast<double>(i) * dx;
            kx[i] = (i <= nx / 2) ? 2 * pi * static_cast<double>(i) / Lx
                                  : 2 * pi * static_cast<double>(i - nx) / Lx;
        }
        ygrid.resize(ny); ky.resize(ny);
        for (long i = 0; i < ny; ++i) {
            ygrid[i] = static_cast<double>(i) * dy;
            ky[i] = (i <= ny / 2) ? 2 * pi * static_cast<double>(i) / Ly
                                  : 2 * pi * static_cast<double>(i - ny) / Ly;
        }
        zgrid.assign(nz, 0.0); kz.assign(nz, 0.0);
        if (!two_d()) {
            for (long i = 0; i < nz; ++i) {
                zgrid[i] = static_cast<double>(i) * dz;
                kz[i] = (i <= nz / 2) ? 2 * pi * static_cast<double>(i) / Lz
                                      : 2 * pi * static_cast<double>(i - nz) / Lz;
            }
        }
    }

    // radius = radius0 + Ur*t, and Ur is 0 whenever the expanding box is off,
    // so this is the identity on every deck in checks/.  It is here because
    // the deck can turn the box on and the rest of the solver reads it.
    void evolve_radius(double t) {
        radius = radius0 + Ur * t;
        tau_exp = radius / Ur;
    }

    double kscale() const { return radius0 / radius; }

    void validate() const {
        if (nx <= 0 || ny <= 0 || nz <= 0)
            throw std::runtime_error("deck: nx, ny and nz must be positive");
        if (!(Lx > 0) || !(Ly > 0) || (!two_d() && !(Lz > 0)))
            throw std::runtime_error("deck: Lx, Ly (and Lz in 3D) must be positive");
        if (!(tmax > 0)) throw std::runtime_error("deck: tmax must be positive");
        if (!(dtout > 0)) throw std::runtime_error("deck: dtout must be positive");
        // What this port does NOT implement, refused loudly rather than
        // ignored.  Every deck in checks/ is ideal MHD in a static box with no
        // Hall term and no restart, and so is the neighbourhood a deck patch
        // moves through - grid, box, cfl, tmax, dtout, gamma, amplitude,
        // dealiasing rule.  Outside it, a wrong answer would be worse than a
        // refusal, and a refusal is what these are.  See
        // skill/references/diary.md for what each would take.
        if (if_restart)
            throw std::runtime_error(
                "deck sets if_restart: this port has no restart reader");
        if (if_AEB)
            throw std::runtime_error(
                "deck sets if_AEB: this port does not implement the expanding "
                "box model (the expand_term source and the time-dependent "
                "wavenumber scaling of AEBmod.f90)");
        if (if_Hall)
            throw std::runtime_error(
                "deck sets if_Hall: this port does not implement the Hall "
                "term (calc_current_density_real and the Hall electric field)");
        if ((if_resis && if_resis_exp) || (if_visc && if_visc_exp))
            throw std::runtime_error(
                "deck asks for EXPLICIT resistivity or viscosity: this port "
                "implements only the implicit form rktmod.f90 uses by default");
    }
};

}  // namespace laps
