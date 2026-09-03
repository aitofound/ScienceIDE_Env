#!/usr/bin/env python3
"""Build the committed SCNet summary from downloaded, hashed run evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import statistics
from datetime import datetime
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--scheduler-arrays", type=Path)
    parser.add_argument("--scheduler-preflight", type=Path)
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_manifests(root: Path) -> int:
    count = 0
    for manifest in sorted(root.rglob("SHA256SUMS")):
        count += 1
        directory = manifest.parent.resolve()
        for line in manifest.read_text().splitlines():
            expected, relative = line.split(maxsplit=1)
            relative = relative.lstrip("*")
            target = (directory / relative).resolve()
            if directory not in target.parents and target != directory:
                raise ValueError(f"manifest path escapes run directory: {target}")
            actual = sha256(target)
            if actual != expected:
                raise ValueError(f"SHA-256 mismatch for {target}")
    return count


def parse_env(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text().splitlines():
        key, value = line.split("=", 1)
        result[key] = value
    return result


def parse_davidson(path: Path) -> dict[str, Any]:
    raw: dict[str, str] = {}
    for line in path.read_text().splitlines():
        key, value = line.split(":", 1)
        raw[key.strip()] = value.strip()
    return {
        "energy_eh": float(raw["energy"]),
        "residual_norm": float(raw["residual norm"]),
        "iterations": int(raw["iterations"]),
        "storage": raw["storage"],
        "effective_sigma_mode": raw["effective sigma mode"],
        "sigma_source_blocks": int(raw["sigma source blocks"]),
        "converged": raw["converged"] == "true",
    }


def parse_wall_seconds(value: str) -> float:
    parts = [float(part) for part in value.split(":")]
    if len(parts) == 2:
        return parts[0] * 60.0 + parts[1]
    if len(parts) == 3:
        return parts[0] * 3600.0 + parts[1] * 60.0 + parts[2]
    raise ValueError(f"unrecognized GNU time value: {value}")


def parse_gnu_time(path: Path) -> dict[str, Any]:
    text = path.read_text()
    wall = re.search(
        r"^\s*Elapsed \(wall clock\) time .*?:\s*([0-9:.]+)$",
        text,
        re.MULTILINE,
    )
    cpu = re.search(
        r"^\s*Percent of CPU this job got:\s*([0-9.]+)%$",
        text,
        re.MULTILINE,
    )
    rss = re.search(
        r"^\s*Maximum resident set size \(kbytes\):\s*(\d+)$",
        text,
        re.MULTILINE,
    )
    status = re.search(
        r"^\s*Exit status:\s*(\d+)$",
        text,
        re.MULTILINE,
    )
    if not all((wall, cpu, rss, status)):
        raise ValueError(f"incomplete GNU time output: {path}")
    return {
        "wall_seconds": parse_wall_seconds(wall.group(1)),
        "cpu_percent": float(cpu.group(1)),
        "max_rss_kib": int(rss.group(1)),
        "exit_status": int(status.group(1)),
    }


def parse_verification(path: Path) -> dict[str, Any]:
    text = path.read_text()
    name = re.search(r"^system:\s+(.+)$", text, re.MULTILINE)
    rust = re.search(r"^Rust dense FCI:\s+(\S+)$", text, re.MULTILINE)
    pyscf = re.search(r"^PySCF FCI:\s+(\S+)$", text, re.MULTILINE)
    error = re.search(r"^absolute error:\s+(\S+)$", text, re.MULTILINE)
    result = re.search(r"^verification:\s+(\S+)$", text, re.MULTILINE)
    if not all((name, rust, pyscf, error, result)):
        raise ValueError(f"incomplete verification output: {path}")
    return {
        "system": name.group(1),
        "rust_fci_eh": float(rust.group(1)),
        "pyscf_fci_eh": float(pyscf.group(1)),
        "absolute_error_eh": float(error.group(1)),
        "passed": result.group(1) == "PASS",
    }


def parse_scheduler(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for line in path.read_text().splitlines():
        fields = line.split("|")
        if len(fields) != 8:
            raise ValueError(f"unexpected sacct row: {line}")
        job_id, state, exit_code, elapsed, start, end, cpus, node = fields
        rows.append(
            {
                "job_id": job_id,
                "state": state,
                "exit_code": exit_code,
                "elapsed": elapsed,
                "start": start,
                "end": end,
                "allocated_cpus": int(cpus),
                "node": node,
            }
        )
    return rows


def peak_concurrency(rows: list[dict[str, Any]]) -> dict[str, Any]:
    parsed = [
        (
            row,
            datetime.fromisoformat(row["start"]),
            datetime.fromisoformat(row["end"]),
        )
        for row in rows
    ]
    best: dict[str, Any] | None = None
    for instant in sorted({start for _, start, _ in parsed}):
        active = [
            row
            for row, start, end in parsed
            if start <= instant < end
        ]
        candidate = {
            "at": instant.isoformat(),
            "tasks": len(active),
            "cpus": sum(row["allocated_cpus"] for row in active),
            "nodes": len({row["node"] for row in active}),
            "job_ids": sorted(row["job_id"] for row in active),
        }
        if best is None or candidate["cpus"] > best["cpus"]:
            best = candidate
    if best is None:
        raise ValueError("no scheduler rows")
    return best


def aggregate(values: list[float]) -> dict[str, float]:
    return {
        "minimum": min(values),
        "median": statistics.median(values),
        "mean": statistics.fmean(values),
        "maximum": max(values),
    }


def parse_single_cases(root: Path) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    for directory in sorted(root.iterdir()):
        config = parse_env(directory / "case.env")
        result = parse_davidson(directory / "davidson.stdout")
        timing = parse_gnu_time(directory / "davidson.time")
        environment = parse_env(directory / "environment-final.env")
        cases.append(
            {
                "case_id": config["case_id"],
                "residual_tolerance": float(config["residual_tolerance"]),
                "max_subspace": int(config["max_subspace"]),
                "node": environment["hostname"],
                "exit_status": int((directory / "exit-status.txt").read_text()),
                **result,
                "timing": timing,
            }
        )
    return cases


def parse_replicate_cases(root: Path) -> list[dict[str, Any]]:
    cases: list[dict[str, Any]] = []
    for directory in sorted(root.iterdir()):
        config = parse_env(directory / "case.env")
        environment = parse_env(directory / "environment-final.env")
        samples: list[dict[str, Any]] = []
        with (directory / "summary.tsv").open(newline="") as stream:
            summary_rows = list(csv.DictReader(stream, delimiter="\t"))
        replicate_dirs = sorted(directory.glob("replicate-*"))
        if len(summary_rows) != len(replicate_dirs):
            raise ValueError(f"replicate count mismatch in {directory}")
        for summary, replicate_dir in zip(summary_rows, replicate_dirs):
            result = parse_davidson(replicate_dir / "davidson.stdout")
            timing = parse_gnu_time(replicate_dir / "davidson.time")
            if int(summary["replicate"]) != int(replicate_dir.name.split("-")[1]):
                raise ValueError(f"replicate order mismatch in {directory}")
            if float(summary["energy_eh"]) != result["energy_eh"]:
                raise ValueError(f"summary energy mismatch in {replicate_dir}")
            samples.append(
                {
                    "replicate": int(summary["replicate"]),
                    **result,
                    "timing": timing,
                }
            )
        energies = [sample["energy_eh"] for sample in samples]
        walls = [sample["timing"]["wall_seconds"] for sample in samples]
        cases.append(
            {
                "case_id": config["case_id"],
                "residual_tolerance": float(config["residual_tolerance"]),
                "max_subspace": int(config["max_subspace"]),
                "node": environment["hostname"],
                "exit_status": int((directory / "exit-status.txt").read_text()),
                "replicate_count": len(samples),
                "unique_energy_count": len(set(energies)),
                "wall_seconds": aggregate(walls),
                "samples": samples,
            }
        )
    return cases


def main() -> None:
    args = parse_args()
    root = args.evidence_root.resolve()
    scheduler_arrays = args.scheduler_arrays or root / "scheduler-arrays.tsv"
    scheduler_preflight = args.scheduler_preflight or root / "scheduler-preflight.tsv"

    manifest_count = verify_manifests(root)
    preflight_root = root / "23015273" / "build-smoke"
    single_cases = parse_single_cases(
        root / "23015277" / "davidson-array"
    )
    replicate_cases = parse_replicate_cases(
        root / "23015308" / "davidson-replicates"
    )
    scheduler_rows = parse_scheduler(scheduler_arrays)
    first_scheduler = [
        row for row in scheduler_rows if row["job_id"].startswith("23015277_")
    ]
    replicate_scheduler = [
        row for row in scheduler_rows if row["job_id"].startswith("23015308_")
    ]
    preflight_scheduler = scheduler_preflight.read_text().splitlines()

    single_energies = [case["energy_eh"] for case in single_cases]
    replicate_samples = [
        sample
        for case in replicate_cases
        for sample in case["samples"]
    ]
    replicate_energies = [
        sample["energy_eh"] for sample in replicate_samples
    ]
    replicate_walls = [
        sample["timing"]["wall_seconds"] for sample in replicate_samples
    ]
    replicate_rss = [
        float(sample["timing"]["max_rss_kib"])
        for sample in replicate_samples
    ]
    replicate_cpu = [
        sample["timing"]["cpu_percent"] for sample in replicate_samples
    ]
    environment = parse_env(preflight_root / "environment-final.env")

    output = {
        "schema_version": 1,
        "experiment": "SCNet H2O/6-31G frozen-core Davidson audit",
        "date_utc": "2026-07-30",
        "units": {
            "energy": "hartree",
            "time": "second",
            "memory": "kibibyte",
        },
        "provenance": {
            "source_commit": environment["source_commit"],
            "cargo_lock_sha256": environment["cargo_lock_sha256"],
            "fcidump_sha256": environment["fcidump_sha256"],
            "binary_sha256": environment["binary_sha256"],
            "libcint_archive_sha256": environment["libcint_archive_sha256"],
            "rustc": environment["rustc"],
            "cpu_model": environment["cpu_model"],
            "verified_sha256_manifests": manifest_count,
            "downloaded_evidence_files": sum(
                1 for path in root.rglob("*") if path.is_file()
            ),
        },
        "preflight": {
            "job_id": "23015273",
            "scheduler_rows": preflight_scheduler,
            "exit_status": int((preflight_root / "exit-status.txt").read_text()),
            "build": parse_gnu_time(preflight_root / "cargo-build.time"),
            "test": parse_gnu_time(preflight_root / "cargo-test.time"),
            "tiny_system_verifications": [
                parse_verification(preflight_root / name)
                for name in (
                    "verify-h2-equilibrium.stdout",
                    "verify-h4.stdout",
                    "verify-h2o-sto3g.stdout",
                )
            ],
            "h2o_ccpvdz_all_electron_bounded": json.loads(
                (preflight_root / "h2o-ccpvdz-ae.json").read_text()
            ),
            "primary_davidson": {
                **parse_davidson(preflight_root / "davidson.stdout"),
                "timing": parse_gnu_time(preflight_root / "davidson.time"),
            },
        },
        "robustness_array": {
            "job_id": "23015277",
            "requested_max_tasks": 18,
            "cpus_per_task": 56,
            "requested_max_cpus": 1008,
            "observed_peak": peak_concurrency(first_scheduler),
            "all_completed": all(
                row["state"] == "COMPLETED" and row["exit_code"] == "0:0"
                for row in first_scheduler
            ),
            "energy_eh": {
                **aggregate(single_energies),
                "range": max(single_energies) - min(single_energies),
            },
            "cases": single_cases,
            "scheduler": first_scheduler,
        },
        "replicate_array": {
            "job_id": "23015308",
            "requested_max_tasks": 18,
            "cpus_per_task": 56,
            "requested_max_cpus": 1008,
            "observed_peak": peak_concurrency(replicate_scheduler),
            "live_pending_reason_at_8_running_tasks": "AssocGrpCpuLimit",
            "all_completed": all(
                row["state"] == "COMPLETED" and row["exit_code"] == "0:0"
                for row in replicate_scheduler
            ),
            "sample_count": len(replicate_samples),
            "all_converged": all(
                sample["converged"] for sample in replicate_samples
            ),
            "all_case_energies_deterministic": all(
                case["unique_energy_count"] == 1 for case in replicate_cases
            ),
            "energy_eh": {
                **aggregate(replicate_energies),
                "range": max(replicate_energies) - min(replicate_energies),
            },
            "wall_seconds": aggregate(replicate_walls),
            "cpu_percent": aggregate(replicate_cpu),
            "max_rss_kib": aggregate(replicate_rss),
            "cases": replicate_cases,
            "scheduler": replicate_scheduler,
        },
        "scope": {
            "parallelism": "18-task ensemble of 56-thread shared-memory solves",
            "single_solve_mpi": False,
            "thousand_cpu_request_submitted": True,
            "thousand_cpu_observed": False,
            "limiting_reason": (
                "The live Slurm snapshot showed eight running elements and ten "
                "pending with AssocGrpCpuLimit; unrelated authorized-account "
                "jobs were not cancelled."
            ),
        },
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
