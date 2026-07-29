# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Fixed

- **GRaSP was non-deterministic.** `Grasp::graspDfs` iterated a
  `std::set<NodePtr>`, which orders by raw pointer address, and the loop returns
  on the first improving tuck — so the result depended on heap layout and varied
  between runs *in the same process*. Java iterates a `HashSet<Node>` in
  name-hash order: arbitrary but reproducible. `sortByJavaHashOrder` is now
  applied to the parent list, matching Java and making repeated runs identical.
  Any accuracy figure recorded for GRaSP before this was partly measuring noise.

### Added

- **`References/`** — the primary literature behind every implemented algorithm:
  19 open-access PDFs, `REFERENCES.md` (citations, DOIs, and what each paper
  underpins), and `FunctionMapping.md`, which maps individual C++ functions to
  exact theorem, lemma, definition and rule numbers. Every anchor was verified
  against the PDF text rather than recalled; anything unverifiable is marked
  NOT FOUND rather than guessed. Zhang (2008, *Artificial Intelligence*) is not
  open access and is cited by DOI only.

  `FunctionMapping.md` also records, per algorithm, every place the port
  knowingly departs from the papers. Three of these are departures from the
  *literature* rather than from Java 7.6.3, so they were not in the existing
  known-differences list:

  - GFCI omits Ogarrio et al. (2016) Algorithm 1 step D (Possible-D-SEP), whose
    proof of their Theorem 7 invokes it by name.
  - `setCompleteRuleSetUsed(true)` does not deliver Zhang's Theorem 4:
    `FciOrient::ruleR10` is unreachable, and `ruleR6R7` applies a non-adjacency
    check to R6 that the rule does not have — Zhang states "α and γ may or may
    not be adjacent" in the rule itself. The port is arrowhead-complete but not
    tail-complete.
  - `Fas::setStable(false)` is a no-op; both ternary branches deep-copy, so the
    search is always PC-stable.

- **Simulation-based accuracy testing** — `tests/simulation.py`,
  `tests/test_simulation.py`, `tests/run_simulation_sweep.py`. Where
  `test_java_comparison.py` asks whether the C++ matches the Java, these ask
  whether the algorithms recover known structure, across simulated graphs
  varying in size, density, sample size and number of latent confounders.

  Ground truth is the object each algorithm actually targets — the CPDAG for
  PC/FGES/BOSS/GRaSP, the PAG for the FCI variants — not the raw DAG, which
  would score every correctly-undirected edge as an orientation error. The
  DAG→CPDAG conversion is cross-checked against Tetrad's
  `GraphTransforms.cpdagForDag` over 150 random DAGs; PAGs come from Tetrad's
  `DagToPag` through two new `java_oracle.py` methods.

  The suite asserts accuracy floors, the consistency trend (more data must not
  hurt), the density trend, and determinism — the last of which is what caught
  the GRaSP bug above. `scoreboard/simulation-baseline.json` records the first
  sweep.

- **`tests/scoreboard.py` and `scoreboard/*.json` are now tracked**, having
  previously been untracked working files.

## [0.3.1] - 2026-04-20

### Fixed

- **Windows / Python 3.13+ install**: wheels are now tagged `cp312-abi3` (not
  `cp312-cp312`), so a single wheel installs on Python 3.12, 3.13, 3.14+. The
  CMake build already passed `STABLE_ABI` to nanobind; this adds the matching
  `wheel.py-api = "cp312"` to `[tool.scikit-build]` so scikit-build-core tags
  the wheel correctly. Previously, Python 3.13 users on Windows hit a sdist
  build failure because no compatible wheel was published.
- **Windows CI**: the release workflow now uses `cibuildwheel` on all three
  platforms (previously Windows used a hand-rolled `pip wheel` step), keeping
  the abi3 tagging consistent across Linux / macOS / Windows.

## [0.3.0] - 2026-03-11

