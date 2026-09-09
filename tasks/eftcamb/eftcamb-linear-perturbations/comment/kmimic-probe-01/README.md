# K-mimic targeted diagnostic

This is a 16-execution, two-deck diagnostic in the existing arm64 oracle image.
The unchanged scientific code was compiled once; each of four configurations ran
Kmimic 1 and 2 with both nominal and two-ULP-variant baryon densities.
All selected numerical bounds, physical parameters and configured L/k controls stayed fixed.
Only scratch copies changed their threading, accuracy_boost, or background sample count.
No setting has been adopted into the benchmark; this is not a fresh CLI selfcheck.

Build: 167.862 s. Total model execution: 294.030 s.

| Configuration | Values | Failing | Max allowance fraction | Combined margin | Bulk margin | Near-zero margin | Model run seconds |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| baseline-8-threads | 427,035 | 3,967 | 2.39405 | 0.417702 | 0.011253 | 88.6997 | 9.50451 |
| baseline-1-thread | 427,035 | 3,967 | 2.39405 | 0.417702 | 0.011253 | 88.6997 | 76.5577 |
| accuracy-boost-2 | 442,697 | 2,710 | 1.35472 | 0.73816 | 0.0102301 | 82.8089 | 196.773 |
| background-2000-points | 427,035 | 5 | 2.16734 | 0.461394 | 0.00504092 | 1545.6 | 11.1947 |

The numerical rule is `abs(candidate-reference) <= atol[file] + 1e-4*abs(reference)`.
Combined margin corresponds to this rule; bulk/near-zero component margins are diagnostics.
The skill flags margins below 50 or above 10,000 for review; these are not extra pass rules.

The eight-thread baseline reproduced 36/36 graded files byte-for-byte from the previous full calibration.
The one- versus eight-thread comparison has 36/36 byte-identical graded files.
Nominal/variant printed coordinate rows differing: 0.

## Per-deck results

| Configuration | Deck | Values | Failing | Combined margin | Bulk margin | Near-zero margin |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| baseline-8-threads | 5_Kmimic_1 | 213,524 | 604 | 0.556105 | 0.0172424 | 132.188 |
| baseline-8-threads | 5_Kmimic_2 | 213,511 | 3,363 | 0.417702 | 0.011253 | 88.6997 |
| baseline-1-thread | 5_Kmimic_1 | 213,524 | 604 | 0.556105 | 0.0172424 | 132.188 |
| baseline-1-thread | 5_Kmimic_2 | 213,511 | 3,363 | 0.417702 | 0.011253 | 88.6997 |
| accuracy-boost-2 | 5_Kmimic_1 | 221,355 | 2,710 | 0.73816 | 0.0102301 | 82.8089 |
| accuracy-boost-2 | 5_Kmimic_2 | 221,342 | 0 | 1.1305 | 0.0629563 | 388.802 |
| background-2000-points | 5_Kmimic_1 | 213,524 | 2 | 0.922935 | 0.660595 | 2114.16 |
| background-2000-points | 5_Kmimic_2 | 213,511 | 3 | 0.461394 | 0.00504092 | 1545.6 |

The accompanying CSVs contain every file, both diagnostic maxima, all margins,
failure counts, and the original coordinates of each file's worst comparison.
A smaller nominal/variant spread alone is not proof of convergence or root cause.
Any proposed benchmark-setting change remains a human decision at STOP 4.

## Cross-setting checks: fewer failures are not enough

The 2000-background-point configuration is NOT an accepted fix. At the identical
printed coordinate k/h=8.24543, its nominal matter power changes by orders of
magnitude relative to both the baseline and accuracy-boost-2 configurations.
Its raw transfer_out also contains a large spike near this coordinate; the
effect is not confined to interpolation of the final matterpower table.
The correct converged value and the root cause have not been established.

| Configuration | Deck | Matter rows | Maximum output k/h | P at k/h=8.24543 |
| --- | --- | ---: | ---: | ---: |
| baseline-8-threads | 5_Kmimic_1 | 654 | 46.977 | 0.273639 |
| baseline-8-threads | 5_Kmimic_2 | 654 | 46.977 | 0.278892 |
| baseline-1-thread | 5_Kmimic_1 | 654 | 46.977 | 0.273639 |
| baseline-1-thread | 5_Kmimic_2 | 654 | 46.977 | 0.278892 |
| accuracy-boost-2 | 5_Kmimic_1 | 689 | 94.6001 | 0.27366 |
| accuracy-boost-2 | 5_Kmimic_2 | 689 | 94.6001 | 0.278937 |
| background-2000-points | 5_Kmimic_1 | 654 | 46.977 | 648057.0 |
| background-2000-points | 5_Kmimic_2 | 654 | 46.977 | 151488.0 |

Although the configured transfer_kmax remained 2, accuracy_boost=2 changes the
internal sampling and increases the emitted matterpower range from k/h=46.977
to 94.6001. It therefore compares more values (442,697 versus 427,035).
The pinned results.f90:4026–4037 writes the table up to the last actual transfer
sample, not a fixed number of rows derived only from the transfer_kmax input.
The paired nominal/variant grids still match within each configuration.

No candidate setting has passed this diagnostic across both decks. No tolerance,
variant, output window, file, or model has been removed or changed in the task.
