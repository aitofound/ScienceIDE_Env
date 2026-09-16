#!/usr/bin/env bash
# Shared TEST half of every check in tasks/hmmer/hmmsearch-profile-search.
#   run.sh nominal | run.sh variant     run one initial condition (ic/<ic>/config.json)
#   run.sh altbuild                     the nominal inputs on the CFLAGS=-O0 build of the same source
#   run.sh --help                       the runtime and resource knobs, and the altbuild line
# Environment supplied by the produce driver: SOURCE_DIR (read-only pinned source),
# OUT_DIR (empty directory for the graded files), CHECK_DIR (this directory).
# Reads only CHECK_DIR and SOURCE_DIR; no network; never modifies SOURCE_DIR.
#
# Every input comes from the pinned tree: the profile and the database named in
# config.json (a shipped file, or a recipe built from shipped files with the
# pinned hmmemit / esl-shuffle / esl-reformat at a fixed seed). The variant is
# the same search on a perturbed copy of the profile (config.json
# "profile_perturbation"), never a different database.

# Runtime and resource knobs; defaults are the graded values.
KNOB_HELP=""
knob(){ local n=$1 d=$2 x=$3; [ -n "${!n:-}" ] || printf -v "$n" '%s' "$d"; export "$n"; KNOB_HELP+="${n}=${d}  ${x}"$'\n'; }
knob SAB_THREADS 4 "cores: hmmsearch --cpu worker threads and make -j; fixed graded default (the declared per-check cpus), never read from the host"
knob SAB_DECOYS 200 "runtime: fixed-seed random decoy sequences (400 aa each) appended to the mixed database; search time scales linearly"
knob SAB_MAX_SEQS 20000 "runtime: fixed-seed random decoy sequences (400 aa each) in the --max database of search-max; search time scales linearly"
ALTBUILD="CFLAGS=-O0 configure of the same pinned source (the graded build is the upstream default -O3 with the SIMD kernels)"
if [ "${1:-}" = "--help" ]; then printf '%s' "$KNOB_HELP"; echo "altbuild: $ALTBUILD"; exit 0; fi

set -euo pipefail
IC="${1:?usage: run.sh <nominal|variant|altbuild> | run.sh --help}"
case "$IC" in nominal|variant|altbuild) ;; *) echo "run.sh: unknown initial condition $IC" >&2; exit 2;; esac
: "${SOURCE_DIR:?}" "${OUT_DIR:?}" "${CHECK_DIR:?}"
INPUTS="$IC"; [ "$IC" = altbuild ] && INPUTS=nominal
CONFIG="$CHECK_DIR/ic/$INPUTS/config.json"
[ -f "$CONFIG" ] || { echo "run.sh: no initial condition ic/$INPUTS" >&2; exit 2; }

W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT

# Build reuse within one produce run: the first check compiles the pinned tree
# into $SAB_BUILD_ROOT/<nominal|altbuild>, later checks find the binaries there.
BUILD_BASE="${SAB_BUILD_ROOT:-${TMPDIR:-/tmp}/sciaccel-hmmer-build}"
BUILD_DIR="$BUILD_BASE/nominal"; [ "$IC" = altbuild ] && BUILD_DIR="$BUILD_BASE/altbuild"
if [ ! -x "$BUILD_DIR/src/src/hmmsearch" ]; then
  mkdir -p "$BUILD_BASE"
  TMP_BUILD="$(mktemp -d "${BUILD_BASE}.tmp.XXXXXX")"
  trap 'rm -rf "$W" "$TMP_BUILD"' EXIT
  cp -R "$SOURCE_DIR/." "$TMP_BUILD/src"
  B=$(date +%s)
  if [ "$IC" = altbuild ]; then
    (cd "$TMP_BUILD/src" && CFLAGS="-O0" ./configure --disable-mpi >configure.log 2>&1 && make -j"$SAB_THREADS" >make.log 2>&1)
  else
    (cd "$TMP_BUILD/src" && ./configure --disable-mpi >configure.log 2>&1 && make -j"$SAB_THREADS" >make.log 2>&1)
  fi
  mv "$TMP_BUILD" "$BUILD_DIR"
  echo "SAB_BUILD_SECONDS=$(( $(date +%s)-B ))"
