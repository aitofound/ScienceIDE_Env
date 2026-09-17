#!/usr/bin/env python3
"""Check inc-tmm-consistency: the computation of the TEST half (run by run.sh).

Reproduces tests.incoherent_test() of the pinned tmm package with the inputs of
ic/<ic>/input.json and writes the graded files into --out:
  caseA.npy  (2, 3)  rows s and p: `inc_tmm` R and T of three incoherent real-index layers, and the closed-form incoherent sum R0 + R1 T0^2 / (1 - R0 R1) built from `interface_r`
  caseB.npy  (2, 7)  rows s and p: `inc_tmm` R and T of one coherent film between incoherent absorbing media, `coh_tmm` R and T of the same stack, and the three `inc_absorp_in_each_layer` fractions
  caseC.npy  (2, 11)  rows s and p: `inc_tmm` T and R of a coherent film plus a 10 um absorbing incoherent layer, the closed-form T and R, and their ingredients R02, R20, T02, T20 (forward and reverse `coh_tmm`), P2 (single-pass transmission), R23, T23 (`interface_R`, `interface_T`)
  caseD_sweep.npy  (2, SAB_NUM_D, 2)  rows s and p: `coh_tmm` R and T at each of the 357 substrate thicknesses from 10 um to 30 um
  caseD.npy  (2, 4)  rows s and p: `inc_tmm` R and T of the film-on-thick-substrate stack, and the thickness-averaged coherent R and T
  caseE_sweep.npy  (2, SAB_NUM_LAMBDA, 8)  rows s and p: at each of the 234 wavelengths from 40 to 50, the four `inc_absorp_in_each_layer` fractions followed by the four `absorp_in_each_layer` fractions of the all-incoherent four-layer stack
  caseE.npy  (2, 8)  rows s and p: the wavelength averages of the eight columns above
Every graded file is a float64 .npy array; complex quantities are stored as [real, imaginary]
pairs on the last axis. The package is imported as `tmm` from PYTHONPATH (set by run.sh).
"""
from __future__ import annotations

import argparse
import json
import os

import numpy as np
from numpy import inf, pi  # noqa: F401  (inf is what the upstream decks use for the semi-infinite media)

import tmm  # the pinned package, on PYTHONPATH (run.sh); every function below is tmm_core's


def num(x):
    """JSON value -> float or complex: a number, "inf"/"-inf", or a [real, imaginary] pair."""
    if isinstance(x, str):
        return float(x)
    if isinstance(x, (list, tuple)):
        return complex(float(x[0]), float(x[1]))
    return float(x)


def c2(z):
    z = complex(z)
    return [z.real, z.imag]


def knob_int(name, default):
    v = os.environ.get(name, "")
    return int(v) if v.strip() else int(default)


def save(out, name, arr):
    a = np.asarray(arr, dtype=np.float64)
    np.save(os.path.join(out, name), a)
    print(f"wrote {name} shape={a.shape}")


ap = argparse.ArgumentParser()
ap.add_argument("--input", required=True)
ap.add_argument("--out", required=True)
args = ap.parse_args()
with open(args.input, encoding="utf-8") as fh:
    inp = json.load(fh)
os.makedirs(args.out, exist_ok=True)
degree = pi / 180

theta0, lam_vac = float(inp["theta0"]), float(inp["lam_vac"])
num_d = knob_int("SAB_NUM_D", inp["caseD"]["num_d"])
num_lambda = knob_int("SAB_NUM_LAMBDA", inp["caseE"]["num_lambda"])
df = lambda a, b: abs(a - b) / max(abs(a), abs(b))

# case A: three incoherent layers with real indices; R against the closed-form incoherent sum
A = inp["caseA"]; nA = [num(v) for v in A["n_list"]]; dA = [num(v) for v in A["d_list"]]
n0, n1, n2 = nA
th0 = theta0
th1, th2 = tmm.snell(n0, n1, th0), tmm.snell(n0, n2, th0)
rowsA = []
for pol in ("s", "p"):
    inc = tmm.inc_tmm(pol, nA, dA, ["i", "i", "i"], th0, lam_vac)
    R0 = abs(tmm.interface_r(pol, n0, n1, th0, th1) ** 2)
    R1 = abs(tmm.interface_r(pol, n1, n2, th1, th2) ** 2)
    T0 = 1 - R0
    RR = R0 + R1 * T0 ** 2 / (1 - R0 * R1)
    rowsA.append([float(inc["R"]), float(inc["T"]), float(RR)])
    print(f"info A [{pol}]: inc R vs closed form = {df(inc['R'], RR):.3e}; R + T - 1 = {inc['R'] + inc['T'] - 1:.3e}")
save(args.out, "caseA.npy", rowsA)   # (2, 3)

