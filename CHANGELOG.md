# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Fixed

- **GRaSP was non-deterministic.** `Grasp::graspDfs` iterated a
  `std::set<NodePtr>`, which orders by raw pointer address, and the loop returns
  on the first improving tuck — so the result depended on heap layout and varied
  between runs *in the same process*. Java iterates a `HashSet<Node>` in
  name-hash order: arbitrary but reproducible. `sortByJavaHashOrder` is now
  applied to the parent list, so repeated runs are identical.

  Measured on Boston EMA, 20 runs before the fix: `grasp` returned three
  distinct results (Jaccard/type `0.833/0.840` ×8, `0.862/0.880` ×5,
  `1.000/1.000` ×7) and `grasp_fci` returned four. **Every GRaSP and GRaSP-FCI
  figure recorded before this fix was a single draw from a distribution**, so
  historical before/after comparisons of those cells mean nothing — including
  the apparent GRaSP gain and GRaSP-FCI regression in `JavaCPPComparison.md`.

  Determinism is achieved; **agreement with Java is not yet right, and the cause
  is still open.** Post-fix, `grasp/boston` settles deterministically on
  `0.862/0.880` — yet perfect agreement (`1.000/1.000`) occurred in 7 of the 20
  pre-fix runs, so the Java-matching trajectory is reachable and this is a
  tie-breaking difference somewhere, not a numerical one.

  What has been ruled out, by experiment rather than inspection:

  - **Not the restart shuffle.** Both sides default to `useDataOrder = true`,
    `numStarts = 1`, so neither shuffles and no RNG is involved.
  - **Not the within-bucket tie-break in the parent list.** Sweeping seven
    orderings through `graspDfs` (bucket order with ties broken by name, reverse
    name, permutation index, reverse permutation index; plus pure name order)
    gave byte-identical results for every bucket-based variant: `0.862/0.880`.
    Only discarding bucket order entirely (pure permutation order) moved the
    number, and it moved it the wrong way, to `0.833/0.840`. So bucket order is
    doing real work and is probably correct; the tie-break within it is not the
    lever. Collisions do exist among the 14 Boston names — `{TST,
    PANAS_NA_lag, PHQ9_lag}`, `{TIB, TIB_lag}`, `{PANAS_NA, worry_scale}`,
    `{alcohol_bev, PANAS_PA_lag}` at capacity 16 — but evidently not inside the
    small parent sets the search actually encounters.
  - **`graspDfs` itself is not implicated**: its body was checked line by line
    against `Grasp.java:439-500`, including the `do/while` with the `first` flag.

  The residual disagreement is therefore upstream of parent iteration order.
  Adjacency differs, not just orientation (`grasp/boston`: same 27 edges, but
  Java has `PHQ9_lag–TIB_lag` and `PHQ9_lag–TST_lag` where C++ has
  `PANAS_NA_lag–TST_lag` and `PHQ9_lag–worry_scale_lag`), which means the search
  settled on a different permutation rather than merely orienting one
  differently. Remaining suspects, in order of likelihood: tie-breaking inside
  `GrowShrinkTree` grow/shrink, which decides *which* parents are found at all;
  and edge insertion order in `TeyssierScorer::getGraph`, which would explain the
  three mark disagreements but not the adjacency ones. The next probe is to
  instrument the first divergent tuck decision on this dataset and compare it
  against Java at that point.

- **`SemBicScore` did not compute Java's score.** It used the full Gaussian
  log-likelihood and doubled it, giving `cpp = 2*java - n*(log(2*pi)+1)` per variable
  (measured: `cpp = 2*java - 25427.3785`, holding to 8e-7 across eight permutations). That is
  a positive affine transform, so it preserves every score comparison — which is why FGES,
  BOSS and GFCI matched Java exactly despite it. Now matches Java's arithmetic term for term
  (`SemBicScore.java:344-372`); the same permutation scores `-6787.150588` on both sides.

  This matters for GRaSP specifically, because `graspDfs` branches on **exact floating-point
  equality**: `sNew == sOld` recurses a level deeper, `sNew > sOld` accepts and returns.
  Changing the scale moved near-ties across that knife-edge.

  No effect on Java agreement (0 improved, 0 regressed across 42 cells). On the ground-truth
  simulation sweep it shuffles four GRaSP cells by 0.02-0.07 in both directions, which is the
  chaotic-branch behaviour described in `docs/fidelity/grasp_divergence.md`, not a systematic
  change.

- **Root cause of the GRaSP/Java divergence identified; see
  `docs/fidelity/grasp_divergence.md`.** The two suspects carried in this changelog —
  `GrowShrinkTree` tie-breaking and `TeyssierScorer::getGraph` insertion order — are **ruled
  out**: scoring eight independent permutations on both sides gives identical scores *and*
  identical edge counts on every one, so for a given permutation the two implementations
  agree completely. The divergence is in which permutation the search reaches.

  With the score aligned, both searches now take identical tucks with identical improvements
  for the first two steps, then C++ accepts a tuck Java does not — with an improvement of
  **9.09495e-13**. The running score is ~6787, whose ULP is ~9.09e-13, so that is exactly one
  unit in the last place. Java sees an exact tie there and recurses instead.

  The residual is therefore floating-point non-associativity (summation order, and Eigen
  versus Apache Commons Math in the residual variance) amplified by an exact-equality branch.
  Matching it would need bit-identical linear algebra, which Eigen will not give. C++ GRaSP
  ends on a permutation its own scorer rates 1.129 *worse* than Java's.

  Practical consequence: treat movement in `grasp` and `grasp_fci` cells as noise unless it
  is large or has a traced cause.

- **`Grasp::graspDfs` now logs each accepted tuck under `setVerbose(true)`**, matching Java's
  format (`Grasp.java:505-508`) so the two traces can be diffed directly.

