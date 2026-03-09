# tetrad-port Python API Reference

## Installation

```bash
pip install -e ".[dev]"
```

## Quick Start

```python
import pandas as pd
from tetrad_port import TetradPort, Knowledge

tp = TetradPort()

# Load your data
df = pd.read_csv("data.csv")

# Run PC (constraint-based, no latent confounders)
results, graph_info = tp.run_pc(df, alpha=0.05)

# Run FGES (score-based, no latent confounders)
results, graph_info = tp.run_fges(df, penalty_discount=1.0)

# Run GFCI (hybrid, handles latent confounders)
results, graph_info = tp.run_gfci(df, alpha=0.05)

# Use background knowledge to constrain search
k = Knowledge()
k.add_to_tier(0, "Age")       # Age cannot be caused by later variables
k.add_to_tier(1, "BMI")
k.set_forbidden("BMI", "Age") # Explicitly forbid BMI -> Age
k.set_required("Smoking", "BP") # Require Smoking -> BP
results, graph_info = tp.run_pc(df, alpha=0.05, knowledge=k)
```

## Algorithm Comparison

| Feature | PC | FGES | GFCI | BOSS | BOSS-FCI | GRaSP | GRaSP-FCI |
|---------|----|----|------|------|----------|-------|-----------|
| Type | Constraint | Score | Hybrid | Permutation | Perm + FCI | Permutation | Perm + FCI |
| Output | CPDAG | CPDAG | PAG | CPDAG | PAG | CPDAG | PAG |
| Latent confounders | No | No | Yes | No | Yes | No | Yes |
| Key parameter | `alpha` | `penalty_discount` | both | `penalty_discount` | both | `penalty_discount` | both |

### When to use which?

- **PC**: Good default when you believe all relevant variables are measured. Uses conditional independence tests.
- **FGES**: Preferred for large, sparse graphs where score-based search is more efficient. Also assumes causal sufficiency.
- **GFCI**: Use when unmeasured confounders may exist. Returns a PAG that can indicate bidirected edges (`<->`) representing latent common causes.
- **BOSS**: Permutation-based, often faster than FGES with higher adjacency/orientation precision. Assumes causal sufficiency.
- **BOSS-FCI**: BOSS combined with FCI orientation rules. Handles latent confounders with BOSS's precision advantages.
- **GRaSP**: Permutation-based with DFS tuck moves. Very high precision for linear Gaussian data.
- **GRaSP-FCI**: GRaSP combined with FCI rules. Highest precision PAG algorithm for linear Gaussian data.

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
- `knowledge` (Knowledge | None): Background knowledge constraints.
- `verbose` (bool | None): Override instance verbosity.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_fges(df, penalty_discount=1.0, faithfulness_assumed=True, max_degree=-1, knowledge=None, verbose=None)`

Run FGES (Fast Greedy Equivalence Search). Returns a CPDAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `penalty_discount` (float): BIC penalty multiplier. 1.0 = standard BIC. Higher = sparser.
- `faithfulness_assumed` (bool): Skip unfaithfulness phase (faster).
- `max_degree` (int): Maximum node degree. -1 for unlimited.
- `knowledge` (Knowledge | None): Background knowledge constraints.

**Returns:** `(results, graph_info)` tuple. `results` includes `model_score`.

---

### `TetradPort.run_gfci(df, alpha=0.05, penalty_discount=1.0, depth=-1, max_degree=-1, complete_rule_set=True, max_disc_path_length=-1, faithfulness_assumed=True, knowledge=None, verbose=None)`

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
- `knowledge` (Knowledge | None): Background knowledge constraints.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_boss(df, penalty_discount=1.0, use_bes=False, num_starts=1, use_data_order=True, knowledge=None, verbose=None)`

Run BOSS (Best Order Score Search). Permutation-based, returns a CPDAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `penalty_discount` (float): BIC penalty multiplier. 1.0 = standard BIC.
- `use_bes` (bool): Run Backward Equivalence Search refinement.
- `num_starts` (int): Number of random restarts. Best result returned.
- `use_data_order` (bool): Use data column order for the first run.
- `knowledge` (Knowledge | None): Background knowledge constraints.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_boss_fci(df, alpha=0.05, penalty_discount=1.0, depth=-1, complete_rule_set=True, max_disc_path_length=-1, use_bes=False, num_starts=1, knowledge=None, verbose=None)`

Run BOSS-FCI. BOSS + FCI orientation rules, returns a PAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `alpha` (float): Significance level for independence tests.
- `penalty_discount` (float): BIC penalty for BOSS scoring phase.
- `depth` (int): Max conditioning set size.
- `complete_rule_set` (bool): Use Zhang's R1-R10 rules (True) or Spirtes' R1-R4 (False).
- `max_disc_path_length` (int): Max discriminating path length for R4.
- `use_bes` (bool): Run BES refinement in BOSS.
- `num_starts` (int): Number of random restarts.
- `knowledge` (Knowledge | None): Background knowledge constraints.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_grasp(df, penalty_discount=1.0, depth=3, uncovered_depth=1, non_singular_depth=1, ordered=False, num_starts=1, use_data_order=True, knowledge=None, verbose=None)`

