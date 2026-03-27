"""
Compare two export_results.py JSON files (e.g. Windows vs Linux).

Usage
-----
  python tests/compare_platforms.py results_windows.json results_linux.json
  python tests/compare_platforms.py results_windows.json results_linux.json --verbose

Output
------
  A table with one row per (algorithm, dataset):
    - Adjacency Jaccard  (skeleton agreement, 1.0 = identical)
    - Type agreement     (fraction of shared edges with identical mark, 1.0 = identical)
    - Edge diff          (only-in-A and only-in-B edge lists when --verbose)

Exit code
---------
  0  all entries match (Jaccard == 1.0 and type agreement == 1.0)
  1  at least one difference found
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Edge comparison helpers (same logic as test_java_comparison.py)
# ---------------------------------------------------------------------------

_SEP_RE = re.compile(r"(<->|-->|o->|o-o|<-o|<--|---)")
_SYMMETRIC_SEPS = {"<->", "o-o", "---"}


def _canonical_edge(e: str):
    m = _SEP_RE.search(e)
    if m is None:
        return None
    sep = m.group()
    a = e[:m.start()].strip()
    b = e[m.end():].strip()
    if sep == "<--":
        sep, a, b = "-->", b, a
    elif sep == "<-o":
        sep, a, b = "o->", b, a
    if sep in _SYMMETRIC_SEPS:
        return (sep, frozenset({a, b}))
    return (sep, a, b)


def _pair_key(e: str):
    m = _SEP_RE.search(e)
    if m is None:
        return None
    return frozenset({e[:m.start()].strip(), e[m.end():].strip()})


def adjacency(edges: list[str]) -> set[frozenset]:
    result = set()
    for e in edges:
        p = _pair_key(e)
        if p is not None:
            result.add(p)
    return result


def jaccard(a: set, b: set) -> float:
    if not a and not b:
        return 1.0
    union = len(a | b)
    return len(a & b) / union if union else 0.0


def agreement_rate(edges_a: list[str], edges_b: list[str]) -> tuple[float, list]:
    """Return (rate, diffs) where diffs is list of (edge_in_a, edge_in_b)."""
    by_pair_a: dict[frozenset, str] = {}
    by_pair_b: dict[frozenset, str] = {}
    for e in edges_a:
        p = _pair_key(e)
        if p is not None:
            by_pair_a[p] = e
    for e in edges_b:
        p = _pair_key(e)
        if p is not None:
            by_pair_b[p] = e

    shared = set(by_pair_a) & set(by_pair_b)
    if not shared:
        return (1.0 if not by_pair_a and not by_pair_b else 0.0), []

    diffs = []
    for p in shared:
        ea, eb = by_pair_a[p], by_pair_b[p]
        if _canonical_edge(ea) != _canonical_edge(eb):
            diffs.append((ea, eb))

    agree = len(shared) - len(diffs)
    return agree / len(shared), diffs


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def load(path: str) -> dict:
    with open(path) as f:
        return json.load(f)


def print_metadata(label: str, meta: dict) -> None:
    print(f"  {label}:")
    print(f"    platform : {meta.get('platform', 'unknown')}")
    print(f"    version  : {meta.get('tetrad_port_version', 'unknown')}")
    print(f"    timestamp: {meta.get('timestamp', 'unknown')}")


def main():
    parser = argparse.ArgumentParser(
        description="Compare two export_results.py JSON outputs.")
    parser.add_argument("file_a", help="First results JSON (e.g. windows)")
    parser.add_argument("file_b", help="Second results JSON (e.g. linux)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print edge-level differences")
    args = parser.parse_args()

    data_a = load(args.file_a)
    data_b = load(args.file_b)

    label_a = Path(args.file_a).stem
    label_b = Path(args.file_b).stem

    print("\n=== Platform Metadata ===")
    print_metadata(label_a, data_a.get("metadata", {}))
    print_metadata(label_b, data_b.get("metadata", {}))

    results_a = data_a.get("results", {})
    results_b = data_b.get("results", {})

    all_keys = sorted(set(results_a) | set(results_b))
    if not all_keys:
        print("\nNo results found in either file.")
        sys.exit(0)

    # Column widths
    key_w = max(len(k) for k in all_keys) + 2
    col_w = 14

    header = (f"{'dataset/algo':<{key_w}}"
              f"{'edges_A':>{col_w}}"
              f"{'edges_B':>{col_w}}"
              f"{'adj_jaccard':>{col_w}}"
              f"{'type_agree':>{col_w}}"
              f"  status")
    sep = "-" * len(header)

    print(f"\n=== Results: {label_a}  vs  {label_b} ===")
    print(sep)
    print(header)
    print(sep)

    any_diff = False

    for key in all_keys:
        entry_a = results_a.get(key)
        entry_b = results_b.get(key)

        # Missing on one side
        if entry_a is None:
            print(f"{key:<{key_w}}{'N/A':>{col_w}}{'':>{col_w}}{'':>{col_w}}{'':>{col_w}}  MISSING in A")
            any_diff = True
            continue
        if entry_b is None:
            print(f"{key:<{key_w}}{'':>{col_w}}{'N/A':>{col_w}}{'':>{col_w}}{'':>{col_w}}  MISSING in B")
            any_diff = True
            continue

        # Error on either side
        if "error" in entry_a or "error" in entry_b:
            err = entry_a.get("error") or entry_b.get("error")
            print(f"{key:<{key_w}}{'ERROR':>{col_w}}{'ERROR':>{col_w}}{'':>{col_w}}{'':>{col_w}}  {err[:60]}")
            any_diff = True
            continue

        edges_a = entry_a["edges_canonical"]
        edges_b = entry_b["edges_canonical"]
        n_a = len(edges_a)
        n_b = len(edges_b)

        adj_a = adjacency(edges_a)
        adj_b = adjacency(edges_b)
        jac = jaccard(adj_a, adj_b)
        rate, diffs = agreement_rate(edges_a, edges_b)

        only_a = adj_a - adj_b
        only_b = adj_b - adj_a

        identical = (jac == 1.0 and rate == 1.0)
        status = "OK" if identical else "DIFF"
        if not identical:
            any_diff = True

        print(f"{key:<{key_w}}{n_a:>{col_w}}{n_b:>{col_w}}"
              f"{jac:>{col_w}.4f}{rate:>{col_w}.4f}  {status}")

        if args.verbose and not identical:
            if only_a:
                pairs = [sorted(p) for p in only_a]
                print(f"    Only in {label_a}: {pairs}")
            if only_b:
                pairs = [sorted(p) for p in only_b]
                print(f"    Only in {label_b}: {pairs}")
            if diffs:
                print(f"    Mark disagreements:")
                for ea, eb in diffs:
                    print(f"      {label_a}: {ea!r}")
                    print(f"      {label_b}: {eb!r}")

    print(sep)

    if any_diff:
        print("\nResult: DIFFERENCES FOUND")
        sys.exit(1)
    else:
        print("\nResult: IDENTICAL on all datasets")
        sys.exit(0)


if __name__ == "__main__":
    main()
