"""Decode the upstream FITS layout into arrays with explicit physical identities.

Duplicate FITS TTYPE values are valid in the upstream output but cannot be
loaded as a NumPy structured dtype. Rename the headers in memory before any
column access; use the documented positions plus full FILTnnn identities.
No values or on-disk FITS headers are changed.
"""
import re
import sys
from pathlib import Path
import numpy as np

from astropy.io import fits


def columns(hdu):
    count = int(hdu.header['TFIELDS'])
    names = [str(hdu.header[f'TTYPE{i}']).strip() for i in range(1, count+1)]
    for i in range(1, count+1):
        hdu.header[f'TTYPE{i}'] = f'column_{i}'
    values = [np.array(hdu.data.field(i), copy=True) for i in range(count)]
    return names, values


def text(a):
    return np.asarray([x.decode().strip() if isinstance(x, bytes) else str(x).strip() for x in a])


def stack(a, nrow):
    return np.column_stack(a).astype(np.float64) if a else np.empty((nrow, 0), dtype=np.float64)


def extract(fits_path, log_path, destination):
    log = log_path.read_text(errors='strict')
    if 'FATAL ERROR' in log or 'ABORT program' in log:
        raise ValueError('kcor reported a fatal error')
    with fits.open(fits_path, mode='readonly', memmap=False) as hdus:
        hdr = hdus[0].header
        nf, nk = int(hdr['NFILTERS']), int(hdr['NKCOR'])
        filters = np.asarray([str(hdr[f'FILT{i:03d}']).strip() for i in range(1,nf+1)])
        _, zp = columns(hdus['ZPoff'])
        if len(zp) != 5 or not np.array_equal(text(zp[0]), filters):
            raise ValueError('unexpected ZP table identity/layout')
        snh = hdus['SN SED'].header
        phase = float(snh['TMIN']) + np.arange(int(snh['NBT'])) * float(snh['TBIN'])
        wave = float(snh['LMIN']) + np.arange(int(snh['NBL'])) * float(snh['LBIN'])
        _, sn = columns(hdus['SN SED'])
        snsed = sn[0].astype(np.float64).reshape(len(phase), len(wave))
        trans_names, trans = columns(hdus['FilterTrans'])
        primary_names, prim = columns(hdus['PrimarySED'])
        if not np.array_equal(trans[0], wave) or not np.array_equal(prim[0], wave):
            raise ValueError('wavelength axes disagree between scientific tables')
        if trans_names[1:] != filters.tolist():
            raise ValueError('filter transmission order differs from FILT identities')
        k_names, k = columns(hdus['KCOR'])
        pairs = []
        for i in range(1,nk+1):
            m = re.fullmatch(r'Kcor (\S+) for rest (\S+) to obs (\S+)', str(hdr[f'KCOR{i:03d}']).strip())
            if not m or m[1] != k_names[i+2]:
                raise ValueError('unrecognized full K-correction identity')
            pairs.append([m[2],m[3]])
        _, mag = columns(hdus['MAG+MWXTCOR'])
        if len(k) != 3+nk or len(mag) != 3+2*nf:
            raise ValueError('unexpected grid table shape')
        summary = {}
        pattern = r'^SNMAG:\s+(\S+)\s+\(\s*(\S+)\)\s+([-+\d.eE]+)\s+([-+\d.eE]+)\s*$'
        for line in log.splitlines():
            match = re.match(pattern, line)
            if match:
                if match[1] in summary:
                    raise ValueError('duplicate SNMAG full filter identity')
                summary[match[1]] = (match[2],float(match[3]),float(match[4]))
        if set(summary) != set(filters):
            raise ValueError('incomplete peak/DM15 scientific summary')
        systems = text(zp[1])
        if any(summary[name][0] != systems[i] for i,name in enumerate(filters)):
            raise ValueError('SNMAG and zero-point systems disagree')
        result = dict(
            success=np.asarray(1), filter_names=filters, system_names=systems,
            zeropoints=stack(zp[2:],nf), snmag_summary=np.asarray([summary[name][1:] for name in filters]),
            wavelength=wave, phase=phase, sn_sed=snsed,
            filter_trans=stack(trans[1:],len(wave)), primary_names=np.asarray(primary_names[1:]),
            primary_sed=stack(prim[1:],len(wave)),
            kcor_pairs=np.asarray(pairs,dtype=str).reshape(nk,2),
            kcor_coords=stack(k[:3],len(k[0])), kcor_values=stack(k[3:],len(k[0])),
            mag_coords=stack(mag[:3],len(mag[0])), grid_magnitudes=stack(mag[3:3+nf],len(mag[0])),
            mw_extinction_slopes=stack(mag[3+nf:],len(mag[0])))
        np.savez_compressed(destination, **result)


if __name__ == '__main__':
    extract(*(Path(x) for x in sys.argv[1:]))
