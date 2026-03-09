"""
Java (Tetrad 7.6.8) vs C++ comparison tests.

Runs every algorithm on the same data with both the Java oracle (via jpype)
and the C++ port (via tetrad_port Python bindings), then compares:
  - Adjacency Jaccard (skeleton agreement)
  - Edge type agreement rate (for shared adjacencies)

Skipped automatically if the Tetrad JAR is not present or jpype is unavailable.

Run with:
    pytest tests/test_java_comparison.py -v
    pytest tests/test_java_comparison.py -v -k gfci
"""

from __future__ import annotations

import re
from itertools import combinations
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

# ---------------------------------------------------------------------------
# Skip guard — both JAR and jpype must be present
# ---------------------------------------------------------------------------

JAR = Path(__file__).parent.parent / "jars" / "tetrad-gui-7.6.8-launch.jar"

jpype_available = True
try:
    import jpype  # noqa: F401
except ImportError:
    jpype_available = False

skip_reason = ""
if not jpype_available:
    skip_reason = "jpype not installed"
elif not JAR.exists():
    skip_reason = f"Tetrad JAR not found at {JAR}"

needs_java = pytest.mark.skipif(bool(skip_reason), reason=skip_reason or "java unavailable")

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def oracle():
    from java_oracle import TetradOracle
    return TetradOracle()


@pytest.fixture(scope="module")
def cpp():
    from tetrad_port import TetradPort
    return TetradPort()


def _make_chain(seed=42, n=3000):
    """X -> Y -> Z chain."""
    rng = np.random.default_rng(seed)
    X = rng.standard_normal(n)
    Y = 0.7 * X + 0.5 * rng.standard_normal(n)
    Z = 0.7 * Y + 0.5 * rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_collider(seed=42, n=3000):
    """X -> Z <- Y collider."""
    rng = np.random.default_rng(seed)
    X = rng.standard_normal(n)
    Y = rng.standard_normal(n)
    Z = 0.8 * X + 0.8 * Y + 0.3 * rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_latent(seed=42, n=3000):
    """L -> X, L -> Y, X -> Z (L hidden)."""
    rng = np.random.default_rng(seed)
    L = rng.standard_normal(n)
    X = 0.8 * L + rng.standard_normal(n)
    Y = 0.7 * L + rng.standard_normal(n)
    Z = 0.6 * X + rng.standard_normal(n)
    return pd.DataFrame({"X": X, "Y": Y, "Z": Z})


def _make_dag(n_obs: int, n_hidden: int = 0, n_samples: int = 2000,
              edge_prob: float = 0.35, seed: int = 1) -> pd.DataFrame:
    """Random linear Gaussian DAG; hidden variables are marginalized out."""
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


# ---------------------------------------------------------------------------
# Comparison helpers
# ---------------------------------------------------------------------------

_SEP_RE = re.compile(r"(<->|-->|o->|o-o|<-o|<--|---)")


def _adjacency(edges: list[str]) -> set[frozenset]:
    result = set()
    for e in edges:
        m = _SEP_RE.search(e)
        if m:
            sep_start = m.start()
            sep_end = m.end()
            a = e[:sep_start].strip()
            b = e[sep_end:].strip()
            result.add(frozenset({a, b}))
    return result


def _edge_map(edges: list[str]) -> dict[frozenset, str]:
    result = {}
    for e in edges:
        m = _SEP_RE.search(e)
        if m:
            sep = m.group()
            a = e[:m.start()].strip()
            b = e[m.end():].strip()
            result[frozenset({a, b})] = sep
    return result


def jaccard(a: set, b: set) -> float:
    if not a and not b:
        return 1.0
    inter = len(a & b)
    union = len(a | b)
    return inter / union if union else 0.0


def agreement_rate(java_edges: list[str], cpp_edges: list[str]) -> tuple[float, list]:
    jmap = _edge_map(java_edges)
    cmap = _edge_map(cpp_edges)
    shared = set(jmap) & set(cmap)
    if not shared:
        return 0.0, []
    diffs = [(sorted(p), jmap[p], cmap[p]) for p in shared if jmap[p] != cmap[p]]
    agree = len(shared) - len(diffs)
    return agree / len(shared), diffs


def assert_similarity(
    algo: str,
    java_edges: list[str],
    cpp_edges: list[str],
    min_jaccard: float = 0.75,
    min_type_agree: float = 0.60,
):
    """Assert that Java and C++ outputs are sufficiently similar."""
    adj_j = _adjacency(java_edges)
    adj_c = _adjacency(cpp_edges)
    jac = jaccard(adj_j, adj_c)
    rate, diffs = agreement_rate(java_edges, cpp_edges)

    only_java = adj_j - adj_c
    only_cpp = adj_c - adj_j

    msg_parts = [
        f"\n[{algo}] Java={len(java_edges)} edges, C++={len(cpp_edges)} edges",
        f"  Adjacency Jaccard: {jac:.3f} (min={min_jaccard})",
        f"  Type agreement:    {rate:.1%} (min={min_type_agree})",
    ]
    if only_java:
        msg_parts.append(f"  Only in Java: {[sorted(p) for p in only_java]}")
    if only_cpp:
        msg_parts.append(f"  Only in C++:  {[sorted(p) for p in only_cpp]}")
    if diffs:
        msg_parts.append("  Type disagreements:")
        for pair, jt, ct in diffs:
            msg_parts.append(f"    {pair[0]} — {pair[1]}: Java={jt}, C++={ct}")
    msg_parts.append(f"  Java edges: {java_edges}")
    msg_parts.append(f"  C++  edges: {cpp_edges}")
    detail = "\n".join(msg_parts)

    assert jac >= min_jaccard, f"Adjacency Jaccard {jac:.3f} < {min_jaccard}{detail}"
    if (adj_j & adj_c):  # only check type agreement when there are shared edges
        assert rate >= min_type_agree, f"Type agreement {rate:.1%} < {min_type_agree}{detail}"


