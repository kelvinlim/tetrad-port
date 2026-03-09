# Java vs C++ Comparison — Tetrad Port

Comparison of **Tetrad 7.6.8 (Java)** against the **C++ port** across all implemented algorithms
and datasets.

*Auto-generated on 2026-03-09 18:01:29 by `tests/generate_comparison_report.py`.*

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
| `A --- B` | Undirected | Undirected (CPDAG output) |

**Direction matters for `-->` and `o->`.** `A --> B` and `B --> A` are distinct causal claims.

Settings: alpha = 0.01, penalty discount = 1.0.

---

## Summary

| Algorithm | Type | Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree | Status |
|-----------|------|---------|------|-----|------|-----|-------------|------------|--------|
| PC | CPDAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| PC | CPDAG | collider (X->Z<-Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| PC | CPDAG | medium DAG | 8 | 2000 | 10 | 10 | 1.000 | 100% | PASS |
| FGES | CPDAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| FGES | CPDAG | collider (X->Z<-Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| FGES | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | PASS |
| BOSS | CPDAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| BOSS | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | PASS |
| GRASP | CPDAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GRASP | CPDAG | medium DAG | 8 | 2000 | 12 | 12 | 1.000 | 100% | PASS |
| GFCI | PAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GFCI | PAG | collider (X->Z<-Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GFCI | PAG | latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GFCI | PAG | medium DAG + 2 latents | 8 | 2000 | 10 | 10 | 1.000 | 100% | PASS |
| GFCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 14 | 15 | 0.933 | 100% | WARN |
| BOSS-FCI | PAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| BOSS-FCI | PAG | latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| BOSS-FCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 15 | 16 | 0.938 | 73% | WARN |
| GRASP-FCI | PAG | chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GRASP-FCI | PAG | latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% | PASS |
| GRASP-FCI | PAG | Boston EMA (temporal knowledge) | 14 | 640 | 14 | 13 | 0.929 | 100% | WARN |

---

## CPDAG Algorithms (PC, FGES, BOSS, GRaSP)

All four CPDAG algorithms produce **identical output** to Java across every test case.

---

## PAG Algorithms (GFCI, BOSS-FCI, GRaSP-FCI)

PAG algorithms agree perfectly on simple synthetic datasets. Discrepancies appear only on the
medium 8-variable DAG with latents and on the real-world Boston EMA dataset.

### Temporal knowledge check

No forbidden-direction `-->` edges appear in any C++ output. The temporal constraint
(lag variables in tier 0, current variables in tier 1, forbidding current -> lag directed edges)
is respected throughout. `<->` (bidirected) edges for current-lag pairs are **not** violations:
`<->` represents a latent common cause and does not imply a directed causal edge in either direction.

---

### GFCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| collider (X->Z<-Y) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| medium DAG + 2 latents | 8 | 2000 | 10 | 10 | 1.000 | 100% |
| Boston EMA (temporal knowledge) | 14 | 640 | 14 | 15 | 0.933 | 100% |

**GFCI — Boston EMA (temporal knowledge) discrepancies:**

C++ finds extra adjacencies: `PANAS_NA — worry_scale`.

<details>
<summary>Full edge lists</summary>

Java edges:
```
PANAS_NA <-> PANAS_NA_lag
PANAS_NA_lag --> worry_scale_lag
PANAS_NA_lag <-> PHQ9_lag
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

C++ edges:
```
PANAS_NA --> worry_scale
PANAS_NA <-> PANAS_NA_lag
PANAS_NA_lag --> worry_scale_lag
PANAS_NA_lag <-> PHQ9_lag
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
</details>

---

### BOSS-FCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| Boston EMA (temporal knowledge) | 14 | 640 | 15 | 16 | 0.938 | 73% |

**BOSS-FCI — Boston EMA (temporal knowledge) discrepancies:**

| Java | C++ | Pattern |
|------|-----|---------|
| `PANAS_NA <-> PANAS_NA_lag` | `PANAS_NA_lag o-> PANAS_NA` | `<->` (Java) vs `o->` (C++) |
| `PANAS_NA --> PHQ9` | `PANAS_NA o-> PHQ9` | `-->` (Java) vs `o->` (C++) |
| `PHQ9_lag o-> PANAS_NA_lag` | `PANAS_NA_lag o-o PHQ9_lag` | `o->` (Java) vs `o-o` (C++) |
| `PANAS_NA_lag --> worry_scale_lag` | `PANAS_NA_lag o-o worry_scale_lag` | `-->` (Java) vs `o-o` (C++) |

C++ finds extra adjacencies: `PANAS_NA_lag — PHQ9`.

<details>
<summary>Full edge lists</summary>

Java edges:
```
PANAS_NA --> PHQ9
PANAS_NA --> worry_scale
PANAS_NA <-> PANAS_NA_lag
PANAS_NA_lag --> worry_scale_lag
PANAS_PA <-> PANAS_PA_lag
PHQ9 --> PANAS_PA
PHQ9_lag o-> PANAS_NA_lag
PHQ9_lag o-> PANAS_PA_lag
PHQ9_lag o-> PHQ9
TIB <-> TIB_lag
TST --> TIB
TST_lag o-> TIB_lag
TST_lag o-> TST
alcohol_bev_lag o-> alcohol_bev
worry_scale_lag --> worry_scale
```

C++ edges:
```
PANAS_NA --> worry_scale
PANAS_NA o-> PHQ9
PANAS_NA_lag o-> PANAS_NA
PANAS_NA_lag o-> PHQ9
PANAS_NA_lag o-o PHQ9_lag
PANAS_NA_lag o-o worry_scale_lag
PANAS_PA <-> PANAS_PA_lag
PHQ9 --> PANAS_PA
PHQ9_lag o-> PANAS_PA_lag
PHQ9_lag o-> PHQ9
TIB <-> TIB_lag
TST --> TIB
TST_lag o-> TIB_lag
TST_lag o-> TST
alcohol_bev_lag o-> alcohol_bev
worry_scale_lag --> worry_scale
```
</details>

---

### GRASP-FCI

| Dataset | Vars | Obs | Java | C++ | Adj. Jaccard | Type agree |
|---------|------|-----|------|-----|-------------|------------|
| chain (X->Y->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| latent (L->X, L->Y, X->Z) | 3 | 3000 | 2 | 2 | 1.000 | 100% |
| Boston EMA (temporal knowledge) | 14 | 640 | 14 | 13 | 0.929 | 100% |

**GRASP-FCI — Boston EMA (temporal knowledge) discrepancies:**

Java finds extra adjacencies: `PANAS_NA_lag — worry_scale_lag`.

<details>
<summary>Full edge lists</summary>

Java edges:
```
PANAS_NA --> PHQ9
PANAS_NA <-> PANAS_NA_lag
PANAS_PA <-> PHQ9
PANAS_PA_lag o-> PANAS_PA
PANAS_PA_lag o-o PHQ9_lag
PHQ9_lag o-> PANAS_NA_lag
PHQ9_lag o-> PHQ9
TIB <-> TST
TIB_lag o-> TIB
TIB_lag o-o TST_lag
TST_lag o-> TST
alcohol_bev_lag o-> alcohol_bev
worry_scale_lag o-> PANAS_NA_lag
worry_scale_lag o-> worry_scale
```

C++ edges:
```
PANAS_NA --> PHQ9
PANAS_NA <-> PANAS_NA_lag
PANAS_PA <-> PHQ9
PANAS_PA_lag o-> PANAS_PA
PHQ9_lag o-> PANAS_NA_lag
PHQ9_lag o-> PHQ9
PHQ9_lag o-o PANAS_PA_lag
TIB <-> TST
TIB_lag o-> TIB
TIB_lag o-o TST_lag
TST_lag o-> TST
alcohol_bev_lag o-> alcohol_bev
worry_scale_lag o-> worry_scale
```
</details>

---

## Known Discrepancy Patterns

All discrepancies fall into three categories:

### 1. `o->` (Java) vs `<->` (C++): Bidirected over-orientation

Java keeps an uncertain circle at one end; C++ resolves it to an arrowhead,
yielding a bidirected edge (`<->`, latent common cause). This happens when C++'s
CPDAG identifies a collider that Java does not, combined with a knowledge-derived
arrowhead at the other end.

Note: `<->` is not a knowledge violation -- it represents a latent confounder,
not a forbidden directed edge.

### 2. `o-o` (Java) vs `o->` (C++): Nondirected partially oriented

Java keeps both marks uncertain (circles); C++ fires R6 or R7 to orient one end.
The cascade traces back to different collider orientations in the initial CPDAG.

### 3. `o->` (Java) vs `-->` (C++): Partial to full orientation

Java keeps a circle at the tail end; C++ fires R8, R9, or R10 to convert it to a
tail, fully directing the edge. Java does not fire the same rule, preserving uncertainty.

---

## Knowledge Verification

Temporal knowledge (tier 0 = lag variables, tier 1 = current variables) is correctly enforced.
Verified by checking all `-->` edges in C++ output against the forbidden-edge list:

- Zero directed `-->` edges from current to lag appear in any C++ output
- All `o->` edges involving lag-current pairs have the arrowhead at the **current** node
  (correct temporal direction: past influences present)
- `<->` edges for current-lag pairs represent latent confounders and do not violate the
  forbidden-direction constraint

---

## Boston EMA Dataset

Real-world ecological momentary assessment (EMA) data from a clinical pain study.
Variables: TIB (time in bed), TST (total sleep time), PANAS_PA (positive affect), PANAS_NA
(negative affect), worry_scale, PHQ9 (depression), alcohol_bev. Lagged (`_lag`) versions
represent the previous day's measurement (641 observations, 7 variables; 640 rows after lag
construction).

**Temporal knowledge**: lag variables -> tier 0 (past); current variables -> tier 1 (present).
This forbids any directed edge from current -> lag, encoding the arrow of time.

Source: `tests/data/boston_data_raw.csv` (from the fastcda package).

---

## Reproducibility

```bash
# Regenerate this report
.venv/bin/python tests/generate_comparison_report.py

# All 21 Java vs C++ comparison tests
pytest tests/test_java_comparison.py -v

# Only Boston real-world tests
pytest tests/test_java_comparison.py -v -k boston

# Only PAG algorithm tests
pytest tests/test_java_comparison.py -v -k "fci or gfci"
```

See [tests/java_oracle.py](tests/java_oracle.py) for JAR download instructions.
See [tests/test_java_comparison.py](tests/test_java_comparison.py) for full test definitions.
