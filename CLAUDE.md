# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ port of CMU's Tetrad causal inference library. Currently implements the PC algorithm with Fisher Z independence testing, with Python bindings via nanobind. The project is expanding to include score-based and latent-variable algorithms: GFCI, BOSS, BOSS-FCI, GRASP, GRASP-FCI.

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
```

## Architecture

### C++ Core (`src/`)

All code lives in `namespace tetrad`. Nodes use `std::shared_ptr<Node>` (`NodePtr`) throughout for reference semantics matching the Java original.

- **`src/graph/`** — Graph data structures (Node, Edge, Triple, EdgeListGraph). Edges have two endpoints (TAIL, ARROW, CIRCLE, NULL_EP) enabling representation of directed, undirected, and partially oriented edges. Edge equality is symmetric: `(A→B) == (B←A)`.

- **`src/data/`** — DataSet wraps `Eigen::MatrixXd` and computes correlation matrices. Knowledge is stubbed (always permissive).

- **`src/search/`** — Search algorithms and independence testing:
  - **PC pipeline** (implemented): FAS → collider orientation → Meek Rules (R1-R3)
  - **Score interface** (planned): abstract `Score` class for BIC-based scoring
  - **FCI orientation** (planned): `FciOrient` for latent-variable algorithms
  - `IndependenceTest` is the abstract interface; `IndTestFisherZ` implements Fisher Z via Cholesky decomposition on correlation submatrices.

- **`src/util/`** — ChoiceGenerator for lexicographic C(n,k) enumeration.

### Python Layer

- **`bindings/tetrad_bindings.cpp`** — nanobind module exposing `run_pc_raw()` and `PcResult` struct. Automatic numpy ↔ Eigen conversion.
- **`python/tetrad_port/__init__.py`** — `TetradPort` facade class with `run_pc()`, SEM fitting helpers (`edges_to_lavaan`, `run_semopy`), and data prep utilities (standardization, lag columns).
- Build uses scikit-build-core (configured in `pyproject.toml`).

### Tests (`tests/`)

Catch2 v3.5.2 framework. Test files mirror source structure: `test_node_edge.cpp`, `test_graph.cpp`, `test_choice_generator.cpp`, `test_fisher_z.cpp`, `test_fas.cpp`, `test_meek_rules.cpp`, `test_pc.cpp`. Python bindings tested with pytest in `test_python_bindings.py`.

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

## Known Differences from Java (Current PC Port)

Remaining deviations from Java 7.6.8 (benign when Knowledge is empty):

- **MeekRules R4**: Not implemented (only active when Knowledge is non-empty).
- **Missing `pcOrientbk()`**: Background knowledge orientation step not implemented.

Previously fixed (Phase 1):
- MeekRules R2/R3 early-return semantics now match Java
- Collider orientation uses PRIORITIZE_EXISTING ConflictRule
- FAS `possibleParents` includes `noEdgeRequired()` and `isRequired` checks
- Knowledge stub has `noEdgeRequired()` method

## Target Algorithm Roadmap

Priority order for next algorithms to port (all from `tetrad-7.6.8/`):

1. **GFCI** — `search/GFci.java` (278 lines). Requires: Fges, FciOrient, Score, SepsetsGreedy.
2. **BOSS** — `search/Boss.java` (505 lines). Requires: PermutationSearch, BesPermutation, GrowShrinkTree, Score.
3. **BOSS-FCI** — `search/BFci.java` (241 lines). Requires: Boss, FciOrient, SepsetsGreedy.
4. **GRASP** — `search/Grasp.java` (532 lines). Requires: TeyssierScorer, Score.
5. **GRASP-FCI** — `search/GraspFci.java` (300 lines). Requires: Grasp, FciOrient, SepsetsGreedy.

Shared infrastructure needed: Score interface, SemBicScore, FciOrient, FAS (already ported), MeekRules (already ported), Fges, TeyssierScorer, GrowShrinkTree, BesPermutation, SepsetsGreedy, DagToPag, PossibleMsepFci.

See `IMPLEMENTATION_PLAN.md` for the original PC vertical slice plan.
