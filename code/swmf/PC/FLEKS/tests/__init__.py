#!/usr/bin/env python3
"""FLEKS standalone test suite.

The suite is split into small per-test ``validate.py`` modules (one per test
directory) so that running a single test only loads the code it needs.  Shared
helpers live in the ``_shared`` subpackage.  ``validate_tests.py`` is the
common runner that discovers the tests and drives the executable.
"""