else
  echo "SAB_BUILD_SECONDS=0"
fi
BIN="$BUILD_DIR/src/src"; ESL="$BUILD_DIR/src/easel/miniapps"; SRC="$BUILD_DIR/src"

# The configuration, one field per line.
C=(); while IFS= read -r line; do C+=("$line"); done < <(python3 - "$CONFIG" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
print(d["profile"]); print(d.get("profile_copies", 1)); print(d["database"])
print(json.dumps(d.get("options", []))); print(json.dumps(d.get("extra_runs", {})))
print(json.dumps(d.get("profile_perturbation", {})))
PY
)
P="${C[0]}"; COPIES="${C[1]}"; D="${C[2]}"; OPTIONS_JSON="${C[3]}"; EXTRA_JSON="${C[4]}"; PERTURB_JSON="${C[5]}"

# The profile: a shipped .hmm, optionally concatenated with itself (the i21 rewind query).
PROFILE="$W/query.hmm"; : >"$PROFILE"
for ((i=0; i<COPIES; i++)); do cat "$SRC/$P" >>"$PROFILE"; done
# The initial-condition difference: config.json "profile_perturbation" adds
# delta_low to the first ten and delta_high to the last ten match-emission
# values (negative log probabilities at five decimals) of every node.
if [ "$PERTURB_JSON" != "{}" ]; then
  python3 - "$PROFILE" "$PERTURB_JSON" <<'PY'
import json, pathlib, re, sys
p = pathlib.Path(sys.argv[1]); spec = json.loads(sys.argv[2])
lo, hi = float(spec["delta_low"]), float(spec["delta_high"])
out, nodes = [], 0
for line in p.read_text().splitlines(True):
    m = re.match(r'^(\s+\d+\s+)(.*)$', line)
    if m:
        tok = m.group(2).split()
        vals = tok[:20]
        if len(vals) == 20 and all(t == "*" or re.match(r'^-?\d+\.\d+$', t) for t in vals):
            new = [t if t == "*" else f"{float(t) + (lo if k < 10 else hi):.5f}" for k, t in enumerate(vals)]
            line = m.group(1) + "  ".join(f"{v:>8}" for v in new) + ("      " + " ".join(tok[20:]) if tok[20:] else "") + "\n"
            nodes += 1
    out.append(line)
if nodes == 0: raise SystemExit("profile perturbation found no match-emission line")
p.write_text("".join(out))
PY
fi

# The database: a shipped file, or a recipe from shipped files at a fixed seed.
DATABASE="$W/db.fa"
case "$D" in
  mixed)     # the tutorial globins and two more tutorial proteins (true hits), the same 45 globins
             # regionally shuffled in 4-residue windows (local composition kept, alignment destroyed:
             # borderline hits with scores and E-values around the reporting, inclusion and filter
             # thresholds), and SAB_DECOYS fully random decoys; every piece from the pinned tree at a fixed seed
    cat "$SRC/tutorial/globins45.fa" "$SRC/tutorial/HBB_HUMAN" >"$DATABASE"
    "$ESL/esl-reformat" fasta "$SRC/tutorial/7LESS_DROME" >>"$DATABASE"
    "$ESL/esl-shuffle" --seed 42 -w 4 "$SRC/tutorial/globins45.fa" >>"$DATABASE"
    "$ESL/esl-shuffle" -G --seed 42 -N "$SAB_DECOYS" -L 400 --amino >>"$DATABASE" ;;
  max)       # the tutorial globins and SAB_MAX_SEQS random decoys: the --max workload
    cat "$SRC/tutorial/globins45.fa" >"$DATABASE"
    "$ESL/esl-shuffle" -G --seed 42 -N "$SAB_MAX_SEQS" -L 400 --amino >>"$DATABASE" ;;
  shufflemix:*) # shufflemix:<stockholm>:<w1,w2,...>  the ungapped seed sequences of a shipped alignment (true hits), the same
             # sequences regionally shuffled in windows of each listed size (borderline hits), and SAB_DECOYS random decoys
    IFS=: read -r _ STO WS <<<"$D"
    "$ESL/esl-reformat" -u fasta "$SRC/$STO" >"$W/seeds.fa"; cat "$W/seeds.fa" >"$DATABASE"
    for w in ${WS//,/ }; do "$ESL/esl-shuffle" --seed 42 -w "$w" "$W/seeds.fa" | sed "s/^>\(.*\)-shuffled\$/>\1-w$w/" >>"$DATABASE"; done
    "$ESL/esl-shuffle" -G --seed 42 -N "$SAB_DECOYS" -L 400 --amino >>"$DATABASE" ;;
  emit:*)    # emit:<N>:<seed>[:unilocal]  sequences sampled from the nominal profile with the pinned hmmemit
    IFS=: read -r _ N SEED MODE <<<"$D"
    EMIT=("$BIN/hmmemit" -N "$N" --seed "$SEED" -p); [ "${MODE:-}" = unilocal ] && EMIT+=(-L 0 --unilocal)
    "${EMIT[@]}" -o "$DATABASE" "$SRC/$P" ;;
  reformat:*) # reformat:<stockholm>  the ungapped sequences of a shipped alignment
    "$ESL/esl-reformat" -u fasta "$SRC/${D#reformat:}" >"$DATABASE" ;;
  nonresidues) printf '>test1\nACDEFGHIK*MNPQRSTVWY\n>test2\nACDEFGHIKL*MNPQRSTVWY\n' >"$DATABASE" ;;   # i8
  duplicate)   printf '>seq\nACDEFGHIKLMNPQRSTVWY\n>seq\nACDEFGHIKLLMNPQRSTVWY\n' >"$DATABASE" ;;         # i10
  zero-length) printf '>foo\n>bar\nYYYYY\n>valid\nACDEFGHIKLMNPQRSTVWY\n' >"$DATABASE" ;;                # i4 (h45), plus one sequence the model hits
  *)           DATABASE="$SRC/$D" ;;
