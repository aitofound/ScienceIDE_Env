# normalize-img-3d

Upstream test: `code/cellpose/tests/test_transforms.py`. Policy: `pointwise`.

## The test

The upstream fixture 3D/rgb_3D.tif (75x2x75x75 uint16), reshaped exactly as the upstream img_3d fixture does — transpose(0, 2, 3, 1).astype('float32'), giving 75x75x75x2 — then passed through transforms.normalize_img in the three configurations tests/test_transforms.py::test_normalize_img uses: norm3D=True; norm3D=True with tile_norm_blocksize=25; and norm3D=False with sharpen_radius=8. All 75 z planes are graded (SAB_PLANES=75, the graded default).

Runtime and resources: under 300 s: 1.5 s measured for the graded run on 1 core, on the 1 core(s) this check declares.

Knobs, with their graded defaults:

- `SAB_PLANES` (default `75`): z planes normalised; runtime scales linearly; the graded default is the full stack
- `SAB_CPUS` (default `1`): threads for numpy/numba; fixed graded default, never read from the host, because a thread count can change the summation order


## The two initial conditions

`ic/nominal` is upstream fixture 3D/rgb_3D.tif exactly as shipped in the OSF archive ((75, 2, 75, 75) uint16)

`ic/variant` is One voxel of the input tif, at index (37, 1, 37, 37), changed from 12752 to 12753 — one least-significant bit of the uint16 input. That is the smallest perturbation the initial condition can physically store, since a sub-LSB change cannot be written to the file. It exercises the policy: the measured response is 2.83e-05 absolute, and the sharpen_radius=8 configuration spreads that single-voxel change across 3703 output voxels through its convolution, so the variant probes the smoothing path and not merely one array entry.

`run.sh altbuild` is not available: cellpose is pure Python with no compiled sources anywhere in the tree, so there is no legitimately different build of this same source — no IEEE mode, no -O0, no second compiler — from which a floor could be measured. The floor below is measured the only way available here, from two runs of ic/nominal on the same build.

## The pass policy

Every voxel and channel of the three normalised fields compared elementwise as float32 under an absolute bound of 1e-3, the meaningful scale for a field normalised to order 1; nothing excluded.

The bound is 1e-3 on a field normalised to order 1, so it admits a relative error of about a tenth of a percent. It is physical because the faults it must catch are all far larger than that: normalize_img's whole job is to map the 1st and 99th intensity percentiles onto 0 and 1 (cellpose/transforms.py, normalize_img, percentile=(1., 99.) default), so a port that shifts a percentile bound, computes the percentile over the wrong axis, normalises per-plane where norm3D=True asks for the whole stack, or silently drops the sharpening convolution moves the affected voxels by order 0.1 to 1 — two to three orders of magnitude past the bound. It is achievable because the floor is 0.0: two runs of the same build agree bit for bit, so the bound is not absorbing numerical noise at all. The headroom over real sensitivity is the measured variant spread of 2.83e-05, which is 2.8 percent of the bound, leaving a factor of 35 between the smallest physically meaningful input change and the bound, and a further factor of roughly 100 to 1000 before a genuine implementation fault would fit underneath it.

## Evidence

- **Floor: 0.** Two runs of `run.sh nominal` on the same build, outputs compared file by file: max |a-b| = 0.0 exactly across every graded file. Cellpose's inference and array transforms are bit-reproducible on a fixed thread count, which was also confirmed independently in Step 1.2 (identical sha256 over masks and cellprob across repeated runs).
- **Variant spread: 2.825e-05.** Measured by running `run.sh nominal` and `run.sh variant`
  into separate output directories and taking the largest absolute difference over every
  graded file.
- **Bound: 0.001 absolute, 0 relative.** The measured spread is 2.8 percent of the
  bound, leaving a factor of about 35 of headroom.
- Calibration and final self-validation figures are recorded by `sab.py task selfcheck`
  in `comment/pipeline/self-validation.json`; the numbers above are the native
  pre-image measurements that set the proposed bound.
