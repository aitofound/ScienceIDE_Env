#!/usr/bin/env python3
"""
Concurrent pass/fail test runner for AMPS validation commands.

===============================================================================
TEST-LIST FILE FORMAT
===============================================================================

The positional ``test_file`` argument is a plain-text list of validation
commands.  The file supports two source-entry forms:

1. the historical scalar form, which describes one command; and
2. an optional ``for`` form, which expands one command template into several
   independent test variants.

A list that contains no ``for`` declarations is parsed and executed with the
historical behavior.  Loop support is additive; it does not change the command,
expected status, metadata, log naming, scheduling, or report semantics of an
ordinary scalar entry.

The examples below use ``P`` for a command expected to pass, meaning exit code
zero, and ``F`` for a command expected to fail, meaning any nonzero exit code.
Timeouts and launch errors are treated as actual ``F`` results.

-------------------------------------------------------------------------------
1. Comments and blank lines
-------------------------------------------------------------------------------

A comment begins when the first non-whitespace character is ``!``::

    ! Run the analytic dipole cutoff tests.
        ! Leading whitespace before ! is allowed.

Blank lines and comment lines are never executed.

Blank lines may appear:

* between a ``for`` declaration and its command template;
* between a command and its optional ``last pass:`` line; or
* between unrelated test entries.

A comment line closes the optional ``last pass:`` association for the preceding
command.  Therefore, put ``last pass:`` before an intervening comment.  A
comment between a ``for`` declaration and its command is allowed and does not
cancel the pending loop declaration.

-------------------------------------------------------------------------------
2. Historical scalar entry
-------------------------------------------------------------------------------

The historical syntax is::

    P <shell command>
    last pass: <optional git commit id>

or::

    F <shell command>
    last pass: <optional git commit id>

Examples::

    P srcEarth/test/C1/run_C1.py --mode gridless -np 4 -nt 16
    last pass: 95469cd297ec8494614c0adefee04da69d6488e6

    F srcEarth/test/experimental/run_expected_failure.py
    last pass:

The ``last pass:`` line is optional.  Its value records the git commit at which
that individual command was last observed to return exit code zero.  It is
provenance, not the expected status: an entry marked ``F`` may still have a
non-empty ``last pass:`` value from an earlier revision.

For scalar entries:

* ``P`` or ``F`` must be the first non-whitespace token;
* everything after that token is retained as the command string;
* the command is not rewritten by this runner;
* a scalar ``last pass:`` value belongs only to that command; and
* ``--update-last-pass`` keeps the scalar metadata format.

-------------------------------------------------------------------------------
3. One-variable loop entry
-------------------------------------------------------------------------------

A loop declaration applies to the next non-comment test command only::

    for $m={RK4,HC4}
    {P,P} srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN \
        --cutoff-scan-n 200 --mover $m
    last pass: {commit-for-RK4,commit-for-HC4}

The example expands, in order, to::

    P srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN \
        --cutoff-scan-n 200 --mover RK4

    P srcEarth/test/C3/run_C3.py --algorithms UPPER_SCAN \
        --cutoff-scan-n 200 --mover HC4

Each expansion is a fully independent test.  It receives its own:

* sequential test index;
* expected P/F value;
* timeout and exit-status classification;
* scheduler slot;
* log file;
* elapsed time;
* JSON and CSV report row;
* terminal start/completion message; and
* ``last pass`` provenance element.

The source command line is not executed directly.  Only its expanded commands
are scheduled.

-------------------------------------------------------------------------------
4. Loop declaration grammar
-------------------------------------------------------------------------------

The accepted loop declaration is conceptually::

    for $name={value1,value2,...} [$other={value1,value2,...} ...]

Variable names must match::

    [A-Za-z_][A-Za-z0-9_]*

Valid examples::

    for $m={RK4,HC4}
    for $policy={LEGACY,ACCURATE}
    for $m={RK4,HC4}, $policy={LEGACY,ACCURATE}
    for $np={1,2,4} $nt={1,8,16}

Spaces and commas may separate assignments.  Variable names may not be
repeated on one declaration.  Empty value lists and empty loop values are not
accepted.

Loop values are comma-separated using Python's CSV parser.  This permits a
quoted value to contain a comma, for example::

    for $label={plain,"value,with,commas"}

Whitespace surrounding an unquoted value is removed.  Braces are structural
and nested braces are not supported.

A loop declaration is an instruction to this test runner; it is not a shell
``for`` command and is never passed to ``/bin/sh``.

-------------------------------------------------------------------------------
5. Variable references and substitution
-------------------------------------------------------------------------------

A command template may reference a declared variable as either::

    $name
    ${name}

Example::

    for $m={RK4,HC4}
    P srcEarth/test/C3/run_C3.py --mover ${m}

Substitution is textual and occurs before the command is scheduled.  Values are
inserted exactly as written in the loop declaration.  Therefore, quote loop
values or command positions appropriately when a value contains shell-special
characters or whitespace.

Every variable declared by the pending loop must appear at least once in the
command template.  A declaration such as::

    for $m={RK4,HC4}
    P command-without-a-mover-option

is rejected because it would create duplicate commands accidentally.

References to variables that were not declared by the pending loop are left
unchanged.  This preserves legitimate shell references such as ``$HOME`` or
``${PATH}``.  The list parser does not perform shell environment expansion;
the shell sees any undeclared references when the command is eventually run.

A loop declaration is consumed by exactly one command.  A second ``for`` line
before a command, or a ``for`` line at end of file with no following command,
is an error.

-------------------------------------------------------------------------------
6. Multiple variables and expansion order
-------------------------------------------------------------------------------

More than one variable creates a Cartesian product::

    for $m={RK4,HC4} $policy={LEGACY,ACCURATE}
    P command --mover $m --cutoff-trace-policy $policy

The expansion order is deterministic and follows declaration order, with the
rightmost variable changing fastest::

    1. m=RK4, policy=LEGACY
    2. m=RK4, policy=ACCURATE
    3. m=HC4, policy=LEGACY
    4. m=HC4, policy=ACCURATE

This order is important because the expected-status vector and ``last pass``
vector are positional and must use this same order.

The total variant count is the product of the value-list lengths.  For example,
three movers and two trace policies produce six tests.

-------------------------------------------------------------------------------
7. Expected-result syntax for loops
-------------------------------------------------------------------------------

A looped command accepts either a scalar expected state or a vector.

Scalar form::

    for $m={RK4,HC4}
    P command --mover $m

The scalar is replicated, so the example is equivalent to ``{P,P}``.

Per-variant form::

    for $m={RK4,HC4}
    {P,F} command --mover $m

The vector length must exactly equal the number of expanded commands.  Every
item must be ``P`` or ``F``.  A vector status without a preceding ``for``
declaration is invalid.

For a Cartesian product, the status vector follows the expansion order shown
above.  For example::

    for $m={RK4,HC4} $policy={LEGACY,ACCURATE}
    {P,P,F,P} command --mover $m --policy $policy

assigns the four statuses to variants 1 through 4 in that order.

-------------------------------------------------------------------------------
8. ``last pass:`` syntax for loops
-------------------------------------------------------------------------------

Looped entries accept scalar or vector provenance.

Scalar shorthand::

    last pass: abc123

means that every expansion initially receives ``abc123`` as its known-passing
commit.  This shorthand is convenient when all variants were validated at the
same revision.

Explicit vector::

    last pass: {abc123,def456}

assigns one value to each expansion.  The vector length must equal the variant
count and uses the same deterministic expansion order as the status vector.

Empty vector elements are allowed and mean that no passing commit is known for
that variant::

    last pass: {abc123,,def456}

Unmatched braces or the wrong number of elements are errors.  Commit strings
are treated as opaque metadata; the runner does not require a particular hash
length.

A ``last pass:`` line belongs to the immediately preceding source command entry
after ignoring blank lines.  Duplicate metadata lines for one source entry are
rejected.

-------------------------------------------------------------------------------
9. ``--update-last-pass`` behavior
-------------------------------------------------------------------------------

After execution, ``--update-last-pass`` obtains the selected commit id and
updates only variants whose actual result is ``P``.  Expected status does not
control the update: even a command marked expected ``F`` is updated if it
actually exits zero.

For a scalar entry::

    P command
    last pass: old

becomes::

    P command
    last pass: new-commit

when the command passes.

For a looped entry, metadata is always written back as an explicit vector so
that later partial updates remain possible.  If only RK4 passes::

    for $m={RK4,HC4}
    {P,P} command --mover $m
    last pass: {old-rk4,old-hc4}

becomes::

    for $m={RK4,HC4}
    {P,P} command --mover $m
    last pass: {new-commit,old-hc4}

A failing variant preserves its previous value.  If it had no previous value,
its vector element remains empty.  If metadata was absent, a new scalar or
vector ``last pass:`` line is inserted immediately below the source command.
The file is rewritten through a temporary sibling file and then atomically
replaced.

-------------------------------------------------------------------------------
10. Compact grammar summary
-------------------------------------------------------------------------------

The effective list grammar is::

    file          := { blank | comment | entry }
    comment       := optional-space "!" text
    entry         := [ loop-declaration blank-or-comment* ] test-line
                     [ blank* last-pass-line ]
    loop-decl     := "for" assignment { separator assignment }
    assignment    := "$" name "={" csv-values "}"
    test-line     := expected whitespace command
    expected      := "P" | "F" | "{" status-list "}"
    status-list   := status { "," status }
    status        := "P" | "F"
    last-pass     := "last pass:" [ scalar | "{" csv-values-with-empty "}" ]

This is descriptive rather than a shell grammar.  Actual validation is stricter
in several useful ways: undeclared duplicate loop variables are rejected,
declared variables must be referenced, vector sizes must match the expansion
count, and unexpected text on a loop line is rejected rather than ignored.

===============================================================================
HOW LOOP ENTRIES ARE IMPLEMENTED
===============================================================================

The implementation deliberately expands loops at parse time instead of adding
special cases to the scheduler or process runner.

1. ``parse_test_file()`` reads source lines and maintains two temporary states:
   ``pending_loop`` for a declaration waiting for its command, and
   ``pending_metadata_tests`` for the just-created scalar test or variant group
   that may receive a ``last pass:`` line.

2. ``_parse_loop_declaration()`` validates assignments and their CSV value
   lists.  Duplicate names, empty lists, malformed separators, unexpected
   trailing text, and nested/unmatched braces are rejected with source line
   numbers.

3. ``_expand_loop_bindings()`` uses ``itertools.product`` to create a stable
   Cartesian product.  Python product order gives the documented rule that the
   rightmost value list changes fastest.

4. ``_substitute_loop_variables()`` replaces declared ``$name`` and
   ``${name}`` references.  It first verifies that every declared name is used.
   Undeclared references remain untouched for later shell expansion.

5. ``_parse_expected_values()`` and ``_parse_last_pass_values()`` replicate a
   scalar or validate a positional vector against the exact variant count.

6. Each expansion becomes an ordinary ``TestCase``.  In addition to the final
   command and expected result, it retains the source template, source line,
   loop line, ordered bindings, one-based variant index, and total variant
   count.  Ordinary entries have empty bindings and variant count one.

7. From that point forward, the dispatcher treats every variant like any other
   test.  There is no loop-aware subprocess path.  This ensures that timeouts,
   memory pacing, ``-j`` concurrency, environment handling, exit-code mapping,
   logs, JSON/CSV output, and triage reports have identical semantics for looped
   and non-looped commands.

8. Log names include the expanded-test index and command source information, so
   variants do not overwrite each other.  Human-readable output and reports add
   a stable suffix such as ``variant 2/4 [m=RK4, policy=ACCURATE]``.

9. ``update_last_pass_entries()`` groups expanded tests by the original command
   source line.  It updates elements by one-based variant index, preserves
   failed variants, and writes one metadata line for the complete source entry.

10. Because loops are flattened before scheduling, the job limit counts
    variants, not source templates.  A two-mover entry consumes two ordinary
    test positions and may run concurrently if the scheduler permits it.

===============================================================================
GENERAL RUNNER PURPOSE AND SCHEDULING
===============================================================================

The runner has five main uses:

1. Run the validation list and report mismatches between expected and actual
   P/F states.
2. Run independent test wrappers concurrently without changing those wrappers.
3. Protect physical memory while several MPI/OpenMP AMPS commands coexist.
4. Maintain per-command or per-variant ``last pass`` provenance.
5. Expand one command template over movers or future CLI parameters without
   duplicating the full entry.

Concurrency is governed by two independent mechanisms:

* ``-j``/``--jobs`` is a hard ceiling on simultaneously running expanded
  commands.
* Unless ``--no-memory-gate`` is used, a physical-memory ramp-up gate controls
  when another command may start.

The runner never changes a command's ``-np``, ``-nt``, mover, or other CLI
arguments.  Parsed MPI-rank and thread counts are used only for reporting and,
when requested, thread-count environment variables.

Memory-gated launch sequence:

1. Start a command when no runner-owned command is active, or when both a job
   slot and the memory gate permit it.
2. Wait ``--memory-check-interval`` seconds before ramping up beside active
   commands.
3. Read physical-memory availability.
4. Launch another command only when available memory exceeds
   ``--min-free-memory-fraction`` of total memory.
5. If memory is below the threshold, keep waiting and rechecking; do not rewrite
   or discard a test.

The first command after the pool becomes empty launches immediately because
there is no active workload against which to pace it.  Short tests that finish
before the next memory check may therefore run effectively serially unless the
check interval is reduced.

Available memory comes from Linux ``/proc/meminfo``.  ``MemAvailable`` is used
when present; older kernels fall back to ``MemFree + Buffers + Cached``.  On a
system without ``/proc/meminfo``, use ``--no-memory-gate``.

By default the runner does not modify ``OMP_NUM_THREADS`` or BLAS thread-count
environment variables.  ``--set-thread-env`` sets them to the command's parsed
``-nt`` value, while ``--overwrite-thread-env`` permits replacing values already
present in the environment.  Neither option changes the command line.

===============================================================================
RESULTS, REPORTS, AND PROVENANCE
===============================================================================

Actual result classification is:

* exit code 0 -> actual ``P``;
* nonzero exit code -> actual ``F``;
* timeout -> actual ``F``; and
* process-launch error -> actual ``F``.

The expected marker and actual result are compared independently.  Reports
retain both values, the command, loop bindings, source and variant indices,
parsed ``-np``/``-nt``, runtime, timeout state, exit code, launch-time memory,
log path, and provenance.

The standard reports include JSON and CSV output plus triage subsets:

``<prefix>_to_address``
    Actual P/F does not match the expected marker.

``<prefix>_actual_failed``
    Every actual failure, including failures that were expected.

``<prefix>_last_pass_failed``
    Actual failures whose scalar or per-variant ``last pass`` value is nonempty;
    these are regressions relative to a previously passing revision.

``<prefix>_new_or_unexpected_passed``
    Actual passes with missing/empty provenance, or commands currently marked
    expected ``F`` that unexpectedly passed.

Progress is printed at command launch, while waiting for memory, and when each
command completes.  Looped variants are labeled with their ordered bindings so
an RK4 failure cannot be confused with the HC4 expansion of the same source
entry.
"""

