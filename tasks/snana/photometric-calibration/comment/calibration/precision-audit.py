"""Independent NumPy quadrature and precision audit for the five fixed KCOR fixtures.

This reviewer-only diagnostic is not the oracle. It reconstructs linear
interpolation and photometry from archived input bytes, using vector reductions.
The HST grid calculation is restricted to this fixture's O'Donnell94 law,
RV=3.1, AV_OPTION=2 and 0.05 redshift spacing.
"""
import argparse
import io
import json
from pathlib import Path
import tarfile

import numpy as np

C = 2.99792458e18
H = 6.6260755e-27
FAB = 3.631e-20


def read_inputs(check, ref):
    case = json.loads((check/'case.json').read_text())
    with tarfile.open(check/'ic/nominal/inputs.tar.gz') as archive:
        def table(name):
            return np.loadtxt(io.BytesIO(archive.extractfile(name).read()))
        deck = archive.extractfile(case['deck']).read().decode()
        filt, prim_files = {}, {'AB':'flatnu.dat'}
        for line in deck.splitlines():
            row = line.split('#')[0].split()
            if not row:
                continue
            key = row[0]
            if key == 'MAGSYSTEM:': system = row[1]
            elif key == 'FILTSYSTEM:': mode = row[1]
            elif key == 'FILTPATH:': folder = row[1].replace('$SNDATA_ROOT/','')
            elif key in ('BD17_SED:','VEGA_SED:'): prim_files[key[:-5]] = row[1]
            elif key == 'FILTER:':
                path = folder if folder.startswith('filters/') else 'filters/'+folder
                filt[row[1]] = (table(path+'/'+row[2]),float(row[3]),system,mode)
        wave, phases = ref['wavelength'], ref['phase']
        trans, zp_input, limits = [], [], []
        args = case['arguments']
        for name in ref['filter_names']:
            base = name.lstrip('*')
            data, zp, system, mode = filt[base]
            shift = 10.0 if name.startswith('*') else 0.0
            if 'FILTER_LAMSHIFT' in args and base == 'SDSS-r': shift += 2.1
            if 'FILTER_LAMSHIFT' in args and base == 'SDSS-i': shift += 3.2
            wl, throughput = data[:,0]+shift, data[:,1].copy()
            limits.append((wl[0],wl[-1]))
            if mode == 'ENERGY': throughput *= 1000/wl
            trans.append(np.interp(wave,wl,throughput,left=0,right=0))
            zp_input.append(zp)
        prim = []
        dl = wave[1]-wave[0]
        lo = np.ceil(min(x[0] for x in limits)/dl)*dl
        hi = np.ceil(max(x[1] for x in limits)/dl)*dl
        for name in ref['primary_names']:
            if name not in set(ref['system_names']): continue
            data = table('standards/'+prim_files[name])
            # rd_primary reads only wavelengths < STORE_LAMBDA_MAX+20 A.
            # Some sparse input standards end below the last requested grid
            # wavelength after that cut. The pinned output is zero there;
            # a disposable source diagnostic verified the SDSS endpoint.
            data = data[data[:,0] < max(x[1] for x in limits)+20]
            values = np.interp(wave,data[:,0],data[:,1],left=0,right=0)
            values[(wave<lo)|(wave>hi)] = 0
            prim.append(values)
        sed = table('snsed/Hsiao07.dat')
        selected = np.isin(sed[:,0],phases) & np.isin(sed[:,1],wave)
        sn = sed[selected,2].reshape(len(phases),len(wave))
        sn = np.where(sn == 0, 1e-19, sn)
    return np.asarray(trans).T, np.asarray(zp_input), np.asarray(prim).T, sn


def extinction_per_av(wave, rv=3.1):
    """CCM89 with O'Donnell94 optical polynomial, using Horner evaluation."""
    x = 10000/np.asarray(wave)
    a, b = np.zeros_like(x), np.zeros_like(x)
    m = (x >= .3) & (x < 1.1)
    a[m],b[m] = .574*x[m]**1.61,-.527*x[m]**1.61
    m = (x >= 1.1) & (x < 3.3)
    y = x[m]-1.82
    a[m] = np.polynomial.polynomial.polyval(y,[1,.104,-.609,.701,1.137,-1.718,-.827,1.647,-.505])
    b[m] = np.polynomial.polynomial.polyval(y,[0,1.952,2.908,-3.989,-7.985,11.102,5.491,-10.805,3.347])
    m = (x >= 3.3) & (x < 8)
    xx = x[m]; yy = np.maximum(xx-5.9,0)
    a[m] = 1.752-.316*xx-.104/((xx-4.67)**2+.341)-.04473*yy**2-.009779*yy**3
    b[m] = -3.090+1.825*xx+1.206/((xx-4.62)**2+.263)+.213*yy**2+.1207*yy**3
    m = (x >= 8) & (x <= 10)
    y = x[m]-8
    a[m] = np.polynomial.polynomial.polyval(y,[-1.073,-.628,.137,-.070])
    b[m] = np.polynomial.polynomial.polyval(y,[13.670,4.257,-.420,.374])
    return a+b/rv


