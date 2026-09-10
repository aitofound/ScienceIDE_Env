# Particle identity fixture

`particle_identity.py` guards any future raw FLEKS particle plot block. The
positive fixture in `tests/particle_identity_smoke.py` permutes rows `(cpu,id)`
and must still compare equal because the whole row follows its identity.
Duplicate identity, missing identity, and non-finite value fixtures are
rejected. The active `pt_tracker.log` is an aggregate moments table with no
per-particle identity field, so it is intentionally not fed through this
unordered-row comparator; its declared time/species columns remain strictly
schema-checked by `validate.py`.