from __future__ import annotations

import argparse
import asyncio
import csv
import json
import os
import re
import shlex
import signal
import subprocess
import sys
import time
from dataclasses import asdict, dataclass, field
from itertools import product
from pathlib import Path
from typing import Iterable, List, Optional


@dataclass
class TestCase:
    index: int
    line_no: int
    expected: str  # "P" or "F"
    command: str
    last_pass: Optional[str] = None
    last_pass_line_no: Optional[int] = None

    # Loop-expanded tests retain enough source information to make reports and
    # --update-last-pass unambiguous.  variant_index is one-based so it matches
    # the order visible in {P,F,...} and last pass: {hash,...} list syntax.
    template_command: Optional[str] = None
    loop_line_no: Optional[int] = None
    loop_bindings: dict[str, str] = field(default_factory=dict)
    variant_index: int = 1
    variant_count: int = 1


@dataclass
class TestResult:
    index: int
    line_no: int
    expected: str
    actual: str
    matched_reference: bool
    exit_code: Optional[int]
    timed_out: bool
    elapsed_s: float
    command: str
    log_file: str
    np_value: int = 1
    nt_value: int = 1
    np_nt_details: str = ""
    mem_total_bytes: Optional[int] = None
    mem_available_bytes_at_launch: Optional[int] = None
    mem_available_fraction_at_launch: Optional[float] = None
    last_pass: Optional[str] = None
    last_pass_line_no: Optional[int] = None
    template_command: Optional[str] = None
    loop_line_no: Optional[int] = None
    loop_bindings: dict[str, str] = field(default_factory=dict)
    variant_index: int = 1
    variant_count: int = 1


@dataclass
class TestPlan:
    """Per-test info the dispatcher and ``run_one_test`` need.

    There is no launch-time command rewriting: ``np_value``/``nt_value`` are
    parsed only for reporting and for ``--set-thread-env``. The command that
    actually runs is always ``test.command``, unmodified.
    """

    test: TestCase
    np_value: int
    nt_value: int
    np_nt_details: str


LAST_PASS_RE = re.compile(r"^(?P<prefix>\s*last\s+pass\s*:\s*)(?P<value>.*)$", re.IGNORECASE)
TEST_LINE_RE = re.compile(
    r"^\s*(?P<expected>P|F|\{[^{}]*\})\s+(?P<command>.+?)\s*$",
    re.IGNORECASE,
)
LOOP_LINE_RE = re.compile(r"^\s*for\b(?P<body>.*)$", re.IGNORECASE)
LOOP_ASSIGN_RE = re.compile(
    r"\$(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{(?P<values>[^{}]*)\}"
)
VARIABLE_REF_RE = re.compile(
    r"\$(?:\{(?P<braced>[A-Za-z_][A-Za-z0-9_]*)\}|"
    r"(?P<plain>[A-Za-z_][A-Za-z0-9_]*))"
)