def stats(reference, predicted, sentinels=()):
    valid = np.ones(reference.shape,dtype=bool)
    for value in sentinels: valid &= reference != value
    r,p = reference[valid],predicted[valid]
    d = abs(p-r)
    spacing = np.spacing(abs(r).astype(np.float32)).astype(float)
    ix = int(np.argmax(d))
    return dict(count=int(valid.sum()),max_abs=float(d.max()),p99_abs=float(np.quantile(d,.99)),
                rms=float(np.sqrt(np.mean(d*d))),reference_min=float(r.min()),reference_max=float(r.max()),
                largest_float32_ulp=float(spacing.max()),worst_flat_valid_index=ix,
                worst_reference=float(r[ix]),worst_predicted=float(p[ix]))


def budget_stats(reference, predicted, spec):
    """Check raw-input reconstruction against the proposed observable budget."""
    valid = np.ones(reference.shape, dtype=bool)
    for sentinel in spec.get('sentinels', []):
        assert np.array_equal(reference == sentinel, predicted == sentinel)
        valid &= reference != sentinel
    peak = np.max(abs(reference), axis=spec['peak_axis'], keepdims=True) if 'peak_axis' in spec else np.max(abs(reference))
    bound = spec['atol'] + spec.get('peak_atol', 0)*peak + spec.get('rtol', 0)*abs(reference)
    fraction = abs(predicted[valid]-reference[valid])/np.broadcast_to(bound, reference.shape)[valid]
    ulp = np.spacing(abs(reference).astype(np.float32)).astype(float)
    bound_in_ulp = np.broadcast_to(bound, reference.shape)[valid]/ulp[valid]
    return dict(passed=bool(np.isfinite(fraction).all() and np.max(fraction) <= 1),
                max_bound_fraction=float(np.max(fraction)),
                smallest_bound_in_float32_ulp=float(np.min(bound_in_ulp)),
                cells_with_bound_below_one_float32_ulp=int(np.sum(bound_in_ulp < 1)))


def photometry(a, trans, zp_input, prim, sn):
    wave = a['wavelength']; dl = wave[1]-wave[0]
    weight = (1/(wave-dl/2)-1/(wave+dl/2))*wave
    denom = np.sum(trans*weight[:,None],axis=0)
    pmean = (prim*wave[:,None]**2/C).T@(trans*weight[:,None])/denom
    zp = zp_input.copy()
    names = [n for n in a['primary_names'] if n in set(a['system_names'])]
    for i,system in enumerate(a['system_names']):
        if system != 'AB': zp[i] += 2.5*np.log10(pmean[names.index(system),i]/FAB)
    mean = (sn*wave[None,:]**2/C)@(trans*weight[:,None])/denom
    mags = -2.5*np.log10(mean/FAB)+zp
    i0,i15 = [int(np.argmin(abs(a['phase']-t))) for t in (0,15)]
    return zp,np.column_stack((mags[i0],mags[i15]-mags[i0])),weight,denom


