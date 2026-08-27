// SPDX-License-Identifier: GPL-2.0
// The time loop, exactly as mhd.f90 runs it, with the arithmetic left to a
// backend.
//
// The loop is here rather than in each backend because the frame schedule and
// the step sequence are graded and they are subtle: tmax is not generally a
// frame boundary, dtout does not bind when dt exceeds it, the CFL timestep is
// recomputed every step in the 3D tree and every twentieth step in the 2D one,
// and the timestep only MOVES when it has drifted by more than two percent.
// One copy of that means one thing to get right.
//
// A backend supplies:
//     void   step(const RK3&)      one whole RK3 step: three stages of
//                                  flux -> transform -> rhs -> update ->
//                                  dealias -> inverse transform -> primitives
//     double cfl_dt()              min over the grid of dx/cmax, per mhd.f90
//                                  vardt, BEFORE the cfl factor
//     void   reset_rk_register()   fnl_rk <- 0, as rkt_init does
//     void   snapshot(iout, time)  write out%03d.dat from the current state
//     bool   state_is_finite()     the 2D tree's periodic NaN check
#pragma once

#include <cstdio>
#include <string>

#include "laps_core.hpp"
#include "laps_io.hpp"
#include "laps_params.hpp"

namespace laps {

template <class Backend>
class TimeLoop {
  public:
    TimeLoop(Params& p, Backend& b, OutputWriter& w) : p_(p), b_(b), w_(w) {}

    // Returns the number of steps taken.
    long long run() {
        // mhd.f90: dt = 0.0 then vardt, before the first output.  The initial
        // dt is therefore computed from the initial condition as handed in,
        // not from a dealiased or round-tripped copy of it.
        dt_ = 0.0;
        vardt();

        double time = 0.0;          // t_restart; this port has no restart path
        long iout = p_.n_start;
        double tout = time + p_.dtout;

        w_.open_times();
        // The sidecar opens before the main loop and writes the step-zero line
        // there, so its first row is (0, t_restart, the initial dt).
        w_.times_line(istep_, time, dt_);

        b_.snapshot(iout, time);
        ++iout;

        const long calcdt_every = p_.dstep_calcdt();
        const long long checknan_every = 200;   // 2D tree only

        for (;;) {
            if (time >= tout) {
                b_.snapshot(iout, time);
                ++iout;
                tout += p_.dtout;
            }

            if (dt_ < 0.00000001) {
                // Upstream prints a banner, writes one last frame and stops.
                std::fprintf(stderr,
                             "laps: dt < 1e-8 at t = %.6f, istep = %lld - "
                             "stopping, as the incumbent does\n", time, istep_);
                b_.snapshot(iout, time);
                break;
            }

            b_.step(rk_);

            time += dt_;
            ++istep_;
            w_.times_line(istep_, time, dt_);
            p_.evolve_radius(time);

            if (time >= p_.tmax) {
                b_.snapshot(iout, time);
                break;
            }

            if (calcdt_every == 1) {
                vardt();
            } else if (istep_ > 0 && istep_ % calcdt_every == 0) {
                vardt();
            }

            if (p_.two_d() && istep_ > 0 && istep_ % checknan_every == 0) {
                if (!b_.state_is_finite()) {
                    std::fprintf(stderr,
                                 "laps: NaN encountered at t = %.6f - stopping, "
                                 "as the incumbent does\n", time);
                    break;
                }
            }
        }

        w_.close_times();
        return istep_;
    }

  private:
    // mhd.f90 vardt.  The reduction is a minimum, so it is exact whatever
    // order a backend walks the grid in; what matters is the hysteresis below,
    // which is the only place in the scheme where a float comparison picks a
    // branch.  Upstream holds dt fixed until it has drifted more than 2%,
    // which is why these decks run at a constant dt for the whole window.
    void vardt() {
        double dtmin = b_.cfl_dt() * p_.cfl;
        if (p_.if_limit_dt_increase) {
            if (dt_ == 0.0 || dt_ > 1.02 * dtmin) dt_ = dtmin;
        } else {
            if (dt_ < 0.98 * dtmin || dt_ > 1.02 * dtmin) dt_ = dtmin;
        }
        rk_.init(dt_);
        b_.reset_rk_register();
    }

    Params& p_;
    Backend& b_;
    OutputWriter& w_;
    RK3 rk_{};
    double dt_ = 0.0;
    long long istep_ = 0;
};

}  // namespace laps
