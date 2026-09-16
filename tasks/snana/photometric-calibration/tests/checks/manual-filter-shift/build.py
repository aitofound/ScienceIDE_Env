"""Build the 12-object kcor target in scratch; key the cache by actual source bytes."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import time


def build(source, alternative=False):
    source = Path(source).resolve()
    src = source / "src"
    cc, cxx = ("clang", "clang++") if alternative else ("gcc", "g++")
    flags = ["-O2", "-fcommon", "-fno-fast-math", "-ffp-contract=off",
             '-DGIT_SNANA_VERSION="sciaccel-kcor"']
    signature = hashlib.sha256(json.dumps([cc, cxx, flags]).encode())
    signature.update(Path(__file__).read_bytes())
    for path in sorted(src.rglob("*")):
        if path.is_file():
            signature.update(str(path.relative_to(src)).encode() + b"\0")
            signature.update(path.read_bytes())
    key = signature.hexdigest()
    cache = Path("/tmp/sab-kcor-build")
    cache.mkdir(exist_ok=True)
    destination = cache / key
    with (cache / (key + ".lock")).open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        exe = destination / "kcor.exe"
        if (destination / "complete.json").is_file() and exe.is_file():
            print("SAB_BUILD_SECONDS=0", flush=True)
            return exe
        destination.mkdir(exist_ok=True)
        start = time.monotonic()
        copied = destination / "src"
        shutil.copytree(src, copied, dirs_exist_ok=True)
        # Upstream optional output backend; all scientific bodies are unchanged.
        header = copied / "sntools_output.h"
        header.write_text(re.sub(r"(?m)^#define USE_ROOT\s*$",
                                 "#define USE_ROOTxxx", header.read_text()))
        gsl_c = shlex.split(subprocess.check_output(["gsl-config", "--cflags"], text=True))
        gsl_l = shlex.split(subprocess.check_output(["gsl-config", "--libs"], text=True))
        names = ['kcor.c', 'sntools.c', 'MWgaldust.c', 'sntools_spectrograph.c', 'genmag_SEDtools.c', 'sntools_dataformat_fits.c', 'sntools_dataformat_text.c', 'sntools_data.c', 'sntools_output.cpp', 'sntools_cosmology.c', 'sntools_npz.cpp', 'cnpy.cpp']
        objects, commands = [], []
        for name in names:
            cpp = name.endswith(".cpp")
            obj = destination / (Path(name).stem + ".o")
            cmd = [cxx if cpp else cc, *flags, "-std=c++11" if cpp else "-std=gnu99",
                   *gsl_c, "-c", str(copied / name), "-o", str(obj)]
            print(shlex.join(cmd), flush=True)
            subprocess.run(cmd, check=True)
            commands.append(cmd)
            objects.append(str(obj))
        cmd = [cxx, "-o", str(exe), *objects, *gsl_l, "-lcfitsio", "-lz", "-lm"]
        subprocess.run(cmd, check=True)
        commands.append(cmd)
        elapsed = time.monotonic() - start
        record = dict(source_digest=key, commands=commands, seconds=elapsed,
                      compiler=subprocess.check_output([cc, "--version"], text=True).splitlines()[0],
                      gsl=subprocess.check_output(["gsl-config", "--version"], text=True).strip())
        (destination / "complete.json").write_text(json.dumps(record, indent=2) + "\n")
        print(f"SAB_BUILD_SECONDS={elapsed:.6f}", flush=True)
        return exe
