"""
Single source of truth for Java-vs-C++ comparison metrics, and a CLI for
recording and diffing scoreboards.

Why this exists
---------------
The metric helpers previously existed in three near-identical copies
(``test_java_comparison.py``, ``compare_platforms.py``,
``generate_comparison_report.py``) which had already drifted apart --
``agreement_rate`` returned 1.0 for the both-empty case in one and 0.0 in
another. More importantly, the pytest suite only ever asserted pass/fail
against thresholds far below observed performance, so a change that moved a
cell from 0.66 to 0.99 was indistinguishable from one that changed nothing.

This module records the actual numbers so improvement (and regression) is
visible.

Usage
-----
    python tests/scoreboard.py --out scoreboard/baseline.json
    python tests/scoreboard.py --compare scoreboard/a.json scoreboard/b.json
    python tests/scoreboard.py --check-determinism --runs 8

Requires Java 21+, jpype, and jars/tetrad-gui-7.6.3-launch.jar for recording.
Comparing two existing JSON files needs none of those.
"""

from __future__ import annotations

import argparse
import json
import platform
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable

import numpy as np
import pandas as pd

REPO = Path(__file__).resolve().parent.parent
if str(REPO / "tests") not in sys.path:
    sys.path.insert(0, str(REPO / "tests"))

# ---------------------------------------------------------------------------
# Edge parsing and canonicalisation
# ---------------------------------------------------------------------------

_SEP_RE = re.compile(r"(<->|-->|o->|o-o|<-o|<--|---)")

# Separators whose meaning is unchanged by swapping the two nodes.
_SYMMETRIC_SEPS = {"<->", "o-o", "---"}


def split_edge(e: str) -> tuple[str, str, str] | None:
    """Split an edge string into (left_node, separator, right_node)."""
    m = _SEP_RE.search(e)
    if m is None:
        return None
    return e[:m.start()].strip(), m.group(), e[m.end():].strip()


def pair_key(e: str) -> frozenset | None:
    """Unordered node-pair key, used for adjacency comparison."""
    parts = split_edge(e)
    if parts is None:
        return None
    a, _, b = parts
    return frozenset({a, b})


def adjacency(edges: Iterable[str]) -> set[frozenset]:
    """Set of unordered node pairs, ignoring orientation."""
    return {k for k in (pair_key(e) for e in edges) if k is not None}


def canonical_edge(e: str) -> tuple | None:
    """
    Direction-preserving canonical key.

    Symmetric marks (``<->``, ``o-o``, ``---``) key as ``(sep, frozenset)``.
    Asymmetric marks key as ``(sep, tail, head)``, so ``A --> B`` and
    ``B --> A`` are different keys. ``<--`` and ``<-o`` are normalised to
    ``-->`` / ``o->`` by swapping the nodes.
    """
    parts = split_edge(e)
    if parts is None:
        return None
    a, sep, b = parts
    if sep == "<--":
        sep, a, b = "-->", b, a
    elif sep == "<-o":
        sep, a, b = "o->", b, a
    if sep in _SYMMETRIC_SEPS:
        return (sep, frozenset({a, b}))
    return (sep, a, b)


def jaccard(a: set, b: set) -> float:
    """|A n B| / |A u B|. Two empty sets are identical, hence 1.0."""
    if not a and not b:
        return 1.0
    union = len(a | b)
    return len(a & b) / union if union else 0.0


def agreement_rate(java_edges: list[str], cpp_edges: list[str]) -> tuple[float, list]:
    """
    Fraction of *shared* adjacencies whose canonical edge marks agree.

    Returns (rate, diffs) where diffs is a list of (java_edge, cpp_edge).
    Returns (1.0, []) when the two graphs share no adjacency *and* both are
    empty; (0.0, []) when they share none but at least one is non-empty --
    there is nothing to agree about, and calling that perfect agreement would
    flatter an empty result.
    """
    j_by_pair = {k: e for e in java_edges if (k := pair_key(e)) is not None}
    c_by_pair = {k: e for e in cpp_edges if (k := pair_key(e)) is not None}

    shared = set(j_by_pair) & set(c_by_pair)
    if not shared:
        return (1.0 if not j_by_pair and not c_by_pair else 0.0), []

    diffs = [
        (j_by_pair[p], c_by_pair[p])
        for p in shared
        if canonical_edge(j_by_pair[p]) != canonical_edge(c_by_pair[p])
    ]
    return (len(shared) - len(diffs)) / len(shared), diffs


