# Pointer directory — the pipeline lives elsewhere

The ScienceAccelBench packaging pipeline (the skill `SKILL.md`, the design
`SPEC.html`, the pitfalls reference, the templates and the `sab` CLI) is
maintained in

    https://github.com/huangzesen/sciaccelbench-pipeline

This directory holds no copy of it. `PIPELINE_REVISION` is the commit of that
repository this benchmark runs; CI checks it out at that commit under
`.pipeline/`, and `scripts/sab.py` is the loader that runs the CLI from a
clone (`$SAB_PIPELINE`, `.pipeline/`, a sibling `../sciaccelbench-pipeline`,
or an installed package) with this repository as the root.

A pipeline change lands here as a one-line bump of `PIPELINE_REVISION`,
opened by the pipeline's `tools/release.py`. The loader and this pointer are
hand-maintained and change only when the pipeline's location or its loading
rule changes.
