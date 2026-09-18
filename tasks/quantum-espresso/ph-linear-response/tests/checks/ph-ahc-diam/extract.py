#!/usr/bin/env python3
"""Extract graded groups from a finished QE chain into metrics.json.

Reads only the run directory and chain.json. No reference values. Units:
energy and eigenvalues in Hartree; forces Ha/Bohr; stress Ha/Bohr^3;
phonon frequencies cm^-1; dielectric and Born charges as stored in the
dynamical-matrix XML; IR/Raman/depol as printed by dynmat.x; lambda as
printed by lambda.x.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from xml.etree import ElementTree as ET


def local(tag: str) -> str:
    return tag.split("}", 1)[-1] if "}" in tag else tag


def strip_ns(root: ET.Element) -> None:
    for el in root.iter():
        el.tag = local(el.tag)


def floats_in(text: str | None) -> list[float]:
    if not text:
        return []
    out: list[float] = []
    for tok in text.replace(",", " ").split():
        try:
            out.append(float(tok))
        except ValueError:
            continue
    return out


def load_xml(path: Path) -> ET.Element:
    tree = ET.parse(path)
    root = tree.getroot()
    strip_ns(root)
    return root


def parse_schema(path: Path) -> dict:
    root = load_xml(path)
    groups: dict = {}
    etot = root.find(".//etot")
    if etot is not None and etot.text:
        groups["energy"] = [float(etot.text.strip())]
    # A ks_energies element is a physical block identified by its k point.
    # QE does not promise that independent implementations traverse those
    # blocks in the same storage order, so canonicalize the blocks before
    # flattening them for the pointwise validator.  Bands at one k point are
    # likewise a spectrum, rather than an iteration order.
    eig_blocks: list[tuple[tuple[float, float, float, float], list[float]]] = []
    for ks in root.findall(".//ks_energies"):
        ev = floats_in("" if ks.findtext("eigenvalues") is None else ks.findtext("eigenvalues"))
        occ = floats_in("" if ks.findtext("occupations") is None else ks.findtext("occupations"))
        if not ev:
            continue
        k_node = ks.find("k_point")
        k_values = floats_in(None if k_node is None else k_node.text)
        if len(k_values) < 3:
            raise ValueError("ks_energies block has no three-component k_point key")
        try:
            weight = float(k_node.get("weight", "0")) if k_node is not None else 0.0
        except ValueError as exc:
            raise ValueError("ks_energies block has a non-numeric k-point weight") from exc
        if occ and len(occ) == len(ev):
            occupied = [v for v, o in zip(ev, occ) if o > 1e-3]
        else:
            occupied = ev
        eig_blocks.append(((k_values[0], k_values[1], k_values[2], weight), sorted(occupied)))
    if eig_blocks:
        groups["eigenvalues"] = [
            value
            for _, values in sorted(eig_blocks, key=lambda block: block[0])
            for value in values
        ]
    fermi: list[float] = []
    for name in ("fermi_energy", "highestOccupiedLevel"):
        el = root.find(f".//{name}")
        if el is not None and el.text:
            fermi.append(float(el.text.strip()))
    if fermi:
        groups["fermi"] = fermi
    mag: list[float] = []
    mnode = root.find(".//magnetization")
    if mnode is not None:
        for name in ("total", "absolute"):
            el = mnode.find(name)
            if el is not None and el.text:
                mag.extend(floats_in(el.text))
        tv = mnode.find("total_vec")
        if tv is not None and tv.text:
            mag = floats_in(tv.text) + mag
    if mag:
        groups["magnetization"] = mag
    # Input control_variables/forces and /stress are booleans. The graded
    # matrices live under output/forces and output/stress (matrixType).
    out = None
    for el in root.iter():
        if local(el.tag) == "output":
            out = el
            break
    if out is not None:
        fnode = out.find("forces")
        if fnode is not None:
            fvals = floats_in(" ".join(fnode.itertext()))
            if fvals:
                groups["forces"] = fvals
        snode = out.find("stress")
        if snode is not None:
            svals = floats_in(" ".join(snode.itertext()))
            if svals:
                groups["stress"] = svals
    return groups


def parse_dyn_xml(path: Path) -> dict:
    root = load_xml(path)
    out: dict = {"q": [], "phonon": [], "dielectric": [], "born": []}
    qp = root.find(".//Q_POINT")
    if qp is not None:
        qv = floats_in(qp.text)
        if len(qv) >= 3:
            out["q"] = qv[:3]
    freqs: list[float] = []
    for el in root.iter():
        if local(el.tag).startswith("OMEGA"):
            vals = floats_in(el.text)
            if len(vals) >= 2:
                freqs.append(vals[1])
            elif len(vals) == 1:
                freqs.append(vals[0])
    out["phonon"] = freqs
    eps = root.find(".//EPSILON")
    if eps is not None:
        out["dielectric"] = floats_in(eps.text)
    born: list[float] = []
    zstar = root.find(".//ZSTAR")
    if zstar is not None:
        for el in list(zstar):
            if local(el.tag).startswith("Z_AT"):
                born.extend(floats_in(el.text))
    out["born"] = born
    return out


def is_gamma(q: list[float]) -> bool:
    return len(q) >= 3 and max(abs(v) for v in q[:3]) < 1e-8


def parse_dynmat_stdout(text: str) -> dict:
    groups: dict = {}
    freqs: list[float] = []
    # Keep every activity attached to the frequency that identifies its
    # eigenmode.  The rows are canonicalized together below; sorting the four
    # arrays independently would destroy their physical association.
    activity_modes: list[tuple[float, float | None, float | None, float | None]] = []
    in_modes = False
    for line in text.splitlines():
        if "# mode" in line:
            in_modes = True
            continue
        if not in_modes:
            continue
        parts = line.split()
        if len(parts) >= 2 and re.match(r"^[0-9]+$", parts[0]):
            try:
                freqs.append(float(parts[1]))
                fcm = float(parts[1])
            except ValueError:
                continue
            # IR-only tables have 4 columns; Raman tables have 6.
            if abs(fcm) > 1e-8:
                try:
                    ir_value = float(parts[3]) if len(parts) >= 4 else None
                    raman_value = float(parts[4]) if len(parts) >= 6 else None
                    depol_value = float(parts[5]) if len(parts) >= 6 else None
                except ValueError:
                    continue
                activity_modes.append((fcm, ir_value, raman_value, depol_value))
    if freqs:
        groups["dynmat_freqs"] = freqs
    polariz: list[float] = []
    grab = False
    for line in text.splitlines():
        if "Polarizability" in line and "A^3" in line.replace(" ", ""):
            grab = True
            continue
        if grab:
            if "multiply" in line.lower():
                continue
            parts = line.split()
            if len(parts) == 3:
                try:
                    polariz.extend(float(p) for p in parts)
                except ValueError:
                    if polariz:
                        grab = False
            elif polariz:
                grab = False
    if polariz:
        groups["polariz"] = polariz
    activity_modes.sort(
        key=lambda mode: tuple(float("inf") if value is None else value for value in mode)
    )
    ir = [mode[1] for mode in activity_modes if mode[1] is not None]
    raman = [mode[2] for mode in activity_modes if mode[2] is not None]
    depol = [mode[3] for mode in activity_modes if mode[3] is not None]
    if ir:
        groups["ir"] = ir
    if raman:
        groups["raman"] = raman
    if depol:
        groups["depol"] = depol
    return groups


def parse_matdyn_freq(path: Path) -> list[float]:
    """Parse matdyn.x flfrq files: '&plot nbnd=N, nks=M /' then per q a
    4-float q+weight line and one or more 6f10.4 frequency rows (cm^-1)."""
    freqs: list[float] = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    i = 0
    while i < len(lines) and "&plot" not in lines[i] and "nbnd=" not in lines[i]:
        i += 1
    if i < len(lines):
        i += 1
    else:
        i = 0
    while i < len(lines):
        line = lines[i].strip()
        i += 1
        if not line or line.startswith("#"):
            continue
        vals = floats_in(line)
        if not vals:
            continue
        if len(vals) == 4:
            # q1 q2 q3 weight
            continue
        freqs.extend(vals)
    return freqs


def parse_postahc_stdout(text: str) -> list[tuple[int, int, float, float]]:
    """Rows of the 'Begin postahc output ... End postahc output' table:
    ik ibnd Re[Total] Upper_DW Lower_DW Upper_Fan Re[Lower_Fan] Im[Total].
    (ik, ibnd) are the physical identity (k-point index and band index of
    the fixed, explicit NSCF k-point list read from stdin); this is stable
    across implementations because the k-point list is an explicit deck
    input, not a solver-chosen order.
    """
    rows: list[tuple[int, int, float, float]] = []
    in_table = False
    for line in text.splitlines():
        if "Begin postahc output" in line:
            in_table = True
            continue
        if "End postahc output" in line:
            break
        if not in_table:
            continue
        parts = line.split()
        if len(parts) != 8:
            continue
        try:
            ik = int(parts[0])
            ibnd = int(parts[1])
            re_total = float(parts[2])
            im_total = float(parts[7])
        except ValueError:
            continue
        rows.append((ik, ibnd, re_total, im_total))
    return rows


def parse_lambda_stdout(text: str) -> list[float]:
    out: list[float] = []
    for line in text.splitlines():
        if "lambda =" in line or "lambda=" in line:
            out.extend(floats_in(line.split("=", 1)[-1] if "lambda=" in line else line))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--run-dir", required=True)
    ap.add_argument("--chain", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    run = Path(a.run_dir)
    chain = json.loads(Path(a.chain).read_text(encoding="utf-8"))
    wanted = list(chain["groups"])
    metrics: dict = {}
    gs_names = ("energy", "eigenvalues", "fermi", "forces", "stress", "magnetization")
    gs_wanted = [g for g in wanted if g in gs_names]
    snapshot = run / "scf-data-file-schema.xml"
    if gs_wanted:
        # The nscf/nscf.nosym steps share the scf prefix/outdir and overwrite
        # diam.save/data-file-schema.xml. Ground-state groups come from the
        # snapshot run.sh took immediately after the scf JOB DONE.
        if not snapshot.is_file():
            raise SystemExit(
                "extract.py: missing scf-data-file-schema.xml snapshot in the run "
                "directory; run.sh must copy the scf XML before any nscf step "
                "overwrites diam.save/data-file-schema.xml"
            )
        parsed = parse_schema(snapshot)
        for k, v in parsed.items():
            if k in wanted:
                metrics[k] = v
    else:
        for prefix in chain.get("scf_prefixes", []):
            schema = run / f"{prefix}.save" / "data-file-schema.xml"
            if not schema.is_file():
                # QE sometimes writes outdir/prefix.save
                hits = list(run.glob(f"**/{prefix}.save/data-file-schema.xml"))
                schema = hits[0] if hits else schema
            if schema.is_file():
                parsed = parse_schema(schema)
                for k, v in parsed.items():
                    if k in wanted:
                        metrics[k] = v

    phonon: list[float] = []
    phonon_ac: list[float] = []
    dielectric: list[float] = []
    born: list[float] = []
    n_ac = int(chain.get("n_acoustic", 0))
    dyn_files = sorted(run.glob("*.xml"))
    dyn_files += sorted(p for p in run.glob("*.xml.*") if p.is_file())
    seen = set()
    for p in dyn_files:
        if p.name == "data-file-schema.xml" or "data-file-schema" in p.name:
            continue
        # QE also writes prefix.xml as a copy of the SCF schema; skip it.
        if p.name.endswith(".xml") and not any(tok in p.name for tok in ("dyn", "frc")):
            head = p.read_text(encoding="utf-8", errors="replace")[:400]
            if "qes:" in head or "data-file-schema" in head or "<espresso" in head:
                continue
        key = str(p.resolve())
        if key in seen:
            continue
        seen.add(key)
        try:
            block = parse_dyn_xml(p)
        except ET.ParseError:
            continue
        if not block["phonon"] and not block["dielectric"] and not block["born"]:
            continue
        freqs = sorted(block["phonon"])
        if n_ac and is_gamma(block["q"]) and len(freqs) >= n_ac:
            phonon_ac.extend(freqs[:n_ac])
            phonon.extend(freqs[n_ac:])
        else:
            phonon.extend(freqs)
        if block["dielectric"]:
            dielectric = block["dielectric"]
        if block["born"]:
            born = block["born"]
    if "phonon" in wanted and phonon:
        metrics["phonon"] = sorted(phonon) if not n_ac else phonon
    if "phonon_acoustic" in wanted and phonon_ac:
        metrics["phonon_acoustic"] = phonon_ac
    if "dielectric" in wanted and dielectric:
        metrics["dielectric"] = dielectric
    if "born" in wanted and born:
        metrics["born"] = born

    for log_name, parser, key in (
        ("dynmat.out", parse_dynmat_stdout, None),
        ("lambda.out", None, "lambda"),
    ):
        p = run / log_name
        if not p.is_file():
            continue
        text = p.read_text(encoding="utf-8", errors="replace")
        if log_name == "dynmat.out":
            parsed = parse_dynmat_stdout(text)
            for g in ("ir", "raman", "depol", "polariz", "dynmat_freqs"):
                if g in wanted and g in parsed:
                    metrics[g] = parsed[g]
        else:
            vals = parse_lambda_stdout(text)
            if "lambda" in wanted and vals:
                metrics["lambda"] = vals

    ahc_rows: list[tuple[int, int, float, float]] = []
    for fi, spec in enumerate(chain.get("postahc_files", []), start=1):
        p = run / spec
        if not p.is_file():
            continue
        text_p = p.read_text(encoding="utf-8", errors="replace")
        for ik, ibnd, re_total, im_total in parse_postahc_stdout(text_p):
            ahc_rows.append((fi, ik, ibnd, re_total, im_total))
    if "ahc_selfen" in wanted and ahc_rows:
        ahc_rows.sort(key=lambda r: (r[0], r[1], r[2]))
        metrics["ahc_selfen"] = [v for row in ahc_rows for v in (row[3], row[4])]

    for spec in chain.get("matdyn_freq_files", []):
        p = run / spec
        if p.is_file() and "phonon_interpolated" in wanted:
            freqs = parse_matdyn_freq(p)
            if freqs:
                metrics.setdefault("phonon_interpolated", []).extend(freqs)

    missing = [g for g in wanted if g not in metrics]
    if missing:
        raise SystemExit(f"extract.py: missing graded groups {missing}; have {sorted(metrics)}")
    Path(a.out).write_text(json.dumps(metrics, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
