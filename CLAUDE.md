# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ port of CMU's Tetrad causal inference library. Implements constraint-based (PC), score-based (FGES, BOSS, GRaSP), and latent-variable (GFCI, BOSS-FCI, GRaSP-FCI) algorithms with Python bindings via nanobind.

## Reference Version

**Tetrad 7.6.8** (`tetrad-7.6.8/`) is the reference Java source for all porting work. This version was chosen because it contains critical FciOrient correctness fixes (R3/R4/R5/R8/R9/R10 reviewed by Spirtes) required by all FCI-variant algorithms. See `TetradVersionRecommendation.md` for full rationale.

The existing PC/FAS/MeekRules port was originally based on 7.6.3. Differences are documented in `TetradVersionRecommendation.md` and are minor (PC itself did not change between versions).

## Build & Test Commands

```bash
# Build (C++ standalone)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run all C++ tests
./build/tetrad_tests

# Run specific test by Catch2 tag
./build/tetrad_tests [pc]         # PC algorithm tests
./build/tetrad_tests [fisher_z]   # Fisher Z tests
./build/tetrad_tests [fas]        # Fast Adjacency Search tests

# Run example CLI
./build/run_pc

# Install Python package (editable)
pip install -e ".[dev]"

# Run Python tests
pytest tests/test_python_bindings.py -v

# Run Java vs C++ comparison tests (requires Tetrad JAR and jpype)
# Download JAR first: see tests/java_oracle.py for the Maven URL
pytest tests/test_java_comparison.py -v
pytest tests/test_java_comparison.py -v -k gfci   # single algorithm
```

## Architecture

### C++ Core (`src/`)

All code lives in `namespace tetrad`. Nodes use `std::shared_ptr<Node>` (`NodePtr`) throughout for reference semantics matching the Java original.

- **`src/graph/`** — Graph data structures (Node, Edge, Triple, EdgeListGraph). Edges have two endpoints (TAIL, ARROW, CIRCLE, NULL_EP) enabling representation of directed, undirected, and partially oriented edges. Edge equality is symmetric: `(A→B) == (B←A)`.

- **`src/data/`** — DataSet wraps `Eigen::MatrixXd` and computes correlation matrices. Knowledge supports forbidden/required edge constraints and temporal tiers.

- **`src/search/`** — Search algorithms and independence testing:
  - **PC pipeline**: FAS → collider orientation → Meek Rules (R1-R4)
  - **Score-based**: abstract `Score` interface, `SemBicScore` (BIC), `Fges` (forward-backward GES)
  - **Permutation-based**: `GrowShrinkTree`, `Boss`, `BesPermutation`, `PermutationSearch`, `TeyssierScorer`, `Grasp`
  - **Latent-variable**: `FciOrient` (R0-R10), `Gfci`, `StarFci` (abstract *-FCI template), `BossFci`, `GraspFci`
  - `IndependenceTest` is the abstract interface; `IndTestFisherZ` implements Fisher Z via Cholesky decomposition on correlation submatrices.

- **`src/util/`** — ChoiceGenerator for lexicographic C(n,k) enumeration, SublistGenerator for subset enumeration.

### Python Layer

- **`bindings/tetrad_bindings.cpp`** — nanobind module exposing `run_pc_raw()`, `run_fges_raw()`, `run_gfci_raw()`, `run_boss_raw()`, `run_boss_fci_raw()`, `run_grasp_raw()`, `run_grasp_fci_raw()`, `run_fci_raw()` and `SearchResult` struct. Automatic numpy ↔ Eigen conversion.
- **`python/tetrad_port/__init__.py`** — `TetradPort` facade class with `run_pc()`, `run_fges()`, `run_gfci()`, `run_boss()`, `run_boss_fci()`, `run_grasp()`, `run_grasp_fci()`, `run_fci()`, SEM fitting helpers, and data prep utilities.
- Build uses scikit-build-core (configured in `pyproject.toml`).

### Tests (`tests/`)

Catch2 v3.5.2 framework. Test files mirror source structure: `test_node_edge.cpp`, `test_graph.cpp`, `test_choice_generator.cpp`, `test_fisher_z.cpp`, `test_fas.cpp`, `test_meek_rules.cpp`, `test_pc.cpp`, `test_knowledge.cpp`, `test_sem_bic_score.cpp`, `test_fges.cpp`, `test_gfci.cpp`, `test_boss.cpp`, `test_boss_fci.cpp`, `test_grasp.cpp`, `test_grasp_fci.cpp`, `test_generic_fci.cpp`. Python bindings tested with pytest in `test_python_bindings.py`.

## Coding Conventions