Run GRaSP (Greedy Relaxations of SP). Permutation-based with DFS tucks, returns a CPDAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `penalty_discount` (float): BIC penalty multiplier.
- `depth` (int): Max DFS depth for singular tucks (default 3).
- `uncovered_depth` (int): Max depth for uncovered tucks (default 1).
- `non_singular_depth` (int): Max depth for non-singular tucks (default 1).
- `ordered` (bool): Enforce GRaSP0/1/2 ordering.
- `num_starts` (int): Number of random restarts.
- `use_data_order` (bool): Use data column order for the first run.
- `knowledge` (Knowledge | None): Background knowledge constraints.

**Returns:** `(results, graph_info)` tuple.

---

### `TetradPort.run_grasp_fci(df, alpha=0.05, penalty_discount=1.0, depth=-1, grasp_depth=3, uncovered_depth=1, non_singular_depth=1, ordered=False, complete_rule_set=True, max_disc_path_length=-1, num_starts=1, use_data_order=True, knowledge=None, verbose=None)`

Run GRaSP-FCI. GRaSP + FCI orientation rules, returns a PAG.

**Parameters:**
- `df` (pd.DataFrame): Continuous numeric data.
- `alpha` (float): Significance level for independence tests.
- `penalty_discount` (float): BIC penalty for GRaSP scoring phase.
- `depth` (int): Max conditioning set size for FCI.
- `grasp_depth` (int): Max DFS depth for GRaSP tucks (default 3).
- `uncovered_depth` (int): Max depth for uncovered tucks.
- `non_singular_depth` (int): Max depth for non-singular tucks.
- `ordered` (bool): Enforce GRaSP ordering.
- `complete_rule_set` (bool): Use Zhang's R1-R10 rules.
- `max_disc_path_length` (int): Max discriminating path length for R4.
- `num_starts` (int): Number of random restarts.
- `use_data_order` (bool): Use data column order for the first run.
- `knowledge` (Knowledge | None): Background knowledge constraints.

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
| `alpha` | float | (PC, GFCI, BOSS-FCI, GRaSP-FCI) Significance level used |
| `penalty_discount` | float | (FGES, GFCI, BOSS, BOSS-FCI, GRaSP, GRaSP-FCI) BIC penalty used |
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

## Background Knowledge

The `Knowledge` class lets you encode domain expertise to constrain causal search. All algorithms accept a `knowledge` parameter.

### `Knowledge()`

Create an empty knowledge object, then add constraints:

```python
from tetrad_port import Knowledge

k = Knowledge()
```

### Temporal Tiers

Variables in lower tiers cannot be caused by variables in higher tiers. This encodes temporal ordering.

```python
k.add_to_tier(0, "Age")        # Tier 0 (earliest)
k.add_to_tier(0, "Genetics")
k.add_to_tier(1, "Smoking")    # Tier 1
k.add_to_tier(1, "Exercise")
k.add_to_tier(2, "BMI")        # Tier 2 (latest)

# Set an entire tier at once
k.set_tier(0, ["Age", "Genetics"])

# Forbid edges between variables within the same tier
k.set_tier_forbidden_within(0, True)
```

### Forbidden Edges

Explicitly forbid a directed edge, even within the same tier:

```python
k.set_forbidden("Exercise", "Cholesterol")  # Forbid Exercise -> Cholesterol
k.remove_forbidden("Exercise", "Cholesterol")  # Remove the constraint
k.is_forbidden("Exercise", "Cholesterol")  # Check if forbidden
```

### Required Edges

Force a directed edge to appear in the output:

```python
k.set_required("Smoking", "BloodPressure")  # Require Smoking -> BP
k.remove_required("Smoking", "BloodPressure")
k.is_required("Smoking", "BloodPressure")
```

### Other Methods

| Method | Description |
|--------|-------------|
| `k.get_tier(i)` | Get list of variables in tier `i` |
| `k.get_num_tiers()` | Number of tiers defined |
| `k.is_empty()` | True if no constraints set |
| `k.clear()` | Remove all constraints |

---

## Utility Methods

> **Note:** These utility methods are also available (and recommended) via the
> [fastcausal](https://github.com/kelvinlim/fastcausal) package:
> `fastcausal.sem`, `fastcausal.transform`, and `fastcausal.knowledge`.
> In a future tetrad-port release, these may be removed from `TetradPort`
> to keep tetrad-port focused on algorithms and data structures.

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
- `causal_discovery_tutorial.ipynb` — Tutorial comparing PC, FGES, and GFCI
- `knowledge_tutorial.ipynb` — Using background knowledge (tiers, forbidden/required edges)
