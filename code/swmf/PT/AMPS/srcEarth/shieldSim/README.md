# shieldSim

`shieldSim` is a compact Geant4 application for modeling energetic-particle transport through a planar shielding slab and estimating transmitted spectra and dose behind the shield.

The package version is split into normal Geant4-style source files while preserving the logic from the single-file prototype:

- CLI-selectable Geant4 physics list (`FTFP_BERT`, `FTFP_BERT_HP`, `Shielding`, `QGSP_BIC_HP`),
- CLI-selectable shield materials using either Geant4/NIST names or built-in aliases for metals, hydrogenous polymers, composites, water, and regolith,
- CLI-selectable detector/absorber/target materials for downstream scoring slabs, including tissue proxies and electronics/semiconductor media,
- source normalization using `J(E) dE`,
- `beam` and `isotropic` source modes controlled by CLI,
- local-coordinate shield rear-face scoring,
- dose accumulation in Geant4 internal units,
- computed TID, DDD, 1-MeV neutron-equivalent fluence, LET spectra, and H100/10 hardness output,
- detailed help text describing input/output units and radiation-effect approximations,
- Layer-1/2/3/4 automated test scripts with deterministic diagnostic and regression output,
- optional test/numerical controls: `--dump-run-summary`, `--production-cut`, and `--max-step`.

## Directory structure

```text
shieldSim/
├── CMakeLists.txt
├── README.md
├── shieldSim.cc
├── include/
│   ├── CLI.hh
│   ├── ComputedQuantities.hh
│   ├── DetectorConstruction.hh
│   ├── EventAction.hh
│   ├── GCRSpectrum.hh
│   ├── MaterialCatalog.hh
│   ├── OutputUtils.hh
│   ├── PrimaryGeneratorAction.hh
│   ├── RunAction.hh
│   ├── ShieldSimConfig.hh
│   ├── SpecBins.hh
│   └── SteppingAction.hh
├── src/
│   ├── CLI.cc
│   ├── ComputedQuantities.cc
│   ├── DetectorConstruction.cc
│   ├── EventAction.cc
│   ├── GCRSpectrum.cc
│   ├── MaterialCatalog.cc
│   ├── OutputUtils.cc
│   ├── PrimaryGeneratorAction.cc
│   ├── RunAction.cc
│   └── SteppingAction.cc
├── examples/
│   └── sep_spectrum.dat
├── macros/
│   └── example_commands.sh
└── tests/
    ├── README.md
    ├── run_layer1_tests.sh
    ├── run_layer2_tests.sh
    ├── run_layer3_tests.sh
    ├── run_layer4_tests.sh
    ├── regression_tools.py
    ├── data/
    │   ├── mono_50MeV_proton.dat
    │   ├── mono_100MeV_proton.dat
    │   ├── mono_100MeV_proton_rate10.dat
    │   ├── mono_100MeV_alpha.dat
    │   ├── mono_100MeV_alpha_rate10.dat
    │   └── mono_150MeV_proton.dat
    ├── expected/
    │   └── layer4/
    └── logs/
```

## Automated tests

The package includes layered test scripts under `tests/`. Layer-1 covers build
and CLI smoke tests, Layer-2 covers geometry/source/scoring diagnostics, Layer-3
covers physics-output sanity checks, and Layer-4 performs baseline-driven
end-to-end regression tests. See `tests/README.md` for details.

Layer-4 baselines are created only after manual review:

```bash
tests/run_layer4_tests.sh --update-baseline
```

Subsequent checks compare against that accepted baseline:

```bash
tests/run_layer4_tests.sh --check
```

## Build

Source your Geant4 environment first, then build with CMake:

```bash
mkdir build
cd build
cmake ..
make -j
```

If CMake cannot find Geant4 automatically, provide `Geant4_DIR`, for example:

```bash
cmake .. -DGeant4_DIR=/path/to/geant4/lib/Geant4-11.*/
```

## Run examples

Single 2 mm Al shield with the built-in GCR-like spectrum and the default physics list:

```bash
./shieldSim --source-mode=beam --shield=Al:2 --events=50000
```