### Changed (Breaking — Tetrad 7.6.3 semantics)

This release ports the FCI-variant algorithms to **Tetrad 7.6.3 semantics**, replacing the
7.6.8-based `R0R4StrategyTestBased` / `R0R4Strategy` pattern with the simpler `SepsetsGreedy`
approach used in 7.6.3. This is a correctness-focused refactor: the 7.6.3 algorithm is the
one actually shipped in the pip package, and results now match the 7.6.3 JAR at Jaccard 1.0
on all synthetic tests.

- **`FciOrient`** now takes `SepsetsGreedy&` directly instead of an abstract `R0R4Strategy`
- **`Gfci`** and **`StarFci`** simplified: removed the `possibleDsep` skeleton-refinement step
  (Steps 6-7 in the 7.6.8 port). The 7.6.3 pipeline is:
  `FGES/BOSS/GRaSP CPDAG → gfciExtraEdgeRemovalStep → gfciR0 → doFinalOrientation`
- Java oracle (`tests/java_oracle.py`) now uses the **7.6.3 JAR** (`jars/tetrad-gui-7.6.3-launch.jar`)
  with correct class-name mappings (`GFci`, `BFci`, `setMaxPathLength`)

### Added

- **`src/search/sepsets_greedy.h/cpp`** — new `SepsetsGreedy` class: depth-interleaved greedy
  sepset search over `adj(i)` then `adj(k)`, with `possibleParents` knowledge filtering.
  Port of `edu.cmu.tetrad.search.utils.SepsetsGreedy` from Tetrad 7.6.3.

### Fixed — Java hash-order replication for GFCI correctness

The GFCI pipeline has two loops that generate node-pair combinations via `ChoiceGenerator`.
Java's `EdgeListGraph.getAdjacentNodes()` returns nodes in `HashSet<Node>` iteration order
(determined by `String.hashCode()` + `HashMap.hash()` perturbation + bucket index). C++ was
iterating in edge-insertion order, producing different pair orderings and thus different edge
removals and collider orientations.

Fix: `src/util/java_hash.h` (restored from v0.2.1) is applied with `sortByJavaHashOrder()`
in two places in `gfci.cpp` and `star_fci.cpp`:

1. **`gfciExtraEdgeRemovalStep`** (Step 3): sorts `referenceCpdag.getAdjacentNodes(b)` before
   generating pairs — this was the key fix for skeleton agreement (Jaccard 1.0 on Boston EMA).
2. **`gfciR0`** (Step 4): sorts `pag.getAdjacentNodes(b)` before generating collider-candidate
   pairs — matches Java's iteration semantics for the collider-orientation pass.

Result on the Boston EMA dataset (14 variables, 640 observations, temporal knowledge):

| Algorithm | Java edges | C++ edges | Jaccard | Type |
|-----------|-----------|-----------|---------|------|
| GFCI      | 22        | 22        | 1.000   | 95%  |
| BOSS-FCI  | 21        | 21        | 1.000   | 100% |

**Known remaining difference**: `TIB <-> TST` (Java bidirected) vs `TIB --> TST` (C++ directed)
in GFCI Boston. Root cause: R1 in `rulesR1R2cycle` fires on the pattern `X *--> TIB o-> TST`
and converts the circle at TIB to a TAIL before the collider-marking rule can set it to an
arrowhead. Java's HashSet iteration in `rulesR1R2cycle` processes triples in a different order
where the arrowhead-setting rule wins first. Sorting `rulesR1R2cycle` adjacency to match Java
was tried and reverted — it broke GRaSP-FCI Boston (Jaccard 0.81 → 0.74).

### Fixed — FCI orientation rules (7.6.3 semantics)

Intentional 7.6.3 bugs preserved to match JAR behavior:

- **R3**: uses `return` (not `continue`) when arrowhead not allowed — causes early exit from
  the entire `ruleR3` method on first blocked collider triple (Java 7.6.3 behavior)