esac

mkdir -p "$OUT_DIR"
ARGS=(); while IFS= read -r o; do [ -n "$o" ] && ARGS+=("${o//__OUT_DIR__/$OUT_DIR}"); done < <(python3 -c 'import json,sys; [print(o) for o in json.loads(sys.argv[1])]' "$OPTIONS_JSON")
search(){ # search <prefix> [extra options...]: one hmmsearch run writing <prefix>.txt/.tbl/.domtbl
  local pfx=$1; shift
  local cmd=("$BIN/hmmsearch" --cpu "$SAB_THREADS" --tblout "$OUT_DIR/$pfx.tbl" --domtblout "$OUT_DIR/$pfx.domtbl")
  [ "${#ARGS[@]}" -eq 0 ] || cmd+=("${ARGS[@]}"); [ "$#" -eq 0 ] || cmd+=("$@")
  "${cmd[@]}" "$PROFILE" "$DATABASE" >"$OUT_DIR/$pfx.txt"
}
search output
# Extra runs of the same search: {"<prefix>": ["--seed","3"]} adds options; "stdin" reads the database from stdin;
# "malformed" records that a gapped alignment masquerading as FASTA is rejected (i23).
python3 -c 'import json,sys; [print(k, json.dumps(v)) for k, v in json.loads(sys.argv[1]).items()]' "$EXTRA_JSON" | while read -r name spec; do
  case "$name" in
    stdin) cmd=("$BIN/hmmsearch" --cpu "$SAB_THREADS" --tblout "$OUT_DIR/stdin.tbl" --domtblout "$OUT_DIR/stdin.domtbl"); [ "${#ARGS[@]}" -eq 0 ] || cmd+=("${ARGS[@]}")
      "${cmd[@]}" "$PROFILE" - <"$DATABASE" >"$OUT_DIR/stdin.txt" ;;
    malformed)
      printf '>bad\n-MALFORMED---\n>worse\nACDE--FGHIK\n' >"$W/bad.fa"
      if "$BIN/hmmsearch" "$PROFILE" "$W/bad.fa" >"$OUT_DIR/malformed.txt" 2>&1; then echo 'malformed_fasta=accepted' >"$OUT_DIR/malformed.status"; else echo 'malformed_fasta=rejected' >"$OUT_DIR/malformed.status"; fi ;;
    *) X=(); while IFS= read -r o; do X+=("$o"); done < <(python3 -c 'import json,sys; [print(o) for o in json.loads(sys.argv[1])]' "$spec"); search "$name" "${X[@]}" ;;
  esac
done
