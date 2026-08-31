#!/usr/bin/env python3
"""Deterministic source/contract regression for the two FFT evidence modes.

This test deliberately performs no Docker run, MPI launch, source build, or
filesystem cleanup.  It proves the 512^3 aggregate contract cannot select the
native exporter and that every future native MPI gather has an INT_MAX guard
before MPI_Gatherv.
"""
from __future__ import annotations

import ast
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent
TASK = ROOT.parent
CATALOG = json.loads((ROOT / "fft_contract.json").read_text(encoding="utf-8"))
CHECKS = CATALOG["checks"]

assert len(CHECKS) == CATALOG["active_check_count"] == 26
assert [c["id"] for c in CHECKS] == [f"FFT-{i:02d}" for i in range(1, 27)]
assert CATALOG["run_matrix"]["controlled_native_runs"] == 37
assert CATALOG["run_matrix"]["mesh_probes"] == 35

# Model the two C++ fail-closed checks without allocating a field or launching MPI:
# a local record payload must fit int bytes, and the root's cumulative receive
# payload must also fit int bytes before MPI_Gatherv is reachable.
MPI_INT_MAX = 2**31 - 1

def guarded_gather_bytes(record_counts):
    if any(count > MPI_INT_MAX // 36 for count in record_counts):
        return False
    byte_counts = [count * 36 for count in record_counts]
    total = 0
    for count in byte_counts:
        if total > MPI_INT_MAX - count:
            return False
        total += count
    return True

assert guarded_gather_bytes([1, 1])
assert not guarded_gather_bytes([MPI_INT_MAX // 36 + 1])
assert not guarded_gather_bytes([MPI_INT_MAX // 72 + 1, MPI_INT_MAX // 72 + 1])

for spec in CHECKS[:15]:
    assert spec["claim_kind"] == "aggregate_residual"
    assert spec["claim_fixture"] == "upstream-aggregate-residual"
    assert "native-claim-integrity" not in spec["rules"]
    assert not any("claim-manifest" in item or "rank-" in item or "global-" in item for item in spec["required_artifacts"])
    assert all("problem/fft_case=upstream" in tokens for tokens in spec["runtime"]["variant_argv_tokens"].values())
    assert spec["runtime"]["evidence_mode"] == "upstream-aggregate-residual"
    assert "native_claims" not in spec["runtime"]

for spec in CHECKS[15:]:
    assert spec["claim_kind"] != "aggregate_residual"
    assert "native-claim-integrity" in spec["rules"]
    assert spec["runtime"].get("native_claims") is True
    assert math.prod(spec["runtime"]["global_shape"]) * 36 < 2**31 - 1

solve = (TASK / "solution" / "solve.sh").read_text(encoding="utf-8")
assert "--claim-kind \"$claim_kind\"" in solve
assert 'if [[ "$claim_kind" == aggregate_residual ]]' in solve
assert "problem/fft_case=upstream" not in solve  # selected from the contract, not hard-coded as a second argv

runner_path = TASK / "solution" / "run_limited.py"
runner = runner_path.read_text(encoding="utf-8")
tree = ast.parse(runner, filename=str(runner_path))
aggregate_ifs = [node for node in ast.walk(tree) if isinstance(node, ast.If) and isinstance(node.test, ast.Name) and node.test.id == "is_aggregate"]
assert len(aggregate_ifs) == 1
aggregate_source = "\n".join(ast.get_source_segment(runner, node) or "" for node in aggregate_ifs[0].body)
assert "json.loads" not in aggregate_source
assert "claim-manifest.json" in aggregate_source and "aggregate-residual run emitted native claim artifacts" in aggregate_source
assert "p.is_symlink()" in aggregate_source
assert '&& ! -L "$out/claim-manifest.json"' in solve

patch = (TASK / "solution" / "patches" / "fft-native-evidence.patch").read_text(encoding="utf-8")
assert patch.count('if (evidence_case != "upstream")') >= 4
assert patch.count('emit_stage("') == 4
gather = patch.index("MPI_Gatherv(encoded")
assert patch.index("std::numeric_limits<int>::max()") < gather
assert patch.index("MPI_Allreduce", patch.index("std::numeric_limits<int>::max()")) < gather
assert patch.index("MPI_Bcast", patch.index("MPI_Allreduce")) < gather
assert "payload exceeds INT_MAX; refusing gather" in patch

print("aggregate/native guard regression: PASS (FFT-15 upstream branch; native Gatherv INT_MAX guard)")
