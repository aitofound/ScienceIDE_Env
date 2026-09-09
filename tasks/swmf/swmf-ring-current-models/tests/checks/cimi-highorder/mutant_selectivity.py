#!/usr/bin/env python3
"""Bounded local schema/mutant tests; never builds, solves, or uses remote data.

The fixture uses the source contract's dimensions but tiny textual numeric tokens.
It proves the independent checker rejects each supported local mutation; the
seven production-variant outputs still belong to parent-authorized calibration.
"""
from __future__ import annotations
import hashlib, json, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
RUBRIC = json.loads((HERE / "rubric.json").read_text(encoding="utf-8"))
SCORED = ["CimiFlux_h.fls", "CimiFlux_o.fls", "CimiFlux_e.fls"]
DIAGNOSTICS = ["CIMI.log"]
EXPECTED = SCORED + DIAGNOSTICS
SPECIES = {"CimiFlux_h.fls": 1.0, "CimiFlux_o.fls": 2.0, "CimiFlux_e.fls": 3.0}
LOG_COLUMNS = "it t dst RbSumH RcSumH HpDrift HpBfield HpChargeEx HpWaves HpStrongDiff HpDecay HpLossCone HpFLC HpDriftIn HpDriftOut RbSumO RcSumO OpDrift OpBfield OpChargeEx OpWaves OpStrongDiff OpDecay OpLossCone OpFLC OpDriftIn OpDriftOut RbSume RcSume eDrift eBfield eChargeEx eWaves eStrongDiff eDecay eLossCone eFLC eDriftIn eDriftOut".split()


def render_flux(species: float, *, energy_shift: bool = False, pitch_shift: bool = False,
                scale: float = 1.0, localized: bool = False) -> str:
    energy = [float(i) for i in range(1, 16)]
    pitch = [float(i) for i in range(1, 19)]
    if energy_shift:
        energy[1] = 2.5
    if pitch_shift:
        pitch[1] = 2.5
    lines = ["0.0 75 48 15 18 0 ! rc in Re,nr,ip,je,ig,ntime",
             " ".join(str(x) for x in energy), " ".join(str(x) for x in pitch),
             " ".join(str(float(i)) for i in range(1, 76))]
    cell_count = 15 * 18
    for iframe, hours in enumerate(i / 60.0 for i in range(16)):
        lines.append(" ".join([str(hours)] + ["0.0"] * 11))
        for ilat in range(75):
            for ilon in range(48):
                coords = [float(ilat + 1), float(ilon) / 2.0, float(ilat), float(ilon),
                          float(iframe), float(ilat * 48 + ilon)]
                values = [species * scale] * cell_count
                if localized and iframe == 1 and ilat == 37 and ilon == 24:
                    values[0] = 9.0e3
                lines.append(" ".join(str(x) for x in coords + values))
    return "\n".join(lines) + "\n"


def render_log() -> str:
    row0 = [0.0] * len(LOG_COLUMNS)
    row1 = [0.0] * len(LOG_COLUMNS)
    row1[0], row1[1] = 1.0, 60.0
    return "CIMI Logfile\n" + " ".join(LOG_COLUMNS) + "\n" + "\n".join(
        " ".join(str(x) for x in row) for row in (row0, row1)
    ) + "\n"


