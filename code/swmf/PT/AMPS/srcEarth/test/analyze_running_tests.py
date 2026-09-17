#!/usr/bin/env python3
"""
Identify AMPS tests that are still running and flag processes that may be stalled.

The script inspects processes matching:
  test_runner
  run_<test>.py
  mpirun / mpiexec
  amps

It groups MPI and AMPS descendants under each Python test runner and samples
process CPU time to distinguish active computation from an apparent stall.

Examples
--------
Single report with a 3-second activity sample:
    ./analyze_running_tests.py

Refresh every 10 seconds:
    ./analyze_running_tests.py --watch 10

Use a longer sample to diagnose an intermittent or slowly progressing test:
    ./analyze_running_tests.py --sample 15

Only flag a test as possibly stalled after it has run for at least 10 minutes:
    ./analyze_running_tests.py --min-runtime 600
"""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Dict, Iterable, List, Mapping, Sequence, Set


MATCH_RE = re.compile(
    r"(?:test_runner(?:\.py)?|run_[A-Za-z0-9_]+\.py|"
    r"(?:^|[\s/])mpirun(?:\s|$)|(?:^|[\s/])mpiexec(?:\s|$)|"
    r"(?:^|[\s/])amps(?:\s|$))"
)
RUNNER_RE = re.compile(r"(?:^|[\s/])(run_[A-Za-z0-9_]+\.py)(?:\s|$)")
MPI_RE = re.compile(r"(?:^|[\s/])(mpirun|mpiexec)(?:\s|$)")
AMPS_RE = re.compile(r"(?:^|[\s/])amps(?:\s|$)")
TEST_RUNNER_RE = re.compile(r"(?:^|[\s/])test_runner(?:\.py)?(?:\s|$)")


@dataclass(frozen=True)
class Proc:
    pid: int
    ppid: int
    elapsed_seconds: int
    elapsed_text: str
    state: str
    cpu_percent: float
    mem_percent: float
    cpu_time_seconds: float
    args: str


def parse_cpu_time(value: str) -> float:
    """Convert [[dd-]hh:]mm:ss to seconds."""
    value = value.strip()
    if not value:
        return 0.0

    days = 0
    if "-" in value:
        day_text, value = value.split("-", 1)
        days = int(day_text)

    parts = [int(part) for part in value.split(":")]
    if len(parts) == 3:
        hours, minutes, seconds = parts
    elif len(parts) == 2:
        hours = 0
        minutes, seconds = parts
    else:
        hours = 0
        minutes = 0
        seconds = parts[0]

    return float(days * 86400 + hours * 3600 + minutes * 60 + seconds)


