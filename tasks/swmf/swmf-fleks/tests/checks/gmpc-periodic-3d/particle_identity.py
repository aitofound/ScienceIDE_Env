"""Identity-keyed comparison for unordered FLEKS particle output blocks.

The active periodic check grades the source ``log_pt`` aggregate (time/species
moments), not raw particle plotfiles. This helper is kept beside the PT check
so future raw-particle observables cannot compare AMReX emission order. Rows
are aligned by complete identity; duplicate, missing, ragged, non-finite, or
malformed records are hard failures.
"""
from __future__ import annotations
import math
from dataclasses import dataclass
from typing import Iterable

@dataclass(frozen=True)
class ParticleBlock:
    names: tuple[str, ...]
    rows: tuple[tuple[float, ...], ...]

    @property
    def identity_columns(self) -> tuple[int, ...]:
        lowered = [n.lower() for n in self.names]
        try:
            return tuple(lowered.index(n) for n in ("cpu", "id"))
        except ValueError as exc:
            raise ValueError("particle block must declare cpu and id identity columns") from exc

    def keyed(self) -> dict[tuple[float, ...], tuple[float, ...]]:
        ids = self.identity_columns
        result = {}
        for row in self.rows:
            if not all(math.isfinite(v) for v in row):
                raise ValueError("particle block contains a non-finite value")
            key = tuple(row[i] for i in ids)
            if key in result:
                raise ValueError(f"duplicate particle identity {key!r}")
            result[key] = row
        return result

def parse_block(lines: Iterable[str]) -> ParticleBlock:
    """Parse one whitespace-delimited block with a named header."""
    content = [line.strip() for line in lines if line.strip()]
    if len(content) < 2:
        raise ValueError("particle block requires a header and at least one row")
    names = tuple(content[0].split())
    if len(set(n.lower() for n in names)) != len(names):
        raise ValueError("particle block has duplicate column names")
    rows = []
    for number, line in enumerate(content[1:], 2):
        fields = line.split()
        if len(fields) != len(names):
            raise ValueError(f"particle row {number} has {len(fields)} columns, expected {len(names)}")
        try:
            row = tuple(float(token.replace("D", "E").replace("d", "e")) for token in fields)
        except ValueError as exc:
            raise ValueError(f"particle row {number} is not numeric") from exc
        rows.append(row)
    block = ParticleBlock(names, tuple(rows))
    block.identity_columns
    block.keyed()
    return block

def compare_blocks(reference: ParticleBlock, candidate: ParticleBlock, *, atol: float = 1e-12, rtol: float = 1e-3) -> tuple[bool, str]:
    """Compare blocks by identity, tolerating only row permutation."""
    if reference.names != candidate.names:
        return False, "particle block schemas differ"
    ref, cand = reference.keyed(), candidate.keyed()
    if set(ref) != set(cand):
        missing, extra = sorted(set(ref) - set(cand)), sorted(set(cand) - set(ref))
        return False, f"particle identities differ (missing={missing}, extra={extra})"
    for key in ref:
        for index, (r, c) in enumerate(zip(ref[key], cand[key])):
            if abs(c - r) > atol + rtol * abs(r):
                return False, f"particle {key} column {index} exceeds tolerance"
    return True, "identity-keyed particle rows match (permutation-tolerant)"
