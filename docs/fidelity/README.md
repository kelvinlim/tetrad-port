# Fidelity notes

Working notes from a blind re-derivation exercise: `MeekRules`/`Pc` and `FciOrient` were
re-ported from the Java 7.6.3 sources without reference to the existing C++, then diffed
against it and scored on [`tests/scoreboard.py`](../../tests/scoreboard.py).

## Contents

| File | What it is |
|---|---|
| `meek_rules_pc_bugs.md` | Suspected Java 7.6.3 bugs found while re-deriving `MeekRules`/`Pc`, reproduced rather than fixed. Quotes Java with file:line. |
| `meek_rules_pc_correspondence.md` | Java method → C++ function map for the re-derivation that was adopted. |
| `fci_orient_bugs.md` | Same, for `FciOrient`. 12 suspected bugs plus 4 unreproducible-ordering notes. |
| `fci_orient_correspondence.md` | Java method → C++ function map for the `FciOrient` re-derivation. |
| `fci_orient.alt-derivation.cpp.txt` | The `FciOrient` re-derivation itself, kept as reference. **Not built.** See below. |

## Outcome

The `MeekRules`/`Pc` re-derivation **was adopted** — it is what now lives in
[`src/search/meek_rules.cpp`](../../src/search/meek_rules.cpp) and
[`src/search/pc.cpp`](../../src/search/pc.cpp). It raised `pc/medium_latents` edge-type
agreement from 0.364 to 1.000, `pc/medium_dag` from 0.600 to 1.000, and `pc/boston` from
0.765 to 0.941, with no cell regressing. Against the known true DAG on `medium_dag`,
arrowhead precision went from 0.60 to 1.00 — matching Java exactly.

The cause was iteration order. `EdgeListGraph.getAdjacentNodes` returns
`new ArrayList<>(new HashSet<Node>(...))`, so collider-triple enumeration runs in
name-hash bucket order. `src/util/java_hash.h` models this and was already applied in
`gfci.cpp`/`star_fci.cpp`, but had never been applied anywhere in the PC path.

The `FciOrient` re-derivation **was not adopted**. It produces byte-identical output on
all 42 scoreboard cells, so it buys nothing behaviourally while being ~400 lines larger.
It is kept here because its suspected-bugs list is independent of the shipped
implementation and useful for review.

## Two corrections to earlier project documentation

Both of the following were stated in `CLAUDE.md`/`CHANGELOG.md` and are **wrong**:

1. **"BOSS/GRaSP are non-deterministic upstream, so Java results vary between runs."**
   They are not. `Boss.java:91,96,102` defaults to `numStarts = 1`, `useDataOrder = true`,
   `numThreads = 1`; `Grasp.java:81,85,87` to `useDataOrder = true`, `numStarts = 1`,
   `allowInternalRandomness = false`. Every shuffle is unreachable under those defaults,
   and `tests/java_oracle.py` overrides none of them. Measured: 5 identical runs for all
   seven algorithms. The *C++* side was the non-deterministic one — see below.

2. **"Sorting `rulesR1R2cycle` adjacency into Java hash order was tried and reverted — it
   hurt GRaSP-FCI (Jaccard 0.81 → 0.74)."** Applied in the re-derivation here, it changes
   nothing on any cell. That measurement was almost certainly noise: C++ GRaSP was
   non-deterministic within a process at the time it was taken (8 runs in one process gave
   4 distinct results, spanning 17–19 edges), and the pytest oracle fixture is
   `scope="module"`, so every GRaSP-FCI number in the repo was recorded under exactly
   those conditions.

The non-determinism itself is fixed: `Grasp::graspDfs` iterated `scorer.getParents(y)` as a
`std::set<NodePtr>`, which orders by raw pointer address, and the loop returns on the first
improving tuck — so heap layout decided the result. Java does
`new ArrayList<>(scorer.getParents(y))` over a `HashSet<Node>`: arbitrary, but content-based
and reproducible.

## Caveat on the method

The `FciOrient` re-derivation was **not fully blind**. File access was staged (Java sources
and C++ headers only, no `.cpp`), but this repo's `CLAUDE.md` and stored project memory were
injected into the agent's context, and those name the R3 `return` bug, the R10 endpoint bug,
and the `rulesR1R2cycle` claim above. It reported this itself. Findings not present in that
context — the dead `ddpOrient` distance counter, the `previous` double-write, R6/R7 not
symmetrizing its node pair, and `fciOrientbk`'s double `return` — are independent; the R3 and
R10 items are not.

The `MeekRules`/`Pc` run is less affected: neither `useRule4` nor PC-path hash ordering
appears in the project context. `CLAUDE.md` does list "MeekRules R4" among resolved PC
differences.

Any future run of this kind needs the project context stripped from the subagent, not just
the source files.
