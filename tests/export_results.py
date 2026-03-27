"""
Export GFCI (and optionally other algorithm) results to a canonical JSON file
for cross-platform comparison.

Usage
-----
  python tests/export_results.py                        # gfci only, saves gfci_results_<platform>.json
  python tests/export_results.py --out my_results.json  # explicit output path
  python tests/export_results.py --algo gfci boss_fci   # multiple algorithms

The JSON is self-contained: platform metadata + one entry per (algorithm, dataset)
with a canonical sorted edge list so a plain diff or compare_platforms.py can be used.

Datasets
--------
  chain          -- X -> Y -> Z synthetic (n=3000, seed=42)
  collider       -- X -> Z <- Y synthetic (n=3000, seed=42)
  latent         -- L->X, L->Y, X->Z with L hidden (n=3000, seed=42)
  medium_latents -- random DAG 8 obs + 2 hidden (n=2000, seed=10)
  boston         -- Boston EMA real data with temporal knowledge
  r34b01         -- R34B01 real data with temporal knowledge (lag cols end with _)
"""

from __future__ import annotations

import argparse
import json
import platform
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import pandas as pd

# Make sure the package is importable when run from the repo root.
sys.path.insert(0, str(Path(__file__).parent.parent / "python"))

from tetrad_port import TetradPort, Knowledge, __version__ as TP_VERSION

# ---------------------------------------------------------------------------
# Canonical edge helpers
# ---------------------------------------------------------------------------

_SEP_RE = re.compile(r"(<->|-->|o->|o-o|<-o|<--|---)")
_SYMMETRIC_SEPS = {"<->", "o-o", "---"}


def _normalize_edge(e: str) -> str:
    """
    Return a canonical string form of an edge:
      - '<--' becomes '-->' (nodes swapped)
      - '<-o' becomes 'o->' (nodes swapped)
      - symmetric edges (---,  o-o, <->) use lexicographically smaller node first

    This lets a plain sorted() deduplicate or sort an edge list for diffing.
    """
    m = _SEP_RE.search(e)
    if m is None:
        return e
    sep = m.group()
    a = e[:m.start()].strip()
    b = e[m.end():].strip()

    if sep == "<--":
        sep, a, b = "-->", b, a
    elif sep == "<-o":
        sep, a, b = "o->", b, a

    if sep in _SYMMETRIC_SEPS and a > b:
        a, b = b, a

    return f"{a} {sep} {b}"


def canonical_edges(edges: list[str]) -> list[str]:
    """Normalize and sort an edge list for deterministic comparison."""
    return sorted(_normalize_edge(e) for e in edges)


# ---------------------------------------------------------------------------
# Dataset factories (same seeds/params as test_java_comparison.py)
# ---------------------------------------------------------------------------

def _make_chain(seed=42, n=3000):
    rng = np.random.default_rng(seed)
    X = rng.standard_normal(n)
    Y = 0.7 * X + 0.5 * rng.standard_normal(n)
    Z = 0.7 * Y + 0.5 * rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_collider(seed=42, n=3000):
    rng = np.random.default_rng(seed)
    X = rng.standard_normal(n)
    Y = rng.standard_normal(n)
    Z = 0.8 * X + 0.8 * Y + 0.3 * rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_latent(seed=42, n=3000):
    rng = np.random.default_rng(seed)
    L = rng.standard_normal(n)
    X = 0.8 * L + rng.standard_normal(n)
    Y = 0.7 * L + rng.standard_normal(n)
    Z = 0.6 * X + rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_dag(n_obs: int, n_hidden: int = 0, n_samples: int = 2000,
              edge_prob: float = 0.35, seed: int = 1) -> pd.DataFrame:
    rng = np.random.default_rng(seed)
    n_total = n_obs + n_hidden
    B = np.zeros((n_total, n_total))
    for i in range(n_total):
        for j in range(i + 1, n_total):
            if rng.random() < edge_prob:
                B[i, j] = rng.uniform(0.5, 1.5) * rng.choice([-1, 1])
    data = np.zeros((n_samples, n_total))
    for j in range(n_total):
        data[:, j] = data @ B[:, j] + rng.standard_normal(n_samples)
    cols = [f"X{i+1}" for i in range(n_obs)]
    return pd.DataFrame(data[:, :n_obs], columns=cols)