def classify_diff(java_edge: str, cpp_edge: str) -> str:
    """Name the disagreement pattern, for aggregating across cells."""
    jp, cp = split_edge(java_edge), split_edge(cpp_edge)
    if jp is None or cp is None:
        return "unparseable"
    ja, jsep, jb = jp
    ca, csep, cb = cp

    def norm(a, sep, b):
        if sep == "<--":
            return "-->", b, a
        if sep == "<-o":
            return "o->", b, a
        return sep, a, b

    jsep, ja, jb = norm(ja, jsep, jb)
    csep, ca, cb = norm(ca, csep, cb)

    if jsep == csep:
        return "direction reversal"
    return f"{jsep} (java) vs {csep} (cpp)"


# ---------------------------------------------------------------------------
# Ground truth metrics
# ---------------------------------------------------------------------------
#
# CAVEAT: with latent variables the true structure over the *observed* margin
# is a MAG/PAG, not the sub-DAG of B, and deriving it needs DagToPag which this
# port does not implement. So orientation-based ground-truth metrics are only
# reported for cells with no hidden variables. Skeleton metrics are reported
# whenever a true B is known, but for latent cells the true skeleton is the
# DAG's induced skeleton, which understates the inducing paths a PAG may
# legitimately include -- read those numbers as indicative, not exact.

def skeleton_metrics(edges: list[str], true_b: np.ndarray,
                     names: list[str]) -> dict[str, float]:
    """Adjacency precision/recall/F1 and skeleton SHD against the true DAG."""
    true_adj = {
        frozenset({names[i], names[j]})
        for i in range(true_b.shape[0])
        for j in range(true_b.shape[1])
        if true_b[i, j] != 0
    }
    est_adj = adjacency(edges)
    tp = len(true_adj & est_adj)
    fp = len(est_adj - true_adj)
    fn = len(true_adj - est_adj)
    precision = tp / (tp + fp) if (tp + fp) else 1.0
    recall = tp / (tp + fn) if (tp + fn) else 1.0
    f1 = 2 * precision * recall / (precision + recall) if (precision + recall) else 0.0
    return {
        "skeleton_precision": round(precision, 4),
        "skeleton_recall": round(recall, 4),
        "skeleton_f1": round(f1, 4),
        "skeleton_shd": fp + fn,
    }


def arrowhead_precision(edges: list[str], true_b: np.ndarray,
                        names: list[str]) -> dict[str, float]:
    """
    Of the definite arrowheads the algorithm emits, what fraction point the
    same way as the true DAG?

    An arrowhead at ``head`` in an estimated ``tail --> head`` / ``tail o-> head``
    / ``a <-> b`` edge is scored correct when the true DAG has an edge into
    that same node. Circles and tails are not counted -- they assert nothing.
    """
    idx = {n: i for i, n in enumerate(names)}
    correct = total = 0
    for e in edges:
        key = canonical_edge(e)
        if key is None:
            continue
        sep = key[0]
        if sep in ("-->", "o->"):
            claims = [(key[1], key[2])]          # (tail, head)
        elif sep == "<->":
            a, b = sorted(key[1])
            claims = [(b, a), (a, b)]            # arrowheads at both ends
        else:
            continue
        for tail, head in claims:
            if tail not in idx or head not in idx:
                continue
            total += 1
            if true_b[idx[tail], idx[head]] != 0:
                correct += 1
    return {
        "arrowhead_precision": round(correct / total, 4) if total else None,
        "arrowheads_emitted": total,
    }


# ---------------------------------------------------------------------------
# Datasets (single definition -- previously triplicated across three files)
# ---------------------------------------------------------------------------

_BOSTON_CSV = REPO / "tests" / "data" / "boston_data_raw.csv"


def make_chain(seed: int = 42, n: int = 3000):
    rng = np.random.default_rng(seed)
    x = rng.normal(size=n)
    y = 0.7 * x + 0.5 * rng.normal(size=n)
    z = 0.7 * y + 0.5 * rng.normal(size=n)
    df = pd.DataFrame({"X": x, "Y": y, "Z": z})
    b = np.zeros((3, 3))
    b[0, 1] = 0.7
    b[1, 2] = 0.7
    return df, None, b, ["X", "Y", "Z"], 0