def _parse_comma_list(text: str, *, what: str, allow_empty: bool) -> List[str]:
    """Parse one comma-separated list used inside ``{...}`` syntax.

    The standard :mod:`csv` parser is used intentionally so quoted values such
    as ``"--label=a,b"`` can contain a comma.  Whitespace around unquoted
    values is ignored.  Empty items are permitted only for ``last pass`` lists,
    where an empty field means that the corresponding loop variant has no known
    passing commit yet.
    """
    try:
        rows = list(csv.reader([text], skipinitialspace=True))
    except csv.Error as exc:
        raise ValueError(f"invalid {what} list {{{text}}}: {exc}") from exc

    if len(rows) != 1:
        raise ValueError(f"invalid {what} list {{{text}}}")

    values = [value.strip() for value in rows[0]]
    if not values or (len(values) == 1 and values[0] == ""):
        if allow_empty:
            return [""]
        raise ValueError(f"{what} list must contain at least one value")
    if not allow_empty and any(value == "" for value in values):
        raise ValueError(f"{what} list contains an empty value")
    return values


def _parse_loop_declaration(raw: str, line_no: int) -> List[tuple[str, List[str]]]:
    """Parse ``for $name={v1,v2}`` assignments from one list-file line.

    More than one assignment is allowed.  Their Cartesian product is expanded
    in declaration order, with the rightmost variable changing fastest.  For
    example ``for $m={RK4,HC4} $dt={1,0.5}`` produces four variants:
    ``(RK4,1)``, ``(RK4,0.5)``, ``(HC4,1)``, and ``(HC4,0.5)``.
    """
    match = LOOP_LINE_RE.match(raw)
    if match is None:
        raise ValueError(f"Invalid loop declaration on line {line_no}: {raw.rstrip()}")

    body = match.group("body")
    assignments: List[tuple[str, List[str]]] = []
    seen: set[str] = set()
    cursor = 0

    for assignment in LOOP_ASSIGN_RE.finditer(body):
        # Only whitespace and optional commas may separate assignments.  This
        # catches misspellings instead of silently ignoring part of a loop line.
        gap = body[cursor:assignment.start()]
        if gap.strip(" \t,"):
            raise ValueError(
                f"Invalid loop declaration on line {line_no}: unexpected text {gap!r}"
            )

        name = assignment.group("name")
        if name in seen:
            raise ValueError(
                f"Invalid loop declaration on line {line_no}: duplicate variable ${name}"
            )
        seen.add(name)

        values = _parse_comma_list(
            assignment.group("values"),
            what=f"values for ${name}",
            allow_empty=False,
        )
        assignments.append((name, values))
        cursor = assignment.end()

    tail = body[cursor:]
    if tail.strip(" \t,"):
        raise ValueError(
            f"Invalid loop declaration on line {line_no}: unexpected text {tail!r}"
        )
    if not assignments:
        raise ValueError(
            f"Invalid loop declaration on line {line_no}: expected syntax "
            "'for $name={value1,value2}'"
        )
    return assignments


def _expand_loop_bindings(
    assignments: List[tuple[str, List[str]]],
) -> List[dict[str, str]]:
    """Return deterministic Cartesian-product bindings for a loop declaration."""
    names = [name for name, _values in assignments]
    value_sets = [values for _name, values in assignments]
    return [dict(zip(names, combination)) for combination in product(*value_sets)]


def _substitute_loop_variables(
    template: str,
    bindings: dict[str, str],
    *,
    line_no: int,
) -> str:
    """Substitute declared ``$name``/``${name}`` references in a command.

    References to undeclared shell variables are deliberately left untouched.
    Every declared loop variable must appear in the command template; otherwise
    the parser fails instead of launching duplicate commands accidentally.
    """
    referenced = {
        match.group("braced") or match.group("plain")
        for match in VARIABLE_REF_RE.finditer(template)
    }
    missing = [name for name in bindings if name not in referenced]
    if missing:
        names = ", ".join(f"${name}" for name in missing)
        raise ValueError(
            f"Invalid looped test on line {line_no}: declared variable(s) {names} "
            "are not referenced by the command"
        )

    def replace(match: re.Match[str]) -> str:
        name = match.group("braced") or match.group("plain")
        return bindings.get(name, match.group(0))

    return VARIABLE_REF_RE.sub(replace, template)


def _parse_expected_values(token: str, count: int, *, line_no: int) -> List[str]:
    """Expand scalar ``P``/``F`` or validate a per-variant ``{P,F,...}`` list."""
    upper = token.upper()
    if upper in {"P", "F"}:
        return [upper] * count

    if not (token.startswith("{") and token.endswith("}")):
        raise ValueError(f"Invalid expected-status token on line {line_no}: {token}")
    values = [
        value.upper()
        for value in _parse_comma_list(
            token[1:-1], what="expected-status", allow_empty=False
        )
    ]
    invalid = [value for value in values if value not in {"P", "F"}]
    if invalid:
        raise ValueError(
            f"Invalid expected-status list on line {line_no}: values must be P or F"
        )
    if len(values) != count:
        raise ValueError(
            f"Invalid expected-status list on line {line_no}: got {len(values)} "
            f"value(s), but the loop expands to {count} command(s)"
        )
    return values


def _parse_last_pass_values(value: str, count: int, *, line_no: int) -> List[str]:
    """Parse scalar or per-variant provenance for one source test entry.

    A scalar commit on a looped entry is accepted as shorthand for the same
    known-passing commit on every expansion.  ``--update-last-pass`` rewrites
    looped entries in explicit ``{hash1,hash2,...}`` form so later partial
    updates remain possible.
    """
    stripped = value.strip()
    if stripped.startswith("{") or stripped.endswith("}"):
        if not (stripped.startswith("{") and stripped.endswith("}")):
            raise ValueError(
                f"Invalid 'last pass:' metadata on line {line_no}: unmatched braces"
            )
        values = _parse_comma_list(
            stripped[1:-1], what="last-pass", allow_empty=True
        )
        if len(values) != count:
            raise ValueError(
                f"Invalid 'last pass:' list on line {line_no}: got {len(values)} "
                f"value(s), but the loop expands to {count} command(s)"
            )
        return values

    return [stripped] * count


def format_loop_bindings(bindings: dict[str, str]) -> str:
    """Return a compact stable label such as ``m=RK4, dt=0.5`` for reports."""
    return ", ".join(f"{name}={value}" for name, value in bindings.items())


def format_variant_suffix(
    bindings: dict[str, str], variant_index: int, variant_count: int
) -> str:
    """Return a user-facing loop-variant suffix, or an empty string."""
    if not bindings:
        return ""
    return (
        f" variant {variant_index}/{variant_count} "
        f"[{format_loop_bindings(bindings)}]"
    )


def _parse_positive_int(text: str) -> Optional[int]:
    """Parse an integer-like value and reject zero/negative values.

    Some wrapper scripts pass values that look like floats, e.g. ``"4.0"``.
    Accepting ``int(float(x))`` makes the parser tolerant without allowing
    invalid nonpositive values.
    """
    try:
        value = int(float(str(text)))
    except (TypeError, ValueError):
        return None
    return value if value > 0 else None


def _option_values(argv: List[str], names: Iterable[str]) -> List[str]:
    """Return values for options that can appear as ``--x v`` or ``--x=v``."""
    names = set(names)
    values: List[str] = []
    i = 0
    while i < len(argv):
        token = argv[i]
        if token in names and i + 1 < len(argv):
            values.append(argv[i + 1])
            i += 2
            continue
        for name in names:
            prefix = name + "="
            if token.startswith(prefix):
                values.append(token[len(prefix):])
                break
        i += 1
    return values


def _first_positive_option(argv: List[str], names: Iterable[str]) -> Optional[int]:
    """Return the first positive integer value found for any option name."""
    for value in _option_values(argv, names):
        parsed = _parse_positive_int(value)
        if parsed is not None:
            return parsed
    return None


def parse_np_nt(command: str, default_np: int, default_nt: int) -> tuple[int, int, str]:
    """Parse MPI rank count and thread count out of a command, for reporting only.

    This used to feed a CPU-slot scheduling estimate; it no longer does.
    Nothing in this module uses the returned values to change scheduling or
    to rewrite a command. They are kept so reports stay informative and so
    ``--set-thread-env`` still has an ``-nt`` value to propagate into
    thread-count environment variables.
    """
    try:
        argv = shlex.split(command, posix=True)
    except ValueError:
        argv = command.split()

    np_value = _first_positive_option(argv, ["-np", "-n", "--np", "--n", "--mpi-ranks"])
    nt_value = _first_positive_option(
        argv,
        [
            "-nt",
            "--nt",
            "--threads",
            "--mode3d-threads",
            "--density-threads",
            "--omp-threads",
        ],
    )

    if np_value is None:
        np_value = max(1, default_np)
    if nt_value is None:
        nt_value = max(1, default_nt)

    details = f"np={np_value}, nt={nt_value}"
    return np_value, nt_value, details