Isotropic source over the upstream plane using high-precision neutron transport:

```bash
./shieldSim --physics-list=FTFP_BERT_HP --source-mode=isotropic \
            --shield=Al:2 --events=50000
```

Tabulated SEP-like source spectrum:

```bash
./shieldSim --source-mode=isotropic --spectrum=examples/sep_spectrum.dat \
            --shield=Al:2 --events=100000
```

Dose-vs-thickness sweep over HDPE:

```bash
./shieldSim --sweep --source-mode=isotropic --sweep-material=HDPE \
            --sweep-tmin=0.5 --sweep-tmax=30 --sweep-n=15 \
            --sweep-log --events=20000
```

## Shield-material selection

Use `--shield=<material>:<thickness_mm>` for a single run, or `--sweep-material=<material>` in sweep mode.  The material name can be either a normal Geant4/NIST material such as `G4_Al`, `G4_WATER`, or `G4_Si`, or one of the built-in keys below.

Print the full material table, including aliases, composition notes, and reference notes, with:

```bash
./shieldSim --list-materials
```

The same table is also printed inside:

```bash
./shieldSim --help
```

Built-in shielding keys:

```text
Structural Metals
  Al               Aluminum (Al), alias to G4_Al
  Cu               Copper (Cu), alias to G4_Cu
  W                Tungsten (W), alias to G4_W
  Ta               Tantalum (Ta), alias to G4_Ta

Hydrogenous Polymers
  HDPE             Custom polyethylene approximation, CH2, density 0.95 g/cm3
  BPE              Custom borated polyethylene, 95 wt% HDPE + 5 wt% B
  Kevlar           Custom aramid/Kevlar approximation, C14H10N2O2
  Kapton           Polyimide/Kapton, alias to G4_KAPTON

Composites
  CFRP             Custom 60 wt% carbon fiber + 40 wt% epoxy approximation
  SiCComposite     Custom 70 wt% SiC + 30 wt% epoxy/plastic approximation

Water / Regolith
  Water            Water, alias to G4_WATER
  LunarRegolith    Approximate bulk lunar regolith
  MarsRegolith     Approximate bulk Martian regolith
```

Examples:

```bash
./shieldSim --shield=HDPE:10 --events=50000
./shieldSim --shield=BPE:10 --physics-list=Shielding --events=50000
./shieldSim --sweep --sweep-material=CFRP --sweep-tmin=1 --sweep-tmax=20 --sweep-n=10
./shieldSim --shield=LunarRegolith:50 --source-mode=isotropic --events=100000
```

The composite and regolith materials are approximate trade-study materials defined in `src/MaterialCatalog.cc`.  Replace their density/composition there if a project-specific measured material is required.


## Detector / absorber / target material selection

Use `--scoring=<material>:<thickness_mm>,...` to define downstream scoring slabs.  The alias `--target=<material>:<thickness_mm>,...` is equivalent and is often clearer when the slabs represent tissue, detector, or electronics media.

Print the full detector/target catalog with:

```bash
./shieldSim --list-target-materials
```

Built-in detector/target keys:

```text
Crewed Mission - Tissue Targets
  Skin             Skin/epidermis proxy; use e.g. Skin:0.07 for 0.07 mm
  EyeLens          Crystalline eye-lens tissue proxy
  BFO              Blood-forming-organ soft-tissue proxy; BFO:50 for 5 cm
  CNS              Brain/CNS tissue proxy
  SoftTissue       ICRU-style 4-element soft tissue / muscle proxy

Electronics - Silicon Family
  Si               Silicon, alias to G4_Si
  SiO2             Silicon dioxide / gate-oxide proxy
  SiC              Dense stoichiometric silicon carbide

Electronics - III-V and Compound Semiconductors
  GaAs             Gallium arsenide
  InGaAs           In0.53Ga0.47As proxy
  Ge               Germanium, alias to G4_Ge
  H2O              Water reference, alias to G4_WATER
```

Examples:

