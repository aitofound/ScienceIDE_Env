#!/usr/bin/env python3
"""Harness for example-check-ppf-approx: transcribes code/class/scripts/
check_PPF_approx.py's three blocks of PPF-vs-fluid dark-energy comparisons
(Hu & Sawicki arXiv:0708.1190; Fang, Hu & Lewis arXiv:0808.3125), dropping
the plotting calls, dumping every Cl array and every requested-k
perturbation the script computes, keyed by model / (Omega_k, gauge) in the
script's own loop order.

    python3 harness.py PARAMS_JSON OUT_JSON
"""
import json
import sys

from classy import Class


def dump_cl(cl):
    return {k: [float(x) for x in v] for k, v in cl.items()}


def dump_perturbations(pts):
    # pts is a list (one dict per k_output_values entry); each dict's values
    # are per-time arrays for that k.
    return [{k: [float(x) for x in v] for k, v in ptk.items()} for ptk in pts]


def main() -> int:
    overrides = json.loads(open(sys.argv[1], encoding="utf-8").read())
    omega_b = overrides.get("omega_b", 0.022)

    # --- Block 1: four dark-energy parametrizations, flat universe ---
    k_out_1 = [5e-5, 5e-4, 5e-3]
    models_1 = ["PPF1", "PPF2", "FLD1", "FLD1S"]
    w0 = {"PPF1": -0.7, "PPF2": -1.15, "FLD1": -0.7, "FLD1S": -0.7}
    wa = {"PPF1": 0.0, "PPF2": 0.5, "FLD1": 0.0, "FLD1S": 0.0}
    omega_cdm = {"PPF1": 0.104976, "PPF2": 0.120376, "FLD1": 0.104976, "FLD1S": 0.104976}
    h = {"PPF1": 0.64, "PPF2": 0.74, "FLD1": 0.64, "FLD1S": 0.64}

    block1 = {}
    for M in models_1:
        use_ppf = "no" if "FLD" in M else "yes"
        gauge = "Synchronous" if "S" in M else "Newtonian"
        c = Class()
        c.set({
            "output": "tCl mPk dTk vTk",
            "k_output_values": str(k_out_1).strip("[]"),
            "h": h[M],
            "omega_b": omega_b,
            "omega_cdm": omega_cdm[M],
            "cs2_fld": 1.0,
            "Omega_Lambda": 0.0,
            "w0_fld": w0[M],
            "wa_fld": wa[M],
            "gauge": gauge,
            "use_ppf": use_ppf,
        })
        cl = c.raw_cl()
        entry = {"cl": dump_cl(cl)}
        if M in ("PPF1", "FLD1"):
            entry["perturbations"] = dump_perturbations(c.get_perturbations()["scalar"])
        block1[M] = entry
        c.struct_cleanup()
        c.empty()

    # --- Blocks 2 & 3: curvature x gauge sweep, PPF1 vs FLD1, at two k values ---
    models_23 = ["PPF1", "FLD1"]

    def curvature_gauge_sweep(k_out):
        out = {}
        for Omega_K in (-0.1, 0.0, 0.1):
            for gauge in ("Synchronous", "Newtonian"):
                cosmo = {}
                for M in models_23:
                    use_ppf = "no" if "FLD" in M else "yes"
                    c = Class()
                    c.set({
                        "output": "tCl mPk dTk vTk",
                        "k_output_values": str(k_out).strip("[]"),
                        "h": h[M],
                        "omega_b": omega_b,
                        "omega_cdm": omega_cdm[M],
                        "Omega_k": Omega_K,
                        "cs2_fld": 1.0,
                        "w0_fld": w0[M],
                        "wa_fld": wa[M],
                        "Omega_Lambda": 0.0,
                        "gauge": gauge,
                        "use_ppf": use_ppf,
                        "hyper_sampling_curved_low_nu": 10.0 if len(k_out) == 1 and k_out[0] == 1e-3 else 6.1,
                    })
                    cosmo[M] = c
                key = f"Omega_k={Omega_K},gauge={gauge}"
                out[key] = {
                    M: {"cl": dump_cl(cosmo[M].raw_cl()),
                        "perturbations": dump_perturbations(cosmo[M].get_perturbations()["scalar"])}
                    for M in models_23
                }
                for M in models_23:
                    cosmo[M].struct_cleanup()
                    cosmo[M].empty()
        return out

    block2 = curvature_gauge_sweep([1e-3])
    block3 = curvature_gauge_sweep([1e-1])

    result = {"four_models_k5e-5_5e-4_5e-3": block1,
              "curvature_gauge_sweep_k1e-3": block2,
              "curvature_gauge_sweep_k1e-1": block3}
    json.dump(result, open(sys.argv[2], "w", encoding="utf-8"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
