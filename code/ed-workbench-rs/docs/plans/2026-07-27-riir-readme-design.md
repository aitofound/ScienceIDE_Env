# RIIR README Redesign

## Goal

Turn the repository README into an unmistakably RIIR-style project front
page: energetic enough to communicate the achievement immediately, while
remaining precise enough to serve as the entry point for a reproducible
electronic-structure benchmark.

## Voice

Use a scientific-hardcore manifesto voice. The opening should sound like a
challenge was accepted and completed, not like a generic library
description. Short declarative sentences, selective exclamation marks, and
the recurring `RIIR` identity provide momentum. Numerical claims remain
literal and qualified.

## Information Architecture

1. Lead with `Rewrite It In Rust!` and a one-line statement of what was
   rewritten.
2. Put the four strongest results above the fold:
   245,025 determinants, CC(1)-CC(8), all 28 CI/MBPT paper anchors, and the
   Python-free libcint-to-FCI path.
3. Add a `This Is Not a Wrapper` section that names the algorithms executed
   in Rust and distinguishes the optional PySCF oracle.
4. Reframe Levels 0-4 as `The Climb`, with each level representing a concrete
   increase in capability.
5. Keep exact commands, energy values, units, limitations, repository links,
   and report links intact.
6. End with the team identity and challenge links so the RIIR theme carries
   through the whole page.

## Scientific Guardrails

- Do not invent performance measurements or speedup claims.
- Do not describe the optional fixture generator as part of the production
  runtime.
- Do not claim that primary 6-31G values reproduce the Kállay DZ/DZP targets.
- Preserve all printed energies, errors, residuals, tolerances, geometries,
  checksums, commands, and paper-comparison counts.
- Keep `hartree`/`E_h`, Angstrom/Bohr, angles, and dimensionless quantities
  explicitly identified.

## Validation

- Check every pre-existing numerical token and runnable command against the
  previous README.
- Verify all relative Markdown links resolve to tracked files.
- Run the repository submission verification script to catch formatting,
  JSON, checksum, Rust, and Python regressions even though the change is
  documentation-only.

