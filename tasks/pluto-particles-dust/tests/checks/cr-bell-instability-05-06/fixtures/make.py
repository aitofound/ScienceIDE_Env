#!/usr/bin/env python3
"""Deterministic discrimination fixtures for the grouped Bell check.

The grouped validator delegates to the two shipped Bell subrun validators. Use
small synthetic output trees from the already-curated active predicate fixture;
this exercises both delegation and the real byte-level acceptance/rejection
logic without running PLUTO or claiming a Bell science result.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys


def load_fixture_builder():
    checks = Path(__file__).resolve().parents[2]
    path = checks / 'cr-gyration-01' / 'fixtures' / 'make.py'
    spec = importlib.util.spec_from_file_location('active_fixture_builder', path)
    if spec is None or spec.loader is None:
        raise SystemExit('cannot load active fixture builder: ' + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main(argv):
    if len(argv) != 2:
        raise SystemExit('usage: make.py OUTDIR')
    out = Path(argv[1])
    out.mkdir(parents=True, exist_ok=True)
    builder = load_fixture_builder()
    reference = [builder.base_state(k) for k in range(builder.NFRAME)]
    particles = [builder.base_particles(k) for k in range(builder.NFRAME)]
    rejected = builder.perturbed(reference, 1.0, only='prs')

    for label, frames in (
        ('accept-identity', reference),
        ('reject-near-miss', rejected),
    ):
        for cfg in ('05', '06'):
            destination = out / label / ('subrun-' + cfg)
            builder.write_tree(str(destination), frames, parts=particles)

    print('grouped Bell discrimination fixtures written to', out)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
