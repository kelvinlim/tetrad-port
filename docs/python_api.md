# tetrad-port Python API Reference

## Installation

```bash
pip install -e ".[dev]"
```

## Quick Start

```python
import pandas as pd
from tetrad_port import TetradPort

tp = TetradPort()

# Load your data
df = pd.read_csv("data.csv")

# Run PC (constraint-based, no latent confounders)
results, graph_info = tp.run_pc(df, alpha=0.05)

# Run FGES (score-based, no latent confounders)
results, graph_info = tp.run_fges(df, penalty_discount=1.0)

# Run GFCI (hybrid, handles latent confounders)
results, graph_info = tp.run_gfci(df, alpha=0.05)
```

## Algorithm Comparison

| Feature | PC | FGES | GFCI |
|---------|----|----|------|
| Type | Constraint-based | Score-based | Hybrid |
| Output | CPDAG | CPDAG | PAG |
| Latent confounders | No | No | Yes |
| Speed (large graphs) | Slower | Faster | Medium |
| Key parameter | `alpha` | `penalty_discount` | `alpha` + `penalty_discount` |

### When to use which?

- **PC**: Good default when you believe all relevant variables are measured. Uses conditional independence tests.
- **FGES**: Preferred for large, sparse graphs where score-based search is more efficient. Also assumes causal sufficiency.
- **GFCI**: Use when unmeasured confounders may exist. Returns a PAG that can indicate bidirected edges (`<->`) representing latent common causes.

## API Reference

### `TetradPort(verbose=False)`

Main facade class for causal discovery.

**Parameters:**
- `verbose` (bool): Default verbosity for all algorithm calls.

---

### `TetradPort.run_pc(df, alpha=0.05, depth=-1, knowledge=None, verbose=None)`

Run the PC algorithm. Returns a CPDAG (Completed Partially Directed Acyclic Graph).

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data (rows = samples, columns = variables).
- `alpha` (float): Significance level for Fisher Z independence tests. Lower = sparser graph.
- `depth` (int): Max conditioning set size. -1 for unlimited.
- `verbose` (bool | None): Override instance verbosity.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_fges(df, penalty_discount=1.0, faithfulness_assumed=True, max_degree=-1, verbose=None)`

Run FGES (Fast Greedy Equivalence Search). Returns a CPDAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `penalty_discount` (float): BIC penalty multiplier. 1.0 = standard BIC. Higher = sparser.
- `faithfulness_assumed` (bool): Skip unfaithfulness phase (faster).
- `max_degree` (int): Maximum node degree. -1 for unlimited.

**Returns:** `(results, graph_info)` tuple. `results` includes `model_score`.

---

### `TetradPort.run_gfci(df, alpha=0.05, penalty_discount=1.0, depth=-1, max_degree=-1, complete_rule_set=True, max_disc_path_length=-1, faithfulness_assumed=True, verbose=None)`

Run GFCI (Greedy FCI). Returns a PAG (Partial Ancestral Graph).

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `alpha` (float): Significance level for independence tests.
- `penalty_discount` (float): BIC penalty multiplier for the FGES phase.
- `depth` (int): Max conditioning set size.
- `max_degree` (int): Maximum node degree.
- `complete_rule_set` (bool): Use Zhang's R1-R10 rules (True) or Spirtes' R1-R4 (False).
- `max_disc_path_length` (int): Max discriminating path length for R4.
- `faithfulness_assumed` (bool): For the FGES phase.

**Returns:** `(results, graph_info)` tuple.

---

## Return Value Structure

All `run_*` methods return `(results, graph_info)`.

### `results` dict

| Key | Type | Description |
|-----|------|-------------|
| `edges` | list[str] | Edge strings like `"X --> Y"`, `"A <-> B"` |
| `nodes` | list[str] | Variable names |
| `num_edges` | int | Number of edges |
| `num_nodes` | int | Number of variables |
| `alpha` | float | (PC, GFCI) Significance level used |
| `penalty_discount` | float | (FGES, GFCI) BIC penalty used |
| `model_score` | float | (FGES only) BIC model score |

### `graph_info` dict

| Key | Type | Description |
|-----|------|-------------|
| `adjacency` | dict[str, list[str]] | Node -> list of adjacent nodes |
| `directed_edges` | list[tuple[str,str]] | `(from, to)` for `-->` edges |
| `undirected_edges` | list[tuple[str,str]] | `(n1, n2)` for `---` edges |
| `bidirected_edges` | list[tuple[str,str]] | `(n1, n2)` for `<->` edges (PAG only) |
| `partially_oriented_edges` | list[tuple] | `(n1, type, n2)` for `o->` edges (PAG only) |
| `circle_edges` | list[tuple[str,str]] | `(n1, n2)` for `o-o` edges (PAG only) |

## Edge Types

| String | Meaning | Graph Type |
|--------|---------|-----------|
| `X --> Y` | X causes Y | CPDAG, PAG |
| `X --- Y` | Undirected (ambiguous direction) | CPDAG, PAG |
| `X <-> Y` | Bidirected (latent common cause) | PAG |
| `X o-> Y` | Partially oriented (circle at X) | PAG |
| `X o-o Y` | Fully ambiguous | PAG |

## Utility Methods

### `TetradPort.edges_to_lavaan(edges)`

Convert edge strings to a lavaan/semopy model string for SEM fitting.

```python
model_str = TetradPort.edges_to_lavaan(results["edges"])
# "Y ~ X\nZ ~ Y"
```

### `TetradPort.run_semopy(lavaan_model, df)`

Fit a SEM model using semopy. Requires `pip install semopy`.

```python
sem_results = TetradPort.run_semopy(model_str, df)
print(sem_results["estimates"])
```

### `TetradPort.standardize_df_cols(df, columns=None)`

Standardize columns to zero mean, unit variance.

### `TetradPort.add_lag_columns(df, columns=None, n_lags=1, lag_stub="_lag")`

Add lagged columns for time-series causal analysis.

### `TetradPort.create_lag_knowledge(columns, lag_stub="_lag")`

Create temporal ordering knowledge dict for lagged data.

## Examples

See the [examples/python/](../examples/python/) directory for:
- `causal_discovery_tutorial.ipynb` — Complete tutorial with all three algorithms