# ---------------------------------------------------------------------------
# Tests: constraint-based (CPDAG algorithms)
# ---------------------------------------------------------------------------


@needs_java
class TestPCComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("pc", df, alpha=0.01)
        cpp_r, _ = cpp.run_pc(df, alpha=0.01)
        assert_similarity("pc/chain", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=0.8)

    def test_collider(self, oracle, cpp):
        df = _make_collider()
        java = oracle.run("pc", df, alpha=0.01)
        cpp_r, _ = cpp.run_pc(df, alpha=0.01)
        assert_similarity("pc/collider", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=0.8)

    def test_medium_dag(self, oracle, cpp):
        df = _make_dag(8, n_hidden=0, seed=10)
        java = oracle.run("pc", df, alpha=0.01)
        cpp_r, _ = cpp.run_pc(df, alpha=0.01)
        assert_similarity("pc/medium", java, cpp_r["edges"], min_jaccard=0.85)


@needs_java
class TestFGESComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("fges", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_fges(df, penalty_discount=1.0)
        assert_similarity("fges/chain", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=1.0)

    def test_collider(self, oracle, cpp):
        df = _make_collider()
        java = oracle.run("fges", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_fges(df, penalty_discount=1.0)
        assert_similarity("fges/collider", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=0.8)

    def test_medium_dag(self, oracle, cpp):
        df = _make_dag(8, n_hidden=0, seed=10)
        java = oracle.run("fges", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_fges(df, penalty_discount=1.0)
        assert_similarity("fges/medium", java, cpp_r["edges"], min_jaccard=0.85)


@needs_java
class TestBOSSComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("boss", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_boss(df, penalty_discount=1.0)
        assert_similarity("boss/chain", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=1.0)

    def test_medium_dag(self, oracle, cpp):
        df = _make_dag(8, n_hidden=0, seed=10)
        java = oracle.run("boss", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_boss(df, penalty_discount=1.0)
        assert_similarity("boss/medium", java, cpp_r["edges"], min_jaccard=0.8)


@needs_java
class TestGRaSPComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("grasp", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_grasp(df, penalty_discount=1.0)
        assert_similarity("grasp/chain", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=1.0)

    def test_medium_dag(self, oracle, cpp):
        df = _make_dag(8, n_hidden=0, seed=10)
        java = oracle.run("grasp", df, penalty_discount=1.0)
        cpp_r, _ = cpp.run_grasp(df, penalty_discount=1.0)
        assert_similarity("grasp/medium", java, cpp_r["edges"], min_jaccard=0.8)


# ---------------------------------------------------------------------------
# Tests: latent-variable (PAG algorithms)
# ---------------------------------------------------------------------------


@needs_java
class TestGFCIComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_gfci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("gfci/chain", java, cpp_r["edges"], min_jaccard=0.9, min_type_agree=0.6)

    def test_collider(self, oracle, cpp):
        df = _make_collider()
        java = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_gfci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("gfci/collider", java, cpp_r["edges"], min_jaccard=0.9, min_type_agree=0.6)

    def test_latent(self, oracle, cpp):
        df = _make_latent()
        java = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_gfci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("gfci/latent", java, cpp_r["edges"], min_jaccard=0.75, min_type_agree=0.5)

    def test_medium_with_latents(self, oracle, cpp):
        df = _make_dag(8, n_hidden=2, seed=10)
        java = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_gfci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("gfci/medium+latents", java, cpp_r["edges"], min_jaccard=0.75, min_type_agree=0.5)


@needs_java
class TestBOSSFCIComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("boss_fci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_boss_fci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("boss_fci/chain", java, cpp_r["edges"], min_jaccard=0.9, min_type_agree=0.6)

    def test_latent(self, oracle, cpp):
        df = _make_latent()
        java = oracle.run("boss_fci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_boss_fci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("boss_fci/latent", java, cpp_r["edges"], min_jaccard=0.75, min_type_agree=0.5)


@needs_java
class TestGRaSPFCIComparison:
    def test_chain(self, oracle, cpp):
        df = _make_chain()
        java = oracle.run("grasp_fci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_grasp_fci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("grasp_fci/chain", java, cpp_r["edges"], min_jaccard=0.9, min_type_agree=0.6)

    def test_latent(self, oracle, cpp):
        df = _make_latent()
        java = oracle.run("grasp_fci", df, alpha=0.01, penalty_discount=1.0)
        cpp_r, _ = cpp.run_grasp_fci(df, alpha=0.01, penalty_discount=1.0)
        assert_similarity("grasp_fci/latent", java, cpp_r["edges"], min_jaccard=0.75, min_type_agree=0.5)
