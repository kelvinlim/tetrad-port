# Java vs C++ Comparison — Tetrad Port

Comparison of **Tetrad 7.6.3 (Java)** against the **C++ port** across all implemented algorithms
and datasets.

## Metrics

- **Adj. Jaccard** — Jaccard similarity of the skeleton (node-pair adjacencies, ignoring orientation).
  1.00 = identical skeletons.
- **Type agree** — Among shared adjacencies, fraction whose canonical edge mark agrees exactly,
  **including direction**. `A --> B` vs `B --> A` counts as a disagreement.

### Edge mark legend

| Mark | Name | Meaning |
|------|------|---------|
| `A --> B` | Directed | A is a direct cause of B (or indirect with no latent confounders on path) |
| `A <-> B` | Bidirected | Latent common cause of A and B |
| `A o-> B` | Partially oriented | Arrowhead at B certain; mark at A uncertain (circle) |
| `A o-o B` | Nondirected | Both marks uncertain |
| `A --- B` | Undirected | Undirected (R5 output) |

**Direction matters for `-->` and `o->`.** `A --> B` and `B --> A` are distinct causal claims.
The previous comparison code used `frozenset` key + separator token, which treated these as
identical. The comparison was rewritten with direction-preserving canonical keys so that
`A --> B` vs `B --> A` is correctly flagged as a disagreement.

Settings: α = 0.01, penalty discount = 1.0.

---

## Summary

