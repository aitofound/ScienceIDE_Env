# Modifications to the vendored tree

`code/legume/` **is not byte-for-byte upstream.** This is the first vendored
tree in this benchmark that deviates from its pin, so every change is listed
here in full. The codebase owner directed each one; upstream was deliberately
not contacted.

Pin: `edef021a98d5972ef52e3a65bd36ccfb742381be` (tag `v1.0.3`) of
<https://github.com/fancompute/legume>.

## The complete deviation, in one table

| # | path | change | lines | commit |
|---|---|---|---:|---|
| 1 | `legume/gme/gme.py` | 2 in-place accumulations rewritten out-of-place | +4 −2 | `dfc2044` |
| 2 | `legume/utils.py` | 1 in-place accumulation rewritten out-of-place | +1 −1 | `dfc2044` |
| 3 | `legume/phc/layer.py` | 1 in-place accumulation rewritten out-of-place | +1 −1 | `1c02ac4` |
| 4 | `docs/examples/legume/` | directory deleted (22 files, 3.7 MB) | −10,979 | `640d0e0` |

Totals: **93 files, was 115. Six source lines changed, four removed, 22 files
deleted.** No file was added. Verify with:

```
git diff 8494b98 HEAD -- code/legume/
```

`8494b98` is the pristine vendoring commit, so that diff *is* the deviation and
nothing else. Each change is also isolated in its own commit above.

## 1–3. Four in-place accumulations (a genuine upstream defect)

```python
legume/gme/gme.py:1720    rad_c['te'][clad_ind] += -1j * bd.sum(rad, axis=0)
legume/gme/gme.py:1743    rad_c['tm'][clad_ind] += bd.sum(rad, axis=0)
legume/utils.py:28        ftinv += ft_coeff[indg]*np.exp(...)
legume/phc/layer.py:175   FT[ind0] += bd.abs(Vmax)
```

Each accumulator is allocated as a plain numpy array (`bd.zeros` or
`np.zeros`). Under the `autograd` backend the right-hand side is an autograd
`ArrayBox`, and `ndarray += ArrayBox` makes numpy return `NotImplemented`,
raising `TypeError`. Assigning out-of-place lets the box propagate. Site 4 uses
`bd.where` rather than an index assignment so the line dispatches on both
backends.

**What each one broke.** Sites 1–2: every differentiated run with
`compute_im=True`, which is the default — so `tests/test_gme_grad.py` and
example notebooks 06 and 08, the tutorials for the gradient-of-Q feature the
README leads with. Site 3: differentiation through `get_field_xy`, the
real-space field reconstruction; no test reaches it, only the notebooks. Site
4: `Layer.compute_exc_ft` under `if FT[ind0][0] < 0`, a branch nothing upstream
takes (`test_polariton` reaches the guard and reads `0.0`).

**Age.** Sites 1–2 date to upstream commit `d663dd5` (2020-03-12) and are
present in every tag from `0.1.2` through `v1.0.3`, so **no tagged release has
ever had a working gradient-of-Q path.** Reproduced identically on
python 3.14.6 with numpy 2.5.3 and on python 3.11.10 with numpy 1.26.4, so it
is not a numpy 2 regression.

**Proof it cannot change a graded value.** For a plain numpy array `a = a + b`
and `a += b` compute identical values; only allocation differs. Measured rather
than argued — on the numpy backend, with all four sites patched:

| observable | pristine vs patched |
|---|---|
| `gme.freqs` | sha256 identical |
| `gme.freqs_im` | sha256 identical |
| reconstructed real-space field (exercises site 3) | sha256 identical |
| site 4's two formulations on plain arrays, branch taken | bit-identical, maxdiff 0.0 |

**Effect.**

| | before | after |
|---|---|---|
| `python -m pytest tests` | 12 passed, 1 failed | **13 passed**, 52.5 s |
| notebook 08 | `TypeError` | passes, 161 s |
| notebook 06 | `TypeError` at once | reaches its real 10-epoch Adam cavity optimization, then exceeds the 300 s per-cell cap used here |
| `grad(sum of Q)` | `TypeError` | one reverse-mode pass, **2.7e-02** vs finite differences (upstream's own test bound is 1e-1) |
| site 4, differentiating a radius, branch taken | `TypeError` | `5.48617265`, **1.9e-11** vs central difference |
| site 4, branch not taken | `9.25608383` | `9.25608383` (no regression) |

**Limits of the search.** A static scan for `X = zeros(...)` followed by `X +=`
found these sites plus six in `legume/primitives.py` that are correct — a vjp
body receives concrete arrays, never boxes, and `test_primitives` passes. That
scan is a heuristic: it did **not** catch the two `gme.py` sites, whose
accumulator is built by a dict comprehension. The class may have further
members on paths nothing executes.

## 4. Deleting `docs/examples/legume/`

A fossilised partial copy of an older legume repository root, committed years
ago: a 2020-era README, a `requirements.txt` long gone from the real root,
`img/`, and a `tests/` directory. **It contains no Python module.** Nothing in
the tree references it.

It caused two of the four upstream defects by one mechanism. The directory is
named `legume` and sits first on `sys.path` whenever a notebook runs from
`docs/examples/`; since PEP 420 a directory without `__init__.py` is still an
importable namespace package. So `import legume` bound to a fossil holding no
modules:

```
AttributeError: module 'legume' has no attribute 'Lattice'
```

— which is what all 14 example notebooks died on, at their first call, when run
where the documentation says to run them. Separately, its nine `test_*.py`
share basenames with `tests/` and neither directory has an `__init__.py`, so a
bare `pytest` at the root hit nine `import file mismatch` collection errors.
Upstream never saw either, because its CI runs `pytest tests`.

**Measured after removal:**

| | before | after |
|---|---|---|
| bare `pytest` at the root | 9 collection errors | **13 collected, 0 errors** |
| notebooks run from `docs/examples/` | all 14 fail at first call | 01, 02, 04 all `rc=0` |

**Nothing unique is lost.** Its nine test files are divergent *older* copies of
`tests/`; it lacks `test_polariton.py` and the three `Polariton_*.npy`
references; and the eight reference data files it does carry are byte-identical
to the real ones.

Removing it also drops the payload's only credential-shaped string, a Travis CI
badge token in its README. That token was read-only, scoped to fetching a build
-status image, for a service the project no longer uses, and upstream deleted
the badge from the real README themselves in their PR #73.

## What a reviewer should weigh

The deviation is small, isolated per commit, and demonstrably cannot move a
graded value on the numpy backend. But it sets a precedent for this benchmark,
and one downstream consequence is a real design question for the task, raised
in `open_questions`: a check that grades the gradient-of-Q path would depend on
behaviour that works **only because this tree deviates from its pin**. The
alternative is to keep graded coverage of the autograd backend on the two
primitive-level vjp tests, which pass on pristine upstream.

Note also that `pyproject.toml` still declares `version = "1.0.3"`, which this
tree no longer exactly is. It was left alone rather than deviate a fifth time.
