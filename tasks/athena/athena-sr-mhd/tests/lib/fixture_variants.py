#!/usr/bin/env python3
"""Named fixture trees and adversarial mutations for the v4 evidence boundary.

``write_tree`` produces the per-check discrimination set used by
``tests/negative_fixtures.py --gate``; ``write_attacks`` adds the byte-level
attack matrix used by ``tests/adversarial_probe.py``.  Every mutation is built
by copying a valid bundle and then writing bytes: nothing is deleted, and a
variant that must omit a file is produced by copying everything except that
file.  ``accept*`` trees must pass in the stated mode, ``reject*`` trees must
always fail.
"""
from __future__ import annotations

import shutil
import struct
from pathlib import Path
from typing import Any

import contract_tools as ct
import fixture_synth as fs


def copy_bundle(source: Path, target: Path, exclude: set[str] | None = None) -> Path:
    """Copy a bundle, optionally leaving one relative path out (never deleting)."""
    exclude = exclude or set()
    target.mkdir(parents=True, exist_ok=True)
    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source).as_posix()
        if relative in exclude or any(relative.startswith(item + "/") for item in exclude):
            continue
        if path.is_dir():
            (target / relative).mkdir(parents=True, exist_ok=True)
        else:
            (target / relative).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target / relative)
    return target


def first_case(contract: dict[str, Any], expectation: str = "run") -> dict[str, Any] | None:
    for case in contract["cases"]:
        if case["expectation"] == expectation:
            return case
    return None


def native_output_of(bundle: Path, case_id: str, suffix: str | None = None) -> Path | None:
    """The last-frame native output, or the native error report for error-only cases."""
    directory = bundle / "raw" / "cases" / case_id / "outputs"
    if not directory.is_dir():
        return None
    for path in sorted(directory.iterdir()):
        if ".00001." not in path.name:
            continue
        if suffix is None or path.name.endswith(suffix):
            return path
    error = directory / "linearwave-errors.dat"
    return error if suffix is None and error.is_file() else None


def perturb_last_value(path: Path | None, factor: float) -> bool:
    """Scale the last value of the last native record, in TAB text or VTK binary."""
    if path is None:
        return False
    if path.name.endswith(".tab"):
        lines = path.read_text(encoding="utf-8").splitlines()
        tokens = lines[-1].split()
        value = float(tokens[-1])
        tokens[-1] = "%24.16e" % (value * factor if value != 0.0 else (factor - 1.0) or 1e-6)
        lines[-1] = " ".join(tokens)
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return True
    if path.name == "linearwave-errors.dat":
        lines = path.read_text(encoding="utf-8").splitlines()
        tokens = lines[-1].split()
        value = float(tokens[4])
        tokens[4] = "%.16e" % (value * factor if value != 0.0 else (factor - 1.0) or 1e-6)
        lines[-1] = "  ".join(tokens)
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return True
    if path.name.endswith(".vtk"):
        # float32 storage: a 1e-12 nudge would round away, and a zero value
        # cannot be scaled at all, so fall back to a small absolute offset.
        data = bytearray(path.read_bytes())
        original = bytes(data[-5:-1])
        value = struct.unpack(">f", original)[0]
        for candidate in (value * factor, value * (1.0 + 1e-4), value + (factor - 1.0), value + 1e-6):
            scaled = struct.pack(">f", candidate)
            if scaled != original:
                data[-5:-1] = scaled
                path.write_bytes(bytes(data))
                return True
        return False
    return False


def perturb_within_bound(path: Path | None, relative: float = 1e-6) -> bool:
    """A perturbation no pinned rule can see: the last TAB value, or the first VTK density.

    The last VTK component is a magnetic field column whose pinned shock
    tolerance is exact zero, so the density channel is used instead.
    """
    if path is None:
        return False
    if path.name.endswith(".tab"):
        return perturb_last_value(path, 1.0 + 1e-12)
    if path.name == "linearwave-errors.dat":
        return perturb_last_value(path, 1.0 + relative)
    if path.name.endswith(".vtk"):
        data = bytearray(path.read_bytes())
        marker = b"SCALARS dens float\nLOOKUP_TABLE default\n"
        at = data.find(marker)
        if at < 0:
            return False
        at += len(marker)
        value = struct.unpack(">f", bytes(data[at:at + 4]))[0]
        scaled = struct.pack(">f", value * (1.0 + relative))
        if scaled == bytes(data[at:at + 4]):
            return False
        data[at:at + 4] = scaled
        path.write_bytes(bytes(data))
        return True
    return False