def set_thread_env(base_env: dict, nt: int, only_if_missing: bool) -> dict:
    """Return environment for one command with optional thread-count controls.

    AMPS wrappers normally receive ``-nt`` explicitly, but imported numerical
    libraries can still start their own thread pools. Setting these variables
    is optional because some HPC environments already configure them
    carefully. ``only_if_missing`` preserves existing settings unless the
    user requested ``--overwrite-thread-env``.
    """
    env = base_env.copy()
    if nt < 1:
        return env
    for name in ("OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS", "NUMEXPR_NUM_THREADS"):
        if only_if_missing and env.get(name):
            continue
        env[name] = str(nt)
    return env


def parse_test_file(path: Path) -> List[TestCase]:
    """Parse legacy P/F entries and optional one-entry loop declarations.

    Backward-compatible scalar entry::

        P command --option value
        last pass: abc123

    Generic loop entry::

        for $m={RK4,HC4}
        {P,P} command --mover $m
        last pass: {abc123,def456}

    A loop declaration applies to the next non-comment test command only.  Each
    expansion becomes an independent :class:`TestCase`, so scheduling, logs,
    reports, timeouts, and exit-status comparisons work exactly as they do for
    ordinary entries.  The source line and variant index are retained so
    ``--update-last-pass`` can update the matching element of the metadata list.
    """
    tests: List[TestCase] = []
    pending_metadata_tests: Optional[List[TestCase]] = None
    pending_loop: Optional[List[tuple[str, List[str]]]] = None
    pending_loop_line_no: Optional[int] = None

    with path.open("r", encoding="utf-8", errors="replace") as f:
        for line_no, raw in enumerate(f, start=1):
            line = raw.strip()

            if not line:
                # Blank lines are ignored. They may separate a loop declaration
                # from its command or a command from its metadata.
                continue

            if line.startswith("!"):
                # Preserve the historical rule that a comment ends the metadata
                # association with the preceding command. A comment between a
                # loop declaration and its command is allowed and does not discard
                # the pending loop.
                pending_metadata_tests = None
                continue

            last_pass_match = LAST_PASS_RE.match(raw.rstrip("\n"))
            if last_pass_match:
                if pending_metadata_tests is None:
                    raise ValueError(
                        f"Invalid test-list line {line_no}: 'last pass:' must "
                        "immediately follow a P/F command or loop-expanded command"
                    )
                if any(test.last_pass_line_no is not None for test in pending_metadata_tests):
                    raise ValueError(
                        f"Invalid test-list line {line_no}: duplicate 'last pass:' "
                        f"metadata for test from line {pending_metadata_tests[0].line_no}"
                    )

                values = _parse_last_pass_values(
                    last_pass_match.group("value"),
                    len(pending_metadata_tests),
                    line_no=line_no,
                )
                for test, value in zip(pending_metadata_tests, values):
                    test.last_pass = value
                    test.last_pass_line_no = line_no
                pending_metadata_tests = None
                continue

            loop_match = LOOP_LINE_RE.match(raw.rstrip("\n"))
            if loop_match:
                # A loop declaration is a new source entry and therefore closes
                # the optional metadata window for the preceding command.
                pending_metadata_tests = None
                if pending_loop is not None:
                    raise ValueError(
                        f"Invalid test-list line {line_no}: loop from line "
                        f"{pending_loop_line_no} has no following test command"
                    )
                pending_loop = _parse_loop_declaration(raw.rstrip("\n"), line_no)
                pending_loop_line_no = line_no
                continue

            match = TEST_LINE_RE.match(raw.rstrip("\n"))
            if match is None:
                raise ValueError(
                    f"Invalid test-list line {line_no}: expected 'P <command>', "
                    f"'F <command>', or a looped '{{P,F,...}} <command>' entry; "
                    f"got: {raw.rstrip()}"
                )

            expected_token = match.group("expected")
            template_command = match.group("command").strip()

            if pending_loop is None:
                if expected_token.startswith("{"):
                    raise ValueError(
                        f"Invalid test-list line {line_no}: a vector expected-status "
                        "marker requires a preceding 'for $name={...}' declaration"
                    )
                bindings_list = [{}]
            else:
                bindings_list = _expand_loop_bindings(pending_loop)

            expected_values = _parse_expected_values(
                expected_token,
                len(bindings_list),
                line_no=line_no,
            )

            expanded_tests: List[TestCase] = []
            for variant_index, (bindings, expected) in enumerate(
                zip(bindings_list, expected_values), start=1
            ):
                command = (
                    template_command
                    if not bindings
                    else _substitute_loop_variables(
                        template_command,
                        bindings,
                        line_no=line_no,
                    )
                )
                test = TestCase(
                    index=len(tests) + 1,
                    line_no=line_no,
                    expected=expected,
                    command=command,
                    template_command=template_command if bindings else None,
                    loop_line_no=pending_loop_line_no if bindings else None,
                    loop_bindings=dict(bindings),
                    variant_index=variant_index,
                    variant_count=len(bindings_list),
                )
                tests.append(test)
                expanded_tests.append(test)

            pending_metadata_tests = expanded_tests
            pending_loop = None
            pending_loop_line_no = None

    if pending_loop is not None:
        raise ValueError(
            f"Invalid test list: loop declaration on line {pending_loop_line_no} "
            "has no following test command"
        )

    return tests


def safe_log_name(test: TestCase) -> str:
    # Keep names deterministic and readable while avoiding shell metacharacters.
    words = shlex.split(test.command, posix=True) if test.command.strip() else []
    base = Path(words[0]).name if words else "command"
    base = "".join(ch if ch.isalnum() or ch in {"-", "_", "."} else "_" for ch in base)
    return f"test_{test.index:03d}_line_{test.line_no}_{base}.log"


def read_proc_meminfo(path: str = "/proc/meminfo") -> dict[str, int]:
    """Parse /proc/meminfo into a dict of field name -> value in bytes."""
    info: dict[str, int] = {}
    unit_multiplier = {"b": 1, "kb": 1024, "mb": 1024**2, "gb": 1024**3}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if ":" not in line:
                continue
            key, _, rest = line.partition(":")
            tokens = rest.split()
            if not tokens:
                continue
            try:
                value = int(tokens[0])
            except ValueError:
                continue
            unit = tokens[1].lower() if len(tokens) > 1 else "kb"
            info[key.strip()] = value * unit_multiplier.get(unit, 1024)
    return info


def read_memory_totals() -> tuple[int, int]:
    """Return (total_bytes, available_bytes) for physical memory.

    Uses ``/proc/meminfo``'s ``MemAvailable`` when present (Linux 3.14+),
    which already accounts for reclaimable page cache/buffers and is a far
    better estimate of real headroom than ``MemFree`` alone. Falls back to
    ``MemFree + Buffers + Cached`` on older kernels. Raises ``RuntimeError``
    with an actionable message if memory cannot be read at all (e.g.
    non-Linux), so callers can fail fast instead of silently mis-scheduling.
    """
    try:
        info = read_proc_meminfo()
    except OSError as exc:
        raise RuntimeError(
            "could not read /proc/meminfo to track available physical memory "
            "(memory tracking currently requires Linux); pass --no-memory-gate "
            "to run with plain -j concurrency and no memory checks"
        ) from exc

    total = info.get("MemTotal")
    if not total:
        raise RuntimeError("/proc/meminfo did not report a usable MemTotal")

    available = info.get("MemAvailable")
    if available is None:
        available = info.get("MemFree", 0) + info.get("Buffers", 0) + info.get("Cached", 0)

    return total, available


def format_bytes(n: int) -> str:
    """Human-readable binary size, e.g. 134744072192 -> '125.5 GiB'."""
    value = float(n)
    for unit in ("B", "KiB", "MiB", "GiB"):
        if value < 1024.0:
            return f"{value:.1f} {unit}"
        value /= 1024.0
    return f"{value:.1f} TiB"


def build_test_plan(test: TestCase, *, default_np: int, default_nt: int) -> TestPlan:
    np_value, nt_value, details = parse_np_nt(test.command, default_np, default_nt)
    return TestPlan(test=test, np_value=np_value, nt_value=nt_value, np_nt_details=details)