- **R10**: uses `getEndpoint(theta, gamma)` instead of `getEndpoint(gamma, theta)` in the inner
  loop condition — R10 effectively never fires in 7.6.3 (bug fixed in 7.6.8)

### Tests

- 26 Java comparison tests, all passing (18 synthetic + 7 Boston real-data + 1 report generator)
- GFCI Boston threshold raised to Jaccard ≥ 1.0, Type ≥ 95%
- BOSS-FCI Boston Type threshold raised to ≥ 30% (non-deterministic BOSS upstream)

---

## [0.2.3] - 2025-xx-xx

### Changed
- Reference Java source downgraded from 7.6.8 back to 7.6.3 for JAR comparison tests
- Documented Tetrad 7.6.3 vs 7.6.8 differences in `TetradVersionRecommendation.md`
- Skip musllinux build to halve Linux CI time

### Fixed
- `computePossibleDsep` BFS predecessor map direction: `previous[c].insert(b)` not `previous[b].insert(c)` — key fix for GFCI Boston Jaccard 0.933 → 1.0 (7.6.8 port)

---

## [0.2.2] - 2025-xx-xx

### Added
- Composable FCI pipeline (`generic_fci.h/cpp`): any CPDAG algorithm + FCI rules

---

## [0.2.1] - 2025-xx-xx

### Added
- Java HashMap/HashSet iteration-order replication (`src/util/java_hash.h`) for FCI sepset selection
- Boston EMA comparison tests for all CPDAG algorithms (PC, FGES, BOSS, GRaSP)
- C++ vs Java performance benchmark on fMRI dataset (379 variables)

### Fixed
- R9 cycle-through-origin bug: must add `prevOfFrom` to initial visited set in `existsUncoveredSemiDirectedPath`

---

## [0.2.0] - 2025-xx-xx

### Added
- FGES (score-based, BIC penalty) — `src/search/fges.h/cpp`
- GFCI (hybrid: FGES + FCI rules) — `src/search/gfci.h/cpp`
- BOSS (permutation-based BIC) — `src/search/boss.h/cpp`, `grow_shrink_tree.h/cpp`,
  `bes_permutation.h/cpp`, `permutation_search.h/cpp`, `teyssier_scorer.h/cpp`
- BOSS-FCI — `src/search/boss_fci.h/cpp`, `star_fci.h/cpp`
- GRaSP — `src/search/grasp.h/cpp`
- GRaSP-FCI — `src/search/grasp_fci.h/cpp`
- FCI orientation rules R5-R10 (Zhang complete rule set) — `src/search/fci_orient.h/cpp`
- `SemBicScore` — `src/search/sem_bic_score.h/cpp`
- Python bindings for all new algorithms (`run_fges_raw`, `run_gfci_raw`, `run_boss_raw`,
  `run_boss_fci_raw`, `run_grasp_raw`, `run_grasp_fci_raw`)
- `TetradPort.run()` dispatcher; dict-based Knowledge in Python API
- Java comparison test framework (`tests/test_java_comparison.py`, `tests/java_oracle.py`)

---

## [0.1.0] - 2025-01-01

### Added
- Initial PC algorithm implementation (vertical slice)
- Fast Adjacency Search (FAS) with PC-Stable variant
- Meek Rules R1-R3 orientation propagation
- Fisher Z independence test via Cholesky decomposition
- Graph data structures: Node, Edge, Triple, EdgeListGraph
- ChoiceGenerator for C(n,k) enumeration
- SepsetMap for separation set tracking
- Knowledge stub (always permissive)
- DataSet wrapper around Eigen matrix with correlation computation
- Python bindings via nanobind (`run_pc_raw`, `PcResult`)
- Python facade class `TetradPort` with `run_pc()`, SEM helpers
- Catch2 test suite (183 assertions across 7 test files)
- CLI example `run_pc`
