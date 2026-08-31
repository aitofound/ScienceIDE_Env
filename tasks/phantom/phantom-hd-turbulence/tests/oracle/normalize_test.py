#!/usr/bin/env python3
"""Normalize the source-owned Phantom test summary without replacing its policy."""
from pathlib import Path
import json
import re
import sys

SOURCE = "e53ea16758d2a261680506852a528f21270dca1c"
PASS_RE = re.compile(r"PASSED:\s*(\d+)\s+of\s+(\d+)", re.IGNORECASE)
FAIL_RE = re.compile(r"FAILED:\s*(\d+)\s+of\s+(\d+)", re.IGNORECASE)


def main(argv):
    if len(argv) != 5:
        sys.stderr.write("usage: normalize_test.py CHECK SELECTOR TRANSCRIPT OUT\n")
        return 2
    check, selector, transcript_path, out_path = argv[1:]
    text = Path(transcript_path).read_text(encoding="utf-8")
    passed, failed = PASS_RE.findall(text), FAIL_RE.findall(text)
    if not passed or not failed:
        raise RuntimeError("official transcript contains no PASSED/FAILED summary")
    npass, ntests = map(int, passed[-1])
    nfail, ntests_fail = map(int, failed[-1])
    if ntests <= 0 or ntests != ntests_fail or npass != ntests or nfail != 0:
        raise RuntimeError("official selector did not report positive all-pass/zero-failure counts")
    value = {"schema": "phantom-upstream-test/v1", "check": check,
             "selector": selector, "source_commit": SOURCE, "tests": ntests,
             "passes": npass, "failures": nfail, "passed": True}
    with open(out_path, "x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