async def run_one_test(
    plan: TestPlan,
    *,
    workdir: Path,
    log_dir: Path,
    timeout_s: Optional[float],
    use_shell: bool,
    base_env: dict,
    set_thread_env_vars: bool,
    preserve_thread_env: bool,
    print_start: bool,
    mem_total_bytes: Optional[int],
    mem_available_bytes: Optional[int],
) -> TestResult:
    """Run one already-scheduled command, exactly as written in the test list.

    This coroutine does not wait on any throttle itself; ``run_all_tests``
    decides when a ``-j`` slot and the memory gate both allow a launch, then
    creates this task. Nothing here -- or anywhere else in this module --
    ever rewrites ``plan.test.command``: it always runs with the same
    ``-np``/``-nt``/other options it was authored with.
    """
    test = plan.test
    log_path = log_dir / safe_log_name(test)
    start = time.monotonic()
    exit_code: Optional[int] = None
    timed_out = False

    env = set_thread_env(base_env, plan.nt_value, preserve_thread_env) if set_thread_env_vars else base_env

    mem_fraction = (
        mem_available_bytes / mem_total_bytes
        if (mem_total_bytes and mem_available_bytes is not None)
        else None
    )

    if print_start:
        if mem_fraction is not None:
            mem_note = (
                f"mem_avail={mem_fraction:.0%} "
                f"({format_bytes(mem_available_bytes)}/{format_bytes(mem_total_bytes)}), "
            )
        else:
            mem_note = ""
        loop_note = (
            f" variant {test.variant_index}/{test.variant_count} "
            f"[{format_loop_bindings(test.loop_bindings)}]"
            if test.loop_bindings
            else ""
        )
        print(
            f"[START] #{test.index:03d} line {test.line_no}{loop_note}: "
            f"{mem_note}command={test.command}",
            flush=True,
        )

    if mem_fraction is not None:
        mem_line = (
            f"# Memory at launch: {mem_fraction:.1%} available "
            f"({format_bytes(mem_available_bytes)} / {format_bytes(mem_total_bytes)})\n"
        )
    else:
        mem_line = "# Memory at launch: not tracked (--no-memory-gate)\n"

    with log_path.open("wb") as log:
        loop_header = (
            f"# Loop variant: {test.variant_index}/{test.variant_count}; "
            f"{format_loop_bindings(test.loop_bindings)}\n"
            if test.loop_bindings
            else ""
        )
        template_header = (
            f"# Command template: {test.template_command}\n"
            if test.template_command is not None
            else ""
        )
        header = (
            f"# Test {test.index} from line {test.line_no}\n"
            f"{loop_header}"
            f"# Expected: {test.expected}\n"
            f"{template_header}"
            f"# Command: {test.command}\n"
            f"# Workdir: {workdir}\n"
            f"# {plan.np_nt_details}\n"
            f"{mem_line}"
            f"# Started: {time.strftime('%Y-%m-%d %H:%M:%S')}\n"
            f"{'=' * 80}\n"
        )
        log.write(header.encode("utf-8", errors="replace"))
        log.flush()

        try:
            if use_shell:
                # start_new_session=True makes timeout cleanup kill the whole
                # process group, including mpirun-launched children when they
                # remain in the same session.
                proc = await asyncio.create_subprocess_shell(
                    test.command,
                    cwd=str(workdir),
                    stdout=log,
                    stderr=asyncio.subprocess.STDOUT,
                    env=env,
                    start_new_session=True,
                )
            else:
                argv = shlex.split(test.command)
                if not argv:
                    raise ValueError("empty command")
                proc = await asyncio.create_subprocess_exec(
                    *argv,
                    cwd=str(workdir),
                    stdout=log,
                    stderr=asyncio.subprocess.STDOUT,
                    env=env,
                    start_new_session=True,
                )

            try:
                exit_code = await asyncio.wait_for(proc.wait(), timeout=timeout_s)
            except asyncio.TimeoutError:
                timed_out = True
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    await asyncio.wait_for(proc.wait(), timeout=10.0)
                except asyncio.TimeoutError:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    await proc.wait()
                exit_code = proc.returncode

        except Exception as exc:  # launch failure is also a failed test
            log.write(f"\nERROR launching command: {exc}\n".encode("utf-8", errors="replace"))
            exit_code = None

        elapsed = time.monotonic() - start
        footer = (
            f"\n{'=' * 80}\n"
            f"# Finished: {time.strftime('%Y-%m-%d %H:%M:%S')}\n"
            f"# Elapsed seconds: {elapsed:.3f}\n"
            f"# Timed out: {timed_out}\n"
            f"# Exit code: {exit_code}\n"
        )
        log.write(footer.encode("utf-8", errors="replace"))

    actual = "P" if (exit_code == 0 and not timed_out) else "F"
    matched = actual == test.expected

    return TestResult(
        index=test.index,
        line_no=test.line_no,
        expected=test.expected,
        actual=actual,
        matched_reference=matched,
        exit_code=exit_code,
        timed_out=timed_out,
        elapsed_s=elapsed,
        command=test.command,
        log_file=str(log_path),
        np_value=plan.np_value,
        nt_value=plan.nt_value,
        np_nt_details=plan.np_nt_details,
        mem_total_bytes=mem_total_bytes,
        mem_available_bytes_at_launch=mem_available_bytes,
        mem_available_fraction_at_launch=mem_fraction,
        last_pass=test.last_pass,
        last_pass_line_no=test.last_pass_line_no,
        template_command=test.template_command,
        loop_line_no=test.loop_line_no,
        loop_bindings=dict(test.loop_bindings),
        variant_index=test.variant_index,
        variant_count=test.variant_count,
    )


async def run_all_tests(
    tests: Iterable[TestCase],
    *,
    jobs: int,
    workdir: Path,
    log_dir: Path,
    timeout_s: Optional[float],
    use_shell: bool,
    default_np: int,
    default_nt: int,
    set_thread_env_vars: bool,
    preserve_thread_env: bool,
    print_start: bool,
    memory_gate_enabled: bool,
    min_free_memory_fraction: float,
    memory_check_interval: float,
) -> List[TestResult]:
    """Schedule tests behind a -j job-count cap and a physical-memory ramp-up gate.

    A launch into an empty pool -- the first test of the run, or any later
    test that starts once nothing else is running -- always launches
    immediately. Every other launch attempt -- ramping up toward ``jobs``
    concurrent tests while others are still running, or backfilling a slot
    next to tests that are still running -- must wait at least
    ``memory_check_interval`` seconds since the previous launch attempt and
    then observe more than ``min_free_memory_fraction`` of total physical
    memory still available. Tests are launched in list order; there is no
    per-test resource estimate to reorder around, since every test is
    subject to exactly the same gate regardless of its own ``-np``/``-nt``.
    """
    log_dir.mkdir(parents=True, exist_ok=True)
    base_env = os.environ.copy()

    pending: List[TestPlan] = [
        build_test_plan(test, default_np=default_np, default_nt=default_nt) for test in tests
    ]
    running: dict[asyncio.Task[TestResult], TestPlan] = {}
    results: List[TestResult] = []

    last_check_time: Optional[float] = None
    last_known_total: Optional[int] = None
    last_known_available: Optional[int] = None

    def try_gate() -> bool:
        """Return True if a new test may launch right now.

        A launch into an empty pool -- nothing of this runner's currently
        running -- always launches immediately and unconditionally: this
        covers the first test of the run, and any later test that starts
        once everything before it has finished. There is no in-flight
        launch to pace against in that case. Every other attempt must wait
        at least ``memory_check_interval`` seconds since the previous
        attempt (successful or not), then observe more than
        ``min_free_memory_fraction`` of total physical memory free. A
        transient failure to read /proc/meminfo during one of those later
        checks is treated as "not enough information to launch yet" rather
        than aborting the run, and is retried on the same cadence.

        Whenever this performs an actual memory read, it records the
        reading in the enclosing ``last_known_*`` variables so callers (the
        [START] line and TestResult) can report it even on calls that
        return False. It also prints a [WAITING] line itself whenever a
        real check comes back insufficient -- this is the only place that
        happens, since a blocked launch can occur equally whether other
        tests are currently running (the common case once the pool is warm)
        or not, and only this function knows when a check was actually
        performed rather than skipped for still being inside the pacing
        window.
        """
        nonlocal last_check_time, last_known_total, last_known_available
        if not memory_gate_enabled:
            return True

        now = time.monotonic()
        if not running:
            # Nothing of ours is currently running, so there is no ongoing
            # ramp-up to pace against: launch immediately. This always covers
            # the very first test of the run, and also re-covers any later
            # point where the pool has fully drained before the next test
            # starts -- there is no in-flight memory footprint left over from
            # a stale pacing timer to wait out. Still take a best-effort
            # reading for display; it does not gate this launch.
            last_check_time = now
            try:
                last_known_total, last_known_available = read_memory_totals()
            except RuntimeError:
                pass
            return True

        if last_check_time is not None and (now - last_check_time) < memory_check_interval:
            return False

        last_check_time = now
        try:
            total, available = read_memory_totals()
        except RuntimeError as exc:
            if print_start:
                print(
                    f"[WAITING] memory check failed ({exc}); will retry in "
                    f"{memory_check_interval:.0f}s ({len(running)} test(s) currently running)",
                    flush=True,
                )
            return False
        last_known_total, last_known_available = total, available
        if available > min_free_memory_fraction * total:
            return True
        if print_start:
            print(
                f"[WAITING] {available / total:.0%} of physical memory free "
                f"(need > {min_free_memory_fraction:.0%}); holding off starting "
                f"another test, rechecking in {memory_check_interval:.0f}s "
                f"({len(running)} test(s) currently running)",
                flush=True,
            )
        return False

    while pending or running:
        while pending and len(running) < jobs:
            if not try_gate():
                break
            plan = pending.pop(0)
            task = asyncio.create_task(
                run_one_test(
                    plan,
                    workdir=workdir,
                    log_dir=log_dir,
                    timeout_s=timeout_s,
                    use_shell=use_shell,
                    base_env=base_env,
                    set_thread_env_vars=set_thread_env_vars,
                    preserve_thread_env=preserve_thread_env,
                    print_start=print_start,
                    mem_total_bytes=last_known_total,
                    mem_available_bytes=last_known_available,
                )
            )
            running[task] = plan

        if not running:
            # Defensive fallback, not expected in normal operation: try_gate()
            # always admits a launch into an empty pool unconditionally, so
            # reaching here with pending tests left would mean that invariant
            # broke. Don't crash the run over a scheduling bug -- wait a
            # moment and retry -- but say so loudly, since it would indicate
            # something worth investigating rather than ordinary memory
            # pressure (which is already reported from inside try_gate).
            print(
                "[WAITING] scheduler made no progress this cycle although the "
                "pool is empty (unexpected); retrying",
                flush=True,
            )
            await asyncio.sleep(memory_check_interval if memory_gate_enabled else 1.0)
            continue

        if pending and len(running) < jobs and memory_gate_enabled and last_check_time is not None:
            wait_timeout = max(0.1, memory_check_interval - (time.monotonic() - last_check_time))
        else:
            wait_timeout = None

        done, _pending_tasks = await asyncio.wait(
            running.keys(), timeout=wait_timeout, return_when=asyncio.FIRST_COMPLETED
        )

        for task in done:
            plan = running.pop(task)
            result = await task
            results.append(result)
            status = "OK" if result.matched_reference else "MISMATCH"
            loop_note = (
                f" variant {result.variant_index}/{result.variant_count} "
                f"[{format_loop_bindings(result.loop_bindings)}]"
                if result.loop_bindings
                else ""
            )
            print(
                f"[{status}] #{result.index:03d} line {result.line_no}{loop_note}: "
                f"expected {result.expected}, actual {result.actual}, "
                f"exit={result.exit_code}, {result.elapsed_s:.1f}s",
                flush=True,
            )

    results.sort(key=lambda r: r.index)
    return results