```bash
# Tissue and electronics scoring behind 2 mm aluminum
./shieldSim --shield=Al:2 --target=BFO:50,Si:1 --events=100000

# Shallow skin and eye-lens scoring
./shieldSim --shield=HDPE:5 --target=Skin:0.07,EyeLens:1 --source-mode=isotropic

# Electronics detector materials
./shieldSim --shield=Al:2 --target=Si:1,SiO2:0.01,GaAs:1,InGaAs:1,Ge:1
```

Organ/tissue names define material composition only.  `shieldSim` does not apply NASA limit checks, quality factors, LET/RBE weighting, organ weighting, or device response functions internally.  It now provides documented first-order TID, DDD, n_eq, LET-spectrum, and hardness outputs; DDD/n_eq remain engineering proxies until replaced with authoritative material-specific NIEL and device-response data.


## Computed quantity selection

The code now computes the quantities shown in the UI for every selected `shield x target` combination.  By default all computed quantities are enabled:

```bash
./shieldSim --shield=Al:2 --target=BFO:50,Si:1 --quantities=all --events=100000
```

You can restrict the output with a comma-separated list:

```bash
./shieldSim --shield=Al:2 --target=Si:1 --quantities=TID,DDD,n_eq,H100/10
./shieldSim --shield=Al:2 --target=Si:1 --quantities=TID,LET
./shieldSim --list-quantities
```

Implemented quantities:

```text
TID       Total Ionizing Dose
          D = E_dep / m.  E_dep is the Geant4 energy deposition in the selected target slab.
          Output units: Gy/primary, rad/primary, Gy/s, rad/s.

DDD       Displacement Damage Dose proxy
          D_d = integral Phi(E) NIEL(E) dE.
          Phi is the area-averaged transmitted fluence at the downstream shield face.
          NIEL is a documented analytic surrogate in src/ComputedQuantities.cc.
          Output units: MeV/g/primary and MeV/g/s, plus rad-equivalent columns.

n_eq      1-MeV Neutron Equivalent fluence proxy
          n_eq = DDD / NIEL_1MeV_neutron(material).
          This convention is most meaningful for silicon electronics; non-silicon values are proxies.
          Output units: cm^-2/primary and cm^-2/s.

LET       LET spectrum
          dPhi/dLET = integral dPhi/dE delta(LET - LET(E,material)) dE.
          LET(E,material) is estimated with a Bethe-Bloch electronic mass-stopping-power approximation.
          Output units: area-averaged fluence per primary and fluence rate per LET bin.

H100/10   Spectral hardness index
          H100/10 = J(>100 MeV) / J(>10 MeV) for transmitted protons + alphas + neutrons.
```

The equations, assumptions, and references are documented in `include/ComputedQuantities.hh` and `src/ComputedQuantities.cc`.  TID is directly scored by Geant4 and is the most robust output.  DDD, `n_eq`, and LET are post-processed engineering estimates.  For quantitative electronics damage work, replace `ComputedQuantities::NIEL_MeV_cm2_g()` with tabulated NIEL/SR-NIEL data for the target material and particle species of interest.

## Material-property references and adding new materials

The material-property references used by the built-in catalogs are documented in `src/MaterialCatalog.cc` and summarized by `./shieldSim --list-materials` and `./shieldSim --list-target-materials`.  In brief:

- `Al`, `Cu`, `W`, `Ta`, `Kapton`, and `Water` are aliases to Geant4/NIST `G4_*` materials.
- `HDPE`, `BPE`, `Kevlar`, `CFRP`, `SiCComposite`, `LunarRegolith`, and `MarsRegolith` are custom ShieldSim trade-study approximations.
- Composite and regolith definitions are intentionally approximate. Replace their density and mass fractions with measured project-specific values before using them for final quantitative analysis.
- Tissue/organ target definitions are simplified material proxies. They are not anatomical phantoms and do not include biological weighting factors.
- Electronics target definitions are transport media only. The built-in DDD, n_eq, and LET outputs are documented engineering proxies; charge collection, dark-current response, threshold shifts, and device-specific damage response still require external post-processing.

To add a new material:

