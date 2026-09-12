# Mink bibliography: scope and verification

[`references.bib`](references.bib) contains eight distinct works, with the upstream
Mink citation first, followed by directly relevant mathematical foundations,
runtime methods, and software/model provenance. Sources were checked on
2026-09-12. Software citations use standard BibTeX `misc` entries so that both
classic BibTeX styles and BibLaTeX can consume the file.

## Coverage

- **Upstream:** `kevinzakka/mink`, commit
  `14625beca2ce0918f88d1fc84a3c0cdb591e0729`, release 1.3.0. This agrees with the
  existing [`codebase-metadata.json`](codebase-metadata.json) and the shipped
  [task metadata](../../tasks/mink/mink-constrained-differential-ik/task.toml).
  The codebase report already exists; its measurements and metadata are unchanged.
- **All shipped Mink tasks:** the sole task is
  [`mink-constrained-differential-ik`](../../tasks/mink/mink-constrained-differential-ik/).
  Its 52 check directories cover 21 official unit-test files, 26 official
  examples, and five benchmark inputs. The task's `science_summary`,
  [`comment/pipeline/module.json`](../../tasks/mink/mink-constrained-differential-ik/comment/pipeline/module.json),
  and the [Panda check README](../../tasks/mink/mink-constrained-differential-ik/tests/checks/examples-arm-panda/README.md)
  identify weighted differential-IK quadratic programs, frame/posture objectives,
  configuration and velocity limits, Lie-group operations, and tangent integration.
  The principal Panda workload combines the upstream circular trajectory with a
  benchmark-authored reachable-pose bank. These are covered by the Mink, Lie
  theory, frame-Jacobian, MuJoCo, and DAQP references; the other robot fixtures
  share this computational foundation and the model provenance below.
- **Open/pending-review PRs:** the supplied inventory's `open_prs` map contains
  no PR mapped to `mink`. A live
  `gh pr list --repo aitofound/ScienceAccelBench --state open --search mink`
  returned [PR #639](https://github.com/aitofound/ScienceAccelBench/pull/639),
  a Pinocchio task. Inspection with `gh pr view 639 --json number,title,url,state,files`
  confirmed that it changes no `tasks/mink/**` or `codebase-reports/mink/**`
  files. It therefore adds no Mink citation requirement. This review predates
  the bibliography PR itself.

## Entry-level authoritative evidence

| BibTeX key | Verification source | Relevance and metadata decisions |
| --- | --- | --- |
| `zakka2026mink` | [Pinned upstream CITATION.cff](https://github.com/kevinzakka/mink/blob/14625beca2ce0918f88d1fc84a3c0cdb591e0729/CITATION.cff) and [pyproject.toml](https://github.com/kevinzakka/mink/blob/14625beca2ce0918f88d1fc84a3c0cdb591e0729/pyproject.toml) | Kevin Zakka; exact software title; version 1.3.0; CFF release date 2026-08-17. The pinned README still offers a 1.1.0 / February citation, so the version-specific CFF and project version take precedence. No paper or DOI is invented. |
| `sola2018microLie` | [arXiv:1812.01537](https://arxiv.org/abs/1812.01537) and the [pinned Mink README references](https://github.com/kevinzakka/mink/blob/14625beca2ce0918f88d1fc84a3c0cdb591e0729/README.md#references) | arXiv verifies title, all three authors, and initial submission on 2018-12-04. Mink explicitly says its matrix-Lie-group code references numbered equations from this work. The arXiv revisions are not separate works. |
| `caron2023taskJacobian` | [Author's article](https://scaron.info/robotics/jacobian-of-a-kinematic-task-and-derivatives-on-manifolds.html), cited by [pinned docs/references.rst](https://github.com/kevinzakka/mink/blob/14625beca2ce0918f88d1fc84a3c0cdb591e0729/docs/references.rst) | The article identifies Stéphane Caron, the exact title, and publication on 2023-02-01. It is a technical web article, not a journal paper; relevant to the owned frame-task residual/Jacobian machinery. |
| `todorov2012mujoco` | [MuJoCo's recommended citation](https://github.com/google-deepmind/mujoco/blob/ba57cabefde8580158266a0f76ac321da19d110d/README.md#citation) | Upstream supplies all three authors, IROS 2012 venue, pages 5026–5033, and DOI `10.1109/IROS.2012.6386109`. MuJoCo supplies the task's robot kinematics and configuration integration. This citation does not extend the task to contact or dynamics. |
| `arnstrom2022dual` | [DAQP's recommended citation](https://github.com/darnstrom/daqp/blob/11e0fd8e690af8e000cb189136406fb8a8cdf067/README.md#citing-daqp) | Upstream supplies authors, title, IEEE Transactions on Automatic Control 67(8), pages 4362–4369, year 2022, and DOI `10.1109/TAC.2022.3176430`. The shipped environment pins `daqp==0.8.5`; this is the QP solver method rather than a second Mink software citation. |
| `caron2026pink` | [Pink CITATION.cff](https://github.com/pink-kinematics/pink/blob/c848c85fbac6790bb752a224cc6c86720d986186/CITATION.cff) and [Mink's acknowledgements](https://github.com/kevinzakka/mink/blob/14625beca2ce0918f88d1fc84a3c0cdb591e0729/README.md#acknowledgements) | Mink calls itself a direct port of Pink. The checked CFF supplies all 12 authors, title, version 4.4.0, and date 2026-09-09. This is a citation of the acknowledged predecessor project, not an assertion that Mink derives from that particular Pink release or runs Pink at benchmark time. The precise historical ancestor version is not established here. |
| `caron2026qpsolvers` | [qpsolvers v4.12.0 CITATION.cff](https://github.com/qpsolvers/qpsolvers/blob/v4.12.0/CITATION.cff) | Exact title, complete CFF author list (including credited handles), version 4.12.0, and release date 2026-05-05. This matches `qpsolvers==4.12.0` in the shipped environment, rather than the newer 4.13.0 citation on the default branch. Mink's pinned `pyproject.toml` declares `qpsolvers[daqp]`. |
| `zakka2022menagerie` | [Task-pinned MuJoCo Menagerie README citation](https://github.com/google-deepmind/mujoco_menagerie/blob/bf756430b615819654b640f321c71ba5c3ebeef8/README.md#citing-menagerie) | Upstream supplies Kevin Zakka, Yuval Tassa, the contributor collective, title, and year 2022. The existing report records this exact commit for five supplemental model directories (Cassie, Panda, Talos, G1, UR5e). The entry credits the model collection, not a newly inferred robotics paper, and does not replace the individual model licenses. |

The [shipped environment](../../tasks/mink/mink-constrained-differential-ik/environment/Dockerfile)
also pins `mujoco==3.11.0`. Its version is not confused with the publication year
of the recommended 2012 paper. No separate publication is claimed for the
benchmark-authored Panda pose bank, and no contact, collision, dynamics, or
training task family is inferred from the example robot names.

## Validation

- BibTeX 0.99d (TeX Live 2024), `plain.bst`, and `\citation{*}` parsed and rendered
  all eight entries with zero warnings or errors. Temporary validation files
  were created and removed within this report directory.
- Entry keys and DOIs are unique; each entry has real authors, a title, a year,
  and an authoritative URL. Placeholder checks passed. Exact works are included
  only once (software packages, their method papers, and the model collection
  are distinct works).
- `git diff --check`, `git diff --cached --check`, and changed-path validation
  passed before commit. This change is confined to `codebase-reports/mink/`.
  No task, registry, source,
  configuration, test, or existing report measurement is modified.