def read_processes() -> Dict[int, Proc]:
    command = [
        "ps",
        "-eo",
        "pid=,ppid=,etimes=,etime=,stat=,pcpu=,pmem=,time=,args=",
    ]
    result = subprocess.run(
        command,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    processes: Dict[int, Proc] = {}
    for raw_line in result.stdout.splitlines():
        line = raw_line.strip()
        if not line:
            continue

        # args may contain arbitrary spaces, so split only the first 8 columns.
        fields = line.split(None, 8)
        if len(fields) < 9:
            continue

        pid_s, ppid_s, etimes_s, etime, stat, pcpu_s, pmem_s, cputime, args = fields
        try:
            proc = Proc(
                pid=int(pid_s),
                ppid=int(ppid_s),
                elapsed_seconds=int(etimes_s),
                elapsed_text=etime,
                state=stat,
                cpu_percent=float(pcpu_s),
                mem_percent=float(pmem_s),
                cpu_time_seconds=parse_cpu_time(cputime),
                args=args,
            )
        except ValueError:
            continue

        processes[proc.pid] = proc

    return processes


def children_map(processes: Mapping[int, Proc]) -> Dict[int, List[int]]:
    children: Dict[int, List[int]] = {}
    for proc in processes.values():
        children.setdefault(proc.ppid, []).append(proc.pid)
    return children


def descendants(root_pid: int, children: Mapping[int, Sequence[int]]) -> Set[int]:
    found: Set[int] = set()
    stack = list(children.get(root_pid, ()))
    while stack:
        pid = stack.pop()
        if pid in found:
            continue
        found.add(pid)
        stack.extend(children.get(pid, ()))
    return found


def matching_processes(processes: Mapping[int, Proc], own_pid: int) -> Dict[int, Proc]:
    return {
        pid: proc
        for pid, proc in processes.items()
        if pid != own_pid and MATCH_RE.search(proc.args)
    }


def test_name(proc: Proc) -> str:
    match = RUNNER_RE.search(proc.args)
    return match.group(1) if match else f"PID {proc.pid}"


def short_command(command: str, width: int = 150) -> str:
    command = " ".join(command.split())
    if len(command) <= width:
        return command
    return command[: width - 3] + "..."


class Colors:
    def __init__(self, enabled: bool) -> None:
        self.enabled = enabled

    def wrap(self, text: str, code: str) -> str:
        if not self.enabled:
            return text
        return f"\033[{code}m{text}\033[0m"

    def green(self, text: str) -> str:
        return self.wrap(text, "32;1")

    def yellow(self, text: str) -> str:
        return self.wrap(text, "33;1")

    def red(self, text: str) -> str:
        return self.wrap(text, "31;1")

    def cyan(self, text: str) -> str:
        return self.wrap(text, "36;1")

    def dim(self, text: str) -> str:
        return self.wrap(text, "2")


def aggregate_delta(
    pids: Iterable[int],
    first: Mapping[int, Proc],
    second: Mapping[int, Proc],
) -> float:
    delta = 0.0
    for pid in pids:
        if pid in first and pid in second:
            delta += max(
                0.0,
                second[pid].cpu_time_seconds - first[pid].cpu_time_seconds,
            )
    return delta


def classify(
    root: Proc,
    group_pids: Set[int],
    first: Mapping[int, Proc],
    second: Mapping[int, Proc],
    sample_seconds: float,
    idle_cpu_threshold: float,
    min_runtime: int,
) -> tuple[str, str, float, float]:
    cpu_delta = aggregate_delta(group_pids, first, second)
    current_cpu = sum(
        second[pid].cpu_percent for pid in group_pids if pid in second
    )
    states = {second[pid].state[:1] for pid in group_pids if pid in second}

    # CPU time can be quantized to whole seconds by ps. Allow either measured
    # CPU-time growth or current CPU usage to indicate activity.
    active_delta = max(0.1, sample_seconds * 0.02)
    active = (
        cpu_delta >= active_delta
        or current_cpu >= idle_cpu_threshold
        or "R" in states
        or "D" in states
    )

    if active:
        return "RUNNING", "green", cpu_delta, current_cpu

    if root.elapsed_seconds >= min_runtime:
        return "POSSIBLY STALLED", "red", cpu_delta, current_cpu

    return "WAITING / STARTING", "yellow", cpu_delta, current_cpu


def report(
    first: Mapping[int, Proc],
    second: Mapping[int, Proc],
    sample_seconds: float,
    idle_cpu_threshold: float,
    min_runtime: int,
    colors: Colors,
) -> int:
    own_pid = os.getpid()
    matched = matching_processes(second, own_pid)
    children = children_map(second)

    suite_processes = [
        proc for proc in matched.values() if TEST_RUNNER_RE.search(proc.args)
    ]
    runner_processes = [
        proc for proc in matched.values() if RUNNER_RE.search(proc.args)
    ]

    print(time.strftime("%Y-%m-%d %H:%M:%S"))
    print(
        colors.dim(
            f"Sample interval: {sample_seconds:g} s; "
            f"possible-stall minimum runtime: {min_runtime} s"
        )
    )

    if suite_processes:
        print("\nTest-suite processes:")
        for proc in sorted(suite_processes, key=lambda p: p.pid):
            print(
                f"  PID={proc.pid:<7} elapsed={proc.elapsed_text:>11} "
                f"state={proc.state:<5} CPU={proc.cpu_percent:6.1f}%  "
                f"{short_command(proc.args)}"
            )

    if not runner_processes:
        print("\n" + colors.yellow("No active run_<test>.py processes were found."))

        orphaned = [
            proc
            for proc in matched.values()
            if MPI_RE.search(proc.args) or AMPS_RE.search(proc.args)
        ]
        if orphaned:
            print(
                colors.red(
                    "MPI/AMPS processes exist without a detected Python test runner:"
                )
            )
            for proc in sorted(orphaned, key=lambda p: p.pid):
                print(
                    f"  PID={proc.pid:<7} PPID={proc.ppid:<7} "
                    f"elapsed={proc.elapsed_text:>11} state={proc.state:<5} "
                    f"CPU={proc.cpu_percent:6.1f}%  {short_command(proc.args)}"
                )
        return 0

    print("\nActive tests:")
    claimed: Set[int] = set()

    for root in sorted(runner_processes, key=lambda p: (-p.elapsed_seconds, p.pid)):
        all_descendants = descendants(root.pid, children)
        group_pids = {root.pid} | all_descendants
        claimed.update(group_pids)

        current_group = [second[pid] for pid in group_pids if pid in second]
        mpi_count = sum(1 for proc in current_group if MPI_RE.search(proc.args))
        amps_count = sum(1 for proc in current_group if AMPS_RE.search(proc.args))

        status, color_name, cpu_delta, current_cpu = classify(
            root=root,
            group_pids=group_pids,
            first=first,
            second=second,
            sample_seconds=sample_seconds,
            idle_cpu_threshold=idle_cpu_threshold,
            min_runtime=min_runtime,
        )
        colored_status = getattr(colors, color_name)(status)

        print(
            f"\n{colors.cyan(test_name(root))}  [{colored_status}]\n"
            f"  runner PID       : {root.pid}\n"
            f"  elapsed          : {root.elapsed_text}\n"
            f"  runner state     : {root.state}\n"
            f"  group CPU now    : {current_cpu:.1f}%\n"
            f"  CPU-time increase: {cpu_delta:.2f} s over {sample_seconds:g} s\n"
            f"  mpirun/mpiexec   : {mpi_count}\n"
            f"  AMPS processes   : {amps_count}\n"
            f"  command          : {short_command(root.args, 220)}"
        )

        print("  relevant process tree:")
        relevant = [
            proc
            for proc in current_group
            if (
                proc.pid == root.pid
                or MPI_RE.search(proc.args)
                or AMPS_RE.search(proc.args)
                or RUNNER_RE.search(proc.args)
            )
        ]
        for proc in sorted(relevant, key=lambda p: (p.ppid, p.pid)):
            print(
                f"    PID={proc.pid:<7} PPID={proc.ppid:<7} "
                f"elapsed={proc.elapsed_text:>11} state={proc.state:<5} "
                f"CPU={proc.cpu_percent:6.1f}% MEM={proc.mem_percent:5.1f}%  "
                f"{short_command(proc.args)}"
            )

    orphaned = [
        proc
        for proc in matched.values()
        if proc.pid not in claimed
        and (MPI_RE.search(proc.args) or AMPS_RE.search(proc.args))
    ]
    if orphaned:
        print(
            "\n"
            + colors.red(
                "MPI/AMPS processes not associated with a detected run_<test>.py:"
            )
        )
        for proc in sorted(orphaned, key=lambda p: p.pid):
            print(
                f"  PID={proc.pid:<7} PPID={proc.ppid:<7} "
                f"elapsed={proc.elapsed_text:>11} state={proc.state:<5} "
                f"CPU={proc.cpu_percent:6.1f}%  {short_command(proc.args)}"
            )

    print(
        "\n"
        + colors.dim(
            "Interpretation: RUNNING means CPU activity was observed. "
            "POSSIBLY STALLED means no CPU activity was observed during the "
            "sample interval after the minimum runtime. Confirm a stall by "
            "checking the corresponding test and AMPS logs."
        )
    )
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Show active AMPS test runners and their MPI/AMPS descendants, "
            "and flag tests that may be stalled."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--sample",
        type=float,
        default=3.0,
        metavar="SECONDS",
        help="CPU-activity sampling interval",
    )
    parser.add_argument(
        "--watch",
        type=float,
        default=0.0,
        metavar="SECONDS",
        help="refresh continuously at this interval; zero prints one report",
    )
    parser.add_argument(
        "--min-runtime",
        type=int,
        default=60,
        metavar="SECONDS",
        help="minimum elapsed runtime before an idle test is flagged",
    )
    parser.add_argument(
        "--idle-cpu-threshold",
        type=float,
        default=0.5,
        metavar="PERCENT",
        help="aggregate current CPU usage considered active",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="disable ANSI colors",
    )
    args = parser.parse_args()

    if args.sample < 0:
        parser.error("--sample must be nonnegative")
    if args.watch < 0:
        parser.error("--watch must be nonnegative")
    if args.min_runtime < 0:
        parser.error("--min-runtime must be nonnegative")
    if args.idle_cpu_threshold < 0:
        parser.error("--idle-cpu-threshold must be nonnegative")

    return args


def main() -> int:
    args = parse_args()
    colors = Colors(
        enabled=(
            not args.no_color
            and sys.stdout.isatty()
            and os.environ.get("TERM", "") != "dumb"
        )
    )

    while True:
        try:
            first = read_processes()
            if args.sample > 0:
                time.sleep(args.sample)
            second = read_processes()
        except (OSError, subprocess.CalledProcessError) as exc:
            print(f"ERROR: could not read process table: {exc}", file=sys.stderr)
            return 2

        if args.watch > 0 and sys.stdout.isatty():
            print("\033[2J\033[H", end="")

        report(
            first=first,
            second=second,
            sample_seconds=args.sample,
            idle_cpu_threshold=args.idle_cpu_threshold,
            min_runtime=args.min_runtime,
            colors=colors,
        )

        if args.watch <= 0:
            return 0

        time.sleep(args.watch)


if __name__ == "__main__":
    raise SystemExit(main())