def hst_grid(a,trans,zp,sn,weight,denom,specs):
    wave = a['wavelength']; phases = a['phase']; nf = len(a['filter_names'])
    pairs = np.array([[a['filter_names'].tolist().index(n) for n in row] for row in a['kcor_pairs']])
    kvalues = np.empty_like(a['kcor_values']); mags = np.full_like(a['grid_magnitudes'],999.)
    slopes = np.full_like(mags,999.); floor_fraction=np.zeros_like(mags)
    no_floor_mags=np.full_like(mags,999.); derivative=np.full_like(mags,999.)
    rounded_mag_slopes=np.full_like(mags,999.)
    active_obs = np.unique(pairs[:,1])
    mw_coeff = extinction_per_av(wave)*3.1
    attenuation = 10**(-.4*.1*mw_coeff)
    floor = np.count_nonzero(trans>0,axis=0)*1e-9*H
    for av in range(-6,7):
        rest_sn = sn*10**(-.4*av*extinction_per_av(wave))
        rest_conv = rest_sn@(trans*wave[:,None])
        for zi in range(5):
            z = zi*.05
            # The upstream driver also evaluates same-filter rest magnitudes
            # at its first redshift for every filter (NKCOR_EXTRA).
            obs_columns = np.arange(nf) if zi == 0 else active_obs
            rest_wave = wave/(1+z)
            red_sn = np.array([np.interp(rest_wave,wave,row) for row in sn])
            red_sn *= 10**(-.4*av*extinction_per_av(rest_wave))
            obs_conv = red_sn@(trans*wave[:,None])
            flux = red_sn*wave[None,:]**2/C
            flux0 = flux@(trans*weight[:,None])
            flux1 = (flux*attenuation)@(trans*weight[:,None])
            m0 = -2.5*np.log10((flux0+floor)/denom/FAB)+zp
            m1 = -2.5*np.log10((flux1+floor)/denom/FAB)+zp
            der = (flux*mw_coeff)@(trans*weight[:,None])/(flux0+floor)
            kval = 2.5*np.log10((1+z)*(rest_conv[:,pairs[:,0]]/obs_conv[:,pairs[:,1]])/(denom[pairs[:,0]]/denom[pairs[:,1]]))+zp[pairs[:,1]]-zp[pairs[:,0]]
            for coords,output,values,columns in [
                (a['kcor_coords'],kvalues,kval,np.arange(len(pairs))),
                (a['mag_coords'],mags,m0,obs_columns),
                (a['mag_coords'],slopes,(m1-m0)/.1,obs_columns),
                (a['mag_coords'],floor_fraction,np.broadcast_to(floor,m0.shape)/(flux0+floor),obs_columns),
                (a['mag_coords'],no_floor_mags,-2.5*np.log10(flux0/denom/FAB)+zp,obs_columns),
                (a['mag_coords'],derivative,der,obs_columns)]:
                rows=np.flatnonzero((coords[:,2]==av)&(np.rint(coords[:,1]/.05)==zi))
                t=np.array([np.flatnonzero(phases==coords[i,0])[0] for i in rows])
                output[np.ix_(rows,columns)] = values[np.ix_(t,columns)]
            rows=np.flatnonzero((a['mag_coords'][:,2]==av)&(np.rint(a['mag_coords'][:,1]/.05)==zi))
            t=np.array([np.flatnonzero(phases==a['mag_coords'][i,0])[0] for i in rows])
            rounded=((m1.astype(np.float32).astype(float)-m0.astype(np.float32).astype(float))/.1)
            rounded_mag_slopes[np.ix_(rows,obs_columns)] = rounded[np.ix_(t,obs_columns)]
    valid = a['grid_magnitudes']!=999
    d = no_floor_mags[valid]-mags[valid]
    slope_diff = abs(derivative[valid]-slopes[valid])
    result = {name:stats(a[name],b,(999,666)) for name,b in [('kcor_values',kvalues),('grid_magnitudes',mags),('mw_extinction_slopes',slopes)]}
    result['budget_checks'] = {name:budget_stats(a[name],b,specs[name]) for name,b in [('kcor_values',kvalues),('grid_magnitudes',mags),('mw_extinction_slopes',slopes)]}
    result['protective_flux'] = dict(max_flux_fraction=float(floor_fraction[valid].max()),
        cells_more_than_one_percent=int((floor_fraction[valid]>.01).sum()),
        removing_floor_max_magnitude_change=float(d.max()),removing_floor_cells_exceed_2e_5_mag=int((d>2e-5).sum()))
    result['secant_vs_zero_derivative'] = dict(max_abs=float(slope_diff.max()),cells_exceed_5e_5=int((slope_diff>5e-5).sum()))
    result['premature_float32_mag_rounding'] = stats(a['mw_extinction_slopes'],rounded_mag_slopes,(999,666))
    return result


def main():
    p=argparse.ArgumentParser()
    for n in ('task','reference','out'):p.add_argument('--'+n,type=Path,required=True)
    args=p.parse_args();report={}
    for check in sorted((args.task/'tests/checks').iterdir()):
        with np.load(args.reference/check.name/'result.npz',allow_pickle=False) as f:a=dict(f)
        trans,zp_input,prim,sn=read_inputs(check,a)
        zp,summary,weight,denom=photometry(a,trans,zp_input,prim,sn)
        active=np.isin(a['primary_names'],a['system_names']);rprim=a['primary_sed'][:,active]
        specs={s['name']:s for s in json.loads((check/'rubric.json').read_text())['comparison']['files']}
        row=dict(zeropoint_integral=stats(a['zeropoints'][:,1],zp),printed_summary=stats(a['snmag_summary'],summary),
            raw_filter_interpolation=stats(a['filter_trans'],trans),raw_primary_interpolation=stats(rprim,prim),
            sn_sed_storage=stats(a['sn_sed'],sn*(a['wavelength'][1]-a['wavelength'][0])),
            primary_scales={str(n):dict(peak=float(abs(rprim[:,i]).max()),min_positive=float(rprim[:,i][rprim[:,i]>0].min())) for i,n in enumerate(a['primary_names'][active])})
        row['budget_checks']={name:budget_stats(r,b,specs[name]) for name,r,b in [
            ('zeropoints',a['zeropoints'][:,1],zp),('snmag_summary',a['snmag_summary'],summary),
            ('filter_trans',a['filter_trans'],trans),('primary_sed',rprim,prim),
            ('sn_sed',a['sn_sed'],sn*(a['wavelength'][1]-a['wavelength'][0]))]}
        if check.name=='example-hst-kgrid':row['full_grid']=hst_grid(a,trans,zp,sn,weight,denom,specs)
        report[check.name]=row
        print(check.name,json.dumps(row),flush=True)
    args.out.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    assert all(x['passed'] for row in report.values() for x in row['budget_checks'].values())
    assert all(x['passed'] for row in report.values() if 'full_grid' in row for x in row['full_grid']['budget_checks'].values())


if __name__=='__main__':main()
