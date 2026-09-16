# K-mimic: physical background and ini audit

Status: diagnostic only, at STOP 4. No new CAMB execution, Docker job, or ODE
integration was performed. The source, ini decks, validators, rubrics, numerical
bounds and CLI self-validation record were not changed. The preceding scratch
initialization patch was not adopted.

## Finding

The three actual K-mimic decks do not show a negative kinetic or gradient sign in
an independent algebraic background audit. Their expansion histories are regular.
The saved expansion rates do, however, depart from the exact model identity by
about 5.5e-5 relatively. Reconstructing the source's independent linear
interpolation of the EFT functions reproduces those saved rates to below 8.7e-11
relative error, consistent with the precision of the printed scale factors and
Hubble rates. This identifies a background interpolation error, not yet the cause
of the failing spectral comparisons. The passing third deck has a similar
expansion-rate error.

The mass/tachyon question remains open: all three decks disable both mass gates.
Do not infer that a physical instability exists, or that all physical stability
conditions have been tested, from the existing logs.

## Inputs actually used

The audit reads the effective saved `5_Kmimic_*_params.ini` files from the stock
arm of `jobs/eftcamb-kmimic-ic-probe-01`, and checks the nominal task decks and
their base file. All have `Kmimic=T`, `m=3`, `H0=70`, `ombh2=0.0226`,
`omch2=0.112`, `omk=0`, `omnuh2=0`, and `TCMB=2.7255`.

| Deck | eps2_0 | gammaA | Effective matching LCDM Omega_m | Early A | Minimum sampled c_s^2 | Scale factor at minimum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| K-mimic 1 | 0.01 | 5 | 0.28869306 | 0.99000 | 0.00331679 | 0.0213796 |
| K-mimic 2 | 0.01 | 20 | 0.28692939 | 0.99125 | 0.00308306 | 0.0229087 |
| K-mimic 3 | 0.001 | 5 | 0.27608118 | 0.99900 | 0.00326195 | 0.0218776 |

The common physical matter density is 0.27469387755, not the matching LCDM
density in the table. The radiation density is 8.5381688253e-5. Constants are
taken from `fortran/constants.f90:24-53` and the radiation normalization from
`fortran/results.f90:451-461`.

The base file's one massive-neutrino species is not an inconsistent leftover:
`results.f90:389-399` converts it to a massless species when `omnuh2=0`.
With `share_delta_neff=T`, the resulting effective count is 2.046 + 1 = 3.046.

`alphaU=0.2` and `gammaU=1` are not used to construct the K-mimic background;
they parameterize the other K-mouflage branch. The common additional-priors
function still tests the sign of alphaU. Changing these two values is therefore
not a supported way to repair these K-mimic spectra.

## Physical interpretation and independent calculation