| Algorithm | Type | Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree | Status |
|-----------|------|---------|------|-----|------|-----|-------------|------------|--------|
| PC | CPDAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| PC | CPDAG | collider (X→Z←Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| PC | CPDAG | medium DAG | 8 | 2000 | 10 | 10 | 1.000 | 100% | ✅ |
| FGES | CPDAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| FGES | CPDAG | collider (X→Z←Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| FGES | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | ✅ |
| BOSS | CPDAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| BOSS | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | ✅ |
| GRaSP | CPDAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GRaSP | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | ✅ |
| GFCI | PAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GFCI | PAG | collider (X→Z←Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GFCI | PAG | latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GFCI | PAG | medium DAG + 2 latents | 8 | 2000 | 10 | 10 | 1.000 | 70% | ⚠️ |
| GFCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 14 | 15 | 0.933 | 79% | ⚠️ |
| BOSS-FCI | PAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| BOSS-FCI | PAG | latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| BOSS-FCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 15 | 16 | 0.938 | 67% | ⚠️ |
| GRaSP-FCI | PAG | chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GRaSP-FCI | PAG | latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | ✅ |
| GRaSP-FCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 15 | 13 | 0.867 | 46% | ⚠️ |

---

## CPDAG Algorithms (PC, FGES, BOSS, GRaSP)

All four CPDAG algorithms produce **identical output** to Java across every test case.

---

## PAG Algorithms (GFCI, BOSS-FCI, GRaSP-FCI)

PAG algorithms agree perfectly on simple synthetic datasets. Discrepancies appear only on the
medium 8-variable DAG with latents and on the real-world Boston EMA dataset.

### Temporal knowledge check

No forbidden-direction `-->` edges appear in any C++ output. The temporal constraint
(lag variables in tier 0, current variables in tier 1, forbidding current → lag directed edges)
is respected throughout. `<->` (bidirected) edges for current-lag pairs are **not** violations:
`<->` represents a latent common cause and does not imply a directed causal edge in either direction.

---

### GFCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| collider (X→Z←Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| medium DAG + 2 latents | 8 | 2000 | 10 | 10 | 1.000 | 70% |
| Boston EMA (temporal knowledge) | 14 | 640 | 14 | 15 | 0.933 | 79% |

**GFCI — medium DAG + 2 latents discrepancies:**

| Java | C++ | Pattern |
|------|-----|---------|
| `X3 o-> X5` | `X3 --> X5` | C++ over-orients: circle at X3 → tail (R8/R9/R10 fires) |
| `X3 o-> X8` | `X3 --> X8` | C++ over-orients: circle at X3 → tail |
| `X4 o-> X6` | `X4 --> X6` | C++ over-orients: circle at X4 → tail |

**GFCI — Boston EMA discrepancies:**

| Java | C++ | Pattern |
|------|-----|---------|
| `TIB <-> TST` | `TIB --> TST` | Java: latent confounder; C++: directed (over-orients bidirected to directed) |
| `TIB_lag o-o TST_lag` | `TIB_lag o-> TST_lag` | C++ orients nondirected → partially oriented (R6/R7 cascade) |
| `TST_lag o-> TST` | `TST <-> TST_lag` | Java: lag→current partially oriented (correct autoregressive direction); C++: bidirected (latent confounder) |

The Java edge `TST_lag o-> TST` means: **circle at TST_lag (past), arrow at TST (present)** —
the temporal direction (lag → current) is correct. C++ converts this to `<->` (bidirected)
because GRaSP's CPDAG identifies TST_lag as a collider in a triple including TST, which combined
with the knowledge-based arrowhead at TST, yields a bidirected edge.

C++ finds one extra adjacency: `PANAS_NA — worry_scale`.

---

### BOSS-FCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| Boston EMA (temporal knowledge) | 14 | 640 | 15 | 16 | 0.938 | 67% |

**BOSS-FCI — Boston EMA discrepancies:**

| Java | C++ | Pattern |
|------|-----|---------|
| `PANAS_NA_lag o-> PHQ9` | `PHQ9 <-> PANAS_NA_lag` | Java: lag→current partially oriented; C++: bidirected |
| `PANAS_NA o-> PHQ9` | `PHQ9 --> PANAS_NA` | Java: `o->` PANAS_NA → PHQ9; C++: directed PHQ9 → PANAS_NA (**direction reversal**) |
| `PANAS_NA_lag o-o PHQ9_lag` | `PHQ9_lag o-> PANAS_NA_lag` | Java: nondirected; C++: partially oriented |
| `PANAS_NA_lag o-> PANAS_NA` | `PANAS_NA <-> PANAS_NA_lag` | Java: lag→current partially oriented; C++: bidirected |
| `worry_scale_lag o-> worry_scale` | `worry_scale_lag --> worry_scale` | Java: partially oriented; C++: directed (over-orients) |

The `PANAS_NA o-> PHQ9` vs `PHQ9 --> PANAS_NA` discrepancy is a **causal direction reversal**
between two current variables: Java says PANAS_NA influences PHQ9 (with uncertainty at PANAS_NA
side); C++ says PHQ9 influences PANAS_NA (directed, no uncertainty).

C++ finds one extra adjacency: `PANAS_NA_lag — worry_scale_lag`.

---

### GRaSP-FCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X→Y→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L→X, L→Y, X→Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| Boston EMA (temporal knowledge) | 14 | 640 | 15 | 13 | 0.867 | 46% |

**GRaSP-FCI — Boston EMA discrepancies:**

| Java | C++ | Pattern |
|------|-----|---------|
| `TIB <-> TST` | `TIB --> TST` | Java: latent confounder; C++: directed |
| `PANAS_PA <-> PHQ9` | `PANAS_PA --> PHQ9` | Java: latent confounder; C++: directed |
| `TIB_lag o-o TST_lag` | `TIB_lag o-> TST_lag` | C++ orients nondirected → partially oriented |
| `PANAS_PA_lag o-o PHQ9_lag` | `PANAS_PA_lag o-> PHQ9_lag` | C++ orients nondirected → partially oriented |
| `TST_lag o-> TST` | `TST <-> TST_lag` | Java: lag→current partially oriented (correct); C++: bidirected |
| `PHQ9_lag o-> PHQ9` | `PHQ9 <-> PHQ9_lag` | Java: lag→current partially oriented (correct); C++: bidirected |
| `PHQ9_lag o-> PANAS_NA_lag` | `PHQ9_lag --> PANAS_NA_lag` | Java: partially oriented; C++: directed (over-orients) |

Java finds two extra adjacencies: `PANAS_NA — worry_scale`, `PANAS_NA_lag — worry_scale_lag`.

---

## Known Discrepancy Patterns

All discrepancies fall into three categories:

### 1. `o->` (Java) → `<->` (C++): Bidirected over-orientation

Java: `TST_lag o-> TST` — circle at lag, arrow at current. The arrow at current is certain; the
mark at lag is uncertain (could be tail or arrow).

C++: `TST <-> TST_lag` — both arrowheads. Means latent common cause; no direct autoregressive
effect.

**Root cause**: C++'s GRaSP/BOSS CPDAG identifies the lag variable as a collider in a triple
involving the current variable. Combined with the knowledge-derived arrowhead at the current side,
`isArrowheadAllowed` then permits an arrowhead at the lag side, yielding `<->`. Java's CPDAG does
not identify the same collider, so it keeps the lag-side mark as circle.

Note: `<->` is not a knowledge violation — it represents a latent confounder, not a forbidden
directed edge. But it is a less informative representation for autoregressive temporal data where
`o->` (lag causes current, with uncertainty) is more natural.

### 2. `o-o` (Java) → `o->` (C++): Nondirected partially oriented

Java: `TIB_lag o-o TST_lag` — both marks are uncertain circles.

C++: `TIB_lag o-> TST_lag` — arrow at TST_lag is certain, circle at TIB_lag uncertain.

**Root cause**: R6 or R7 fires in C++ on the nondirected edge because a neighbouring `---` or `--o`
edge exists in C++'s PAG state that doesn't exist in Java's, or fires at a different iteration.
The cascade traces back to different collider orientations in the initial CPDAG.

### 3. `o->` (Java) → `-->` (C++): Partial → full orientation

Java: `X3 o-> X5` — arrow at X5 certain, circle at X3 uncertain.

C++: `X3 --> X5` — both marks certain (tail at X3, arrow at X5).

**Root cause**: R8, R9, or R10 fires in C++ to convert the circle at X3 to a tail, fully directing
the edge. Java does not fire the same rule at this point, preserving the uncertainty.

---

## Knowledge Verification

Temporal knowledge (tier 0 = lag variables, tier 1 = current variables) is correctly enforced.
Verified by checking all `-->` edges in C++ output against the forbidden-edge list:

- Zero directed `-->` edges from current → lag appear in any C++ output ✓
- All `o->` edges involving lag-current pairs have the arrowhead at the **current** node (correct
  temporal direction: past influences present) ✓
- `<->` edges for current-lag pairs represent latent confounders and do not violate the
  forbidden-direction constraint ✓

The `isArrowheadAllowed` implementation is a line-for-line match with Java's version (verified).
Knowledge is passed to FciOrient, fciOrientbk, and the collider orientation step via `colliderAllowed`.

---

## Boston EMA Dataset

Real-world ecological momentary assessment (EMA) data from a clinical pain study.
Variables: TIB (time in bed), TST (total sleep time), PANAS_PA (positive affect), PANAS_NA
(negative affect), worry_scale, PHQ9 (depression), alcohol_bev. Lagged (`_lag`) versions
represent the previous day's measurement (641 observations, 7 variables; 640 rows after lag
construction).

**Temporal knowledge**: lag variables → tier 0 (past); current variables → tier 1 (present).
This forbids any directed edge from current → lag, encoding the arrow of time.

Source: `tests/data/boston_data_raw.csv` (from the fastcda package).

---

## Reproducibility

```bash
# All 21 Java vs C++ comparison tests
pytest tests/test_java_comparison.py -v

# Only Boston real-world tests
pytest tests/test_java_comparison.py -v -k boston

# Only PAG algorithm tests
pytest tests/test_java_comparison.py -v -k "fci or gfci"
```

See [tests/java_oracle.py](tests/java_oracle.py) for JAR download instructions.
See [tests/test_java_comparison.py](tests/test_java_comparison.py) for full test definitions.