# case B: one coherent film between two incoherent absorbing media; must agree with coh_tmm
B = inp["caseB"]; nB = [num(v) for v in B["n_list"]]; dB = [num(v) for v in B["d_list"]]
th0 = tmm.snell(1.0, nB[0], theta0)
rowsB = []
for pol in ("s", "p"):
    inc = tmm.inc_tmm(pol, nB, dB, ["i", "c", "i"], th0, lam_vac)
    coh = tmm.coh_tmm(pol, nB, dB, th0, lam_vac)
    absorp = tmm.inc_absorp_in_each_layer(inc)
    rowsB.append([float(inc["R"]), float(inc["T"]), float(coh["R"]), float(coh["T"])] + [float(a) for a in absorp])
    print(f"info B [{pol}]: inc vs coh R = {df(inc['R'], coh['R']):.3e}, T = {df(inc['T'], coh['T']):.3e}; 1 - sum(absorp) = {1 - sum(absorp):.3e}")
save(args.out, "caseB.npy", rowsB)   # (2, 7)

# case C: coherent film, thick absorbing incoherent layer, absorbing final medium; closed-form sums
C = inp["caseC"]; nC = [num(v) for v in C["n_list"]]; d1, d2 = float(C["d1"]), float(C["d2"])
n0, n1, n2, n3 = nC
dC = [inf, d1, d2, inf]
th0 = tmm.snell(1.0, n0, theta0)
rowsC = []
for pol in ("s", "p"):
    inc = tmm.inc_tmm(pol, nC, dC, ["i", "c", "i", "i"], th0, lam_vac)
    coh = tmm.coh_tmm(pol, [n0, n1, n2], [inf, d1, inf], th0, lam_vac)
    th2, th3 = tmm.snell(n0, n2, th0), tmm.snell(n0, n3, th0)
    cohb = tmm.coh_tmm(pol, [n2, n1, n0], [inf, d1, inf], th2, lam_vac)
    R02, R20, T02, T20 = coh["R"], cohb["R"], coh["T"], cohb["T"]
    P2 = np.exp(-4 * pi * d2 * (n2 * np.cos(th2)).imag / lam_vac)
    R23 = tmm.interface_R(pol, n2, n3, th2, th3)
    T23 = tmm.interface_T(pol, n2, n3, th2, th3)
    T = T02 * P2 * T23 / (1 - R23 * P2 * R20 * P2)
    R = R02 + T02 * P2 * R23 * P2 * T20 / (1 - R20 * P2 * R23 * P2)
    rowsC.append([float(inc["T"]), float(inc["R"]), float(T), float(R), float(R02), float(R20), float(T02), float(T20), float(P2), float(R23), float(T23)])
    print(f"info C [{pol}]: inc vs closed form T = {df(inc['T'], T):.3e}, R = {df(inc['R'], R):.3e}")
save(args.out, "caseC.npy", rowsC)   # (2, 11)

# case D: the coherent program over many substrate thicknesses averages to the incoherent result
D = inp["caseD"]; nD = [num(v) for v in D["n_list"]]
th0 = tmm.snell(1.0, nD[0], theta0)
sweepD, rowsD = [], []
for pol in ("s", "p"):
    inc = tmm.inc_tmm(pol, nD, [inf, float(D["d_film"]), 1.0, inf], ["i", "c", "i", "i"], th0, lam_vac)
    rows = []
    for dsub in np.linspace(float(D["d_sub_min"]), float(D["d_sub_max"]), num_d):
        coh = tmm.coh_tmm(pol, nD, [inf, float(D["d_film"]), dsub, inf], th0, lam_vac)
        rows.append([float(coh["R"]), float(coh["T"])])
    rows = np.asarray(rows)
    sweepD.append(rows)
    rowsD.append([float(inc["R"]), float(inc["T"]), float(rows[:, 0].mean()), float(rows[:, 1].mean())])
    print(f"info D [{pol}]: thickness-averaged coherent vs incoherent R = {df(rows[:, 0].mean(), inc['R']):.3e}, T = {df(rows[:, 1].mean(), inc['T']):.3e} (finite averaging, 1e-5 expected)")
save(args.out, "caseD_sweep.npy", sweepD)   # (2, SAB_NUM_D, 2)
save(args.out, "caseD.npy", rowsD)          # (2, 4)

# case E: the coherent program over many wavelengths averages to the incoherent per-layer absorption
E = inp["caseE"]; nE = [num(v) for v in E["n_list"]]; dE = [num(v) for v in E["d_list"]]
th0 = tmm.snell(1.0, nE[0], theta0)
sweepE, rowsE = [], []
for pol in ("s", "p"):
    rows = []
    for lam in np.linspace(float(E["lam_min"]), float(E["lam_max"]), num_lambda):
        inc = tmm.inc_tmm(pol, nE, dE, ["i", "i", "i", "i"], th0, lam)
        coh = tmm.coh_tmm(pol, nE, dE, th0, lam)
        rows.append([float(a) for a in tmm.inc_absorp_in_each_layer(inc)] + [float(a) for a in tmm.absorp_in_each_layer(coh)])
    rows = np.asarray(rows)
    sweepE.append(rows)
    rowsE.append(rows.mean(axis=0).tolist())
    print(f"info E [{pol}]: wavelength-averaged per-layer absorption, incoherent then coherent: {np.array2string(rows.mean(axis=0), precision=5)}")
save(args.out, "caseE_sweep.npy", sweepE)   # (2, SAB_NUM_LAMBDA, 8)
save(args.out, "caseE.npy", rowsE)          # (2, 8)