The model paper defines the reconstruction and stability conditions in
Eqs. (2.15)-(2.29), with the implemented matching matter density in Eq. (8.4).
It explicitly allows the matching LCDM matter density to differ from the physical
one. Positive eps2_0 is the K-mimic sign convention. A general warning about large
gammaA is not a diagnosis for a particular deck; the actual stability combinations
must be evaluated. The paper's observational parameter bounds are not numerical
well-posedness criteria. See [Benevento et al., arXiv:1809.09958](https://arxiv.org/abs/1809.09958).

For the following derivation, u and v denote the implementation's dimensionless
`ufunc` and `vfunc`, proportional to A^4 rho_phi and A^4 p_phi. They are explicit
functions of a, so no scalar-background integration is needed to evaluate their
signs or their derivatives. From the source at
`fortran/eftcamb/08f_full_models/008p3_Kmouflage.f90:704-750`:

```text
K_chi       = (u+v)/(2 chi Omega_phi0)
K_chichi    = (u+v)(u'-v')/(4 chi^2 Omega_phi0 v')
K_chi + 2 chi K_chichi = K_chi u'/v'
c_s^2       = K_chi/(K_chi + 2 chi K_chichi) = v'/u'
d ln chi / d ln a = 2 a v'/(u+v)
```

Primes here mean derivatives with respect to a. This is the scalar kinetic
sound speed, not a claim about finite-wavelength mass stability of the coupled
matter/scalar system.

On 1,801 log-spaced samples covering 1e-9 <= a <= 1, each deck has A>0,
Omega_phi0>0, u+v>0, u'<0 and v'<0. The prescribed present-day chi is positive:
0.00708897344 for decks 1/2, and 0.000691350898 for deck 3. A regular solution
of the displayed logarithmic evolution preserves that sign. Consequently, the
sampled branch satisfies K_chi>0 and K_chi+2 chi K_chichi>0. This is a finite
sampling audit, not an interval proof or a complete stability certification.

The no-ghost matching-density bound is exceeded by 0.01010101 for decks 1/2
and 0.001001001 for deck 3. No tested deck sits exactly on that bound. All
three have a low sound-speed period around a=0.02; the third deck passes the
existing additive spectral comparison despite having essentially the same
minimum sound speed. Low sound speed alone does not separate failure from pass.

There is a real physical distinction between the decks: the extra scalar does
not vanish in the early radiation-era limit. Directly taking that limit in the
source identities gives rho_phi/rho_r = A_early^-2 - 1 and w_phi -> 1/3.
Its fraction of total radiation-plus-scalar density tends to 1-A_early^2:
1.9900%, 1.74234%, and 0.1999% respectively. Deck 3 therefore carries a much
smaller early scalar contribution. This is a model feature, not evidence of an
instability, and does not establish the source of numerical error amplification.
The source's return-to-GR warning at a=1e-8 is consistent with this nonzero
early contribution; moving the turn-on earlier would not make A_early become 1.
This audit does not establish that the perturbation initial conditions are wrong.

## Expansion-rate error: source of the discrepancy is identifiable

Substituting 2 chi K_chi-K = u/Omega_phi0 into the implemented Friedmann equation
cancels chi and gives, exactly,

```text
H(a)^2 / H0^2 = Omega_m_matching/a^3 + Omega_r/a^4 + Omega_Lambda_matching
```

There is no physical background singularity in this expression for these decks.
The scalar-background ODE normalization cannot change this exact identity.

The source stores Omega, Omega', c and Lambda on 1,000 equally spaced ln(a)
nodes from 1e-9 to 1, interpolates each independently, then evaluates the
Friedmann equation from those interpolated values. The audit reconstructs this
procedure directly from the analytic identities, with no fit to saved output.
It uses `008p3_Kmouflage.f90:815-833`,
`02_equispaced_interpolation_linear_1D.f90:112-176` and
`06p1_abstract_EFTCAMB_full_map.f90:94-96`.

| Deck | Saved H vs exact identity: maximum relative error | Saved H vs reconstructed interpolation: maximum relative error | Saved nominal/variant background |
| --- | ---: | ---: | --- |
| K-mimic 1 | 5.49162e-5 | 5.82521e-11 | Byte-identical |
| K-mimic 2 | 5.48345e-5 | 8.60606e-11 | Byte-identical |
| K-mimic 3 | 5.73265e-5 | 5.95798e-11 | Byte-identical |

Each comparison uses the 20 already saved rows, a=1e-5 through 1. These sparse
distance/background files do not contain the dense kinetic, mass, or EFT
coefficient histories. Their byte equality does not imply internal background
arrays are bitwise identical, or that their derivatives are accurate. A common
bias can be invisible to nominal/variant self-comparison.

Combined with the independently observed background-grid phase pattern in
`../kmimic-ic-probe-01/interpretation.md`, interpolation consistency is a stronger
lead than an assumed background pathology. It still needs a controlled
perturbation-evolution test. The previous 2,000-node experiment produced huge
nominal matter-power spikes, so merely increasing node count is not an accepted
fix or proof of convergence.

## What remains untested

Both `EFT_mass_stability=F` and `EFT_mass_math_stability=F` are explicit deck
settings. Ghost and gradient gates remain enabled. The physical mass criterion
would reject a mass eigenvalue below -100 H^2 at the configured rate of 10;
see `09_EFTCAMB_stability.f90:357-369`. Those mass eigenvalues are not computed
by the existing disabled-gate runs (`09_EFTCAMB_stability.f90:538-540`).

A future mass diagnostic must also check numerical reliability: its extra
derivatives use finite differences of the interpolated functions
(`06p1_abstract_EFTCAMB_full_map.f90:155-170`). A negative result from a noisy
mass calculation alone would not prove a physical tachyon. No mass-gate change
or new run is authorized by this report.

The next useful controlled experiment would hold the physical decks, source
perturbation initialization and all bounds fixed, check a consistent analytic
background-to-EFT evaluation against the exact identities, then measure whether
the spectral sensitivity changes. Any scratch source modification and compute
command must be separately previewed and approved. A source-level repair cannot
be silently folded into a benchmark whose oracle is the untouched pinned source.

## Reproducibility and limits

- Local analysis: `jobs/eftcamb-kmimic-background-audit.py`; Python standard
  library only, 60-digit Decimal arithmetic, under one second on this host.
- Coupling derivatives through A'''' and eps2'' match the source's explicit
  formulas to 4.94e-58 relative on 19 sampled locations per deck.
- Independent u/v first derivatives match their source formulas to 6.15e-57.
- A separate five-point stencil in ln(a), step 1e-6, agrees with first and
  second derivatives to 8.54e-24 and 2.85e-24 respectively. Its truncation error
  is distinct from solver accuracy.
- A 40-versus-80-digit repeat at seven selected scale factors per deck changes
  c_s^2, u+v, and reconstructed H by at most 3.81e-37 relatively.
- `summary.json`, `per-deck.csv`, `background-grid.csv`,
  `saved-expansion.csv`, and `derivative-crosscheck.csv` retain the calculations.
- These are background diagnostics, not new calibration spreads, revised bounds,
  a replacement self-validation record, or a GPU performance result.
