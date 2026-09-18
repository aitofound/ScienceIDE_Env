---
name: Debug Session
description: Set up and launch a debugging session for FLEKS using VS Code or lldb
---

# Debug Session

This skill helps set up and run debugging sessions for FLEKS.

## Prerequisites

- FLEKS built with debug symbols (the default `DEBUGC` flag in
  `Makefile.conf` already includes `-g`)
- Optimization level set to `-O0` for best debuggability (check
  `OPT0`–`OPT4` in `Makefile.conf`)
- For VS Code: C/C++ extension (cppdbg) installed
- For command line: `lldb` (macOS default)

## VS Code Debugging

### 1. Ensure Debug Build

The debug flags are configured in `Makefile.conf` (referenced via SWMF).
The default `DEBUGC` is `-g -Wall -Wextra -Wno-unused-parameter`, which
includes debug symbols. To ensure no optimization interferes, verify
`OPT3 = -O0` in `Makefile.conf`.

### 2. Launch Configuration

The existing launch configuration is in `.vscode/launch.json`:
- **Configuration name**: `"cpp build and debug active file"`
- **Program**: `${workspaceFolder}/../../run/SWMF.exe`
- **Working Directory**: `${workspaceFolder}/../../run/`
- **Debugger**: `lldb` on macOS (via `osx.MIMode`), `gdb` on Linux
  (via `linux.MIMode`)
- **Pre-launch task**: `"cpp build active file"` (defined in
  `.vscode/tasks.json` if present)

### 3. Start Debugging

1. Open VS Code in the FLEKS workspace
2. Press `F5` or go to Run → Start Debugging
3. Select `"cpp build and debug active file"`

### 4. Setting Breakpoints

- Click in the gutter (left margin) next to line numbers
- Or use `Debug: Toggle Breakpoint` command
- Conditional breakpoints: Right-click breakpoint → Edit Breakpoint

## Command Line Debugging (lldb)

### 1. Start lldb

```bash
cd ../../run
lldb ./SWMF.exe
```

### 2. Common lldb Commands

| Command | Description |
|---------|-------------|
| `b <file>:<line>` | Set breakpoint at file:line |
| `b <function>` | Set breakpoint at function |
| `r` | Run program |
| `c` | Continue execution |
| `n` | Step over (next) |
| `s` | Step into |
| `finish` | Step out of function |
| `p <var>` | Print variable value |
| `po <expr>` | Print object / evaluate expression |
| `bt` | Show backtrace |
| `frame select <n>` | Select stack frame |
| `thread list` | List all threads |
| `quit` | Exit debugger |

### 3. Example Session

```bash
cd ../../run
lldb ./SWMF.exe
(lldb) b Pic.cpp:100
(lldb) r
(lldb) p variableName
(lldb) bt
(lldb) c
```

## MPI Debugging

For debugging parallel runs, you have several options:

### Option 1: Attach to Running Process

```bash
# Run MPI program
cd ../../run
mpirun -np 4 ./SWMF.exe &

# Find process ID
ps aux | grep SWMF

# Attach lldb
lldb -p <pid>
```

### Option 2: Debug Single Rank (Linux with xterm)

```bash
mpirun -np 4 xterm -e lldb ./SWMF.exe
```

### Option 3: Single-Process Debug Run

For many debugging scenarios, running with a single MPI process is
the simplest approach:

```bash
cd ../../run
lldb -- mpirun -np 1 ./SWMF.exe
```

## Useful Watch Expressions

For physics simulations, useful things to watch:
- `tc.iCycle` — Current simulation cycle
- `tc.timeSI` — Simulation time in SI units
- `nGst` — Number of ghost cells
- Particle counts per species
- Field values at specific indices (e.g., `nodeE(iv, 0)` for E_x)

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No symbols | Rebuild with `-g` flag (default in `DEBUGC`) |
| Can't find source | Check `sourceFileMap` in `launch.json` or use `settings set target.source-map` in lldb |
| Breakpoint not hit | Verify code path is executed; check MPI rank |
| Variables optimized away | Set `OPT3 = -O0` in `Makefile.conf` and rebuild |
| Fortran crash / backtrace | Add `-fbacktrace` to `DEBUGFLAG` in `Makefile.conf` (default on) |
