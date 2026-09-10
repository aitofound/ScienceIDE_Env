# Blocker 3 investigation and calibration plan

## Local evidence result

The narrow authorized roots were limited to `tasks/epoch/epoch-physics-packages/comment/pipeline/{runtime-metadata,self-validation}.json`, the task leaf, and the revision evidence root. The named metadata records nominal/variant/altbuild oracle paths on the worker and validator results, but no preserved local `series.txt` or SDF time records. The revision root contains only synthetic contract fixtures, not science outputs. No local grid is therefore accepted as provenance tied to the exact applicable leaf/input/build. The current rubric grids remain explicit known-NO_GO placeholders; no tolerance or time value was widened or fabricated.

## Minimum post-integration extraction-only probes

After current-main integration, before the final acceptance selfcheck, run the existing task leaf once for each of `SAB_IC=nominal`, `SAB_IC=variant`, and `SAB_IC=altbuild` for all eight checks, using the exact applicable deck, source revision, build macros, rank layout, and input knobs recorded by each leaf. These are extraction-only probes: retain the emitted `.sdf` dumps and extractor `series.txt`, do not use them as the final acceptance selfcheck, and do not change science inputs. The existing N/V/A solve paths and their output roots are the only intended sources; no remote/Docker/science run is part of this revision.

For every leaf and each N/V/A probe:

1. Preserve the extractor's complete `series.txt` byte-for-byte, including every timestamp rendered with `%.17e`; record SHA-256, source/build identity, input/deck identity, dump filenames, and row count in the additive calibration record.
2. Parse only the timestamp column and require a nonempty, finite, strictly increasing complete sequence. Confirm the final dump is present and record the observed final time; do not substitute the requested `t_end`.
3. Compare nominal, variant, and altbuild arrays by length and pair alignment. Accept a grid only if all three arrays cover all eight N/V/A records for that leaf and pairwise timestamp differences are within an explicitly measured serialization/scheduling bound. If any leaf has unequal count or unpaired final dump, stop calibration for that leaf rather than inventing an array.
4. Once aligned, use the nominal observed sequence as the declared physical grid, emit its exact `%.17e` values into `time_grid.files.series.txt.values_s`, and set tolerance to the measured maximum N/V/A pair difference plus one binary64 serialization ULP (or exact zero when the records are byte-identical). This is a measured bound, not a widened few-ULP guess.
5. Derive every early/mid/tail declaration from the complete canonical sequence using explicit inclusive `start_index`, `end_index`, and `count`. Preserve the pre-existing physical window intent only after checking the actual timestamps; otherwise document a leaf-specific physical-window decision from the records. Validator inputs remain only canonical `series.txt`; convenience files are never graded.
6. Run the repaired validator against the extracted N/V/A records and dedicated malformed/canonical-series fixtures. Only after these extraction-only records and provenance checks pass should the parent consume one fresh final N/V/A acceptance selfcheck. The acceptance selfcheck must not be used to discover or calibrate the grid.
