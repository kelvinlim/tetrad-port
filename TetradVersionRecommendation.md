# Tetrad Version Recommendation: 7.6.3 vs Latest (7.6.10)

## Purpose

This document evaluates which version of CMU's Tetrad causal inference library should serve as the reference for our C++ port, given the target algorithms: **GFCI, BOSS, BOSS-FCI, GRASP, GRASP-FCI**.

---

## Algorithm Availability in 7.6.3

All 5 target algorithms exist in 7.6.3 and are mature:

| Algorithm | Java Class | Lines | Key Dependencies |
|-----------|-----------|-------|-----------------|
| **GFCI** | `GFci.java` | 278 | Fges, FciOrient, SepsetsGreedy, Score + IndependenceTest |
| **BOSS** | `Boss.java` | 505 | PermutationSearch, BesPermutation, GrowShrinkTree, Score |
| **BOSS-FCI** | `BFci.java` | 241 | Boss, FciOrient, SepsetsGreedy, Score + IndependenceTest |
| **GRASP** | `Grasp.java` | 532 | TeyssierScorer, Score, IndependenceTest (optional) |
| **GRASP-FCI** | `GraspFci.java` | 300 | Grasp, FciOrient, SepsetsGreedy, Score + IndependenceTest |

---

## Version Timeline: 7.6.3 → 7.6.10

Seven releases since 7.6.3 (Feb 2024 → Jan 2025):

| Version | Date | Key Changes Relevant to Target Algorithms |
|---------|------|------------------------------------------|
| **7.6.3** | Feb 2024 | Bug fix release. Our current PC port base. |
| **7.6.4** | May 2024 | ForkJoinPool threading standardization. Seed params added. Minor fixes. |
| **7.6.5** | Aug 2024 | **LV-Lite** added (score-based FCI alternative). **BOSS-PAG** added. Final FCI rule optimization. |
| **7.6.6** | Dec 2023 | **FciOrient refactored** — R8/R9/R10 reimplemented. Final FCI rules corrected (reviewed by Spirtes). Discriminating path rule improvements. Meek rules fixes. Unshielded collider and R4 logic refactoring. |
| **7.6.7** | Jun 2024 | **EJML migration** replacing all matrix libs. Fixed R3/R4/R5/R9 FCI orientation rules. Renamed LV-Dumb→BOSS-PAG. Removed Fisher Z caching. |
| **7.6.8** | Jun 2024 | Fixed "guarantee legal PAG" code. Reset discriminating path rule to published version. |
| **7.6.9** | Oct 2024 | Improved Fisher Z with Ledoit-Wolf shrinkage. New KCI, RCIT tests. PC algorithms default to max-P. |
| **7.6.10** | Jan 2025 | Fixed rare CPDAG-to-DAG bug in FGES. Recursive adjustment sets. |

---

## Critical Changes Affecting Target Algorithms

### 1. FciOrient — HEAVILY MODIFIED (7.6.6–7.6.8)

FciOrient (1254 lines) is used by **all FCI variants**: GFCI, BOSS-FCI, GRASP-FCI. This is the single most important difference between versions.

Changes since 7.6.3:
- **R3, R4, R5, R9 rules fixed** (7.6.7) — actual correctness bugs
- **R8, R9, R10 reimplemented** for efficiency (7.6.6)
- **Discriminating path rule reset to published version** (7.6.8)
- **Final FCI rules reviewed and corrected by Peter Spirtes himself** (7.6.6)
- **"Guarantee legal PAG" code fixed** (7.6.8)
- **Unshielded collider and R4 logic refactored** (7.6.6)
- Multiple discriminating paths handling via msep blocking (7.6.6)

**Impact**: Porting FciOrient from 7.6.3 means porting code with **known bugs** in the FCI orientation rules. Since GFCI, BOSS-FCI, and GRASP-FCI all depend on FciOrient, this affects 3 of our 5 target algorithms.

### 2. BOSS and GRASP — Minor Changes

- **Seed parameter** added for reproducibility (7.6.2/7.6.4)
- **Threading** standardized via ForkJoinPool (7.6.4)
- **Constant column handling** fixed (7.6.1)
- **Knowledge bug fix** in tiered knowledge (7.6.1)

The core BOSS and GRASP algorithms are **algorithmically stable** between 7.6.3 and 7.6.10. No fundamental changes to the search logic.

### 3. Score Infrastructure — Moderate Changes

- **EJML migration** (7.6.7) — all matrix operations moved from Apache Commons Math to EJML. Does not affect us (we use Eigen).
- **Fisher Z caching removed** (7.6.7)
- **Fisher Z improved** with Ledoit-Wolf shrinkage (7.6.9) — better for high-dimensional data
- **Rare CPDAG-to-DAG bug fixed** in FGES scoring (7.6.10)

### 4. New Algorithms Worth Noting

