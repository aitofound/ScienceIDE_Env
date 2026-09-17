# ScienceAccelBench vendoring note for UA/GITM

This tree is vendored from https://github.com/GITMCode/GITM at commit
`c4fc3150aca274e0e0496ba415696a57d0703e00` (2026-09-04), Apache-2.0.

## srcData subset

Upstream `srcData/` is ~190 MB. Only the parts read by the shipped test
decks are vendored here (~93 MB):

- `srcData/Examples/` (23 MB) — example run decks.
- `srcData/FISM/` (9.7 MB) — solar EUV flux model input used by the default
  Earth configuration.
- `srcData/f107.txt` (856 KB) — F10.7 solar flux time series, read directly
  by SWMF's `Param/PARAM.in.test.GMIEIMUA` (test3) as `UA/DataIn/f107.txt`.
- `srcData/Mars/` (49 MB) — needed by the Mars deck
  (`make test_mars_rundir` in GITM's own `Makefile`, `UAM.in.Mars`).
- `srcData/Rcmr/` (11 MB) — needed by SWMF's test3
  (`Param/PARAM.in.test.GMIEIMUA` reads `UA/DataIn/power.test.rcmr_quick`,
  which GITM's `make rundir` symlinks from `srcData/Rcmr/`). Confirmed by
  reading the deck, not assumed from the directory name.
- `srcData/Earth/` (5.1 MB) — added in a follow-up commit after native
  verification found it was actually required: `Config.pl -earth` (the
  default planet, selected when SWMF's own `Config.pl -v=...,UA/GITM`
  runs) invokes `set_planet`, which does
  `cd srcData; cp Earth/UAM.in.Earth UAM.in`. Without this directory the
  install dies right after creating the `ModPlanet.f90`/`planet.f90`/
  `ModChemistry.f90` symlinks. Not obvious from grepping the SWMF Param
  decks; found by actually running the install.

Left out (no shipped deck reads them):

- `srcData/Aurora/` (28 MB)
- `srcData/Gswm/` (53 MB)
- `srcData/Hme/` (1.3 MB)
- `srcData/HIME/` (1.7 MB)
- `srcData/Purgatory/` (5.3 MB)
- `srcData/Eclipses/` (208 KB), `srcData/LowerBCs/` (728 KB),
  `srcData/Titan/` (60 KB), `srcData/Venus/` (28 KB)

To restore any of these, clone the full repo at the pinned commit above and
copy the missing `srcData/<name>` directory into place; nothing else needs
to change.

## GITM_data

`SWMFsoftware/GITM_data` (a separate 48 MB repo) is not vendored: nothing
in SWMF's `Config.pl`, `Makefile`, `Makefile.test`, `share/Scripts/gitclone`,
or `Param/PARAM.in.test.GMIEIMUA` references it (checked by grep over the
whole `code/swmf` tree). If a future deck needs it, vendor it under
`UA/GITM/GITM_data` and update this note.

## ext/Electrodynamics

`UA/GITM/ext/Electrodynamics` is vendored from
https://github.com/GITMCode/Electrodynamics at commit
`756d8cb806caa6e17f934c60937d6dc0a4663e89`, Apache-2.0. Standalone GITM's
`Config.pl -install` clones this automatically; inside SWMF (`IsCompGitm`
set) the clone step is skipped, so it must be vendored explicitly here.
