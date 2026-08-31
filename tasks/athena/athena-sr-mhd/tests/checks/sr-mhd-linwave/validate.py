#!/usr/bin/env python3
"""Validate this official upstream regression artifact."""
from __future__ import annotations
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from lib.runtime_validator import validate
