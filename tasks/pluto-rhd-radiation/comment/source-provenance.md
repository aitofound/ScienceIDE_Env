# PLUTO source provenance (staging record)

This Harbor leaf vendors the complete locally available PLUTO 4.4-patch4 archive
under `code/pluto/`. No network fetch or external checkout is used by the leaf.

- Upstream project: <https://plutocode.ph.unito.it/>
- Archive name: `pluto-4.4-patch4.tar.gz`
- Archive URL (provenance only; never fetched at runtime):
  `https://plutocode.ph.unito.it/Downloads/pluto-4.4-patch4.tar.gz`
- Archive SHA-256:
  `1ba5527b76d49fdd78ae24dbfbdad085ec83393748f1e618516a9d63bd945787`
- License: GPL-2.0, from the vendored `COPYING`
- Local verified source used: pre-existing `/Users/huangzesen/work/projects/very_long_alfven_wave/.lingtai/codex/pluto-4.4-patch4.tar.gz`
- Extracted source root: archive member prefix `PLUTO/`, with archive `._*`
  metadata excluded as non-source metadata.
- Offline archive copy: `code/pluto/.source/pluto-4.4-patch4.tar.gz`
- Verification record: `code/pluto/.source/archive.sha256`

The preflight supplied to this implementation run independently verified the
archive SHA-256 before extraction. `solution/solve.sh` repeats that hash check
against the vendored archive and fails closed when it is absent or mismatched.
The source tree is not edited in place by the solution or verifier.

The owned physics boundary is `Src/RHD/`, `Src/EOS/Taub/`, and
`Src/Radiation/`; shared PLUTO files are included as transitive dependencies.
`Src/RMHD/` and `Src/RMHD/Radiation/` remain separately owned. Official inputs
for rows 1--8 remain in their original `Test_Problems/RHD/*` directories and
are selected only in copied build workspaces.
