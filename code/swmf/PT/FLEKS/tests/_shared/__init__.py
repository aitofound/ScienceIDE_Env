#!/usr/bin/env python3
"""Shared helper modules for the per-test validation scripts.

Tests that share validation logic (e.g. the hybrid family) keep their common
code here instead of duplicating it in each test directory.  The per-test
``validate.py`` modules import only the pieces they need, so running a single
test does not load the code for unrelated tests.
"""
