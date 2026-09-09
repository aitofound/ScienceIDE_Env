#!/usr/bin/env python3
"""Reproduce the source investigation in a NEW scratch directory, without Docker.

Usage: python3 investigate.py --source /path/to/upstream --scratch /new/directory
Requires gfortran, make, nf-config and nc-config. Each executable has a 180 s limit.
This is an investigation tool, not a ScienceAccelBench check or pass policy.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import signal
import subprocess
import tarfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--scratch', type=Path, required=True)
    parser.add_argument('--only', choices=('all', 'routing'), default='all')
    args = parser.parse_args()
    original = args.source.resolve()
    scratch = args.scratch.resolve()
    scratch.mkdir(parents=True, exist_ok=False)
    source = scratch / 'source'
    shutil.copytree(original, source, ignore=shutil.ignore_patterns('.git'))
    logs = scratch / 'logs'
    logs.mkdir()
    records = []
    env = dict(os.environ, OMP_NUM_THREADS='1', OPENBLAS_NUM_THREADS='1')

    def run(name, command, cwd, limit=180):
        started = time.monotonic()
        with (logs / (name + '.log')).open('wb') as stream:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdout=stream,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            timed_out = False
            try:
                process.wait(timeout=limit)
            except subprocess.TimeoutExpired:
                timed_out = True
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        record = {'name': name, 'command': [str(x).replace(str(scratch), '<scratch>') for x in command],
                  'cwd': str(cwd.relative_to(scratch)), 'exit_code': process.returncode,
                  'timed_out': timed_out, 'wall_seconds': round(time.monotonic() - started, 6)}
        records.append(record)
        print(json.dumps(record), flush=True)
        return record

    flags = ['-fopenmp', '-O3', '-cpp', '-ffree-line-length-none', '-fimplicit-none', '-Dheatlink']
    build = scratch / 'unit-build'
    build.mkdir()
    common = ['parkind1.F90', 'yos_cmf_input.F90', 'common/const_mod.f90',
              'common/text_mod.f90', 'common/funit_mod.f90', 'common/numeric_utils_mod.f90',
              'common/time_mod.f90', 'common/datetime_mod.f90']
    phys = ['phys/' + x + '.f90' for x in ('phys_const_mod', 'water_mod', 'heat_flux_mod',
                                         'heat_budget_mod', 'ice_cover_mod')]
    heat = ['yos_cmf_map.F90'] + ['heatlink/' + x + '.f90' for x in (
        'heatlink_config_mod', 'heatlink_velocity_mod', 'topo_mod',
        'water_storage_adapter_mod', 'river_water_advection_mod', 'river_ice_advection_mod')]
    if args.only == 'all':
        support = run('unit-support-build', ['gfortran'] + flags + ['-c'] +
                      [str(source / 'src' / p) for p in common + phys + heat], build)
        if support['exit_code'] == 0:
            objects = [str(p) for p in sorted(build.glob('*.o'))]
            for group in ('common', 'phys', 'heatlink'):
                for test in sorted((source / 'src' / group / 'test').glob('test_*.f90')):
                    name = test.stem
                    if name in ('test_key_table', 'test_ranked_array'):
                        continue
                    result = run(name + '-build', ['gfortran'] + flags + ['-I.', str(test)] +
                                 objects + ['-o', str(build / name)], build)
                    if result['exit_code'] == 0:
                        # The official config test resolves test/namelist relative to this directory.
                        result = run(name, [str(build / name)], source / 'src' / group)
                        if name == 'test_river_water_advection_cold_inflow':
                            output = (logs / (name + '.log')).read_text(errors='replace')
                            result['upstream_expected_failure'] = True
                            result['expected_diagnostic_found'] = 'Liquid inflow temperature is below the melting point.' in output
            run('array-mod-build', ['gfortran'] + flags + ['-I.', '-c',
                str(source / 'src/common/array_mod.f90')], build)

        # The official build script expects its working directory to be gosh/.
        run('stock-compile-script', ['sh', 'compile.sh'], source / 'gosh')
        run('stock-main-make', ['make', 'all'], source / 'src')
        run('stock-main-make-no-implicit-rules', ['make', '-r', 'all'], source / 'src')

    # Build the routing executable with NetCDF and binary64, using make overrides.
    nfflags = subprocess.check_output(['nf-config', '--fflags'], text=True).strip()
    nflibs = subprocess.check_output(['nf-config', '--flibs'], text=True).strip()
    # Some installations put the C and Fortran libraries in separate prefixes.
    nflibs += ' ' + subprocess.check_output(['nc-config', '--libs'], text=True).strip()
    run('netcdf-clean', ['make', 'clean'], source / 'src')
    result = run('netcdf-main-build', ['make', '-r', 'all', 'DSINGLE=', 'DCDF=-DUseCDF_CMF',
                                     'INC=' + nfflags, 'LIB=' + nflibs], source / 'src')
    if result['exit_code'] == 0:
        example_dir = source / 'etc/sealev_boundary'
        archives = [example_dir / n for n in ('moz_06min.tar.gz', 'test_moz_06min_sealev.tar.gz')]
        if all(p.is_file() for p in archives):
            for archive in archives:
                with tarfile.open(archive) as bundle:
                    for member in bundle:
                        relative = Path(member.name)
                        if relative.is_absolute() or '..' in relative.parts or not (member.isdir() or member.isfile()):
                            raise ValueError('Unsafe archive member: ' + member.name)
                        target = example_dir / relative
                        if member.isdir():
                            target.mkdir(parents=True, exist_ok=True)
                        else:
                            target.parent.mkdir(parents=True, exist_ok=True)
                            with bundle.extractfile(member) as incoming, target.open('wb') as outgoing:
                                shutil.copyfileobj(incoming, outgoing)
            script = example_dir / 'test5-moz_06min_sealev.sh'
            body = script.read_text()
            body, count = re.subn(r'^BASE="/home/yamadai/work/CaMa_v401/cmf_v401_test"$',
                                 'BASE=' + shlex.quote(str(source)), body, flags=re.M)
            assert count == 1
            body, count = re.subn(r'^export OMP_NUM_THREADS=16\b', 'export OMP_NUM_THREADS=1', body, flags=re.M)
            assert count == 1
            script.write_text(body)
            result = run('mozambique-official-example', ['sh', script.name], example_dir)
            run_dir = source / 'out/test5-moz_06min_sealev'
            result['outputs'] = [{'name': p.name, 'bytes': p.stat().st_size,
                                  'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                                 for p in sorted(run_dir.glob('*.nc'))]
            model_log = run_dir / 'log_CaMa-2019.txt'
            if model_log.is_file():
                result['model_log_tail'] = model_log.read_text(errors='replace').splitlines()[-15:]
        else:
            records.append({'name': 'mozambique-official-example', 'status': 'not run',
                            'reason': 'The source payload omits an input archive; supply an independently licensed upstream checkout.'})

    summary = {'platform': platform.platform(), 'machine': platform.machine(),
               'compiler': subprocess.check_output(['gfortran', '--version'], text=True).splitlines()[0],
               'netcdf_fortran': subprocess.check_output(['nf-config', '--version'], text=True).strip(),
               'threads': 1, 'per_process_limit_seconds': 180, 'records': records}
    (scratch / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('Results: ' + str(scratch / 'results.json'))


if __name__ == '__main__':
    main()