def _make_boston_lagged():
    boston_csv = Path(__file__).parent / "data" / "boston_data_raw.csv"
    if not boston_csv.exists():
        return None, None
    raw = pd.read_csv(boston_csv).dropna()
    curr_cols = list(raw.columns)
    lag_cols = [c + "_lag" for c in curr_cols]
    curr = raw.iloc[1:].reset_index(drop=True)
    lag = raw.iloc[:-1].reset_index(drop=True)
    lag.columns = lag_cols
    df = pd.concat([curr, lag], axis=1)
    kn = Knowledge()
    for v in lag_cols:
        kn.add_to_tier(0, v)
    for v in curr_cols:
        kn.add_to_tier(1, v)
    return df, kn


def _make_r34b01():
    csv = Path(__file__).parent / "data" / "R34B01.csv"
    if not csv.exists():
        return None, None
    df = pd.read_csv(csv).dropna()
    lag_cols = [c for c in df.columns if c.endswith("_")]
    curr_cols = [c for c in df.columns if not c.endswith("_")]
    kn = Knowledge()
    for v in lag_cols:
        kn.add_to_tier(0, v)
    for v in curr_cols:
        kn.add_to_tier(1, v)
    return df, kn


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------

DATASETS = {
    "chain":          (lambda: (_make_chain(), None)),
    "collider":       (lambda: (_make_collider(), None)),
    "latent":         (lambda: (_make_latent(), None)),
    "medium_latents": (lambda: (_make_dag(8, n_hidden=2, seed=10), None)),
    "boston":         _make_boston_lagged,
    "r34b01":         _make_r34b01,
}

ALGO_PARAMS = {
    "gfci":      {"alpha": 0.01, "penalty_discount": 1.0},
    "boss_fci":  {"alpha": 0.01, "penalty_discount": 1.0},
    "grasp_fci": {"alpha": 0.01, "penalty_discount": 1.0},
    "pc":        {"alpha": 0.01},
    "fges":      {"penalty_discount": 1.0},
    "boss":      {"penalty_discount": 1.0},
    "grasp":     {"penalty_discount": 1.0},
}


def run_one(tp: TetradPort, algo: str, df: pd.DataFrame, knowledge=None) -> dict:
    params = ALGO_PARAMS[algo]
    run_fn = getattr(tp, f"run_{algo}")
    if knowledge is not None:
        results, _ = run_fn(df, knowledge=knowledge, **params)
    else:
        results, _ = run_fn(df, **params)
    return {
        "edges_canonical": canonical_edges(results["edges"]),
        "num_edges": results["num_edges"],
        "nodes": sorted(results["nodes"]),
        "params": params,
    }


def main():
    parser = argparse.ArgumentParser(description="Export algorithm results for cross-platform comparison.")
    parser.add_argument("--out", default=None,
                        help="Output JSON path (default: gfci_results_<platform>.json in tests/)")
    parser.add_argument("--algo", nargs="+", default=["gfci"],
                        choices=list(ALGO_PARAMS), metavar="ALGO",
                        help="Algorithms to run (default: gfci). Choices: " + ", ".join(ALGO_PARAMS))
    parser.add_argument("--dataset", nargs="+", default=list(DATASETS),
                        choices=list(DATASETS), metavar="DATASET",
                        help="Datasets to run (default: all)")
    args = parser.parse_args()

    plat = platform.system().lower()
    if args.out is None:
        out_path = Path(__file__).parent / f"gfci_results_{plat}.json"
    else:
        out_path = Path(args.out)

    tp = TetradPort()

    output = {
        "metadata": {
            "platform": platform.platform(),
            "python_version": sys.version,
            "tetrad_port_version": TP_VERSION,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "algorithms": args.algo,
            "datasets": args.dataset,
        },
        "results": {},
    }

    for ds_name in args.dataset:
        factory = DATASETS[ds_name]
        result = factory()
        # factories return (df, knowledge) or ((df, knowledge), None)
        if isinstance(result[0], tuple):
            df, knowledge = result[0]
        else:
            df, knowledge = result

        if df is None:
            print(f"  SKIP {ds_name}: data file not found")
            continue

        for algo in args.algo:
            key = f"{algo}/{ds_name}"
            print(f"  Running {key} ... ", end="", flush=True)
            try:
                entry = run_one(tp, algo, df, knowledge)
                output["results"][key] = entry
                print(f"{entry['num_edges']} edges")
            except Exception as exc:
                output["results"][key] = {"error": str(exc)}
                print(f"ERROR: {exc}")

    with open(out_path, "w") as f:
        json.dump(output, f, indent=2)
    print(f"\nSaved to {out_path}")


if __name__ == "__main__":
    main()
