# Layer-4 Regression Baselines

This directory stores the accepted Layer-4 regression baseline JSON file.

The expected file name used by default is:

```text
layer4_reference.json
```

Create or update it with:

```bash
tests/run_layer4_tests.sh --update-baseline
```

Only update this file after reviewing the new outputs and deciding that the new
behavior is correct.  A changed baseline means that future Layer-4 checks will
compare against the newly accepted behavior.

The baseline is not an independent physics-validation reference.  It is a
software-regression reference generated from a trusted version of `shieldSim`.
The JSON contains compact signatures extracted from the machine-readable run
summaries and selected output files.