def write_reports(results: List[TestResult], report_prefix: Path) -> tuple[Path, Path, Path, Path, Path, Path, Path, Path, Path, Path]:
    json_path = report_prefix.with_suffix(".json")
    csv_path = report_prefix.with_suffix(".csv")
    to_address_txt_path = report_prefix.with_name(report_prefix.name + "_to_address.txt")
    to_address_csv_path = report_prefix.with_name(report_prefix.name + "_to_address.csv")
    actual_failed_txt_path = report_prefix.with_name(report_prefix.name + "_actual_failed.txt")
    actual_failed_csv_path = report_prefix.with_name(report_prefix.name + "_actual_failed.csv")
    last_pass_failed_txt_path = report_prefix.with_name(report_prefix.name + "_last_pass_failed.txt")
    last_pass_failed_csv_path = report_prefix.with_name(report_prefix.name + "_last_pass_failed.csv")
    new_or_unexpected_passed_txt_path = report_prefix.with_name(report_prefix.name + "_new_or_unexpected_passed.txt")
    new_or_unexpected_passed_csv_path = report_prefix.with_name(report_prefix.name + "_new_or_unexpected_passed.csv")

    with json_path.open("w", encoding="utf-8") as f:
        json.dump([asdict(r) for r in results], f, indent=2)
        f.write("\n")

    fieldnames = [
        "index",
        "line_no",
        "expected",
        "actual",
        "matched_reference",
        "exit_code",
        "timed_out",
        "elapsed_s",
        "command",
        "log_file",
        "np_value",
        "nt_value",
        "np_nt_details",
        "mem_total_bytes",
        "mem_available_bytes_at_launch",
        "mem_available_fraction_at_launch",
        "last_pass",
        "last_pass_line_no",
        "template_command",
        "loop_line_no",
        "loop_bindings",
        "variant_index",
        "variant_count",
    ]

    def write_csv(path: Path, rows: List[TestResult]) -> None:
        with path.open("w", encoding="utf-8", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=fieldnames)
            writer.writeheader()
            for r in rows:
                row = asdict(r)
                row["elapsed_s"] = f"{r.elapsed_s:.6f}"
                if r.mem_available_fraction_at_launch is not None:
                    row["mem_available_fraction_at_launch"] = f"{r.mem_available_fraction_at_launch:.6f}"
                writer.writerow(row)

    def write_txt(path: Path, title: str, rows: List[TestResult]) -> None:
        with path.open("w", encoding="utf-8") as f:
            f.write(title + "\n")
            f.write("=" * len(title) + "\n")
            f.write(f"Count: {len(rows)}\n\n")
            if not rows:
                f.write("None.\n")
                return
            for r in rows:
                if r.mem_total_bytes and r.mem_available_bytes_at_launch is not None:
                    mem_line = (
                        f"  memory:   {r.mem_available_fraction_at_launch:.0%} available at launch "
                        f"({format_bytes(r.mem_available_bytes_at_launch)} / {format_bytes(r.mem_total_bytes)})\n"
                    )
                else:
                    mem_line = "  memory:   not tracked (--no-memory-gate)\n"
                loop_line = (
                    f"  variant:  {r.variant_index}/{r.variant_count} "
                    f"[{format_loop_bindings(r.loop_bindings)}]\n"
                    if r.loop_bindings
                    else ""
                )
                f.write(
                    f"#{r.index:03d} line {r.line_no}\n"
                    f"{loop_line}"
                    f"  expected: {r.expected}\n"
                    f"  actual:   {r.actual}\n"
                    f"  exit:     {r.exit_code}\n"
                    f"  timeout:  {r.timed_out}\n"
                    f"  elapsed:  {r.elapsed_s:.3f} s\n"
                    f"  last pass: {(r.last_pass or '<empty>')}\n"
                    f"  {r.np_nt_details}\n"
                    f"{mem_line}"
                    f"  log:      {r.log_file}\n"
                    f"  command:  {r.command}\n\n"
                )

    to_address = [r for r in results if not r.matched_reference]
    actual_failed = [r for r in results if r.actual == "F"]
    last_pass_failed, new_or_unexpected_passed = list_delta_rows(results)

    write_csv(csv_path, results)
    write_csv(to_address_csv_path, to_address)
    write_csv(actual_failed_csv_path, actual_failed)
    write_csv(last_pass_failed_csv_path, last_pass_failed)
    write_csv(new_or_unexpected_passed_csv_path, new_or_unexpected_passed)
    write_txt(to_address_txt_path, "Tests to address: actual result differs from reference P/F", to_address)
    write_txt(actual_failed_txt_path, "Actual failed commands: exit code was nonzero or timed out", actual_failed)
    write_txt(last_pass_failed_txt_path, "Previously passing tests that failed in this run", last_pass_failed)
    write_txt(
        new_or_unexpected_passed_txt_path,
        "Tests that passed but need last-pass or P/F marker review",
        new_or_unexpected_passed,
    )

    return (
        json_path,
        csv_path,
        to_address_txt_path,
        to_address_csv_path,
        actual_failed_txt_path,
        actual_failed_csv_path,
        last_pass_failed_txt_path,
        last_pass_failed_csv_path,
        new_or_unexpected_passed_txt_path,
        new_or_unexpected_passed_csv_path,
    )


def get_git_commit_id(workdir: Path) -> str:
    """Return the current git commit hash for the requested working tree."""
    try:
        completed = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(workdir),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError(
            "could not determine the current git commit id; run from inside a "
            "git work tree or pass --commit-id explicitly"
        ) from exc

    commit_id = completed.stdout.strip()
    if not commit_id:
        raise RuntimeError("git rev-parse HEAD returned an empty commit id")
    return commit_id


