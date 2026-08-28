# SR-MHD leaf preparation evidence

This runtime-hidden directory records package evidence only; `instruction.md`
and the verifier are authoritative.

- Source: complete PrincetonUniversity/athena tree at
  `823614c90b594472747a0ac2a699e4a454f300d2`, copied under exactly one direct
  `code/athena` and recorded in `source-manifest.json`.
- Build configurations: serial double-precision Cartesian `-s -b` builds for
  `gr_linear_wave`/HLLD and `gr_shock_tube`/HLLD, HLLE, and LLF.
- Real coverage: all seven linear-wave flags, all four audited `mub_*` decks,
  all three relativistic solver variants, and the 128x64x64/32^3-block direct
  repeated-update workload.
- Artifact: complete primitive + Bcc TAB-derived state and finite recovery,
  admissibility, Lorentz, magnetic, FaceField-shape, corner-EMF, and discrete
  divB records. FaceField/EdgeField records explicitly state that formatted_table
  does not serialize internal arrays; they are derived projections, not claims
  of direct internal dumps. A few real pinned shock states are outside the
  sub-luminal recovery domain; they are retained unmodified with an explicit
  non-admissible status/count, never clipped or silently accepted.
- Self-validation: the trusted CPU source is run for reference and candidate
  paths separately with the same executable as explicit provisional identity
  wiring. This is not a finalized accelerator equivalence policy. Jason owns
  final tolerances, noise/repeatability treatment, and fixture decisions.
- Frozen HLLD shock fixtures omit case 3 and do not establish HLLE/LLF
  associations; no fabricated final fixture policy is used.
- No GPU, accelerator, speedup, or performance result is claimed.
