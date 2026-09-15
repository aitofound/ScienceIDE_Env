#!/usr/bin/env bash
set -euo pipefail
CHECK_DIR=$(cd "$(dirname "$0")" && pwd -P)
IC=${1:-}
if [ "$IC" = "--help" ]; then echo 'SAB_ITENSOR_THREADS=1'; exit 0; fi
SOURCE_DIR=$SOURCE_DIR
OUT_DIR=$OUT_DIR
case "$IC" in nominal|variant) ;; *) exit 2;; esac
BUILD_DIR=/tmp/sab-itensor-build-sample-trg-cc-$IC
SUBDIR="sample"
TARGET="trg"
MODE="trg"
mkdir -p "$BUILD_DIR"
BUILD_SECONDS=0
if [ ! -f "$BUILD_DIR/build.ok" ] || [ ! -x "$BUILD_DIR/src/$SUBDIR/$TARGET" ]; then
  t0=$(date +%s)
  mkdir -p "$BUILD_DIR/src"
  cp -R "$SOURCE_DIR/." "$BUILD_DIR/src"
  printf '%s\n' 'CCCOM=g++ -std=c++17 -fPIC' 'PLATFORM=lapack' 'BLAS_LAPACK_LIBFLAGS=-llapack -lblas -lpthread' 'OPTIMIZATIONS=-O2 -DNDEBUG -Wall -Wno-unknown-pragmas' 'DEBUGFLAGS=-DDEBUG -g -Wall -Wno-unknown-pragmas -pedantic' "ITENSOR_INCLUDEFLAGS=-I$BUILD_DIR/src" 'ITENSOR_MAKE_DYLIB=0' > "$BUILD_DIR/src/options.mk"
  printf 'THIS_DIR=%s\n' "$BUILD_DIR/src" > "$BUILD_DIR/src/this_dir.mk"
  if [ "$IC" = variant ]; then sed -i 's/Real beta = 1.1 \* betac;/Real beta = 1.100001 * betac;/' "$BUILD_DIR/src/sample/trg.cc"; fi
  (cd "$BUILD_DIR/src/$SUBDIR" && make -j1 "$TARGET" PREFIX="$BUILD_DIR/src" ITENSOR_INCLUDEFLAGS="-I$BUILD_DIR/src" ITENSOR_LIBDIR="$BUILD_DIR/src/lib" ITENSOR_LIBFLAGS="-litensor -llapack -lblas -lpthread")
  touch "$BUILD_DIR/build.ok"; BUILD_SECONDS=$(( $(date +%s)-t0 )); [ "$BUILD_SECONDS" -gt 0 ] || BUILD_SECONDS=1
fi
echo SAB_BUILD_SECONDS=$BUILD_SECONDS
export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1
LOG=$BUILD_DIR/run.log
"$BUILD_DIR/src/$SUBDIR/$TARGET" >"$LOG" 2>&1
python3 - "$MODE" "$LOG" "$OUT_DIR/result.txt" <<'PY'
import re,sys
mode,log,out=sys.argv[1:]
text=open(log,encoding='utf-8',errors='replace').read()
num=r'[-+]?(?:\d+\.?\d*|\.\d+)(?:[Ee][-+]?\d+)?'
def one(p):
    m=re.findall(p,text,re.M)
    if not m: raise SystemExit('missing observable')
    return [float(x) for x in m]
if mode in ('ctmrg','trg'):
    vals=one(r'kappa\s*=\s*('+num+r')')
    if mode=='ctmrg': vals+=one(r'^m\s*=\s*('+num+r')')
elif mode in ('dmrg','dmrg-table','dmrgj1j2'):
    vals=one(r'Ground State Energy\s*=\s*('+num+r')')+one(r'Using inner\s*=\s*('+num+r')')
elif mode=='exthubbard':
    vals=one(r'Ground State Energy\s*=\s*('+num+r')')
    sec=text.split('Total Density:',1)[1].split('Ground State Energy',1)[0]
    vals += [float(x) for x in re.findall(r'^\s*\d+\s+('+num+r')\s*$',sec,re.M)]
elif mode=='svd':
    line=re.findall(r'^d\s*=\s*(.*)$',text,re.M)
    vals=[float(x) for x in re.findall(num,line[0])]
    vals += one(r'\|M-Mtrunc\|\^2\s*=\s*('+num+r')')
else: raise SystemExit('unknown mode')
open(out,'w').write(''.join(f'{x:.17g}\n' for x in vals))
PY