def write_manifest(root: Path) -> None:
    files = {}
    for name in EXPECTED:
        p = root / name
        st = p.stat()
        files[name] = {"bytes": st.st_size, "mtime_ns": st.st_mtime_ns,
                       "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
    (root / "run-manifest.json").write_text(json.dumps({
        "schema": "cimi-highorder-run-manifest-v2", "initial_condition": "nominal",
        "scored_files": SCORED, "diagnostic_files": DIAGNOSTICS,
        "expected_files": EXPECTED, "files": files}, sort_keys=True), encoding="utf-8")


def write_fixture(root: Path, **kwargs) -> None:
    for name, species in SPECIES.items():
        (root / name).write_text(render_flux(species, **kwargs), encoding="utf-8")
    (root / "CIMI.log").write_text(render_log(), encoding="utf-8")
    write_manifest(root)


def result(ref: Path, cand: Path):
    out = cand / "result.json"
    proc = subprocess.run([sys.executable, str(HERE / "validate.py"), "--reference", str(ref),
                           "--candidate", str(cand), "--rubric", str(HERE / "rubric.json"), "--out", str(out)],
                          capture_output=True, text=True, check=False)
    return proc, json.loads(out.read_text(encoding="utf-8")) if out.is_file() else {}


def assert_rejected(ref: Path, cand: Path, fragment: str) -> None:
    _proc, doc = result(ref, cand)
    assert doc.get("passed") is False, f"mutation unexpectedly passed: {fragment}"
    assert fragment in doc.get("reason", ""), (fragment, doc.get("reason", ""))


def main():
    with tempfile.TemporaryDirectory(prefix="cimi-highorder-mutants-") as td:
        base = Path(td); ref, cand = base / "ref", base / "cand"; ref.mkdir(); cand.mkdir()
        write_fixture(ref); write_fixture(cand)
        _proc, doc = result(ref, cand)
        assert doc.get("passed") is True, doc

        # Species omission and stale/missing output fail at the manifest gate.
        (cand / "CimiFlux_h.fls").unlink()
        assert_rejected(ref, cand, "manifest")
        write_fixture(cand)
        (cand / "CimiFlux_e.fls").write_text("stale", encoding="utf-8")
        assert_rejected(ref, cand, "manifest hash")

        # Rebuild each mutated candidate manifest so these are checker tests,
        # not merely stale-file tests.
        write_fixture(cand)
        h, o = cand / "CimiFlux_h.fls", cand / "CimiFlux_o.fls"
        h_text, o_text = h.read_text(encoding="utf-8"), o.read_text(encoding="utf-8")
        h.write_text(o_text, encoding="utf-8"); o.write_text(h_text, encoding="utf-8"); write_manifest(cand)
        assert_rejected(ref, cand, "CimiFlux_h.fls")  # species swap is pointwise, not a total-only check

        write_fixture(cand, energy_shift=True); assert_rejected(ref, cand, "schema axes differ")
        write_fixture(cand, pitch_shift=True); assert_rejected(ref, cand, "schema axes differ")
        write_fixture(cand, scale=1000.0); assert_rejected(ref, cand, "values exceed")
        write_fixture(cand, localized=True); assert_rejected(ref, cand, "values exceed")

    mutant_ids = {x["id"] for x in RUBRIC["selectivity_mutants"]}
    required = {"species-omission", "species-swap", "stale-output", "energy-axis", "pitch-axis", "unit-scale", "localized-value"}
    assert required <= mutant_ids
    variants = {
        "cimi-dipole": ("quiet_h.fin", "1.260000002e+08", "1.512e+08"),
        "cimi-drift": ("quiet_h.fin", "1.260000002e+08", "1.512e+08"),
        "cimi-flux": ("quiet_h.fin", "1.260000002e+08", "1.512e+08"),
        "swpc-cimi-init": ("PARAM.in", "28.00000002", "35.0"),
        "swpc-cimi-restart": ("PARAM.in", "28.00000002", "35.0"),
        "swpc-cimi-species-init": ("PARAM.in", "28.00000002", "35.0"),
        "swpc-cimi-species-restart": ("PARAM.in", "28.00000002", "35.0"),
    }
    for check, (name, old, new) in variants.items():
        v = HERE.parent / check / "ic" / "variant" / ("imdata/" + name if name.endswith(".fin") else name)
        assert new in v.read_text(encoding="utf-8"), f"{check}: stronger variant missing"
        assert old not in v.read_text(encoding="utf-8"), f"{check}: ineffective probe remains"
        run = (HERE.parent / check / "run.sh").read_text(encoding="utf-8")
        assert 'cp -R "$CHECK_DIR/ic/$INPUTS/imdata/.' in run, f"{check}: staging not explicit"
    print("mutant_selectivity: baseline schema, omission/swap/stale/axis/unit/localized gates, and seven staging mutations passed")


if __name__ == "__main__":
    main()
