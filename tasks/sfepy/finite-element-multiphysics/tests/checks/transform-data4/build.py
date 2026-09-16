"""Offline source build, reusable only for the same source and toolchain."""
import fcntl,hashlib,os,shutil,subprocess,sys,time
from importlib.metadata import version
from pathlib import Path
src=Path(os.environ['SOURCE_DIR']).resolve();started=time.monotonic()
h=hashlib.sha256(sys.version.encode())
for package in ['numpy','scipy','Cython','scikit-build','cmake','ninja','setuptools','wheel']:
    h.update(package.encode()+version(package).encode())
h.update(subprocess.check_output([os.environ.get('CC','cc'),'--version']))
for name in ['CFLAGS','CXXFLAGS','CC','CXX','CMAKE_ARGS','SKBUILD_CONFIGURE_OPTIONS']:
    h.update(name.encode()+os.environ.get(name,'').encode())
for p in sorted(src.rglob('*')):
    if p.is_file():h.update(str(p.relative_to(src)).encode()+b'\0'+p.read_bytes())
cache=Path('/tmp/sfepy-check-builds')/h.hexdigest()
cache.mkdir(parents=True,exist_ok=True)
with (cache/'lock').open('w') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX)
    target=cache/'installed'
    if not (cache/'complete').exists():
        work=cache/'source'
        if work.exists():shutil.rmtree(work)
        if target.exists():shutil.rmtree(target)
        shutil.copytree(src,work)
        env=dict(os.environ,CMAKE_BUILD_PARALLEL_LEVEL=os.environ.get('SAB_BUILD_JOBS','1'),PIP_NO_INDEX='1')
        subprocess.run([sys.executable,'-m','pip','install','--no-build-isolation','--no-deps','--no-index','--target',str(target),str(work)],env=env,cwd=work,check=True,stdout=sys.stderr,stderr=sys.stderr)
        (cache/'complete').write_text('built from source\n')
        print(f'SAB_BUILD_SECONDS={time.monotonic()-started:.6f}',file=sys.stderr)
    else:print('SAB_BUILD_SECONDS=0',file=sys.stderr)
print(target)