1. Edit `src/MaterialCatalog.cc` and add a `MaterialCatalogEntry` in `ShieldMaterialCatalog()` for a new shield/absorber material, or in `DetectorMaterialCatalog()` for a new detector/target scoring material.
2. If the material exists in Geant4/NIST, set `canonicalName` to the corresponding `G4_*` name and set `isCustom=false`.
3. If the material is custom, implement a `Build...()` function in `src/MaterialCatalog.cc`, use explicit Geant4 units such as `g/cm3`, register the builder in `BuildCustomMaterial()`, and document the density/composition/reference above the builder.
4. Add useful aliases. The parser ignores case and punctuation, so aliases can be user-friendly names.
5. Rebuild. The `--help`, `--list-materials`, and `--list-target-materials` output is generated from the catalogs automatically.

## Physics-list selection

Use `--physics-list=<name>` to select the Geant4 reference physics list at runtime. Supported values are:

```text
FTFP_BERT
FTFP_BERT_HP
Shielding
QGSP_BIC_HP
```

The default is `FTFP_BERT`. For shielding calculations where low-energy secondary neutrons can affect dose behind the absorber, compare against one of the high-precision neutron options, especially `FTFP_BERT_HP`, `QGSP_BIC_HP`, or `Shielding`.

Example:

```bash
./shieldSim --physics-list=Shielding --source-mode=isotropic \
            --shield=Al:5 --events=100000
```

## Numerical controls for verification

The default behavior is to use the selected Geant4 physics list with its normal
production cuts and stepping behavior.  For tests and numerical-convergence
studies, the CLI also provides:

```bash
--production-cut=<mm>
--max-step=<mm>
```

`--production-cut=<mm>` calls `SetDefaultCutValue()` on the selected Geant4
physics list.  This controls the range cut used for explicit secondary
production.  It can affect low-energy secondaries, TID, DDD, `n_eq`, and LET
tails, so production studies should record the selected value.

`--max-step=<mm>` attaches `G4UserLimits` to the shield and scoring slabs and
registers `G4StepLimiterPhysics`.  This is useful for thin-target dose checks
and LET-spectrum convergence studies.  Omit it to use the normal physics-list
stepping behavior.

These options are primarily intended for Layer-3 tests and convergence studies,
not as substitutes for physics validation.

## Spectrum input units

A tabulated spectrum file has three columns:

```text
E[MeV]   protonSpectrum   alphaSpectrum
```

`E` is total kinetic energy per particle. For alpha particles, `E` is total alpha kinetic energy, not MeV/nucleon.

In `beam` mode, spectrum columns are interpreted as differential pencil-beam source rates, for example:

```text
particles / s / MeV
```

In `isotropic` mode, spectrum columns are interpreted as differential intensities, for example:

```text
particles / cm2 / s / sr / MeV
```

The code applies the `pi` angular factor for isotropic plane-crossing flux.

## Output files

### `shieldSim_spectra.dat`

Tecplot-format differential spectra. Energy is MeV total kinetic energy per particle.

- `*_MC` columns: raw Monte Carlo spectra in `counts/(MeV primary)`.
- `*_Norm` columns: source-normalized spectra.

For beam mode, normalized units are `particles/s/MeV` if the input spectrum is `particles/s/MeV`.

For isotropic mode, normalized units are `particles/(cm2 s MeV)` if the input spectrum is `particles/(cm2 s sr MeV)`.

### `shieldSim_quantities.dat`

Tecplot-format scalar quantity table.  One row is written for each selected target material in each shielding configuration.  In sweep mode a new zone is appended for every shield thickness.

- TID columns: `Gy/primary`, `rad/primary`, `Gy/s`, `rad/s`.
- DDD columns: `MeV/g/primary`, `MeV/g/s`, and rad-equivalent conversions.
- `n_eq` columns: `cm^-2/primary` and `cm^-2/s`.
- `H100_10`: dimensionless hardness index.

### `shieldSim_let_spectrum.dat`

Tecplot-format LET spectra, one zone for each target material and shielding configuration when LET is enabled.  LET is in `MeV cm2/mg`.  Per-primary columns are area-averaged fluence per source primary per LET unit; rate columns multiply by the source-primary rate.

### `shieldSim_dose_sweep.dat`

