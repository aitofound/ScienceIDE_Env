# Run records

`runs/` contains dated execution products, solver skills, device receipts, and
verdicts. These records are evidence produced while attempting or grading tasks;
they are not task definitions and must not be used as hidden input by a solver.

Current layout:

```text
runs/<task>/<YYYY-MM-DD>/...
```

Preserve run content and provenance. New tooling may append a new dated record,
but root-layout cleanup must never rewrite scientific outputs or verdict data.
