# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++ port of CMU's Tetrad causal inference library, currently implementing the PC algorithm with Fisher Z independence testing. Provides Python bindings via nanobind.

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

- **`src/search/`** — The PC algorithm pipeline:
  1. **FAS** (Fast Adjacency Search): skeleton discovery via iterative conditional independence testing at increasing depths. PC-Stable variant defers edge removals per depth.
  2. **Collider orientation**: unshielded triples X—Z—Y where Z ∉ sepset(X,Y) become X→Z←Y.
  3. **Meek Rules** (R1-R3): iterative orientation propagation with cycle prevention.
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

This is a port from Java Tetrad. Key source mappings: `Node.java` → `node.h/cpp`, `Edge.java` → `edge.h/cpp`, `EdgeListGraph.java` → `graph.h/cpp`, `IndTestFisherZ.java` → `ind_test_fisher_z.h/cpp`, `Pc.java` → `pc.h/cpp`, `Fas.java` → `fas.h/cpp`, `MeekRules.java` → `meek_rules.h/cpp`. See `IMPLEMENTATION_PLAN.md` for detailed specifications.