def truncate_native_output(path: Path | None) -> bool:
    """Remove the last native record (a row, or the last VTK float) without deleting the file."""
    if path is None:
        return False
    if path.name.endswith(".tab"):
        lines = path.read_text(encoding="utf-8").splitlines()
        path.write_text("\n".join(lines[:-1]) + "\n", encoding="utf-8")
        return True
    if path.name == "linearwave-errors.dat":
        lines = path.read_text(encoding="utf-8").splitlines()
        path.write_text("\n".join(lines[:-1]) + "\n", encoding="utf-8")
        return True
    if path.name.endswith(".vtk"):
        data = path.read_bytes()
        path.write_bytes(data[:-5])
        return True
    return False


def write_tree(root: Path, contract: dict[str, Any], source: Path, check_dir: Path) -> list[str]:
    """The per-check discrimination set (kept small enough to run for every check)."""
    root.mkdir(parents=True, exist_ok=True)
    names: list[str] = []
    reference = fs.write_bundle(root / "reference", contract, source, check_dir, variant="accept", identity=fs.fixture_identity("reference"))
    names.append("reference")
    fs.write_bundle(root / "accept-identity", contract, source, check_dir, variant="accept", identity=fs.fixture_identity("candidate"))
    names.append("accept-identity")
    grounded = bool(contract["rules"])

    # A retained native value perturbed far below every pinned bound: not
    # identical to the oracle, still inside the pinned tolerance.
    within = copy_bundle(root / "accept-identity", root / "within")
    case = first_case(contract)
    perturbed = case is not None and perturb_within_bound(native_output_of(within, case["id"]))
    if perturbed:
        fs.rebind_case(within, contract, case["id"])
        name = "accept-within-bound" if grounded else "reject-owner-pending-non-identical"
        copy_bundle(within, root / name)
        names.append(name)

    if grounded:
        fs.write_bundle(root / "reject-out-of-bound", contract, source, check_dir, variant="out-of-bound", identity=fs.fixture_identity("out-of-bound"))
        names.append("reject-out-of-bound")

    # Metadata-only: the index without any retained bytes.
    metadata = root / "reject-metadata-only"
    metadata.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(root / "accept-identity" / "observables.json", metadata / "observables.json")
    names.append("reject-metadata-only")

    # A forged index summary over untouched raw bytes.
    forged = copy_bundle(root / "accept-identity", root / "reject-forged-index-summary")
    document = fs.load(forged / "observables.json")
    for record in document["cases"]:
        if record["evidence"]["frames"]:
            frame = record["evidence"]["frames"][-1]
            name = frame["variables"][0]
            frame["stats"][name] = dict(frame["stats"][name], max=frame["stats"][name]["max"] * 1.5)
            break
    else:
        document["cases"][0]["runtime"]["exit_status"] = 1
    fs.store(forged / "observables.json", document)
    names.append("reject-forged-index-summary")

    # A copy of the reference tree presented as the second execution.
    copy_bundle(root / "reference", root / "reject-copied-run")
    names.append("reject-copied-run")

    (root / "reject-malformed").mkdir(parents=True, exist_ok=True)
    (root / "reject-malformed" / "observables.json").write_text('{"schema":', encoding="utf-8")
    (root / "reject-malformed" / "raw").mkdir(parents=True, exist_ok=True)
    (root / "reject-malformed" / "raw" / "source").mkdir(parents=True, exist_ok=True)
    (root / "reject-malformed" / "raw" / "source" / "manifest.json").write_text("{}\n", encoding="utf-8")
    names.append("reject-malformed")
    (root / "reject-missing").mkdir(parents=True, exist_ok=True)
    names.append("reject-missing")
    del reference
    return names


