#!/usr/bin/env python3
"""Parser-only aggregate provenance fixture; fixture_origin can never pass production."""
from __future__ import annotations
import json,sys
from pathlib import Path

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: make.py OUTPUT")
    out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
    doc={"schema":"athena-fft-parser-fixture/v1","fixture_origin":True,"check":'mpi-rank2-x-slab',"id":'FFT-08',"claim_kind":'aggregate_residual',"fixture":'aggregate',"not_native":True,"negative_mutations":["missing_fft_errors","native_claim_artifact","wrong_row","wrong_mesh_probe","report_only","hash_refresh"]}
    (out/"fixture-manifest.json").write_text(json.dumps(doc,sort_keys=True,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__": main()