- **LV-Lite** (7.6.5) — score-based FCI correlate that substitutes score-based steps for independence testing steps in GFCI. Described as "made correct from a d-separation oracle." Could be a better algorithm than GFCI for some use cases.
- **BOSS-PAG** (7.6.5/7.6.7) — heuristic that runs BOSS and reports the PAG of the DAG it generates.

---

## Dependency Graph for Target Algorithms

```
Score (interface, 170 lines)
├── SemBicScore (716 lines) — continuous data
├── BdeuScore (340 lines) — discrete data
└── [others as needed]

GFCI ─────────────┐
BOSS-FCI (BFci) ──┤── FciOrient (1254 lines) ← HEAVILY CHANGED since 7.6.3
GRASP-FCI ────────┘   ├── SepsetProducer / SepsetsGreedy (48 + 191 lines)
                      ├── PossibleMsepFci (225 lines)
                      ├── PossibleMConnectingPath (274 lines)
                      └── DagToPag (272 lines)

GFCI ──────→ Fges (1237 lines) → Bes (554 lines)

BOSS-FCI ──→ Boss (505 lines) → PermutationSearch (195 lines)
                                → BesPermutation (596 lines)
                                → GrowShrinkTree (226 lines)

GRASP-FCI ─→ Grasp (532 lines) → TeyssierScorer (773 lines)

Shared infrastructure (already partially ported):
  Knowledge, MeekRules, Graph/Edge/Node, IndependenceTest, SepsetMap
```

Total new Java code to port: **~7,500 lines** across all algorithms and dependencies.

---

## Recommendation

### Use 7.6.8 as the reference version.

| Factor | 7.6.3 | 7.6.8 | Winner |
|--------|-------|-------|--------|
| FciOrient correctness | Known bugs in R3/R4/R5/R8/R9/R10 | Fixed and reviewed by Spirtes | **7.6.8** |
| BOSS/GRASP core | Stable | Stable + seed param + threading | **7.6.8** |
| Discriminating path rule | Pre-fix | Reset to published version | **7.6.8** |
| PAG legality guarantee | Broken | Fixed | **7.6.8** |
| Matrix library | Apache Commons Math | EJML | Neutral (we use Eigen) |
| API stability | Stable | Minor additions only | Neutral |
| PC algorithm (already ported from 7.6.3) | Matches | Minor drift | Slight cost to update |

### Rationale

1. **7.6.8 has all the critical FciOrient fixes** (accumulated from 7.6.6 + 7.6.7 + 7.6.8).
2. The PAG legality guarantee is fixed — essential for producing valid PAGs.
3. BOSS and GRASP are algorithmically identical to 7.6.3 — no porting cost difference.
4. The EJML migration (7.6.7) does not affect us — we use Eigen for all linear algebra.
5. Changes after 7.6.8 (7.6.9, 7.6.10) add new algorithms and tests but do not modify our target algorithms.
6. The cost to update our existing PC port is minimal — PC itself did not change between versions.

### Practical Path Forward

1. **Keep 7.6.3** as reference for PC/FAS/MeekRules (already ported, differences documented in the 7.6.3 review).
2. **Obtain tetrad 7.6.8** as the reference for porting FciOrient, GFCI, BOSS, BOSS-FCI, GRASP, GRASP-FCI.
3. **Apply Phase 1 fixes** from the 7.6.3 review to align existing PC/MeekRules code.
4. **Port new algorithms** from 7.6.8 source, starting with shared infrastructure (Score, FciOrient) then the algorithms in priority order.

---

## Existing PC Port: 7.6.3 Review Summary

Our current C++ port of PC/FAS/MeekRules from 7.6.3 has the following known differences (documented separately):

### Critical
- MeekRules R2/R3: C++ continues iterating after first orientation; Java returns immediately
- Collider orientation: Missing ConflictRule system (C++ always overwrites)
- MeekRules R4: Not implemented (only matters with Knowledge)

### Moderate
- FAS: Missing `noEdgeRequired()` check, missing `isRequired` in `possibleParentOf`
- Missing `pcOrientbk()` step for background knowledge
- IndTestFisherZ: Missing pseudoinverse fallback for singular matrices

### Low Impact (no-knowledge case)
- Knowledge stub is functionally correct for the empty-knowledge case
- Fisher Z: C++ is actually safer (correlation clamping, df validation)

These differences do not affect correctness when Knowledge is empty (our current usage), but should be addressed before adding Knowledge support.

---

## Sources

- [Tetrad GitHub Releases](https://github.com/cmu-phil/tetrad/releases)
- [Boss 7.6.4 Javadoc](https://www.phil.cmu.edu/tetrad-javadocs/7.6.4/edu/cmu/tetrad/search/Boss.html)
- [Grasp 7.6.5 Javadoc](https://www.phil.cmu.edu/tetrad-javadocs/7.6.5/edu/cmu/tetrad/search/Grasp.html)
- [BOSS Paper — NeurIPS](https://openreview.net/forum?id=80g3Yqlo1a)
- [BOSS Paper — PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC11393735/)
- [Tetrad Maven Repository](https://mvnrepository.com/artifact/io.github.cmu-phil/tetrad-lib)
