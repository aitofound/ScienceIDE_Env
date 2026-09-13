# awsom-signb

Upstream test: the Makefile.test target `test_awsom_signb`, whose PARAM file is `code/swmf/GM/BATSRUS/Param/CORONA/PARAM.in.Awsom.signb`. Policy: `pointwise`.

## The test

Config.pl -default -u=Awsom -e=AwsomSA -ng=2 -g=6,4,4; Param/CORONA/PARAM.in.Awsom.signb as PARAM.in; mpiexec -n 2; PostProc.pl -M -f=ascii. 24 steady-state iterations of the AWSoM corona in the AwsomSA (stream-aligned) equation set with the signed-B representation, then a time-accurate session to t = 25 s. Graded: log.log, x0_var.outs, z0_var.outs.

The window was shortened on 2026-09-13 under the 60 s ruling: SAB_MAX_ITERATION
50 -> 24 and SAB_TIME_SCALE 1.0 -> 0.25 (t = 25 s instead of 100 s), measured 25
s of run time. The graded x=0/z=0 VAR idl series writes 5 snapshots over the
shortened window; `run.sh` rewrites its cadence from SAB_MAX_ITERATION/
SAB_TIME_SCALE and the new SAB_PLOT_FRAMES knob (default 8) and prints
`SAB_PLOT_FRAMES=<count>` after the run. All three knobs are tunable in
`run.sh` for later retuning.

`run.sh --help` lists the runtime knobs; every default is the graded value. The check builds the pinned source itself, in a scratch copy, so the
build is part of the check and never touches the source tree; `run.sh` prints
`SAB_BUILD_SECONDS` after the build and the suite budget counts run time only.

## The two initial conditions

`ic/nominal/` holds the inputs above, unchanged from the pinned tree.

ic/variant/PARAM.in is ic/nominal/PARAM.in with the single value 1e6 of #POYNTINGFLUX PoyntingFluxPerBSi written as 1.0000000000000002e6: two ulps of binary64, a relative change of 2.2e-16. PoyntingFluxPerBSi is the Poynting flux per unit magnetic field injected at the inner boundary, the one number that sets the Alfven-wave energy the whole AWSoM solution is driven by (src/ModTurbulence.f90 PoyntingFluxPerB), so the perturbation enters the very first source-term evaluation of every cell; it is far below any precision at which the parameter is known and far below the eleven digits the graded files carry, so the two runs differ by rounding alone. The two PARAM.in files differ byte-wise and the graded files differ. `run.sh altbuild` runs ic/nominal on the same configuration built with `./Config.pl -O0` added before `make`, instead of a second initial condition.

## The pass policy

The graded observable is the RAW volume-average log over every step and the x=0 and z=0 plot slices of the stream-aligned corona, compared value by value under |candidate - reference| <= 1e-06 * s + 1e-06 * |reference| with s the largest reference magnitude in that column, and under 1e-4 * s + 1e-4 * |reference| for the log group. BATSRUS writes tables whose columns span twenty decades side by side -- one RAW log line holds a volume-averaged density near 1e-19, magnetic-field components near 1e-5 and a peak pressure near 1e-1, and one IDL plot row holds g/cm3 next to km/s next to K next to G -- so a single absolute floor is meaningless for most of the table and a purely relative bound is unusable on the velocity and field components that pass through zero inside the domain. Scaling the absolute term by each column's own peak is the smallest scheme that is well posed for both. The components of one vector share the largest of the three, because in a one- or two-dimensional configuration the transverse components are zero by symmetry and are carried at the cancellation level, ten decades below the component that is physical; measuring them against their own peak would compare pure round-off and no correct port could pass. The bound is physical: getting the sign convention of the reversed field wrong in ModReverseField, or applying the stream-aligned rotation of ModStreamAligned to the wrong component set crosses it by a wide margin, because the signed-B representation stores the field relative to its own sign, so a wrong sign flips By and Bz in whole blocks and moves the graded field columns by order one. It is achievable: the pinned build is bit-reproducible: two runs of the same executable in the same directory gave byte-identical log and plot files (rerun of Param/CORONA/PARAM.in.AwsomR, 2026-09-04), so the whole spread between two legitimate runs comes from the different order of floating-point operations a port introduces. The state is binary64 end to end (share/build/Makefile.Linux.gfortran compiles with -fdefault-real-8, so State_VGB in src/ModAdvance.f90 is a real(8) array) and PostIDL writes the ASCII plot files with eleven significant digits, while the RAW log line is printed with six (src/ModWriteLogSatFile.f90), which is why the log group carries a looser pair. The calibration selfcheck of 2026-09-04 measured a nominal-versus-variant spread of 4.85e-11 in these units, so the bound sits a factor of 20612 above the measured floor while staying at least a decade, and for most of these faults four or more decades, below the smallest fault above. evidence.floor_how says how the floor was measured and what it does and does not represent.

The graded files are `log.log`, `x0_var.outs`, `z0_var.outs`. Nothing in
them carries a run date, a host name or a timing line: the log files hold one row per saved step and the IDL files hold the plotted state, both written by the pinned source.
Structure is a gate: a differing number of snapshots, grid size, column list,
step number, snapshot time or scalar header parameter fails the check before any
tolerance is applied, and so does a non-finite value.

## Evidence

nominal versus variant, sab.py task selfcheck run 20260904T113532Z on the consented host ale-worker (x86_64, 88 cores, Docker 29.1.3, 8 declared cpus), calibration run started 2026-09-04T11:35:32Z: the largest |candidate - reference| / s over every graded value of every graded file was 4.85e-11, with s the column scale that comparison.rule defines. The nominal solve reproduces itself byte for byte on rerun (verified natively on Param/CORONA/PARAM.in.AwsomR, 2026-09-04), so this is the whole distance a two-ulp (2.2e-16 relative) perturbation of the initial condition travels over the graded window. Measured run time 141 s on the declared 8 cpus; the whole suite measured 1121 s of run time and 838 s of build time.
