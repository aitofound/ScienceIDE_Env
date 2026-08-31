#!/usr/bin/env python3
"""Generate and evolve one official production setup in an isolated directory."""
from pathlib import Path
import json
import re
import subprocess
import sys


def replace_option(text, name, value):
    pattern = re.compile(r"^(\s*%s\s*=\s*)\S+(.*)$" % re.escape(name), re.MULTILINE)
    changed, count = pattern.subn(r"\g<1>%s\2" % value, text)
    if count == 0:
        return text.rstrip() + "\n%s = %s\n" % (name, value)
    if count != 1:
        raise RuntimeError("input option %s occurs %d times" % (name, count))
    return changed


def main(argv):
    if len(argv) != 9:
        sys.stderr.write("usage: run_setup.py SCENARIO SETUP_EXE PHANTOM_EXE READER WORK OUT CHECK EXTRACT\n")
        return 2
    scenario_path, setup_exe, phantom_exe, reader, work_text, out_text, check, extract = argv[1:]
    work, out = Path(work_text), Path(out_text)
    if out.exists():
        raise SystemExit("refusing existing row output: %s" % out)
    out.mkdir(parents=True)
    with open(scenario_path, encoding="utf-8") as stream:
        scenario = json.load(stream)
    prefix = scenario["prefix"]
    setup_text = scenario.get("setup_text", "")
    if setup_text:
        (work / (prefix + ".setup")).write_text(setup_text, encoding="utf-8")
    with open(out / "setup.stdout", "w", encoding="utf-8") as log:
        subprocess.run([setup_exe, prefix], cwd=work, input=scenario.get("stdin", ""),
                       text=True, stdout=log, stderr=subprocess.STDOUT, check=True)
    infile = work / (prefix + ".in")
    if not infile.is_file():
        raise RuntimeError("phantomsetup produced no %s" % infile.name)
    text = infile.read_text(encoding="utf-8")
    text = replace_option(text, "tmax", scenario["tmax"])
    text = replace_option(text, "dtmax", scenario["dtmax"])
    text = replace_option(text, "nfulldump", "1")
    infile.write_text(text, encoding="utf-8")
    initial = list(work.glob(prefix + "_00000*"))
    if not any(path.is_file() for path in initial):
        raise RuntimeError("phantomsetup produced no initial dump for %s" % prefix)
    with open(out / "solver.stdout", "w", encoding="utf-8") as log:
        subprocess.run([phantom_exe, infile.name], cwd=work, stdout=log,
                       stderr=subprocess.STDOUT, check=True)
    subprocess.run([sys.executable, extract, reader, str(work), prefix, str(out),
                    check, scenario["setup"]], check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
