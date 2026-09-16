# Bundled third-party code

`colorpy/` is the Python 3 fork of Mark Kness's ColorPy 0.1.1 (LGPL-3.0, see
`colorpy/COPYING.LESSER.txt` and `colorpy/license.txt`), taken verbatim from
https://github.com/fish2000/ColorPy at commit
`4c9e5b2e334851b42626f9a1eda31197739b097e` (2014-02-06) with `git archive`
(the `.gitignore` dropped, nothing else changed). The upstream `tmm` files
next to this directory are untouched.

Why it is here: `color.py` and `examples.sample5()` of tmm import `colorpy`
(illuminants, CIE XYZ integration, sRGB conversion). The colorpy release on
PyPI (0.1.1) is Python 2 only (`import colormodels` fails on Python 3), and
tmm's README points to this fork as the Python 3 version. It is not a
well-known public package, so the pipeline vendors it with the source rather
than fetching it at image build time. The package is flat like tmm itself:
put `third_party/` on `PYTHONPATH` and `import colorpy` resolves to it.
`colorpy.illuminants` imports `pylab`, so Matplotlib is a runtime dependency
of the colour path.
