# tetrad-port

C++ port of [CMU's Tetrad](https://github.com/cmu-phil/tetrad) causal inference library, with Python bindings via nanobind.

## Algorithms

| Algorithm | Type | Output | Latent Confounders |
|-----------|------|--------|--------------------|
| **PC** | Constraint-based (Fisher Z) | CPDAG | No |
| **FGES** | Score-based (BIC) | CPDAG | No |
| **GFCI** | Hybrid (FGES + FCI rules) | PAG | Yes |
| **BOSS** | Permutation-based (BIC) | CPDAG | No |
| **BOSS-FCI** | BOSS + FCI rules | PAG | Yes |
| **GRaSP** | Permutation-based (tuck DFS) | CPDAG | No |
| **GRaSP-FCI** | GRaSP + FCI rules | PAG | Yes |

All algorithms support **background knowledge**: temporal tiers, forbidden edges, and required edges.

## Quick Start

```bash
pip install -e ".[dev]"
```

```python
import pandas as pd
from tetrad_port import TetradPort, Knowledge

tp = TetradPort()
df = pd.read_csv("data.csv")

# Run PC (constraint-based)
results, graph_info = tp.run_pc(df, alpha=0.05)

# Run FGES (score-based, faster for large graphs)
results, graph_info = tp.run_fges(df, penalty_discount=1.0)

# Run GFCI (handles latent confounders)
results, graph_info = tp.run_gfci(df, alpha=0.05)

# Run BOSS (permutation-based, often faster than FGES)
results, graph_info = tp.run_boss(df, penalty_discount=1.0)

# Run GRaSP (permutation-based with DFS tucks)
results, graph_info = tp.run_grasp(df, penalty_discount=1.0)

# Run BOSS-FCI or GRaSP-FCI (latent confounders)
results, graph_info = tp.run_boss_fci(df, alpha=0.05)
results, graph_info = tp.run_grasp_fci(df, alpha=0.05)

# Add background knowledge
k = Knowledge()
k.set_tier(0, ["Age", "Genetics"])    # Cannot be caused by later variables
k.set_tier(1, ["Exercise", "Diet"])
k.set_forbidden("Exercise", "Cholesterol")
k.set_required("Smoking", "BP")
results, graph_info = tp.run_pc(df, alpha=0.05, knowledge=k)
```

## Building from Source

### C++ standalone

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tetrad_tests        # Run all C++ tests
./build/run_pc              # Example CLI
```

### Python package

```bash
pip install -e ".[dev]"
pytest tests/test_python_bindings.py -v
```

## Documentation

- [Python API Reference](docs/python_api.md)
- [Examples](examples/python/)
  - [Causal Discovery Tutorial](examples/python/causal_discovery_tutorial.ipynb) — PC, FGES, and GFCI comparison
  - [Knowledge Tutorial](examples/python/knowledge_tutorial.ipynb) — Temporal tiers, forbidden/required edges

## Architecture

The C++ core lives in `src/` under `namespace tetrad`:

- **`src/graph/`** — Node, Edge, Graph (EdgeListGraph with TAIL/ARROW/CIRCLE endpoints)
- **`src/data/`** — DataSet (Eigen matrix wrapper), Knowledge (tiers, forbidden/required edges)
- **`src/search/`** — PC, FAS, MeekRules, FGES, FciOrient, GFCI, BOSS, BOSS-FCI, GRaSP, GRaSP-FCI, IndTestFisherZ, SemBicScore, GrowShrinkTree
- **`src/util/`** — ChoiceGenerator, SublistGenerator

Python bindings (`bindings/tetrad_bindings.cpp`) expose algorithms via nanobind. The `TetradPort` facade class (`python/tetrad_port/__init__.py`) provides a pandas-friendly API with SEM fitting helpers.

## Dependencies

**C++ (auto-fetched via CMake FetchContent):**
- Eigen 3.4.0 — linear algebra
- Catch2 v3.5.2 — testing
- nanobind 2.0+ — Python bindings

**Python:**
- numpy, pandas (required)
- semopy (optional, SEM fitting)

## Reference

Ported from [Tetrad 7.6.8](https://github.com/cmu-phil/tetrad) (Java). This version contains critical FciOrient correctness fixes reviewed by Peter Spirtes.
