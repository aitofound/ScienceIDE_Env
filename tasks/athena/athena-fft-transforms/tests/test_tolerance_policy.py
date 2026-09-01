#!/usr/bin/env python3
"""Regression checks for the owner-approved Pluto-style Athena tolerance."""
from __future__ import annotations
import json,math
from pathlib import Path
from common import AuthenticationError,_tolerance,_within_combined_tolerance
ROOT=Path(__file__).resolve().parent
ATOL=1e-12;RTOL=1e-8;POLICY='pluto-style-combined-tolerance/v1'
EXPECTED={'absolute':ATOL,'relative':RTOL,'policy':POLICY,'provenance':'Owner-directed adoption of the dominant non-particle Pluto task policy: abs(candidate - oracle) <= ATOL + RTOL*abs(oracle); hard structural and native-evidence gates remain mandatory.'}
def main()->None:
    assert _within_combined_tolerance(1.0,1.0,ATOL,RTOL)
    assert _within_combined_tolerance(5e-13,0.0,ATOL,RTOL)
    assert not _within_combined_tolerance(2e-12,0.0,ATOL,RTOL)
    assert _within_combined_tolerance(1000.0+9e-6,1000.0,ATOL,RTOL)
    assert not _within_combined_tolerance(1000.0+2e-5,1000.0,ATOL,RTOL)
    assert _within_combined_tolerance(1+9e-9j,1+0j,ATOL,RTOL)
    assert not _within_combined_tolerance(1+2e-8j,1+0j,ATOL,RTOL)
    assert not _within_combined_tolerance(math.nan,1.0,ATOL,RTOL)
    assert not _within_combined_tolerance(math.inf,1.0,ATOL,RTOL)
    contract=json.loads((ROOT/'fft_contract.json').read_text())
    specialized=[c for c in contract['checks'] if c['claim_kind']!='aggregate_residual']
    assert len(specialized)==11 and all(c['claim_tolerances']==EXPECTED for c in specialized)
    fft01=next(c for c in contract['checks'] if c['id']=='FFT-01')
    assert 'official-serial-mpi1-combined-tolerance' in fft01['rules'] and 'official-serial-mpi1-equality' not in fft01['rules']
    for check in specialized:
        rubric=json.loads((ROOT/check['folder']/'rubric.json').read_text())
        assert rubric['tolerance']==EXPECTED and _tolerance(check)==(ATOL,RTOL)
    tampered=dict(specialized[0]);tampered['claim_tolerances']=dict(EXPECTED,relative=1e-7)
    try:_tolerance(tampered)
    except AuthenticationError:pass
    else:raise AssertionError('tampered tolerance was accepted')
    print('PASS: 11 Athena native rubrics/contracts use Pluto-style combined tolerance; bounded roundoff passes; near-miss, non-finite and tampered policy fail')
if __name__=='__main__':main()
