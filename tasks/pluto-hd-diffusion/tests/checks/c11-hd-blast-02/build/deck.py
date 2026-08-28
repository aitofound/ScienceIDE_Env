#!/usr/bin/env python3
from pathlib import Path
import hashlib, json, struct, sys
DEFINITIONS = "definitions_02.h"
INI = "pluto_02.ini"


def prepare_external_input(root):
    """Materialize the official external-input contract for the isolated run.

    The Blast/02 deck intentionally exercises PLUTO's InputDataOpen() path.  In
    the upstream campaign, grid0.out and rho0.dbl were made by a preceding
    turbulence job; an isolated Harbor row has no preceding working directory,
    so make a deterministic, finite background field as part of deck
    preprocessing instead of allowing a missing-input failure at runtime.
    """
    nx = ny = 200
    xmin = ymin = -0.5
    dx = dy = 1.0 / nx
    grid = [
        "# PLUTO HD Blast/02 external input grid",
        "# Generated deterministically during Docker image preprocessing",
        "# DIMENSIONS: 2",
        "# GEOMETRY: CARTESIAN",
        f"{nx}",
    ]
    grid.extend(f" {i + 1} {xmin + i * dx:.12e} {xmin + (i + 1) * dx:.12e}"
                for i in range(nx))
    grid.append(str(ny))
    grid.extend(f" {j + 1} {ymin + j * dy:.12e} {ymin + (j + 1) * dy:.12e}"
                for j in range(ny))
    grid.append("1")
    grid.append(" 1 0.000000000000e+00 1.000000000000e+00")
    (root / "grid0.out").write_text("\n".join(grid) + "\n", encoding="utf-8")

    # InputDataOpen() reads one little-endian FP64 value per x1/x2 cell.
    # A weak deterministic perturbation keeps interpolation live while
    # preserving positive density everywhere in this reproducible smoke deck.
    with (root / "rho0.dbl").open("wb") as stream:
        for j in range(ny):
            y = ymin + (j + 0.5) * dy
            for i in range(nx):
                x = xmin + (i + 0.5) * dx
                value = 1.0 + 0.02 * (x + y)
                stream.write(struct.pack("<d", value))


def file_hashes(root):
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.iterdir() if path.is_file()}
def main(argv):
    if len(argv) != 2:
        raise SystemExit("usage: deck.py BUILD_DECK")
    root = Path(argv[1])
    required = [root / "init.c", root / DEFINITIONS, root / INI, root / "definitions.h", root / "pluto.ini"]
    if not all(path.is_file() for path in required):
        raise SystemExit("deck closure is incomplete")
    if (root / "definitions.h").read_bytes() != (root / DEFINITIONS).read_bytes():
        raise SystemExit("definitions.h alias mismatch")
    if (root / "pluto.ini").read_bytes() != (root / INI).read_bytes():
        raise SystemExit("pluto.ini alias mismatch")
    prepare_external_input(root)
    manifest = {"check": "C11", "family": "Test_Problems/HD/Blast", "configuration": "02",
                "external_input": {"grid": "grid0.out", "data": "rho0.dbl",
                                   "generated_during": "Docker image preprocessing",
                                   "grid_points": [200, 200, 1], "dtype": "little-endian FP64"},
                "source_files": file_hashes(root)}
    (root / "deck_manifest.json").write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n", encoding="utf-8")
if __name__ == "__main__":
    main(sys.argv)
