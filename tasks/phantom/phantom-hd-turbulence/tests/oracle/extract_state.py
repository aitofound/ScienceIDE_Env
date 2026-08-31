#!/usr/bin/env python3
"""Canonicalize complete gas particle states and numeric Phantom diagnostics."""
from pathlib import Path
import glob
import json
import os
import re
import struct
import sys

import numpy as np

SOURCE = "e53ea16758d2a261680506852a528f21270dca1c"
STATE_MAGIC = b"PHHDST01"
DIAG_MAGIC = b"PHHDEV01"
VARIABLES = ("x", "y", "z", "vx", "vy", "vz", "h", "u")
HEADER = struct.Struct("<8sIIII")


def main(argv):
    if len(argv) != 7:
        sys.stderr.write("usage: extract_state.py READER WORK PREFIX OUT CHECK SETUP\n")
        return 2
    reader_path, work_text, prefix, out_text, check, setup = argv[1:]
    work, out = Path(work_text), Path(out_text)
    sys.path.insert(0, os.path.dirname(reader_path))
    from readPhantomDump import read_dump

    regex = re.compile(r"^%s_(\d{5})$" % re.escape(prefix))
    dumps = sorted((int(match.group(1)), path) for path in work.iterdir()
                   if path.is_file() and (match := regex.match(path.name)))
    if len(dumps) < 2:
        raise RuntimeError("expected at least two evolved full dumps, found %d" % len(dumps))
    selected = dumps[-2:]
    identities = None
    times = []
    states = []
    for _, path in selected:
        dump = read_dump(str(path))
        blocks = dump.get("blocks", [])
        if not blocks:
            raise RuntimeError("dump %s has no particle block" % path.name)
        data = blocks[0].get("data", {})
        current = np.asarray(data["iorig"], dtype="<i8")
        order = np.argsort(current, kind="stable")
        current = current[order]
        if current.size == 0 or np.unique(current).size != current.size:
            raise RuntimeError("dump %s has empty or duplicate iorig" % path.name)
        if identities is None:
            identities = current
        elif not np.array_equal(identities, current):
            raise RuntimeError("particle identities changed between evolved dumps")
        fields = []
        for variable in VARIABLES:
            if variable == "u" and variable not in data:
                values = np.zeros(current.size, dtype="<f8")
            else:
                values = np.asarray(data[variable], dtype="<f8")
                if values.size != current.size:
                    raise RuntimeError("field %s has wrong particle count" % variable)
                values = values[order]
            fields.append(values)
        states.append(np.stack(fields))
        times.append(float(dump["quantities"]["time"]))
    values = np.asarray(states, dtype="<f8")
    times_array = np.asarray(times, dtype="<f8")
    if not np.isfinite(values).all() or not np.isfinite(times_array).all():
        raise RuntimeError("canonical particle state contains NaN or infinity")
    if not times_array[1] > times_array[0]:
        raise RuntimeError("selected evolved dump times are not increasing")
    with open(out / "state.bin", "xb") as stream:
        stream.write(HEADER.pack(STATE_MAGIC, identities.size, 2, len(VARIABLES), 0))
        stream.write(identities.astype("<i8", copy=False).tobytes(order="C"))
        stream.write(times_array.tobytes(order="C"))
        stream.write(values.tobytes(order="C"))

    evfiles = sorted(work.glob(prefix + "*.ev"))
    if len(evfiles) != 1:
        raise RuntimeError("expected one diagnostic .ev file, found %d" % len(evfiles))
    rows = []
    width = None
    with open(evfiles[0], encoding="utf-8") as stream:
        for line in stream:
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            row = [float(field) for field in line.split()]
            width = len(row) if width is None else width
            if len(row) != width:
                raise RuntimeError("diagnostic table has inconsistent row widths")
            rows.append(row)
    diagnostics = np.asarray(rows, dtype="<f8")
    if diagnostics.ndim != 2 or diagnostics.shape[0] <= 0 or diagnostics.shape[1] <= 0:
        raise RuntimeError("diagnostic table is empty")
    if not np.isfinite(diagnostics).all():
        raise RuntimeError("diagnostic table contains NaN or infinity")
    with open(out / "diagnostics.bin", "xb") as stream:
        stream.write(HEADER.pack(DIAG_MAGIC, diagnostics.shape[0], diagnostics.shape[1], 0, 0))
        stream.write(diagnostics.tobytes(order="C"))

    document = {"schema": "phantom-hd-setup/v1", "check": check, "setup": setup,
                "source_commit": SOURCE, "variables": list(VARIABLES), "snapshots": 2}
    with open(out / "result.json", "x", encoding="utf-8") as stream:
        json.dump(document, stream, indent=2, sort_keys=True)
        stream.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
