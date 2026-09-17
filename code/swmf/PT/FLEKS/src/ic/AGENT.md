# src/ic/ — FLEKS Initial-Condition Plugins

This directory holds the `InitialCondition` plugins. Each plugin is a
self-contained test-case seeding module, registered through `RegisterAll.cpp`
into the `ICRegistry` (see `include/InitialCondition.h`).

## Layout Convention

**Unlike the rest of FLEKS, the plugin headers live here, not in `include/`.**

The general FLEKS rule ("all public `.h` files live in `include/`") applies to
*public* API headers. `BeamIC.h`, `TopHatIC.h`, and `WaveIC.h` are **private
plugin headers**:

- They are implementation details of the IC subsystem. The only public surface
  a consumer needs is the abstract `InitialCondition` base class plus the
  `ICRegistry` factory, both in `include/InitialCondition.h`.
- They are referenced **only** by `.cpp` files inside this directory (each
  plugin's own `.cpp` plus `RegisterAll.cpp`). Nothing outside `src/ic/`
  includes them.
- Co-locating each private header with its `.cpp` signals "not public API" and
  keeps a new wave test's files together when one is added.

If a future IC header ever needs to be referenced from outside `src/ic/`, that
is the signal that it has become public — move it to `include/` at that point
and regenerate dependencies (`make DEPEND`).

## Files

| File            | Contents                                       | Description                                             |
|-----------------|------------------------------------------------|---------------------------------------------------------|
| `RegisterAll.cpp` | `register_all_initial_conditions()`          | Registers every built-in IC name into `ICRegistry`.     |
| `WaveIC.h/.cpp` | `WaveIC` (seeded from `#WAVEIC` / `#TESTCASE`) | Unified transverse/sinusoidal wave seeding. Covers the legacy `lightwave`, `hybridwave`, `convectionwave`, `ionacousticwave` presets plus a generic `waveic`. |
| `BeamIC.h/.cpp` | `BeamIC`                                      | Charged-particle beam seeding (`#TESTCASE beam`).       |
| `TopHatIC.h/.cpp` | `TopHatIC`                                  | Pure EM step test (`#TESTCASE tophat`).                 |

## Adding a New Initial Condition

1. Write `MyIC.h` + `MyIC.cpp` here, deriving from `InitialCondition`.
2. Add a registration lambda to `RegisterAll.cpp`.
3. Add `ic/MyIC.cpp` to the `SRCS` variable in `../Makefile`.
4. Run `make LIB -j8` from the project root (regenerates `Makefile.DEPEND`).
