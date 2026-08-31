#!/usr/bin/env python3
"""Record the source-owned Phantom test summary and process provenance."""
from pathlib import Path
import hashlib
import json
import re
import sys

SOURCE = "e53ea16758d2a261680506852a528f21270dca1c"
TREE = "ae40f54661feb12f0550092fd2188e5738b7b955"
PASS_RE = re.compile(r"PASSED:\s*(\d+)\s+of\s+(\d+)", re.IGNORECASE)
FAIL_RE = re.compile(r"FAILED:\s*(\d+)\s+of\s+(\d+)", re.IGNORECASE)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main(argv):
    if len(argv) != 7:
        sys.stderr.write("usage: normalize_test.py CHECK SELECTOR TRANSCRIPT RESULT EXECUTION EXECUTABLE\n")
        return 2
    check, selector, transcript_path, result_path, execution_path, executable_path = argv[1:]
    text = Path(transcript_path).read_text(encoding="utf-8")
    passed, failed = PASS_RE.findall(text), FAIL_RE.findall(text)
    if not passed or not failed:
        raise RuntimeError("official transcript contains no PASSED/FAILED summary")
    npass, ntests = map(int, passed[-1])
    nfail, ntests_fail = map(int, failed[-1])
    if ntests <= 0 or ntests != ntests_fail or npass != ntests or nfail != 0:
        raise RuntimeError("official selector did not report positive all-pass/zero-failure counts")
    value = {"schema":"phantom-upstream-test/v1","check":check,"selector":selector,
             "source_commit":SOURCE,"source_tree":TREE,"tests":ntests,"passes":npass,
             "failures":nfail,"passed":True,"official_transcript":"official-test.stdout"}
    Path(result_path).write_text(json.dumps(value,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    execution = {"schema":"phantom-execution-attestation/v1","attested":True,
                 "attestation_kind":"phantomtest-process","check":check,
                 "program":"bin/phantomtest","argv":[selector],"exit_code":0,
                 "source_commit":SOURCE,"source_tree":TREE,
                 "executable":{"path":"bin/phantomtest","sha256":sha(executable_path)},
                 "raw_evidence":{"stdout":"official-test.stdout","stderr":"official-test.stderr",
                                  "stdout_sha256":sha(transcript_path),
                                  "stderr_sha256":sha(Path(transcript_path).with_name("official-test.stderr"))},
                 "result_sha256":sha(result_path)}
    Path(execution_path).write_text(json.dumps(execution,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
