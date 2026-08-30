#!/usr/bin/env python3
"""Superseded metadata-only enumerator (kept for history; never executed).

The contract-driven compiled runtime lives in tests/lib/runtime_runner.py and
is the only path run.sh invokes.  This stub refuses to run so that a
metadata-only artifact (real_solver_runs=0) can never be produced by mistake.
"""
raise SystemExit("superseded: use tests/lib/runtime_runner.py via this check's run.sh")
