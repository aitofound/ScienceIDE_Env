"""Native-run provenance constants for the official Athena++ verifier."""
from __future__ import annotations
import hashlib
from pathlib import Path
ROLE_ORACLE='reference-oracle'
ROLE_CANDIDATE='candidate'
EVIDENCE_DOCKER='docker-oracle-run'
EVIDENCE_HOST_NATIVE='host-native-oracle-run'
RUN_DIR='.runs'

def sha256_file(path: Path):
    h=hashlib.sha256(); h.update(path.read_bytes()); return h.hexdigest()