def make_collider(seed: int = 42, n: int = 3000):
    rng = np.random.default_rng(seed)
    x = rng.normal(size=n)
    y = rng.normal(size=n)
    z = 0.8 * x + 0.8 * y + 0.3 * rng.normal(size=n)
    df = pd.DataFrame({"X": x, "Y": y, "Z": z})
    b = np.zeros((3, 3))
    b[0, 2] = 0.8
    b[1, 2] = 0.8
    return df, None, b, ["X", "Y", "Z"], 0


def make_latent(seed: int = 42, n: int = 3000):
    rng = np.random.default_rng(seed)
    latent = rng.normal(size=n)
    x = 0.8 * latent + 0.6 * rng.normal(size=n)
    y = 0.7 * latent + 0.7 * rng.normal(size=n)
    z = 0.6 * x + 0.8 * rng.normal(size=n)
    df = pd.DataFrame({"X": x, "Y": y, "Z": z})
    return df, None, None, ["X", "Y", "Z"], 1


def make_dag(n_obs: int = 8, n_hidden: int = 0, n_samples: int = 2000,
             edge_prob: float = 0.35, seed: int = 10):
    """Random DAG over n_obs + n_hidden variables; hidden ones are dropped."""
    rng = np.random.default_rng(seed)
    p = n_obs + n_hidden
    b = np.zeros((p, p))
    for i in range(p):
        for j in range(i + 1, p):
            if rng.random() < edge_prob:
                b[i, j] = rng.uniform(0.5, 1.5) * rng.choice([-1, 1])

    data = np.zeros((n_samples, p))
    for j in range(p):
        data[:, j] = data @ b[:, j] + rng.normal(size=n_samples)

    names = [f"V{i}" for i in range(p)]
    df = pd.DataFrame(data, columns=names)
    if n_hidden:
        keep = names[:n_obs]
        return df[keep], None, b[:n_obs, :n_obs], keep, n_hidden
    return df, None, b, names, 0


def make_boston():
    """Boston EMA data with t-1 lags; tier 0 = lag, tier 1 = current."""
    from tetrad_port import Knowledge

    raw = pd.read_csv(_BOSTON_CSV).dropna()
    curr_cols = list(raw.columns)
    lag_cols = [c + "_lag" for c in curr_cols]
    curr = raw.iloc[1:].reset_index(drop=True)
    lag = raw.iloc[:-1].reset_index(drop=True)
    lag.columns = lag_cols
    df = pd.concat([curr, lag], axis=1)

    kn_cpp = Knowledge()
    for v in lag_cols:
        kn_cpp.add_to_tier(0, v)
    for v in curr_cols:
        kn_cpp.add_to_tier(1, v)
    kn_java = {"tiers": {0: lag_cols, 1: curr_cols}}
    # Real data: no ground truth, and latents are certainly present.
    return df, (kn_cpp, kn_java), None, list(df.columns), None


DATASETS = {
    "chain": make_chain,
    "collider": make_collider,
    "latent": make_latent,
    "medium_dag": lambda: make_dag(8, 0),
    "medium_latents": lambda: make_dag(8, 2),
    "boston": make_boston,
}

ALGOS = ["pc", "fges", "gfci", "boss", "boss_fci", "grasp", "grasp_fci"]

ALPHA_ALGOS = {"pc", "gfci", "boss_fci", "grasp_fci"}
PD_ALGOS = {"fges", "gfci", "boss", "boss_fci", "grasp", "grasp_fci"}

ALPHA = 0.01
PENALTY_DISCOUNT = 1.0


# ---------------------------------------------------------------------------
# Running
# ---------------------------------------------------------------------------

def _cpp_kwargs(algo: str, kn_cpp) -> dict:
    kwargs: dict[str, Any] = {}
    if kn_cpp is not None:
        kwargs["knowledge"] = kn_cpp
    if algo in ALPHA_ALGOS:
        kwargs["alpha"] = ALPHA
    if algo in PD_ALGOS:
        kwargs["penalty_discount"] = PENALTY_DISCOUNT
    return kwargs


