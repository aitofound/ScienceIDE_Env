# awsom-bvector

Upstream test: the Makefile.test target `test_awsom_bvector`, whose PARAM file is `code/swmf/GM/BATSRUS/Param/CORONA/PARAM.in.awsom.bvector`. Policy: `pointwise`.

## The test

Config.pl -default -u=Awsom -e=Awsom -ng=2 -g=8,8,8; Param/CORONA/PARAM.in.awsom.bvector as PARAM.in; mpiexec -n 2; PostProc.pl -M -f=ascii. 20 steady-state iterations in which B0 is the CR2157 HMI potential field plus the current-carrying B0local lookup table of a magnetofriction solution, so #CURLB0 turns on the curl B0 momentum flux and the force-free correction. Graded: log.log (dt, the volume-integrated |J x B|, the magnetic energy of B1 and the kinetic energy), y0_var.outs and z0_var.outs.

Under the 2026-09-13 60 s/5-frame ruling: the 20-iteration window itself was
not shortened (it was already small), but the graded y=0/z=0 VAR idl plot
cadence was too coarse to satisfy the 5-frame floor -- DnSavePlot=1000 exceeds
the 20-iteration window, so the check previously wrote only 1 frame (the
forced final save). `run.sh` now rewrites that cadence from the new
SAB_PLOT_FRAMES knob (default 5, DnSavePlot -> 4), which writes 5 snapshots.
This raises run time from 44 s to about 72-77 s (measured), because each
snapshot of this check's grid is I/O-heavy; **this check could not be brought
under the 60 s cap while also meeting the >= 5 frame floor** -- the 5-frame
floor was kept and the cap was not, per the shortest-window-that-separates-them
guidance. SAB_MAX_ITERATION and SAB_PLOT_FRAMES are both tunable in `run.sh`
for later retuning.

`run.sh --help` lists the runtime knobs; every default is the graded value. The check builds the pinned source itself, in a scratch copy, so the
build is part of the check and never touches the source tree; `run.sh` prints
`SAB_BUILD_SECONDS` after the build and the suite budget counts run time only.

## The two initial conditions

`ic/nominal/` holds the inputs above, unchanged from the pinned tree.

ic/variant/PARAM.in is ic/nominal/PARAM.in with the single value 0.3e6 of #POYNTINGFLUX PoyntingFluxPerBSi written as 0.30000000000000004e6: two ulps of binary64, a relative change of 2.2e-16. PoyntingFluxPerBSi is the Poynting flux per unit magnetic field injected at the inner boundary, the one number that sets the Alfven-wave energy the whole AWSoM solution is driven by (src/ModTurbulence.f90 PoyntingFluxPerB), so the perturbation enters the very first source-term evaluation of every cell; it is far below any precision at which the parameter is known and far below the eleven digits the graded files carry, so the two runs differ by rounding alone. The two PARAM.in files differ byte-wise and the graded files differ. `run.sh altbuild` runs ic/nominal on the same configuration built with `./Config.pl -O0` added before `make`, instead of a second initial condition.

## The pass policy

The graded observable is the VAR log (dt, absjxb, emag1, ekin) and the y=0 and z=0 plot slices of AWSoM on a non-potential B0, compared value by value under |candidate - reference| <= 4e-5 * s + 4e-5 * |reference| with s the largest reference magnitude in that column, and under 1e-4 * s + 1e-4 * |reference| for the log group. BATSRUS writes tables whose columns span twenty decades side by side -- one RAW log line holds a volume-averaged density near 1e-19, magnetic-field components near 1e-5 and a peak pressure near 1e-1, and one IDL plot row holds g/cm3 next to km/s next to K next to G -- so a single absolute floor is meaningless for most of the table and a purely relative bound is unusable on the velocity and field components that pass through zero inside the domain. Scaling the absolute term by each column's own peak is the smallest scheme that is well posed for both. The components of one vector share the largest of the three, because in a one- or two-dimensional configuration the transverse components are zero by symmetry and are carried at the cancellation level, ten decades below the component that is physical; measuring them against their own peak would compare pure round-off and no correct port could pass. The bound is physical: dropping the curl B0 momentum flux of src/ModB0.f90 or the B0 source terms of src/ModCalcSource.f90, or reading the B0local lookup table without its real4 conversion crosses it by a wide margin, because absjxb is the volume-integrated Lorentz force; without the curl B0 terms it changes by order one, which is exactly what this configuration is for. It is achievable: the pinned build is bit-reproducible: two runs of the same executable in the same directory gave byte-identical log and plot files (rerun of Param/CORONA/PARAM.in.AwsomR, 2026-09-04), so the whole spread between two legitimate runs comes from the different order of floating-point operations a port introduces. The state is binary64 end to end (share/build/Makefile.Linux.gfortran compiles with -fdefault-real-8, so State_VGB in src/ModAdvance.f90 is a real(8) array) and PostIDL writes the ASCII plot files with eleven significant digits, while the RAW log line is printed with six (src/ModWriteLogSatFile.f90), which is why the log group carries a looser pair. The calibration selfcheck of 2026-09-04 measured a nominal-versus-variant spread of 2.64e-11 in these units, a bound_fraction of 3.6e-7 under the raised bound. The bound on the two plot files was raised from 1e-6 to 4e-5 on 2026-09-06 by the curator's worker, under the standing ruling that a legitimate alternative build sets the bound and that a floor landing near a bound raises it: the -O0 build of this same configuration and deck (evidence.altbuild) moves the uyrot column of y0_var.outs by 4.49e-7 in these column-scaled units, a bound_fraction of 0.397 against the old 1e-6 bound, so a legitimate build cleared the old bound by only a factor of 2.5. At 4e-5 that same measured floor is a bound_fraction of 9.93e-3, a factor of 101 inside; the log group keeps its 1e-4 pair, which the -O0 build matches exactly (bound_fraction 0.0). The raised bound still rejects the fault above by four decades: dropping the curl B0 momentum flux changes absjxb, the volume-integrated Lorentz force, by order one. evidence.floor_how says how the floor was measured and what it does and does not represent.

The graded files are `log.log`, `y0_var.outs`, `z0_var.outs`. Nothing in
them carries a run date, a host name or a timing line: the log files hold one row per saved step and the IDL files hold the plotted state, both written by the pinned source.
Structure is a gate: a differing number of snapshots, grid size, column list,
step number, snapshot time or scalar header parameter fails the check before any
tolerance is applied, and so does a non-finite value.

## Evidence

nominal versus variant, sab.py task selfcheck run 20260904T113532Z on the consented host ale-worker (x86_64, 88 cores, Docker 29.1.3, 8 declared cpus), calibration run started 2026-09-04T11:35:32Z: the largest |candidate - reference| / s over every graded value of every graded file was 2.64e-11, with s the column scale that comparison.rule defines. The nominal solve reproduces itself byte for byte on rerun (verified natively on Param/CORONA/PARAM.in.AwsomR, 2026-09-04), so this is the whole distance a two-ulp (2.2e-16 relative) perturbation of the initial condition travels over the graded window. Measured run time 19 s on the declared 8 cpus; the whole suite measured 1121 s of run time and 838 s of build time. The `-O0` alternative build of the same configuration and deck was measured on the consented host huangzesen@136.114.2.6 on 2026-09-05 and moves the graded output by 4.49e-7 in these units; the plot-file bound was raised from 1e-6 to 4e-5 on 2026-09-06 to admit that measured floor with a factor of 101 of headroom.
