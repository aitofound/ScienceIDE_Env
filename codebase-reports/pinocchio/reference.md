# Pinocchio bibliography: sources and task coverage

`references.bib` contains **12 distinct works**, with the primary software paper
and versioned software record first, followed by foundations and task methods.
The generated `codebase-metadata.{json,md,html}` report already exists and is
unchanged; its unknowns and warnings are not replaced by bibliographic inference.

## Scope inspected

Verified on **2026-09-12**, using ScienceAccelBench base
`9b9ac0eea13712f2e40d25e28ac4c4b49a07c761` and the report's upstream pin
[`2ae77666e894a39127b283dcce3e2399ec19242d`](https://github.com/stack-of-tasks/pinocchio/tree/2ae77666e894a39127b283dcce3e2399ec19242d)
(Pinocchio 4.1.0).

There are **no shipped `tasks/pinocchio/**` files at this base**. The supplied
inventory maps six open task PRs to this codebase. All six were inspected using
`gh pr view`, their `instruction.md`, `task.toml`, and
`comment/pipeline/module.json` at the heads below, plus the complete changed-file
lists using paginated `gh api`. All were **OPEN, non-draft**, with no
`reviewDecision` recorded at inspection time; this is not a claim of review
approval. Together they propose **70 checks**.

| PR | Task under `tasks/pinocchio/` | Checks | Inspected head |
| --- | --- | ---: | --- |
| [#639](https://github.com/aitofound/ScienceAccelBench/pull/639) | `rigid-body-algorithms` | 12 | `d62fc17f1e5ab155fd9bd2079ebbd717e92c2cbe` |
| [#640](https://github.com/aitofound/ScienceAccelBench/pull/640) | `analytical-derivatives` | 11 | `00c67668dd0b2b1dc3451baf95835e31ee1dd954` |
| [#641](https://github.com/aitofound/ScienceAccelBench/pull/641) | `spatial-algebra-and-joint-models` | 11 | `444c3e3ac2ccb3b556a5c7e807053f07ba2a790c` |
| [#642](https://github.com/aitofound/ScienceAccelBench/pull/642) | `constrained-dynamics` | 13 | `f057f4c921d799604203b85914172943b9f4e23c` |
| [#643](https://github.com/aitofound/ScienceAccelBench/pull/643) | `contact-solvers-and-constraint-sets` | 12 | `7ddd5dfcc9b5a272a00e37ba776a3cfd802c8f03` |
| [#644](https://github.com/aitofound/ScienceAccelBench/pull/644) | `collision-and-geometry` | 11 | `50eab4b4581aa2e96b31a30ee49eff387fae48a1` |

## Why these references cover the tasks

The keys `carpentier2019pinocchio` and `pinocchio2026software` identify the
implementation for **every row**. Method references are shared where appropriate;
a task/check does not require an invented separate publication.

| Task | Observed scientific surface | Additional BibTeX keys |
| --- | --- | --- |
| `rigid-body-algorithms` | RNEA, ABA, CRBA, kinematics and joint Jacobians, center-of-mass/centroidal quantities, energy, sparse Cholesky, and parallel ABA/RNEA | `featherstone2008rigidbody` |
| `analytical-derivatives` | First derivatives of forward/inverse dynamics, gravity, center-of-mass, centroidal/frame/point kinematics; kinematic Hessians; the separate second-order RNEA tensor check | `carpentier2018analytical`, `singh2022secondorder` |
| `spatial-algebra-and-joint-models` | Spatial vectors/inertias and symmetric algebra, SE(3)/SO(3) exp/log, Lie-group/configuration operations, roll-pitch-yaw, composite/generic/mimic/revolute joints and motion subspaces | `featherstone2008rigidbody`; the software references document Pinocchio's specific joint and configuration-manifold API |
| `constrained-dynamics` | Constrained/contact/impulse dynamics and derivatives, constraint Jacobians and Cholesky, contact ABA, PV solver, Delassus operators, and loop-constrained ABA | `carpentier2021proximal`, `sathya2025constrained`, `sathya2026closedloop`, `sathya2026matrixfree` |
| `contact-solvers-and-constraint-sets` | ADMM/PGS frictional-contact solvers, preconditioner, point/frame anchors and point/joint-friction constraints, box/orthant/Coulomb-cone projections and orthant/second-order-cone Jordan operations | `carpentier2024contact`, `lelidec2024contactmodels`; the software record covers the concrete constraint-set and Jordan-operation implementation |
| `collision-and-geometry` | Geometry model/object copying and merging, placements/radii, signed distances, SE3-to-Coal transforms, broadphase/tree-broadphase bounding boxes, parallel geometry and the geometry-models example | `montaut2024gjk` is upstream-recommended **external collision-method background**; the software references cover Pinocchio's geometry integration |

The collision module explicitly excludes Coal itself from its owned source.
For example, `cpp-collision` grades the pose handed to the narrow phase, and
`cpp-parallel-geometry` grades signed sphere gaps. The GJK++ citation does **not**
assert that these checks grade GJK++ iterations, require that algorithm, or port
Coal. Likewise, `singh2022secondorder` is method background for second-order
inverse dynamics, not a replacement for the task's tensor-layout contract.
Pinocchio's [second-order integration PR #1860](https://github.com/stack-of-tasks/pinocchio/pull/1860)
credits the refactoring of Shubham Singh's implementation; the pinned README
also credits Singh for second-order inverse-dynamics derivatives.

## Authoritative verification

The pinned upstream
[`CITATION.bib`](https://github.com/stack-of-tasks/pinocchio/blob/2ae77666e894a39127b283dcce3e2399ec19242d/CITATION.bib),
[`CITATION.cff`](https://github.com/stack-of-tasks/pinocchio/blob/2ae77666e894a39127b283dcce3e2399ec19242d/CITATION.cff),
and [`README.md` citation sections](https://github.com/stack-of-tasks/pinocchio/blob/2ae77666e894a39127b283dcce3e2399ec19242d/README.md#citing-pinocchio)
were retrieved directly. CFF supplies the six software authors, version 4.1.0,
release date 2026-07-07, repository and license. The README recommends the primary
paper and the analytical, constrained, contact and collision contributions.
Its introductory description explicitly identifies Featherstone's algorithms
as the foundation; its
[spatial-algebra documentation](https://github.com/stack-of-tasks/pinocchio/blob/2ae77666e894a39127b283dcce3e2399ec19242d/doc/a-features/a-spatial.md)
explains the SE3, Motion, Force and Inertia classes.

For the following works, author lists, titles, venues, years, and any included
volume/issue/pages were checked against the **publisher-deposited Crossref DOI
records** (`https://api.crossref.org/works/<DOI>`). RSS publisher pages were
additionally checked where listed. Every DOI below names the cited work, not a
generic topic search result.

| Key | Verified DOI / publisher source |
| --- | --- |
| `carpentier2019pinocchio` | [10.1109/SII.2019.8700380](https://doi.org/10.1109/SII.2019.8700380), plus upstream CITATION.bib |
| `featherstone2008rigidbody` | [10.1007/978-1-4899-7560-7](https://doi.org/10.1007/978-1-4899-7560-7) (Springer book, 2008) |
| `carpentier2018analytical` | [10.15607/RSS.2018.XIV.038](https://doi.org/10.15607/RSS.2018.XIV.038), [RSS XIV publisher page](https://roboticsproceedings.org/rss14/p38.html) |
| `singh2022secondorder` | [10.1109/IROS47612.2022.9981356](https://doi.org/10.1109/IROS47612.2022.9981356) |
| `carpentier2021proximal` | [10.15607/RSS.2021.XVII.017](https://doi.org/10.15607/RSS.2021.XVII.017), [RSS XVII publisher page](https://roboticsproceedings.org/rss17/p017.html) |
| `sathya2025constrained` | [10.1109/TRO.2024.3502515](https://doi.org/10.1109/TRO.2024.3502515) |
| `sathya2026closedloop` | [10.1109/TRO.2026.3651683](https://doi.org/10.1109/TRO.2026.3651683) |
| `sathya2026matrixfree` | [10.1109/LRA.2026.3701549](https://doi.org/10.1109/LRA.2026.3701549) |
| `carpentier2024contact` | [10.15607/RSS.2024.XX.108](https://doi.org/10.15607/RSS.2024.XX.108), [RSS XX publisher page and BibTeX](https://roboticsproceedings.org/rss20/p108.html) |
| `lelidec2024contactmodels` | [10.1109/TRO.2024.3434208](https://doi.org/10.1109/TRO.2024.3434208) |
| `montaut2024gjk` | [10.1109/TRO.2024.3386370](https://doi.org/10.1109/TRO.2024.3386370) |

## Version choices and limits

- The software paper and CFF-derived software release are distinct citation
  objects. The older README website citation is not added as another software
  entry. No DOI is invented for the software release.
- The pinned README lists *Constrained Articulated Body Dynamics Algorithms*
  twice, under 2024. Those mentions are deduplicated into its final journal
  publication: volume 41 (2025), pages 430–449; the DOI retains 2024.
- The closed-loop article is a **different work**, listed under 2025 upstream
  but published in volume 42 (2026), pages 819–838. The matrix-free Delassus
  paper, called a preprint upstream, now has a verified 2026 journal record.
  Final publication metadata is used without adding duplicate preprint entries.
- The RSS contact paper's Crossref author field shortens one surname to
  "Lidec". The full "Le Lidec" follows the publisher page/BibTeX and upstream.
- Method coverage is not a claim that every implementation detail has an
  individually attributable paper. No task performance, correctness or review
  status is inferred from the bibliography. Existing task/report unknowns
  remain unknown.
