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

# Symmetric separators: edge meaning is the same regardless of node ordering.
_SYMMETRIC_SEPS = {"<->", "o-o", "---"}


def _adjacency(edges: list[str]) -> set[frozenset]:
    result = set()
    for e in edges:
        m = _SEP_RE.search(e)
        if m:
            a = e[:m.start()].strip()
            b = e[m.end():].strip()
            result.add(frozenset({a, b}))
    return result


def _canonical_edge(e: str) -> tuple | None:
    """
    Return a direction-preserving canonical key for an edge string.

    For symmetric marks (<->, o-o, ---) the key is (sep, frozenset).
    For directed/asymmetric marks (--> <-- o-> <-o) the key is (sep, tail_node, head_node)
    so that 'A --> B' and 'B --> A' produce DIFFERENT keys.

    Canonicalises '<-o' as 'o->' and '<--' as '-->' by swapping a and b, so
    that mark comparisons are always in terms of --> / o-> canonical forms.
    """
    m = _SEP_RE.search(e)
    if m is None:
        return None
    sep = m.group()
    a = e[:m.start()].strip()
    b = e[m.end():].strip()

    # Normalise reversed-arrow spellings to their canonical form
    if sep == "<--":
        sep, a, b = "-->", b, a
    elif sep == "<-o":
        sep, a, b = "o->", b, a

    if sep in _SYMMETRIC_SEPS:
        return (sep, frozenset({a, b}))
    # directed/asymmetric: preserve (tail, head) order
    return (sep, a, b)


def _edge_map(edges: list[str]) -> dict:
    """Map canonical edge key → full edge string."""
    result = {}
    for e in edges:
        key = _canonical_edge(e)
        if key is not None:
            result[key] = e
    return result


def _pair_key(e: str) -> frozenset | None:
    """Frozenset node-pair key (for adjacency lookup only)."""
    m = _SEP_RE.search(e)
    if m is None:
        return None
    return frozenset({e[:m.start()].strip(), e[m.end():].strip()})


def jaccard(a: set, b: set) -> float:
    if not a and not b:
        return 1.0
    inter = len(a & b)
    union = len(a | b)
    return inter / union if union else 0.0


def agreement_rate(java_edges: list[str], cpp_edges: list[str]) -> tuple[float, list]:
    """
    Compute the fraction of shared adjacencies whose canonical edge marks agree.

    Uses direction-preserving canonical keys so that 'A --> B' vs 'B --> A'
    is counted as a disagreement, not an agreement.

    Returns (rate, diffs) where diffs is a list of (java_full, cpp_full) pairs.
    """
    # Build pair → full-string maps for each side
    j_by_pair: dict[frozenset, str] = {}
    c_by_pair: dict[frozenset, str] = {}
    for e in java_edges:
        p = _pair_key(e)
        if p is not None:
            j_by_pair[p] = e
    for e in cpp_edges:
        p = _pair_key(e)
        if p is not None:
            c_by_pair[p] = e

    shared_pairs = set(j_by_pair) & set(c_by_pair)
    if not shared_pairs:
        return 0.0, []

    diffs = []
    for p in shared_pairs:
        je = j_by_pair[p]
        ce = c_by_pair[p]
        if _canonical_edge(je) != _canonical_edge(ce):
            diffs.append((je, ce))

    agree = len(shared_pairs) - len(diffs)
    return agree / len(shared_pairs), diffs


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
        msg_parts.append("  Edge disagreements (full strings):")
        for je, ce in diffs:
            msg_parts.append(f"    Java: {je!r}")
            msg_parts.append(f"    C++:  {ce!r}")
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


# ---------------------------------------------------------------------------
# Boston EMA dataset: real-world lagged data with temporal knowledge
# ---------------------------------------------------------------------------

_BOSTON_CSV = Path(__file__).parent / "data" / "boston_data_raw.csv"


def _make_boston_lagged():
    """
    Load Boston EMA data and create lagged (t-1) versions of each variable.

    Returns (df, knowledge_cpp, knowledge_java) where:
      - df has columns [v1, v2, ..., v1_lag, v2_lag, ...]
      - knowledge_cpp is a tetrad_port.Knowledge with lag vars in tier 0,
        current vars in tier 1
      - knowledge_java is a dict {"tiers": {0: [...], 1: [...]}} for the oracle
    """
    from tetrad_port import Knowledge

    raw = pd.read_csv(_BOSTON_CSV).dropna()
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

    knowledge_java = {"tiers": {0: lag_cols, 1: curr_cols}}

    return df, kn, knowledge_java


@needs_java
@pytest.mark.skipif(not _BOSTON_CSV.exists(), reason="Boston data not found in tests/data/")
class TestBostonKnowledgeComparison:
    """Compare Java vs C++ on real EMA data with temporal knowledge (tier 0=lag, tier 1=current)."""

    @pytest.fixture(scope="class")
    def boston(self):
        return _make_boston_lagged()

    def test_pc_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("pc", df, alpha=0.01, knowledge=kn_java)
        cpp_r, _ = cpp.run_pc(df, alpha=0.01, knowledge=kn)
        # One Meek rule orientation can differ due to adjacency list ordering
        assert_similarity("pc/boston", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=0.90)

    def test_fges_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("fges", df, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_fges(df, penalty_discount=1.0, knowledge=kn)
        assert_similarity("fges/boston", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=1.0)

    def test_boss_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("boss", df, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_boss(df, penalty_discount=1.0, knowledge=kn)
        # BOSS uses random permutations — Java results can vary between runs.
        assert_similarity("boss/boston", java, cpp_r["edges"], min_jaccard=0.65, min_type_agree=0.60)

    def test_grasp_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("grasp", df, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_grasp(df, penalty_discount=1.0, knowledge=kn)
        # GRaSP uses randomized DFS — Java results can vary between runs.
        assert_similarity("grasp/boston", java, cpp_r["edges"], min_jaccard=0.65, min_type_agree=0.60)

    def test_gfci_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("gfci", df, alpha=0.01, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_gfci(df, alpha=0.01, penalty_discount=1.0, knowledge=kn)
        assert_similarity("gfci/boston", java, cpp_r["edges"], min_jaccard=1.0, min_type_agree=1.0)

    def test_boss_fci_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("boss_fci", df, alpha=0.01, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_boss_fci(df, alpha=0.01, penalty_discount=1.0, knowledge=kn)
        # BOSS uses random permutations, so Java results can vary between runs.
        assert_similarity("boss_fci/boston", java, cpp_r["edges"], min_jaccard=0.65, min_type_agree=0.60)

    def test_grasp_fci_boston(self, oracle, cpp, boston):
        df, kn, kn_java = boston
        java = oracle.run("grasp_fci", df, alpha=0.01, penalty_discount=1.0, knowledge=kn_java)
        cpp_r, _ = cpp.run_grasp_fci(df, alpha=0.01, penalty_discount=1.0, knowledge=kn)
        # GRaSP uses randomized DFS, so Java results can vary between runs.
        assert_similarity("grasp_fci/boston", java, cpp_r["edges"], min_jaccard=0.80, min_type_agree=0.40)


# ---------------------------------------------------------------------------
# Report generation — runs last, regenerates JavaCPPComparison.md
# ---------------------------------------------------------------------------

@needs_java
@pytest.mark.skipif(not _BOSTON_CSV.exists(), reason="Boston data not found in tests/data/")
class TestGenerateReport:
    """Regenerate JavaCPPComparison.md after all comparison tests pass."""

    def test_generate_comparison_report(self):
        from generate_comparison_report import generate_report
        generate_report()