- **PC over-oriented, and the cause was iteration order.** `EdgeListGraph.getAdjacentNodes`
  returns `new ArrayList<>(new HashSet<Node>(...))`, so Java enumerates collider triples in
  name-hash bucket order. `src/util/java_hash.h` has modelled that since v0.3.0 and was
  applied in `gfci.cpp`/`star_fci.cpp`, but had never been applied anywhere in the PC path.
  It is now applied at collider-triple enumeration in `pc.cpp`, at the `orientImplied` edge
  sweep, and in `getCommonAdjacents`.

  Edge-type agreement with the 7.6.3 JAR: `pc/medium_latents` 0.364 -> **1.000**,
  `pc/medium_dag` 0.600 -> **1.000**, `pc/boston` 0.765 -> **0.941**. No cell regressed, and
  the simulation ground-truth sweep is exactly neutral (0 improved, 0 regressed).

  This is a correctness fix, not merely an agreement fix. On `medium_dag`, where the true DAG
  is known and both sides produce an identical skeleton, C++ emitted 10 arrowheads of which 6
  were correct (precision 0.60); it now emits 9, all correct (1.00), matching Java exactly.

- **Meek R4 fired in C++ where Java's never does.** Java gates `meekR4` on a `useRule4` field
  assigned exactly once, in the constructor, as `!this.knowledge.isEmpty()` — at which point
  `knowledge` is still the freshly-constructed empty `Knowledge` (`MeekRules.java:47,64,281`).
  `setKnowledge` never recomputes it and nothing else assigns it, so R4 is unreachable. The
  port tested `knowledge_.isEmpty()` live, implementing the Javadoc's stated intent rather
  than the code's behaviour, and so fired R4 on every knowledge-bearing run. R4 is now
  transcribed in full but left unreachable, matching the JAR. No measured effect on any
  current cell — its preconditions never fired on this data — but it was a live
  over-orientation path.

- **`doDdpOrientation` set one of Java's two arrowheads.** Java's R4 collider branch guards
  with `isArrowheadAllowed(a, b)` *and* `isArrowheadAllowed(c, b)`, then sets both
  `setEndpoint(a, b, ARROW)` and `setEndpoint(c, b, ARROW)` (`FciOrient.java:867-877`); the
  port checked and set only the `c` end, and discarded `a` in the signature. Its tail branch
  also carried an extra `sepset.contains(b)` guard that Java's `else if
  (doDiscriminatingPathTailRule)` does not have (`FciOrient.java:885`). Both corrected, and
  Java's `isAdjacentTo(d, c)` precondition added.

- **`gfciR0` called the wrong `fciOrientbk`.** 7.6.3 has two separate implementations:
  `FciOrient.fciOrientbk` (`FciOrient.java:1020`), which guards every edge with
  `isArrowheadAllowed`, and `GraphUtils.fciOrientbk` (`GraphUtils.java:1833`), which has no
  such guard and forces the endpoint. `GraphUtils.gfciR0` calls the unguarded one, and calls
  it unconditionally rather than gating on `knowledge.isEmpty()` (`GraphUtils.java:1793`).
  `Gfci::gfciR0` and `StarFci::gfciR0` used the guarded variant behind an emptiness check.
  Added `FciOrient::graphUtilsFciOrientbk` and switched both call sites.

  **Neither fix moves any current cell**, and the ground-truth simulation sweep is likewise
  neutral. On the Boston tier knowledge the removed guard never rejected anything — tiers
  forbid `current -> lag`, while the guard tests the opposite direction — and the added R4
  arrowhead lands where one already exists. They are recorded because they are confirmed
  divergences from the JAR that will bite on knowledge sets this test suite does not cover
  (explicit forbidden edges in both directions, or required edges).

### Changed

- **Java-comparison thresholds tightened to measured values.** All 25 bounds in
  `tests/test_java_comparison.py` were far below observed performance, so the suite could not
  distinguish a fixed cell from a broken one. Eighteen are now exact (1.0/1.0). The worst
  offender was `boss_fci/boston`, asserting >= 0.30 edge-type agreement against an actual
  **1.000** — a perfect result nobody knew about, hidden behind a 0.70 slack bound.

- **Two claims in this changelog and `CLAUDE.md` were wrong and are corrected in
  `docs/fidelity/README.md`.** (1) The Java oracle is *not* non-deterministic for BOSS/GRaSP:
  every randomness path is gated off by default (`Boss.java:91,96,102`,
  `Grasp.java:81,85,87`) and `java_oracle.py` overrides none of them; measured 5 identical
  runs for all seven algorithms. (2) "Sorting `rulesR1R2cycle` adjacency was tried and
  reverted — it hurt GRaSP-FCI (0.81 -> 0.74)" does not reproduce: an independent
  re-derivation of `FciOrient` applying Java hash order at all six `getAdjacentNodes` sites,
  including that one, produces byte-identical output on all 42 scoreboard cells. That
  measurement was taken while C++ GRaSP was still non-deterministic.

### Added

- **`docs/fidelity/`** — notes from a blind re-derivation exercise: `MeekRules`/`Pc` and
  `FciOrient` re-ported from the Java 7.6.3 sources without reference to the existing C++,
  then diffed and scored. Includes suspected-bug lists (12 for `FciOrient`, 9 for
  `MeekRules`/`Pc`, all quoted with Java file:line and reproduced rather than fixed),
  Java-method correspondence tables, and the unadopted `FciOrient` derivation kept for
  reference. The README records the method's one failure: the `FciOrient` run was not truly
  blind, because this repo's `CLAUDE.md` and stored project memory were injected into the
  agent's context and name two of the bugs it "found".

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