def write_attacks(root: Path, contract: dict[str, Any], source: Path, check_dir: Path) -> list[str]:
    """Synthesise a valid pair and then the byte-level attack matrix."""
    root.mkdir(parents=True, exist_ok=True)
    fs.write_bundle(root / "reference", contract, source, check_dir, variant="accept", identity=fs.fixture_identity("reference"))
    fs.write_bundle(root / "valid", contract, source, check_dir, variant="accept", identity=fs.fixture_identity("candidate"))
    return mutations(root, contract)


def mutations(root: Path, contract: dict[str, Any]) -> list[str]:
    """Mutate the bundles at root/valid and root/reference; both may be real oracle output."""
    names: list[str] = ["valid"]
    run_case = first_case(contract)
    rejection_case = first_case(contract, "rejection")

    def variant(name: str, exclude: set[str] | None = None) -> Path:
        names.append(name)
        return copy_bundle(root / "valid", root / name, exclude)

    # 1. an omitted native output file, with the index rebound around the loss
    if run_case is not None:
        target = native_output_of(root / "valid", run_case["id"])
        if target is not None:
            relative = target.relative_to(root / "valid").as_posix()
            bundle = variant("reject-omitted-native-file", {relative})
            document = fs.load(bundle / "observables.json")
            entry = document["raw_evidence"]["cases"][run_case["id"]]
            entry["outputs"] = [item for item in entry["outputs"] if not item["path"].endswith(target.name)]
            fs.store(bundle / "observables.json", document)
            fs.rebind_case(bundle, contract, run_case["id"])

        # 2. an extra native file that the index does not cover
        bundle = variant("reject-extra-unindexed-file")
        (bundle / "raw" / "cases" / run_case["id"] / "outputs" / "fixture.block0.out9.00000.tab").write_text("# stray\n", encoding="utf-8")

        # 3. an extra native file that the index does cover
        bundle = variant("reject-extra-indexed-file")
        (bundle / "raw" / "cases" / run_case["id"] / "outputs" / "extra-notes.txt").write_text("stray evidence\n", encoding="utf-8")
        fs.rebind_case(bundle, contract, run_case["id"])

        # 4. altered raw output with the index left untouched
        bundle = variant("reject-altered-output-unrebound")
        perturb_last_value(native_output_of(bundle, run_case["id"]), 1.05)

        # 5. altered raw output with every digest rebound (execution record and index)
        bundle = variant("reject-altered-output-rebound-truncated")
        truncate_native_output(native_output_of(bundle, run_case["id"]))
        fs.rebind_case(bundle, contract, run_case["id"])

        # 6. forged stdout termination block (zone-cycles no longer cycles x cells)
        bundle = variant("reject-forged-termination")
        stdout = bundle / "raw" / "cases" / run_case["id"] / "stdout.log"
        text = stdout.read_text(encoding="utf-8")
        line = [item for item in text.splitlines() if item.startswith("zone-cycles = ")][0]
        stdout.write_text(text.replace(line, "zone-cycles = %d" % (int(line.split("=")[1]) + 7)), encoding="utf-8")
        fs.rebind_case(bundle, contract, run_case["id"])

        # 7. a fatal marker in a normal run that still exited zero
        bundle = variant("reject-fatal-exit-zero")
        stdout = bundle / "raw" / "cases" / run_case["id"] / "stdout.log"
        stdout.write_text(stdout.read_text(encoding="utf-8") + "### FATAL ERROR in fixture\nsomething went wrong after the frames\n", encoding="utf-8")
        fs.rebind_case(bundle, contract, run_case["id"])

        # 8. substituted deck bytes
        bundle = variant("reject-substituted-deck")
        deck = next((bundle / "raw" / "cases" / run_case["id"] / "deck").iterdir())
        deck.write_text("<comment>\nproblem = substituted deck\n" + deck.read_text(encoding="utf-8"), encoding="utf-8")
        fs.rebind_case(bundle, contract, run_case["id"])

        # 9. two cases' raw trees swapped
        if len(contract["cases"]) > 1:
            bundle = variant("reject-case-swap")
            first, second = contract["cases"][0]["id"], contract["cases"][1]["id"]
            root_cases = bundle / "raw" / "cases"
            shutil.move(str(root_cases / first), str(root_cases / (first + ".swap")))
            shutil.move(str(root_cases / second), str(root_cases / first))
            shutil.move(str(root_cases / (first + ".swap")), str(root_cases / second))
            fs.rebind_index(bundle)

        # 10. stale/edited pinned-source manifest
        bundle = variant("reject-stale-source-manifest")
        manifest = fs.load(bundle / "raw" / "source" / "manifest.json")
        manifest["files"][0]["sha256"] = fs.fixture_sha("stale-source")
        manifest["tree_sha256"] = ct.tree_digest(manifest["files"])
        fs.store(bundle / "raw" / "source" / "manifest.json", manifest)
        fs.rebind_index(bundle)

        # 11. build evidence that does not describe the contract build
        bundle = variant("reject-wrong-build-evidence")
        role = run_case["binary_role"]
        record = fs.load(bundle / "raw" / "build" / f"{role}.json")
        record["configure_argv"] = record["configure_argv"] + ["--flux=roe"]
        fs.store(bundle / "raw" / "build" / f"{role}.json", record)
        fs.rebind_index(bundle)

        # 12. build evidence whose log is empty
        bundle = variant("reject-empty-build-log")
        (bundle / "raw" / "build" / f"{role}.log").write_text("", encoding="utf-8")
        record = fs.load(bundle / "raw" / "build" / f"{role}.json")
        record["log_sha256"] = ct.sha256_bytes(b"")
        fs.store(bundle / "raw" / "build" / f"{role}.json", record)
        fs.rebind_index(bundle)

        # 13. the run identity of one case rewritten (mixed producing runs)
        bundle = variant("reject-mixed-run-identity")
        execution = fs.load(bundle / "raw" / "cases" / run_case["id"] / "execution.json")
        execution["run_nonce"] = fs.fixture_identity("other")["nonce"]
        fs.store(bundle / "raw" / "cases" / run_case["id"] / "execution.json", execution)
        document = fs.load(bundle / "observables.json")
        for record in document["cases"]:
            if record["id"] == run_case["id"]:
                record["runtime"] = execution
        fs.store(bundle / "observables.json", document)
        fs.rebind_index(bundle)

        # 14. a forged error-file row (cycle count no longer the native cycle)
        if run_case["error_file"]:
            bundle = variant("reject-forged-error-file")
            errors = bundle / "raw" / "cases" / run_case["id"] / "outputs" / "linearwave-errors.dat"
            lines = errors.read_text(encoding="utf-8").splitlines()
            tokens = lines[1].split()
            tokens[3] = str(int(tokens[3]) + 5)
            errors.write_text(lines[0] + "\n" + "  ".join(tokens) + "\n", encoding="utf-8")
            fs.rebind_case(bundle, contract, run_case["id"])

    # 15. a forged deterministic rejection: the marker is claimed, the streams do not carry it
    if rejection_case is not None:
        bundle = variant("reject-forged-rejection")
        stderr = bundle / "raw" / "cases" / rejection_case["id"] / "stderr.log"
        stderr.write_text("terminating with uncaught exception: ### FATAL ERROR in fixture\nsome unrelated message\n", encoding="utf-8")
        # rebind_case also refreshes the execution record's stream digests and
        # fatal text, so the bundle stays internally consistent and only the
        # declared rejection marker is missing from the retained streams.
        fs.rebind_case(bundle, contract, rejection_case["id"])

    # 16. the reference tree copied and presented as the second execution
    names.append("reject-copied-run")
    copy_bundle(root / "reference", root / "reject-copied-run")
    return names
