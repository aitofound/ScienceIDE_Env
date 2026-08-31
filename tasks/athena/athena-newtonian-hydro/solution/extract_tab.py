#!/usr/bin/env python3
"""Compatibility note for the official native-output extraction interface.

The pinned upstream scripts emit native ``carbuncle-diff.dat``,
``linearwave-errors.dat``, and ``shock-errors.dat``.  ``oracle_runner.py``
preserves those exact bytes and records their parsed rows; no extra workload or
synthetic observable is extracted here.
"""