def run_cpp(port, algo: str, df: pd.DataFrame, kn_cpp) -> list[str]:
    out = port.run(df, algo, **_cpp_kwargs(algo, kn_cpp))
    res = out[0] if isinstance(out, tuple) else out
    return [str(e).strip() for e in res["edges"]]


def run_java(oracle, algo: str, df: pd.DataFrame, kn_java) -> list[str]:
    return oracle.run(algo, df, alpha=ALPHA,
                      penalty_discount=PENALTY_DISCOUNT, knowledge=kn_java)


def build_cell(java_edges, cpp_edges, true_b, names, n_hidden) -> dict:
    adj_j, adj_c = adjacency(java_edges), adjacency(cpp_edges)
    rate, diffs = agreement_rate(java_edges, cpp_edges)
    cell: dict[str, Any] = {
        "adjacency_jaccard": round(jaccard(adj_j, adj_c), 4),
        "type_agreement": round(rate, 4),
        "java_edge_count": len(java_edges),
        "cpp_edge_count": len(cpp_edges),
        "only_java": sorted(sorted(p) for p in (adj_j - adj_c)),
        "only_cpp": sorted(sorted(p) for p in (adj_c - adj_j)),
        "disagreements": [
            {"java": j, "cpp": c, "class": classify_diff(j, c)} for j, c in diffs
        ],
        "java_edges": sorted(java_edges),
        "cpp_edges": sorted(cpp_edges),
    }
    if true_b is not None:
        cell["ground_truth"] = {
            "java": {**skeleton_metrics(java_edges, true_b, names),
                     **(arrowhead_precision(java_edges, true_b, names)
                        if not n_hidden else {})},
            "cpp": {**skeleton_metrics(cpp_edges, true_b, names),
                    **(arrowhead_precision(cpp_edges, true_b, names)
                       if not n_hidden else {})},
            "n_hidden": n_hidden,
        }
    return cell


def record(out_path: Path, algos: list[str], datasets: list[str],
           label: str | None = None) -> dict:
    from java_oracle import TetradOracle
    import tetrad_port

    oracle = TetradOracle()
    port = tetrad_port.TetradPort()

    results: dict[str, Any] = {}
    for ds_name in datasets:
        df, kn, true_b, names, n_hidden = DATASETS[ds_name]()
        kn_cpp, kn_java = kn if kn else (None, None)
        for algo in algos:
            key = f"{algo}/{ds_name}"
            try:
                java_edges = run_java(oracle, algo, df, kn_java)
            except Exception as e:
                results[key] = {"error": f"java: {type(e).__name__}: {e}"}
                print(f"  {key:<28} JAVA ERROR", file=sys.stderr)
                continue
            try:
                cpp_edges = run_cpp(port, algo, df, kn_cpp)
            except Exception as e:
                results[key] = {"error": f"cpp: {type(e).__name__}: {e}"}
                print(f"  {key:<28} CPP ERROR", file=sys.stderr)
                continue
            cell = build_cell(java_edges, cpp_edges, true_b, names, n_hidden)
            results[key] = cell
            print(f"  {key:<28} jaccard={cell['adjacency_jaccard']:.3f} "
                  f"type={cell['type_agreement']:.3f}", file=sys.stderr)

    import tetrad_port as tp
    payload = {
        "metadata": {
            "label": label or out_path.stem,
            "platform": platform.platform(),
            "python": platform.python_version(),
            "tetrad_port_version": getattr(tp, "__version__", "unknown"),
            "jar": "tetrad-gui-7.6.3-launch.jar",
            "alpha": ALPHA,
            "penalty_discount": PENALTY_DISCOUNT,
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "algorithms": algos,
            "datasets": datasets,
        },
        "results": results,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2, sort_keys=True))
    return payload