- **Classes**: PascalCase. **Methods**: camelCase. **Members**: snake_case with trailing underscore (`alpha_`).
- **Enums**: PascalCase values (`Endpoint::TAIL`, `NodeType::MEASURED`).
- Headers use `#pragma once`. All code wrapped in `namespace tetrad {}`.
- `shared_ptr` for Node ownership across APIs; `unique_ptr` for owned sub-objects; no raw pointers in public APIs.
- Custom `std::hash` specializations for Node, Edge, Triple to enable use in unordered containers.

## Dependencies

All C++ dependencies auto-fetched via CMake FetchContent:
- **Eigen 3.4.0** — linear algebra (header-only)
- **Catch2 v3.5.2** — testing (standalone builds only)
- **nanobind 2.0+** — Python bindings (when built via scikit-build-core)

Python: numpy, pandas (required); semopy (optional, SEM fitting); dgraph-flex (optional, visualization).

## Reference Material

This is a port from Java Tetrad 7.6.8. Key source mappings for implemented code:

| C++ | Java (in `tetrad-7.6.8/tetrad-lib/src/main/java/edu/cmu/tetrad/`) |
|-----|-------------------------------------------------------------------|
| `node.h/cpp` | `graph/GraphNode.java`, `graph/Node.java` |
| `edge.h/cpp` | `graph/Edge.java` |
| `graph.h/cpp` | `graph/EdgeListGraph.java` |
| `ind_test_fisher_z.h/cpp` | `search/test/IndTestFisherZ.java` |
| `pc.h/cpp` | `search/Pc.java`, `search/utils/PcCommon.java` |
| `fas.h/cpp` | `search/Fas.java` |
| `meek_rules.h/cpp` | `search/utils/MeekRules.java` |
| `sem_bic_score.h/cpp` | `search/score/SemBicScore.java` |
| `fges.h/cpp` | `search/Fges.java` |
| `fci_orient.h/cpp` | `search/utils/FciOrient.java`, `search/utils/R0R4StrategyTestBased.java` |
| `gfci.h/cpp` | `search/Gfci.java` |
| `grow_shrink_tree.h/cpp` | `search/utils/GrowShrinkTree.java` |
| `boss.h/cpp` | `search/Boss.java` |
| `bes_permutation.h/cpp` | `search/utils/BesPermutation.java` |
| `permutation_search.h/cpp` | `search/PermutationSearch.java` |
| `star_fci.h/cpp` | `search/StarFci.java` |
| `boss_fci.h/cpp` | `search/BossFci.java` |
| `teyssier_scorer.h/cpp` | `search/utils/TeyssierScorer.java` |
| `grasp.h/cpp` | `search/Grasp.java` |
| `grasp_fci.h/cpp` | `search/GraspFci.java` |
| `generic_fci.h/cpp` | (composable FCI — no direct Java equivalent) |

## Java vs C++ Comparison Testing

`tests/java_oracle.py` — thin jpype wrapper around Tetrad 7.6.8 JAR. Requires the JAR at `jars/tetrad-gui-7.6.8-launch.jar` (gitignored) and jpype (`pip install jpype1`). Also requires Java 21+.

`tests/test_java_comparison.py` — 18 pytest tests comparing Java and C++ outputs across all 7 algorithms using adjacency Jaccard and edge-type agreement metrics. Auto-skips if JAR or jpype is unavailable.

## Known Differences from Java (Resolved)

All previously identified deviations from Java 7.6.8 have been resolved:

- **PC**: MeekRules R2/R3 early-return, PRIORITIZE_EXISTING collider orientation, FAS `possibleParents` knowledge checks, full Knowledge class, `pcOrientbk()`, MeekRules R4, `colliderAllowed()` knowledge check
- **FGES/BOSS/GRaSP**: `MeekRules::orientImplied` DAG→CPDAG conversion (`revertToUnshieldedColliders` flag was declared but never executed — fixed by implementing the full revert-colliders-then-undirect-then-re-orient logic)
- **GFCI**: Missing possible d-sep removal step (`Paths.possibleDsep` BFS + sepset testing from candidate list) that Java performs between the first collider orientation pass and the FCI rules — added as Step 6 with a second collider orientation pass (Step 7) after

## Implemented Algorithms

| Algorithm | Type | Output | Status |
|-----------|------|--------|--------|
| **PC** | Constraint-based | CPDAG | Complete |
| **FGES** | Score-based (BIC) | CPDAG | Complete |
| **GFCI** | Hybrid (score + constraint) | PAG | Complete |
| **BOSS** | Permutation-based (BIC) | CPDAG | Complete |
| **BOSS-FCI** | BOSS + FCI rules | PAG | Complete |
| **GRaSP** | Permutation-based (tuck DFS) | CPDAG | Complete |
| **GRaSP-FCI** | GRaSP + FCI rules | PAG | Complete |
| **FCI** | Composable: any CPDAG + FCI rules | PAG | Complete |

All target algorithms from the roadmap have been implemented, plus a composable FCI pipeline. See `IMPLEMENTATION_PLAN.md` for the original PC vertical slice plan.
