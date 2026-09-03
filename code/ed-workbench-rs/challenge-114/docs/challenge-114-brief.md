# Challenge 114 Brief

Source: https://github.com/QuantumBFS/quantum.harness/issues/114

## Metadata

| Field | Value |
|---|---|
| Upstream issue | QuantumBFS/quantum.harness#114 |
| Title | Agentic verification of an experimental Rust tensor library |
| State | Open |
| Labels | challenge, accepted |
| Released by | Hiroshi Shinaoka, Saitama University |
| Contact | shinaoka@mail.saitama-u.ac.jp |
| Method | Other |

## Objective

Grow the canonical record for `tenferro-rs` by adding benchmark coverage,
correctness oracles, and attributed gap reports. A useful finding must become a
reproducible benchmark or oracle case, not just an issue comment.

## Local Scope

This repository uses #129 ED/FCI workloads as the source of realistic cases.
The first pass focuses on:

- small eager loops that mimic determinant and amplitude updates;
- permutation-heavy or indexed-access-heavy contractions inspired by FCI
  sigma-vector construction;
- hardware-profiled timing against independent references.

## What Counts Here

- Workload specs with shapes, dtypes, operation semantics, and correctness
  checks.
- Benchmark plans comparing `tenferro-rs` with PyTorch, JAX, and Rust-native
  alternatives where relevant.
- Gap records with enough detail to reproduce the measurement and diagnose a
  likely cause.