def update_last_pass_entries(
    test_file: Path,
    tests: List[TestCase],
    results: List[TestResult],
    commit_id: str,
) -> int:
    """Update scalar and loop-vector ``last pass:`` metadata after a run.

    Ordinary entries retain the historical scalar format::

        last pass: <commit>

    Looped entries are always written in explicit expansion order::

        last pass: {<commit-for-variant-1>,<commit-for-variant-2>,...}

    Only variants that actually passed in the current run are changed.  Failed
    variants preserve their previous commit, or remain empty when no passing
    commit was known.  This is important for mixed expectation/result vectors:
    one successful mover must not overwrite provenance for another mover that
    failed.
    """
    results_by_key = {
        (result.line_no, result.variant_index): result
        for result in results
    }

    tests_by_source_line: dict[int, List[TestCase]] = {}
    for test in tests:
        tests_by_source_line.setdefault(test.line_no, []).append(test)

    update_existing_line: dict[int, str] = {}
    insert_after_line: dict[int, str] = {}

    for source_line, variants in tests_by_source_line.items():
        variants.sort(key=lambda test: test.variant_index)
        values = [(test.last_pass or "").strip() for test in variants]
        changed = False

        for position, test in enumerate(variants):
            result = results_by_key.get((source_line, test.variant_index))
            if result is not None and result.actual == "P":
                values[position] = commit_id
                changed = True

        if not changed:
            continue

        if len(variants) == 1 and not variants[0].loop_bindings:
            metadata_value = values[0]
        else:
            # Commit hashes cannot contain commas or braces. Empty fields are
            # intentional and represent variants with no known passing commit.
            metadata_value = "{" + ",".join(values) + "}"

        metadata_lines = {
            test.last_pass_line_no
            for test in variants
            if test.last_pass_line_no is not None
        }
        if len(metadata_lines) > 1:
            raise RuntimeError(
                f"internal error: variants from test-list line {source_line} "
                "refer to different 'last pass:' metadata lines"
            )

        if metadata_lines:
            update_existing_line[next(iter(metadata_lines))] = metadata_value
        else:
            insert_after_line[source_line] = metadata_value

    if not update_existing_line and not insert_after_line:
        return 0

    original_lines = test_file.read_text(
        encoding="utf-8", errors="replace"
    ).splitlines(keepends=True)
    new_lines: List[str] = []

    for line_no, raw in enumerate(original_lines, start=1):
        if line_no in update_existing_line:
            newline = "\n" if raw.endswith("\n") else ""
            stripped = raw.rstrip("\n")
            match = LAST_PASS_RE.match(stripped)
            if match:
                indent_match = re.match(r"^(\s*)", match.group("prefix"))
                indent = indent_match.group(1) if indent_match else ""
                prefix = f"{indent}last pass: "
            else:
                prefix = "last pass: "
            new_lines.append(
                f"{prefix}{update_existing_line[line_no]}{newline}"
            )
        else:
            new_lines.append(raw)

        if line_no in insert_after_line:
            new_lines.append(f"last pass: {insert_after_line[line_no]}\n")

    tmp_path = test_file.with_name(test_file.name + ".tmp")
    tmp_path.write_text("".join(new_lines), encoding="utf-8")
    tmp_path.replace(test_file)
    return len(update_existing_line) + len(insert_after_line)




def has_nonempty_last_pass(result: TestResult) -> bool:
    """Return True when the test-list provenance field is present and non-empty.

    The parser stores a blank metadata line such as ``last pass:`` as an empty
    string and stores a completely missing line as ``None``.  For the regression
    summary both cases mean "no known passing commit", so they are treated the
    same here.  Whitespace-only values are also considered empty.
    """
    return bool((result.last_pass or "").strip())


def list_delta_rows(results: List[TestResult]) -> tuple[List[TestResult], List[TestResult]]:
    """Return the two user-facing deltas between the list metadata and this run.

    ``last_pass_failed`` identifies regressions: a test that has a non-empty
    ``last pass:`` value was known to pass at some commit, but failed now.

    ``new_or_unexpected_passed`` identifies tests whose current success should
    change the list: either the test has no passing commit recorded yet, or the
    list still marks it as an expected failure even though the command now exits
    successfully.  The two conditions intentionally form a union, so an
    F-marked test with an empty ``last pass:`` appears only once.
    """
    last_pass_failed = [r for r in results if has_nonempty_last_pass(r) and r.actual == "F"]
    new_or_unexpected_passed = [
        r for r in results
        if r.actual == "P" and ((not has_nonempty_last_pass(r)) or r.expected == "F")
    ]
    return last_pass_failed, new_or_unexpected_passed

