# Development Log: Tetrad C++ Port

## Overview

This is a C++ port of CMU's [Tetrad](https://github.com/cmu-phil/tetrad) causal inference library. The goal is a clean C++ implementation that can be easily wrapped for Python and R, avoiding the complexity of interfacing with the Java original.

The initial milestone is a **vertical slice**: the PC algorithm with the Fisher Z independence test, working end-to-end from data to CPDAG output.

## What Was Built

### Core Graph Layer (`src/graph/`)

| File | Description |
|------|-------------|
| `endpoint.h` | `Endpoint` enum: TAIL, ARROW, CIRCLE, NULL_EP |
| `node_type.h` | `NodeType` enum: MEASURED, LATENT, ERROR |
| `node.h/cpp` | `Node` class — identified by name, with `shared_ptr` ownership (`NodePtr`) and custom hash |
| `edge.h/cpp` | `Edge` class — two nodes + two endpoints, symmetric equality, factory functions for directed/undirected/bidirected edges |
| `triple.h` | `Triple` class (header-only) — three nodes with symmetric equality `(x,y,z) == (z,y,x)` |
| `graph.h/cpp` | `Graph` class (EdgeListGraph) — adjacency storage, node/edge CRUD, directed queries, collider detection, BFS path queries (`existsDirectedPath`, `existsSemiDirectedPath`) |

### Data Layer (`src/data/`)

| File | Description |
|------|-------------|
| `data_set.h/cpp` | Wrapper around `Eigen::MatrixXd` with variable names, correlation matrix computation, CSV loading |
| `knowledge.h/cpp` | Background knowledge stub — all methods return permissive defaults. Full implementation deferred. |

### Search Layer (`src/search/`)

| File | Description |
|------|-------------|
| `independence_test.h` | Abstract base class for independence tests |
| `independence_result.h` | Result struct: `independent`, `pValue`, `score` |
| `ind_test_fisher_z.h/cpp` | Fisher Z conditional independence test — partial correlations via precision matrix, Fisher Z transform, normal CDF for p-values |
| `sepset_map.h/cpp` | Separation set storage with order-independent key canonicalization |
| `fas.h/cpp` | Fast Adjacency Search — PC-Stable skeleton discovery with deferred edge removal per depth level |
| `meek_rules.h/cpp` | Meek orientation rules R1-R3 applied to fixed point, with directed cycle prevention |
| `pc.h/cpp` | PC algorithm — three phases: FAS skeleton, collider orientation (SEPSETS strategy), Meek rules propagation |

### Utilities (`src/util/`)

| File | Description |
|------|-------------|
| `choice_generator.h/cpp` | Iterative lexicographic combination generator — port of Java's `ChoiceGenerator` |

### Tests (`tests/`)

7 test files with **38 test cases** and **183 assertions** covering all components:

| File | What it tests |
|------|---------------|
| `test_node_edge.cpp` | Node creation/equality, Edge types/directionality/symmetry |
| `test_graph.cpp` | Graph CRUD, adjacency, directed queries, collider detection, path algorithms |
| `test_choice_generator.cpp` | Combination counts, edge cases (n choose 0, n choose n, impossible) |
| `test_fisher_z.cpp` | Correlated data detection, conditional independence, p-value validity |
| `test_fas.cpp` | Chain skeleton recovery, independent variables, separation set storage |
| `test_meek_rules.cpp` | Rules R1/R2/R3, cycle prevention |
| `test_pc.cpp` | End-to-end: chain CPDAG, collider/v-structure, diamond graph, independent variables |

### Example (`examples/`)

| File | Description |
|------|-------------|
| `run_pc.cpp` | CLI demo — runs PC on synthetic data or a user-provided CSV |

## Architecture

```
                    ┌──────────┐
                    │    PC    │  orchestrates the 3 phases
                    └────┬─────┘
                         │
            ┌────────────┼────────────┐
            v            v            v
        ┌──────┐   ┌──────────┐  ┌──────────┐
        │ FAS  │   │ Collider │  │  Meek    │
        │      │   │ Orient.  │  │  Rules   │
        └──┬───┘   └────┬─────┘  └────┬─────┘
           │             │             │
           v             v             v
      ┌─────────┐  ┌──────────┐  ┌─────────┐
      │ IndTest │  │ SepsetMap│  │  Graph   │
      │ FisherZ │  └──────────┘  └─────────┘
      └────┬────┘
           v
      ┌─────────┐
      │ DataSet │
      │ (Eigen) │
      └─────────┘
```

### Key Design Decisions

- **C++17** with CMake and FetchContent (Eigen 3.4.0, Catch2 v3.5.2)
- **`std::shared_ptr<Node>`** ownership throughout — simple, correct, mirrors Java's reference semantics
- **No Boost** — minimal dependency tree
- **Symmetric Edge equality** — `(A→B) == (B←A)` matching the Java implementation
- **PC-Stable variant** by default — edge removals deferred per depth level for order-independence
- **Deterministic ordering** — nodes and triples sorted by name for reproducible output

### Simplifications vs. Java

The Java Tetrad library has ~2100 files with GUI, serialization, and extensive configurability. This port strips it to essentials:

- No GUI concerns (colors, coordinates, PropertyChangeListener)
- No Java serialization
- No CONSERVATIVE or MAX_P collider strategies (SEPSETS only)
- No shrinkage modes for Fisher Z (direct Cholesky only)
- No background knowledge enforcement (stub only)
- No Meek R4 (requires non-empty knowledge)
- Graph has no ancestor cache or ancillary graph support

## Build & Run

```bash
# Prerequisites: CMake 3.18+, C++17 compiler

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
./build/tetrad_tests

# Run demo
./build/run_pc                    # synthetic data demo
./build/run_pc path/to/data.csv   # custom CSV
```

## Test Results

```
All tests passed (183 assertions in 38 test cases)
```

## Next Steps

- Python bindings via nanobind
- R bindings via Rcpp/cpp11
- Golden Master conformance testing against Java Tetrad
- Full Knowledge implementation
- CONSERVATIVE and MAX_P collider strategies
- Additional algorithms (FCI, GES)
- Shrinkage modes for Fisher Z
- Performance optimization
