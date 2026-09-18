# Doc/ — User and Algorithm Documentation

## Files

| File | Description |
|------|-------------|
| `Algorithm.tex` | Mathematical derivations for FLEKS: unit normalization (CGS/SI), Boris particle mover (standard + relativistic), hybrid PIC solver (generalized Ohm's law, cell-centered layout, Faraday advance), pressure tensor from sub-groups. Build with `pdflatex Algorithm.tex`. |
| `Coding_standards.md` | Coding conventions for the project: naming, memory management, header order, `const` usage, lambdas, commit messages. |

## Output

- `Algorithm.pdf` is produced alongside `Algorithm.tex` after `pdflatex`.

## Validation

- Rebuild docs with `cd docs && pdflatex Algorithm.tex`.
- Keep `Doc/Coding_standards.md` aligned with any style guidance added to root `AGENT.md`.
