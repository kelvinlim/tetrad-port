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

### Prerequisites — Windows 11

Building the C++ extension on Windows requires the MSVC compiler and CMake.

**Step 1 — Visual Studio Build Tools**

Download **Build Tools for Visual Studio 2022** (or 2026) from:
https://visualstudio.microsoft.com/downloads/ → "Tools for Visual Studio" section

Run the installer and select the **"Desktop development with C++"** workload. No other workloads are needed (~4 GB installed).

**Step 2 — CMake**

Download the Windows x64 installer from https://cmake.org/download/ and during install choose **"Add CMake to the system PATH for all users"**.

**Step 3 — Verify (open a new terminal after install)**

```powershell
cl
cmake --version
```

Both commands should print version information. If `cl` is not found, use the **"x64 Native Tools Command Prompt for VS 2022"** from the Start menu — it pre-configures the compiler environment. All `pip install` and `cmake` commands below work from that prompt.

---

### Python package (recommended)

**Option A — install from requirements.txt (pinned environment)**

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

`requirements.txt` includes a pinned `tetrad_port` git URL that is fetched and compiled automatically. This is the easiest path to a reproducible environment.

**Option B — editable install from local source (development)**

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -e ".[dev]"
```

**Verify the install:**

```powershell
python -c "import tetrad_port; print(tetrad_port.__version__)"
pytest tests/test_python_bindings.py -v
```

---

### C++ standalone

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\tetrad_tests.exe   # Run all C++ tests
.\build\Release\run_pc.exe         # Example CLI
```

On Linux/macOS:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/tetrad_tests
./build/run_pc
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

## Cross-Platform Result Comparison

To verify that algorithm outputs are consistent between Windows and Linux (or any two machines), use the scripts in `tests/`:

```bash
# On each machine — export canonical edge lists for all datasets
python tests/export_results.py --out tests/gfci_results_windows.json
python tests/export_results.py --out tests/gfci_results_linux.json

# Run multiple algorithms at once
python tests/export_results.py --algo gfci boss_fci grasp_fci --out tests/results_windows.json

# Compare two result files (copy both to the same machine first)
python tests/compare_platforms.py tests/gfci_results_windows.json tests/gfci_results_linux.json
python tests/compare_platforms.py tests/gfci_results_windows.json tests/gfci_results_linux.json --verbose
```

The comparison reports adjacency Jaccard and edge-type agreement per dataset. An exit code of 0 means all results are identical; 1 means differences were found.

## Reference

Ported from [Tetrad 7.6.3](https://github.com/cmu-phil/tetrad) (Java). Results validated against the Tetrad 7.6.3 JAR across all algorithms — see `JavaCPPComparison.md`.
