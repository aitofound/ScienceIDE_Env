#!/usr/bin/env python3
"""Validation for the HYBRID_WHISTLER test.

This directory now runs TWO variants of the same whistler eigenmode, driven by
the test runner (tests/validate_tests.py):

  * FULL PIC  (PARAM.in,           base_name = whistler)
      kinetic ions AND kinetic electrons, standard Maxwell/GMRES solver
      (solveEM = T).
  * HYBRID    (PARAM.in.hybrid,     base_name = whistler_hybrid)
      kinetic ions + fluid electrons, generalized Ohm's-law solver
      (useHybridPIC = T, Hall-only).

Both variants seed the identical x-aligned, transverse, circularly-polarized
whistler wave (#TESTCASE HybridWave, frac = 0.02) so the field diagnostics are
directly comparable.  The shared, solver-agnostic checks in tests/_shared/hybrid.py
are used for both:

  * validate_log  -> energy-log checks (finite energies, bounded magnetic
    energy, conserved particle number under periodic BCs).
  * validate_plot -> seeded-wavelength (n=1) check + whistler-dispersion check
    measuring the transverse-wave frequency and comparing it against
    omega/Omega_i = (k d_i)^2 / (1 + (k d_i)^2) from the .out plot frames.

Because the dispersion check lives in tests/_shared/hybrid.py and reads only the
By/Bz components, it applies unchanged to the full-PIC solver output.
"""

import sys
import os

# Make the shared test helpers importable when run directly.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from _shared import hybrid as hybrid_helpers


def set_run_dir(run_dir):
    """Mirror the runner's RUN_DIR into the shared hybrid helper."""
    hybrid_helpers.set_run_dir(run_dir)


def validate_log(pic_diags, test_name=None):
    """Energy / stability checks from the shared hybrid-family validator.

    Works for both the full-PIC and hybrid variants: the full-PIC run
    additionally carries an electron species (reported as Epart1 / Ee), which
    the shared log reader and validator handle.
    """
    return hybrid_helpers.validate_hybrid(pic_diags=pic_diags, test_name=test_name)


def validate_plot(test_name):
    """Seeded-wavelength + whistler-dispersion checks (shared, solver-agnostic).

    The dispersion relation is read from tests/whistler/PARAM.in (the
    full-PIC variant), which both variants share for geometry / normalization.
    For the hybrid variant that is the exact Hall branch it must reproduce; for
    the full-PIC variant the dominant whistler branch matches this frequency at
    k d_i ~ 1, so the same check is a meaningful regression guard.
    """
    return hybrid_helpers.validate_plot(test_name)


if __name__ == "__main__":
    # Allow a quick standalone sanity check (reads run_test/ output).
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    from tests.validate_tests import read_pic_log, RUN_DIR
    diags = read_pic_log(RUN_DIR)
    ok, reason = validate_log(diags, test_name="whistler")
    print(("PASSED" if ok else "FAILED"), "-", reason)
    sys.exit(0 if ok else 1)