Written only in sweep mode.

- `Dose_*_perPrimary` columns are `Gy/primary` when TID is enabled.
- `DoseRate_*` columns are `Gy/s` when the source spectrum has physical units.
- DDD, `n_eq`, and `H100_10` sweep columns are also included when those quantities are enabled.

### `shieldSimOutput*.csv`

Geant4 analysis histograms for exit energies of protons, alphas, and neutrons.

### Run-summary diagnostic file

Written only when requested with:

```bash
--dump-run-summary=<file>
```

This is a machine-readable diagnostic used by the Layer-3 tests.  It repeats
integrated quantities such as source normalization, transmitted counts, TID,
DDD, `n_eq`, and H100/10 in simple keyword rows.  The normal Tecplot files remain
the science-facing outputs.

## Notes and limitations

- The default physics list is `FTFP_BERT`, and the runtime option `--physics-list=` supports `FTFP_BERT`, `FTFP_BERT_HP`, `Shielding`, and `QGSP_BIC_HP`.
- The built-in material catalog is intended for shielding trade studies.  Use `G4_` NIST names or replace the custom definitions if an exact alloy, polymer formulation, composite layup, or regolith composition is required.
- The isotropic source uses a finite upstream plane, not an infinite half-space source. Very oblique trajectories can interact with the finite side boundaries.
- The default detector transverse size is 5 cm × 5 cm. Dose per primary depends on the scoring mass and therefore on this finite detector area.
- DDD and n_eq are based on a transparent analytic NIEL surrogate, not authoritative material-specific NIEL tables.  Replace the NIEL function for final electronics-damage analyses.
- LET spectra use a Bethe-Bloch estimate for binning.  Use NIST/Geant4 stopping-power tables if high-accuracy LET spectra are required.

## Testing and Diagnostic Options

The package includes automated test scripts under `tests/`:

```bash
tests/run_layer1_tests.sh   # build and CLI smoke tests
tests/run_layer2_tests.sh   # geometry, source, and shield-exit scoring tests
tests/run_layer3_tests.sh   # physics-output and numerical-sanity tests
```

Layer-1 tests check build/CLI behavior.  Layer-2 tests check geometry, source
sampling, and shield rear-face scoring using short deterministic runs.  Layer-3
tests check source-normalization scaling, computed-quantity output, H100/10,
LET folding, physics-list selection, sweep output, and optional numerical
controls.

Diagnostic CLI options used by the tests are:

```bash
--random-seed=<integer>
--output-prefix=<name>
--dump-source-samples=<file>
--dump-exit-particles=<file>
--dump-run-summary=<file>
--diagnostic-max-rows=<n>
--production-cut=<mm>
--max-step=<mm>
```

`--dump-source-samples` writes one row per primary generated by
`PrimaryGeneratorAction`:

```text
row species E_MeV x_mm y_mm z_mm ux uy uz
```

`--dump-exit-particles` writes one row for each particle accepted by
`SteppingAction` as crossing the downstream shield face:

```text
row species E_MeV xg_mm yg_mm zg_mm xl_mm yl_mm zl_mm uxl uyl uzl
```

`--dump-run-summary` writes one machine-readable scalar block per run:

```text
scalar H100_10 <value>
count output_proton <value>
target index name thickness_mm TID_Gy_perPrimary TIDRate_Gy_s DDD_MeV_g_perPrimary DDDRate_MeV_g_s n_eq_cm2_perPrimary n_eq_rate_cm2_s
```

Run the tests from the top-level package directory:

```bash
chmod +x tests/run_layer1_tests.sh tests/run_layer2_tests.sh tests/run_layer3_tests.sh
tests/run_layer1_tests.sh
tests/run_layer2_tests.sh
tests/run_layer3_tests.sh
```

Each script accepts `-h`, `-help`, and `--help`:

```bash
tests/run_layer3_tests.sh --help
```

Layer-3 runtime can be increased for higher-statistics checks:

```bash
EVENTS_SHORT=10000 EVENTS_SMOKE=1000 tests/run_layer3_tests.sh
```

Generated build/test directories and logs are ignored by `.gitignore`.