def check_determinism(algos: list[str], datasets: list[str], runs: int) -> int:
    """
    Run each cell `runs` times in this process and report any variation.

    This exists because C++ GRaSP was silently non-deterministic: several
    containers are keyed on shared_ptr identity, so iteration order tracked
    heap addresses and varied between runs within a single process (but not
    across processes, which is why it went unnoticed). Any scoreboard recorded
    while that is true measures noise.
    """
    import tetrad_port
    from java_oracle import TetradOracle

    port = tetrad_port.TetradPort()
    oracle = TetradOracle()
    bad = 0
    for ds_name in datasets:
        df, kn, _, _, _ = DATASETS[ds_name]()
        kn_cpp, kn_java = kn if kn else (None, None)
        for algo in algos:
            for side, fn, k in (("cpp", run_cpp, kn_cpp), ("java", run_java, kn_java)):
                try:
                    seen = {tuple(sorted(fn(port if side == "cpp" else oracle,
                                            algo, df, k))) for _ in range(runs)}
                except Exception as e:
                    print(f"  {algo}/{ds_name} [{side}]: ERROR {type(e).__name__}: {e}")
                    continue
                status = "OK" if len(seen) == 1 else f"NON-DETERMINISTIC ({len(seen)} results)"
                if len(seen) != 1:
                    bad += 1
                    print(f"  {algo}/{ds_name} [{side}]: {status}")
    if bad == 0:
        print(f"All cells deterministic over {runs} runs.")
    return 1 if bad else 0


def compare(path_a: Path, path_b: Path, verbose: bool = False) -> int:
    a = json.loads(path_a.read_text())
    b = json.loads(path_b.read_text())
    ra, rb = a["results"], b["results"]

    print(f"A: {a['metadata'].get('label')}  ({a['metadata'].get('timestamp')})")
    print(f"B: {b['metadata'].get('label')}  ({b['metadata'].get('timestamp')})")
    print()
    header = (f"{'cell':<26} {'jaccard A':>9} {'jaccard B':>9} {'d':>7}   "
              f"{'type A':>7} {'type B':>7} {'d':>7}  status")
    print(header)
    print("-" * len(header))

    regressions = improvements = 0
    for key in sorted(set(ra) | set(rb)):
        ca, cb = ra.get(key), rb.get(key)
        if ca is None or cb is None:
            print(f"{key:<26} {'MISSING in ' + ('A' if ca is None else 'B'):>60}")
            continue
        if "error" in ca or "error" in cb:
            print(f"{key:<26} {'ERROR: ' + (ca.get('error') or cb.get('error'))[:50]:>60}")
            continue
        ja, jb = ca["adjacency_jaccard"], cb["adjacency_jaccard"]
        ta, tb = ca["type_agreement"], cb["type_agreement"]
        dj, dt = jb - ja, tb - ta
        if dj < -1e-9 or dt < -1e-9:
            status, regressions = "REGRESSED", regressions + 1
        elif dj > 1e-9 or dt > 1e-9:
            status, improvements = "improved", improvements + 1
        else:
            status = "same"
        print(f"{key:<26} {ja:>9.3f} {jb:>9.3f} {dj:>+7.3f}   "
              f"{ta:>7.3f} {tb:>7.3f} {dt:>+7.3f}  {status}")
        if verbose and status != "same":
            for e in cb.get("disagreements", []):
                print(f"    B disagreement: java={e['java']!r} cpp={e['cpp']!r}  [{e['class']}]")

    print()
    print(f"{improvements} improved, {regressions} regressed")
    return 1 if regressions else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, help="record a scoreboard to this JSON path")
    ap.add_argument("--label", help="label stored in the JSON metadata")
    ap.add_argument("--compare", nargs=2, type=Path, metavar=("A", "B"),
                    help="diff two scoreboard JSON files")
    ap.add_argument("--check-determinism", action="store_true",
                    help="run each cell repeatedly and report any variation")
    ap.add_argument("--runs", type=int, default=8,
                    help="runs per cell for --check-determinism (default 8)")
    ap.add_argument("--algo", action="append", choices=ALGOS,
                    help="restrict to these algorithms (repeatable)")
    ap.add_argument("--dataset", action="append", choices=list(DATASETS),
                    help="restrict to these datasets (repeatable)")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    algos = args.algo or ALGOS
    datasets = args.dataset or list(DATASETS)

    if args.compare:
        return compare(args.compare[0], args.compare[1], args.verbose)
    if args.check_determinism:
        return check_determinism(algos, datasets, args.runs)
    if args.out:
        record(args.out, algos, datasets, args.label)
        print(f"\nWrote {args.out}", file=sys.stderr)
        return 0

    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