def print_final_summary(
    results: List[TestResult],
    *,
    json_path: Path,
    csv_path: Path,
    to_address_txt_path: Path,
    to_address_csv_path: Path,
    actual_failed_txt_path: Path,
    actual_failed_csv_path: Path,
    last_pass_failed_txt_path: Path,
    last_pass_failed_csv_path: Path,
    new_or_unexpected_passed_txt_path: Path,
    new_or_unexpected_passed_csv_path: Path,
) -> None:
    n = len(results)
    n_expected_pass = sum(r.expected == "P" for r in results)
    n_expected_fail = sum(r.expected == "F" for r in results)
    n_actual_pass = sum(r.actual == "P" for r in results)
    n_actual_fail = sum(r.actual == "F" for r in results)
    n_match = sum(r.matched_reference for r in results)
    n_mismatch = n - n_match
    n_timeout = sum(r.timed_out for r in results)
    to_address = [r for r in results if not r.matched_reference]
    actual_failed = [r for r in results if r.actual == "F"]
    last_pass_failed, new_or_unexpected_passed = list_delta_rows(results)

    print("\nSummary")
    print("-------")
    print(f"Total tests:        {n}")
    print(f"Expected pass/fail: {n_expected_pass}/{n_expected_fail}")
    print(f"Actual pass/fail:   {n_actual_pass}/{n_actual_fail}")
    print(f"Matched reference:  {n_match}")
    print(f"Mismatches:         {n_mismatch}")
    print(f"Timeouts:           {n_timeout}")
    print(f"JSON report:        {json_path}")
    print(f"CSV report:         {csv_path}")
    print(f"To-address text:    {to_address_txt_path}")
    print(f"To-address CSV:     {to_address_csv_path}")
    print(f"Actual-failed text: {actual_failed_txt_path}")
    print(f"Actual-failed CSV:  {actual_failed_csv_path}")
    print(f"Last-pass-failed text: {last_pass_failed_txt_path}")
    print(f"Last-pass-failed CSV:  {last_pass_failed_csv_path}")
    print(f"New/unexpected passed text: {new_or_unexpected_passed_txt_path}")
    print(f"New/unexpected passed CSV:  {new_or_unexpected_passed_csv_path}")

    if to_address:
        print("\nTests to address: actual result differs from reference P/F")
        for r in to_address:
            reason = "unexpected failure" if r.expected == "P" and r.actual == "F" else "unexpected pass"
            variant = format_variant_suffix(r.loop_bindings, r.variant_index, r.variant_count)
            print(
                f"  #{r.index:03d} line {r.line_no}{variant}: {reason}; "
                f"expected {r.expected}, actual {r.actual}, exit={r.exit_code}, log={r.log_file}\n"
                f"      {r.command}"
            )
    else:
        print("\nTests to address: none; all actual results match the reference P/F markers.")

    print("\nTest-list delta summary")
    print("-----------------------")
    print(
        "Previously passing tests that failed: "
        f"{len(last_pass_failed)} "
        "(non-empty 'last pass:' but actual F)"
    )
    if last_pass_failed:
        for r in last_pass_failed:
            variant = format_variant_suffix(r.loop_bindings, r.variant_index, r.variant_count)
            print(
                f"  #{r.index:03d} line {r.line_no}{variant}: last pass {r.last_pass}; "
                f"expected {r.expected}, actual F, exit={r.exit_code}, log={r.log_file}\n"
                f"      {r.command}"
            )

    print(
        "Passed tests needing list update/review: "
        f"{len(new_or_unexpected_passed)} "
        "(empty/missing 'last pass:' or expected F, but actual P)"
    )
    if new_or_unexpected_passed:
        for r in new_or_unexpected_passed:
            variant = format_variant_suffix(r.loop_bindings, r.variant_index, r.variant_count)
            reasons = []
            if not has_nonempty_last_pass(r):
                reasons.append("empty last pass")
            if r.expected == "F":
                reasons.append("marked F")
            reason_text = ", ".join(reasons) if reasons else "needs review"
            print(
                f"  #{r.index:03d} line {r.line_no}{variant}: {reason_text}; "
                f"expected {r.expected}, actual P, exit={r.exit_code}, log={r.log_file}\n"
                f"      {r.command}"
            )

    # This second list is useful when the reference file intentionally contains
    # expected-failure tests. It shows every command that physically failed,
    # even when that failure matched an expected F marker.
    if actual_failed:
        print("\nAll actual failed commands, including expected F tests:")
        for r in actual_failed:
            marker = "expected" if r.expected == "F" else "unexpected"
            variant = format_variant_suffix(r.loop_bindings, r.variant_index, r.variant_count)
            print(
                f"  #{r.index:03d} line {r.line_no}{variant}: {marker} actual F; "
                f"exit={r.exit_code}, log={r.log_file}\n"
                f"      {r.command}"
            )


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Run P/F commands from a test list concurrently and compare with "
            "reference pass/fail markers."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Loop syntax in the list file:\n"
            "  for $m={RK4,HC4}\n"
            "  {P,P} command --mover $m\n"
            "  last pass: {commit-for-RK4,commit-for-HC4}\n\n"
            "Without a preceding 'for' line, the historical scalar P/F and "
            "last-pass behavior is unchanged."
        ),
    )
    parser.add_argument("test_file", help="Path to the test list file")
    parser.add_argument(
        "jobs_positional",
        nargs="?",
        type=int,
        help="Number of tests to run concurrently; can also be set with -j/--jobs",
    )
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        help="Number of tests to run concurrently; overrides the positional jobs value",
    )
    # Memory-aware scheduling controls. These are runner-level options rather
    # than test-list syntax so existing validation lists keep working
    # unchanged, and no test's -np/-nt is ever rewritten because of them.
    parser.add_argument(
        "--min-free-memory-fraction",
        type=float,
        default=0.5,
        help=(
            "Before starting another concurrent test, require more than this "
            "fraction of total physical memory (from /proc/meminfo's "
            "MemAvailable) to still be free. Default: 0.5 (more than half)."
        ),
    )
    parser.add_argument(
        "--memory-check-interval",
        type=float,
        default=60.0,
        help=(
            "Seconds to wait after a test launch attempt before considering "
            "another (giving a just-started test time to reach its "
            "steady-state memory footprint), and how often to recheck while "
            "waiting for memory to free up. A launch into an empty pool "
            "(the first test of the run, or any later test that starts once "
            "nothing else is running) is exempt and always starts "
            "immediately, since there is no in-flight test to pace against. "
            "Default: 60."
        ),
    )
    parser.add_argument(
        "--no-memory-gate",
        action="store_true",
        help=(
            "Disable the physical-memory ramp-up gate; launch tests up to -j "
            "immediately with no pacing or memory checks, like plain "
            "job-count-limited concurrency. Also needed on systems without "
            "/proc/meminfo."
        ),
    )
    parser.add_argument(
        "--default-np",
        type=int,
        default=1,
        help=(
            "MPI rank count assumed for commands without -np/--np; used only "
            "for reporting and --set-thread-env, not for scheduling. Default: 1"
        ),
    )
    parser.add_argument(
        "--default-nt",
        type=int,
        default=1,
        help=(
            "Thread count assumed for commands without -nt/--nt; used only "
            "for reporting and --set-thread-env, not for scheduling. Default: 1"
        ),
    )
    parser.add_argument(
        "--set-thread-env",
        action="store_true",
        help=(
            "Set OMP_NUM_THREADS/OPENBLAS_NUM_THREADS/MKL_NUM_THREADS/NUMEXPR_NUM_THREADS "
            "for each launched command to its own parsed -nt value"
        ),
    )
    parser.add_argument(
        "--overwrite-thread-env",
        action="store_true",
        help="With --set-thread-env, overwrite existing thread-count environment variables",
    )
    parser.add_argument(
        "--workdir",
        default=".",
        help="Working directory used to launch all commands; default: current directory",
    )
    parser.add_argument(
        "--log-dir",
        default="test_runner_logs",
        help="Directory for per-test stdout/stderr logs; default: test_runner_logs",
    )
    parser.add_argument(
        "--report-prefix",
        default="test_runner_report",
        help="Prefix for JSON/CSV reports; default: test_runner_report",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="Optional timeout per test in seconds; timeout is treated as actual F",
    )
    parser.add_argument(
        "--no-shell",
        action="store_true",
        help="Execute commands without a shell using shlex.split(); default uses the shell",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Parse and list tests without executing them",
    )
    parser.add_argument(
        "--no-start-progress",
        action="store_true",
        help=(
            "Do not print [START]/[WAITING] lines when commands launch or the "
            "memory gate blocks the next launch; completion [OK]/[MISMATCH] "
            "lines are still printed"
        ),
    )
    parser.add_argument(
        "--update-last-pass",
        action="store_true",
        help=(
            "After running tests, update or insert the 'last pass:' git commit id "
            "for each test whose command actually exited 0 this run, regardless "
            "of its P/F marker"
        ),
    )
    parser.add_argument(
        "--commit-id",
        default=None,
        help=(
            "Commit id to write with --update-last-pass; default: current "
            "'git rev-parse HEAD' in --workdir"
        ),
    )

    args = parser.parse_args(argv)

    jobs = args.jobs if args.jobs is not None else args.jobs_positional
    if jobs is None:
        jobs = 1
    if jobs < 1:
        parser.error("jobs must be >= 1")
    if args.default_np < 1:
        parser.error("--default-np must be >= 1")
    if args.default_nt < 1:
        parser.error("--default-nt must be >= 1")
    if not (0.0 <= args.min_free_memory_fraction <= 1.0):
        parser.error("--min-free-memory-fraction must be between 0 and 1")
    if args.memory_check_interval < 0:
        parser.error("--memory-check-interval must be >= 0")

    memory_gate_enabled = not args.no_memory_gate

    test_file = Path(args.test_file).expanduser().resolve()
    workdir = Path(args.workdir).expanduser().resolve()
    log_dir = Path(args.log_dir).expanduser().resolve()
    report_prefix = Path(args.report_prefix).expanduser().resolve()

    try:
        tests = parse_test_file(test_file)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    if not tests:
        print(f"No tests found in {test_file}")
        return 0

    # Fail fast, before running anything, if memory can't be tracked at all.
    mem_total_at_start: Optional[int] = None
    mem_available_at_start: Optional[int] = None
    if memory_gate_enabled:
        try:
            mem_total_at_start, mem_available_at_start = read_memory_totals()
        except RuntimeError as exc:
            print(f"ERROR: {exc}", file=sys.stderr)
            return 2

    source_entries = len({test.line_no for test in tests})
    if source_entries == len(tests):
        print(f"Parsed {len(tests)} tests from {test_file}")
    else:
        print(
            f"Parsed {source_entries} source test entries expanding to "
            f"{len(tests)} tests from {test_file}"
        )
    print(f"Concurrent jobs: {jobs}")
    if memory_gate_enabled:
        print(
            f"Memory gate: start next test only when > {args.min_free_memory_fraction:.0%} "
            f"of physical memory is free, rechecked every {args.memory_check_interval:.0f}s "
            "after each launch attempt (launches into an empty pool are exempt)"
        )
        print(
            f"Physical memory: {format_bytes(mem_total_at_start)} total, "
            f"{format_bytes(mem_available_at_start)} available now "
            f"({mem_available_at_start / mem_total_at_start:.0%})"
        )
    else:
        print("Memory gate: disabled (--no-memory-gate); launching up to -j immediately")
    print(f"Workdir: {workdir}")
    print(f"Execution mode: {'no shell' if args.no_shell else 'shell'}")

    if args.dry_run:
        # Dry-run lists the tests and their parsed np/nt, but actual
        # concurrency now depends on runtime memory use, so it cannot be
        # previewed the way the old CPU-slot plan could be.
        print("\nDry run test list:")
        for t in tests:
            plan = build_test_plan(t, default_np=args.default_np, default_nt=args.default_nt)
            suffix = f"; last pass: {t.last_pass}" if t.last_pass else "; last pass: <none>"
            variant = (
                f"; variant {t.variant_index}/{t.variant_count} "
                f"[{format_loop_bindings(t.loop_bindings)}]"
                if t.loop_bindings
                else ""
            )
            print(
                f"  #{t.index:03d} line {t.line_no}: expected {t.expected}{variant}: "
                f"{plan.np_nt_details}: {t.command}{suffix}"
            )
        if memory_gate_enabled:
            print(
                f"\nActual concurrency depends on runtime memory use and cannot be "
                f"previewed here: once a test is already running, the next one only "
                f"starts once > {args.min_free_memory_fraction:.0%} of physical memory "
                f"is free, checked every {args.memory_check_interval:.0f}s (a launch "
                f"into an idle pool always starts immediately)."
            )
        return 0

    try:
        results = asyncio.run(
            run_all_tests(
                tests,
                jobs=jobs,
                workdir=workdir,
                log_dir=log_dir,
                timeout_s=args.timeout,
                use_shell=not args.no_shell,
                default_np=args.default_np,
                default_nt=args.default_nt,
                set_thread_env_vars=args.set_thread_env,
                preserve_thread_env=not args.overwrite_thread_env,
                print_start=not args.no_start_progress,
                memory_gate_enabled=memory_gate_enabled,
                min_free_memory_fraction=args.min_free_memory_fraction,
                memory_check_interval=args.memory_check_interval,
            )
        )
    except KeyboardInterrupt:
        print("Interrupted by user", file=sys.stderr)
        return 130

    (
        json_path,
        csv_path,
        to_address_txt_path,
        to_address_csv_path,
        actual_failed_txt_path,
        actual_failed_csv_path,
        last_pass_failed_txt_path,
        last_pass_failed_csv_path,
        new_or_unexpected_passed_txt_path,
        new_or_unexpected_passed_csv_path,
    ) = write_reports(results, report_prefix)
    print_final_summary(
        results,
        json_path=json_path,
        csv_path=csv_path,
        to_address_txt_path=to_address_txt_path,
        to_address_csv_path=to_address_csv_path,
        actual_failed_txt_path=actual_failed_txt_path,
        actual_failed_csv_path=actual_failed_csv_path,
        last_pass_failed_txt_path=last_pass_failed_txt_path,
        last_pass_failed_csv_path=last_pass_failed_csv_path,
        new_or_unexpected_passed_txt_path=new_or_unexpected_passed_txt_path,
        new_or_unexpected_passed_csv_path=new_or_unexpected_passed_csv_path,
    )

    if args.update_last_pass:
        try:
            commit_id = args.commit_id or get_git_commit_id(workdir)
            n_updated = update_last_pass_entries(test_file, tests, results, commit_id)
        except Exception as exc:
            print(f"ERROR updating last-pass metadata: {exc}", file=sys.stderr)
            return 2
        print(
            f"Updated {n_updated} last-pass entr{'y' if n_updated == 1 else 'ies'} "
            f"in {test_file} to commit {commit_id}"
        )

    return 0 if all(r.matched_reference for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
