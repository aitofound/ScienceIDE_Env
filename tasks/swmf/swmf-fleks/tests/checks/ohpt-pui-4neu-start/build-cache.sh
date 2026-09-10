#!/usr/bin/env bash
# Path-stable, solve-scoped SWMF build-family cache.
#
# A family owner atomically claims its final cache path, copies only the pristine
# SOURCE_DIR there, configures/builds in that same absolute path, creates one
# immutable runtime template, and publishes .ready last. Other checks wait for
# .ready and never mutate the configured source or the template. Each caller
# receives a private writable copy of the runtime template. Thus generated
# absolute DIR/MYDIR values and absolute executable/component symlinks always
# continue to name the path in which Config.pl generated them.

sab_make_immutable() {
  local root
  for root in "$@"; do
    find "$root" -type f -exec chmod a-w {} +
    find "$root" -type d -exec chmod a-w {} +
  done
}

sab_make_runtime_writable() {
  find "$WORK/run" -type f -exec chmod u+w {} +
  find "$WORK/run" -type d -exec chmod u+w {} +
}

# Repoint only absolute source-root symlinks in the caller's private runtime.
# Relative links are already rerooted by copying the template and are untouched.
sab_reroot_runtime_links() {
  local old_root="$1" new_root="$2" link target suffix
  while IFS= read -r -d '' link; do
    target="$(readlink "$link")"
    case "$target" in
      "$old_root") ln -sfn "$new_root" "$link" ;;
      "$old_root"/*)
        suffix="${target#"$old_root"}"
        ln -sfn "$new_root$suffix" "$link"
        ;;
    esac
  done < <(find "$WORK/run" -type l -print0)
}

# Caller defines sab_build_family(), which must build at SAB_BUILD_SRC and make
# SAB_RUNTIME_TEMPLATE. Argument 2 is "stage" (default) or "no-stage".
sab_acquire_build() {
  local family="$1" stage="${2:-stage}" cache_root key start status waited
  case "$family" in *[!A-Za-z0-9._-]*|'') echo "run.sh: invalid build family: $family" >&2; return 2 ;; esac
  case "$stage" in stage|no-stage) ;; *) echo "run.sh: invalid cache stage mode: $stage" >&2; return 2 ;; esac
  case "$IC" in altbuild) SAB_BUILD_OPT=O0 ;; nominal|variant) SAB_BUILD_OPT=O3 ;; *) return 2 ;; esac

  cache_root="${SAB_BUILD_CACHE_ROOT:-$WORK/.sab-build-cache}"
  mkdir -p "$cache_root"
  cache_root="$(cd "$cache_root" && pwd -P)"
  # Family names are not sufficient cache identity: the AMReX dimension,
  # FLEKS level, and SWMF user source all alter generated headers/objects.
  # Keep these dimensions explicit in the immutable family key.
  case "$family" in
    standalone-fleks-2d-v1) dim=2; lev=2; user=Exo ;;
    standalone-fleks-3d-v1) dim=3; lev=2; user=Exo ;;
    gm-pc-alfven-3d-v1) dim=3; lev=2; user=Default ;;
    gm-pc-fastwave-2d-v1) dim=2; lev=9; user=Default ;;
    gm-pc-fastwave-lightwave-3d-v1) dim=3; lev=9; user=Default ;;
    gmpc-periodic-2d-v1) dim=2; lev=9; user=Default ;;
    gmpc-periodic-3d-v1) dim=3; lev=9; user=Default ;;
    gmpc-reconnection-3d-v1) dim=3; lev=default; user=GemReconnect ;;
    ohpt-outerhelio-v1) dim=3; lev=5; user=OuterHelio ;;
    ohpt-outerheliopui-v1) dim=3; lev=5; user=OuterHelioPUI ;;
    ohpt-shocktube-v1) dim=3; lev=default; user=OuterHelio ;;
    ohpt-swh-final-v1) dim=3; lev=5; user=OuterHelio ;;
    ohpt-swhpui-final-v1) dim=3; lev=5; user=OuterHelioPUI ;;
    swpc-aepic-v1) dim=3; lev=default; user=Default ;;
    *) echo "run.sh: family lacks explicit dimension/level/user key metadata: $family" >&2; return 2 ;;
  esac
  key="swmf-127a73cb13951351d60e7936583f69f39bd0272e--${family}--dim${dim}--lev${lev}--user${user}--gfortran--${SAB_BUILD_OPT}"
  SAB_BUILD_FAMILY_ROOT="$cache_root/$key"
  SAB_BUILD_SRC="$SAB_BUILD_FAMILY_ROOT/src"
  SAB_RUNTIME_TEMPLATE="$SAB_BUILD_FAMILY_ROOT/runtime-template"
  export SAB_BUILD_OPT SAB_BUILD_FAMILY_ROOT SAB_BUILD_SRC SAB_RUNTIME_TEMPLATE

  if mkdir "$SAB_BUILD_FAMILY_ROOT" 2>/dev/null; then
    start=$(date +%s)
    set +e
    (
      set -e
      mkdir "$SAB_BUILD_SRC"
      cp -R "$SOURCE_DIR/." "$SAB_BUILD_SRC/"
      sab_build_family
      [ -d "$SAB_RUNTIME_TEMPLATE" ]
      sab_make_immutable "$SAB_BUILD_SRC" "$SAB_RUNTIME_TEMPLATE"
    )
    status=$?
    set -e
    if [ "$status" -ne 0 ]; then
      printf 'status=%s\nfamily=%s\n' "$status" "$family" > "$SAB_BUILD_FAMILY_ROOT/.failed"
      echo "run.sh: build-family owner failed: $key" >&2
      for f in "$SAB_BUILD_FAMILY_ROOT"/*.log; do
        [ -f "$f" ] || continue
        echo "== $f" >&2
        tail -40 "$f" >&2
      done
      return "$status"
    fi
    SAB_ACQUIRE_SECONDS=$(( $(date +%s) - start ))
    [ "$SAB_ACQUIRE_SECONDS" -ge 0 ]
    printf '%s\n' "$SAB_ACQUIRE_SECONDS" > "$SAB_BUILD_FAMILY_ROOT/build-seconds"
    printf 'family=%s\nsource_pin=%s\ncompiler=gfortran\noptimization=%s\n' \
      "$family" '127a73cb13951351d60e7936583f69f39bd0272e' "$SAB_BUILD_OPT" \
      > "$SAB_BUILD_FAMILY_ROOT/build-metadata"
    # Source/template were already frozen; freeze top-level logs, metadata, and root too.
    find "$SAB_BUILD_FAMILY_ROOT" -maxdepth 1 -type f -exec chmod a-w {} +
    chmod a-w "$SAB_BUILD_FAMILY_ROOT"
    # Publish a sibling marker atomically as the literal final owner operation.
    mkdir -m 555 "$SAB_BUILD_FAMILY_ROOT.ready"
  else
    waited=0
    while [ ! -d "$SAB_BUILD_FAMILY_ROOT.ready" ]; do
      if [ -f "$SAB_BUILD_FAMILY_ROOT/.failed" ]; then
        echo "run.sh: build-family owner failed: $key" >&2
        sed -n '1,20p' "$SAB_BUILD_FAMILY_ROOT/.failed" >&2
        return 1
      fi
      if [ "$waited" -ge 14400 ]; then
        echo "run.sh: timed out waiting for build family: $key" >&2
        return 1
      fi
      sleep 1
      waited=$((waited + 1))
    done
    SAB_ACQUIRE_SECONDS=0
  fi

  [ -d "$SAB_BUILD_SRC" ] && [ -d "$SAB_RUNTIME_TEMPLATE" ] || {
    echo "run.sh: incomplete ready build family: $key" >&2; return 1;
  }
  if [ "$stage" = stage ]; then
    [ ! -e "$WORK/run" ] || { echo "run.sh: private runtime already exists: $WORK/run" >&2; return 1; }
    mkdir "$WORK/run"
    cp -a "$SAB_RUNTIME_TEMPLATE/." "$WORK/run/"
    sab_make_runtime_writable
  fi
  export SAB_ACQUIRE_SECONDS
}
