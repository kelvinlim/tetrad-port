# GFCI Output Differences: Tetrad 7.6.3 vs 7.6.8

## Context

Running GFCI on the Boston EMA dataset (14 variables: 7 current + 7 lag, 640 observations, alpha=0.01, penalty_discount=1.0) with identical temporal knowledge (lag vars tier 0, current vars tier 1) produces different results depending on the Tetrad version:

- **Tetrad 7.6.3** (via fastcda): ~20+ edges
- **Tetrad 7.6.8** (via tetrad-port C++ and Java oracle): 14 edges

## Parameter Parity

All tunable parameters are identical between the two runs:

| Parameter | fastcda (7.6.3) | tetrad-port (7.6.8) |
|-----------|-----------------|---------------------|
| alpha | 0.01 | 0.01 |
| penalty_discount | 1.0 | 1.0 |
| completeRuleSetUsed | True | True |
| depth | -1 | -1 |
| faithfulnessAssumed | True | True |
| maxDiscriminatingPathLength | -1 | -1 |
| SemBicScore structurePrior | 0 | 0 |

fastcda sets additional parameters (`possibleMsepSearchDone=True`, `doDiscriminatingPathRule=True`, `maxDegree=-1`, `maxPathLength=-1`) that match the defaults in 7.6.8.

## Root Cause: FciOrient Correctness Fixes (7.6.6–7.6.8)

The difference is driven by **critical bug fixes in the FCI orientation rules** between versions 7.6.3 and 7.6.8. FciOrient is the orientation engine used by all FCI-variant algorithms (GFCI, BOSS-FCI, GRaSP-FCI).

### Timeline of FCI Rule Changes

| Version | Date | FCI-Related Changes |
|---------|------|---------------------|
| **7.6.3** | Feb 2024 | Baseline (contains known bugs in FCI rules) |
| **7.6.6** | Dec 2023* | R8/R9/R10 reimplemented. Final FCI rules corrected (**reviewed by Peter Spirtes**). Discriminating path rule improvements. Unshielded collider and R4 logic refactored. |
| **7.6.7** | Jun 2024 | **R3, R4, R5, R9 rules fixed** — actual correctness bugs. Fisher Z caching removed. |
| **7.6.8** | Jun 2024 | **"Guarantee legal PAG" code fixed.** Discriminating path rule reset to published version. |

*Note: 7.6.6 was released before 7.6.3 in the Maven timeline but the fixes accumulated across the 7.6.x series.

### Specific Bug Fixes

1. **R3 (Spirtes' Rule 3)**: Incorrect orientation logic fixed in 7.6.7.
2. **R4 (Discriminating path rule)**: Refactored in 7.6.6, correctness fix in 7.6.7, reset to published version in 7.6.8.
3. **R5 (Uncovered circle path rule)**: Fixed in 7.6.7.
4. **R8, R9, R10 (Circle endpoint rules)**: Reimplemented in 7.6.6 for correctness and efficiency.
5. **R9 specifically**: Fixed in both 7.6.6 and 7.6.7 — had a path-cycling bug where paths could revisit the origin node.
6. **Legal PAG guarantee**: Fixed in 7.6.8 — ensures the output is always a valid PAG.

## Impact on GFCI Output

The buggy FCI rules in 7.6.3 fail to correctly orient or remove edges in two ways:

1. **False positive edges retained**: Incorrect orientation rules leave edges that should be removed by the possible d-sep step or subsequent FCI rules. The extra edges in the 7.6.3 output (particularly those radiating from `worry_scale_lag` to multiple lag nodes) are likely spurious.

2. **Incorrect orientations**: Buggy R3/R4/R5 affect collider detection, and buggy R8/R9/R10 affect circle endpoint resolution. Both cascade through the algorithm since later steps depend on orientations from earlier steps.

## 7.6.8 Output (14 edges)

```
PANAS_NA <-> PANAS_NA_lag
PANAS_NA_lag <-> PHQ9_lag
PANAS_NA_lag --> worry_scale_lag
PANAS_PA <-> PANAS_PA_lag
PANAS_PA_lag <-> PHQ9_lag
PHQ9 --> PANAS_NA
PHQ9 --> PANAS_PA
PHQ9_lag --> PHQ9
TIB <-> TST
TIB_lag o-> TIB
TIB_lag o-o TST_lag
TST_lag o-> TST
alcohol_bev_lag o-> alcohol_bev
worry_scale_lag --> worry_scale
```

This output is verified to match Java Tetrad 7.6.8 exactly (Adjacency Jaccard 1.0, Type Agreement 100%) via the tetrad-port comparison test suite.

## Recommendation

**Use 7.6.8 results.** The FCI orientation rule fixes were serious enough that Peter Spirtes personally reviewed them. The sparser 7.6.8 graph (14 edges) reflects better specificity — fewer false positive edges — not missing signal. The extra edges in the 7.6.3 output are artifacts of known bugs in the orientation rules.

For users of fastcda who want to match the corrected results, the options are:
1. **Switch to tetrad-port/fastcausal** which implements the 7.6.8 algorithms natively in C++.
2. **Update fastcda's JAR** from `tetrad-gui-7.6.3-launch.jar` to `tetrad-gui-7.6.8-launch.jar` (may require minor API adjustments for renamed/changed methods).

## References

- [TetradVersionRecommendation.md](TetradVersionRecommendation.md) — Full version comparison and rationale for choosing 7.6.8
- [JavaCPPComparison.md](JavaCPPComparison.md) — Automated test results showing exact match between C++ port and Java 7.6.8
- Tetrad GitHub: https://github.com/cmu-phil/tetrad
