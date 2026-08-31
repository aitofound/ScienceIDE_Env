#!/usr/bin/env python3
"""Parser-only discrimination fixture; fixture_origin can never pass production."""
from __future__ import annotations
import json,sys
from pathlib import Path

def main():
    if len(sys.argv)!=2: raise SystemExit("usage: make.py OUTPUT")
    out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
    doc={"schema":"athena-fft-parser-fixture/v1","fixture_origin":True,"check":'serial-3d-roundtrip-normalization',"id":'FFT-21',"claim_kind":'normalization',"fixture":'serial-3d-roundtrip-normalization',"not_native":True,"negative_mutations":["missing_record","duplicate_coordinate","wrong_sign","swapped_axes","wrong_global_cell_count","omitted_imaginary_channel","dropped_rank","duplicate_slab","wrong_parseval_weighting","report_only","hash_refresh"]}
    (out/"fixture-manifest.json").write_text(json.dumps(doc,sort_keys=True,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__": main()
