"""
Accuracy tests against simulated data of differing size and complexity.

These complement ``test_java_comparison.py``, which only asks whether the C++
matches the Java oracle -- a test both would pass if the algorithm were wrong.
Here the ground truth is known by construction, so the assertions are about
whether the algorithms actually recover causal structure, and whether they
behave the way the papers in ``References/`` say they should:

* accuracy improves with sample size (the consistency results),
* accuracy degrades as the graph gets denser (harder conditional independence
  tests, more equivalent structures),
* the same seed gives the same answer.

Absolute floors are set from a recorded run and deliberately left below observed
performance: they are regression guards, not performance targets. The numbers
that track performance live in ``scoreboard/simulation-*.json``, written by
``tests/run_simulation_sweep.py``.

Latent-variable cells and every FCI variant need the Java oracle (Java 21+,
jpype, the Tetrad JAR) because the true PAG comes from Tetrad's ``DagToPag``;
without it those tests skip rather than fail.
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

REPO = Path(__file__).resolve().parent.parent
if str(REPO / "tests") not in sys.path:
    sys.path.insert(0, str(REPO / "tests"))

from run_simulation_sweep import (ALGOS, Config, run_cell,  # noqa: E402
                                  run_grid)
from scoreboard import run_cpp  # noqa: E402
from simulation import (CPDAG_ALGOS, PAG_ALGOS, dag_to_cpdag,  # noqa: E402
                        endpoint_marks, make_simulation, simulate_dag,
                        simulate_data, structural_metrics, choose_latents)

# Floors observed with margin on a recorded baseline; see module docstring.
MIN_SKELETON_F1 = 0.85
MIN_ARROWHEAD_PRECISION = 0.60

# BOSS and GRaSP randomise restarts, so a trend can wobble without anything
# having changed. Trends must hold beyond this margin to count.
TREND_TOL = 0.03

SEEDS = (0, 1, 2, 3, 4)

# The regime where the theory's asymptotic guarantees should be visible: sparse
# graph, large sample. Denser and smaller-sample cells are recorded by the sweep
# but not asserted on -- finite-sample failure there is expected, not a bug.
EASY_CONFIGS = [Config(5, 2, 10000, 0), Config(10, 2, 10000, 0),
                Config(20, 2, 10000, 0)]

SAMPLE_TREND_CONFIGS = [(Config(10, 2, 500, 0), Config(10, 2, 10000, 0)),
                        (Config(20, 2, 500, 0), Config(20, 2, 10000, 0))]

DENSITY_CONFIGS = [(Config(10, 2, 10000, 0), Config(10, 4, 10000, 0)),
                   (Config(20, 2, 10000, 0), Config(20, 4, 10000, 0))]

LATENT_CONFIGS = [Config(10, 2, 10000, 2), Config(20, 2, 10000, 2)]


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture(scope="session")
def port():
    import tetrad_port
    return tetrad_port.TetradPort()


@pytest.fixture(scope="session")
def oracle():
    """The Java oracle, or None. PAG ground truth is unavailable without it."""
    try:
        from java_oracle import TetradOracle
        return TetradOracle()
    except Exception:
        return None


@pytest.fixture(scope="session")
def require_oracle(oracle):
    if oracle is None:
        pytest.skip("Java oracle unavailable (needs Java 21+, jpype, Tetrad JAR)")
    return oracle


def _cells(port, oracle, algo, configs):
    """Aggregate metrics for one algorithm over the given configs."""
    return run_grid(port, oracle, list(configs), SEEDS, [algo])


def _mean(cells: dict, algo: str, cfg: Config, metric: str):
    cell = cells.get(f"{algo}/{cfg.key}")
    if not cell or metric not in cell:
        return None
    return cell[metric]["mean"]


# ---------------------------------------------------------------------------
# The simulators themselves
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("n_nodes,avg_degree", [(5, 2), (10, 2), (10, 4), (20, 3), (40, 2)])
def test_simulated_dag_is_acyclic_and_reproducible(n_nodes, avg_degree):
    b = simulate_dag(n_nodes, avg_degree, seed=7)
    assert np.array_equal(b, simulate_dag(n_nodes, avg_degree, seed=7))

    # Reachability closure: a DAG has no node reachable from itself.
    reach = (b != 0).astype(bool)
    closure = reach.copy()
    for _ in range(n_nodes):
        closure |= closure @ reach
    assert not closure.diagonal().any(), "simulated graph contains a cycle"


def test_average_degree_is_approximately_honoured():
    """A density knob nothing checks is a density knob that silently lies."""
    for avg_degree in (2, 4, 6):
        degrees = []
        for seed in range(20):
            b = simulate_dag(30, avg_degree, seed)
            degrees.append(2 * np.count_nonzero(b) / 30)
        assert abs(float(np.mean(degrees)) - avg_degree) < 0.5


def test_simulated_data_follows_the_dag():
    """Each variable should correlate with its parents; a mis-ordered sampler would not."""
    b = simulate_dag(10, 2, seed=3)
    data = simulate_data(b, 20000, seed=4)
    corr = np.corrcoef(data, rowvar=False)
    for i in range(10):
        for j in range(10):
            if b[i, j] != 0:
                assert abs(corr[i, j]) > 0.1, f"edge {i}->{j} left no marginal trace"


def test_latents_are_chosen_to_be_confounders():
    """
    Hiding a leaf would make the latent cells silently equivalent to the
    observed ones, so latents must have at least two children when the graph
    offers any such node.
    """
    for seed in range(20):
        b = simulate_dag(12, 3, seed)
        out_deg = [int(np.count_nonzero(b[i, :])) for i in range(12)]
        if max(out_deg) < 2:
            continue
        for idx in choose_latents(b, 2):
            if sorted(out_deg, reverse=True)[1] >= 2:
                assert out_deg[idx] >= 2


def test_simulation_drops_latent_columns():
    sim = make_simulation(n_nodes=8, avg_degree=2, n_samples=200, n_hidden=2, seed=1)
    assert sim.df.shape[1] == 8
    assert len(sim.all_names) == 10
    assert set(sim.latent_names).isdisjoint(sim.observed_names)


# ---------------------------------------------------------------------------
# Ground truth
# ---------------------------------------------------------------------------

def test_dag_to_cpdag_matches_tetrad(require_oracle):
    """
    Our CPDAG conversion is the yardstick every CPDAG algorithm is measured
    against, so it is checked against Tetrad's own ``GraphTransforms``, not
    against itself.
    """
    def normalise(edges):
        out = []
        for e in edges:
            a, sep, c = e.split(" ")
            if sep == "<--":
                sep, a, c = "-->", c, a
            if sep == "---" and a > c:
                a, c = c, a
            out.append(f"{a} {sep} {c}")
        return sorted(out)

    for seed in range(30):
        for n_nodes, avg_degree in ((5, 2), (8, 2), (8, 4), (12, 3), (20, 2)):
            b = simulate_dag(n_nodes, avg_degree, seed)
            names = [f"V{i}" for i in range(n_nodes)]
            assert normalise(dag_to_cpdag(b, names)) == \
                   normalise(require_oracle.dag_to_cpdag(b, names)), \
                   f"CPDAG mismatch at seed={seed} p={n_nodes} d={avg_degree}"


def test_cpdag_ground_truth_is_rejected_for_latent_simulations():
    """Scoring a CPDAG algorithm against a marginalised DAG would flatter it."""
    sim = make_simulation(6, 2, 200, n_hidden=2, seed=0)
    with pytest.raises(ValueError, match="CPDAG"):
        sim.true_graph("fges")


# ---------------------------------------------------------------------------
# Metrics
# ---------------------------------------------------------------------------

def test_endpoint_marks_reads_both_ends():
    marks = endpoint_marks(["A o-> B", "B --- C", "C <-> D"])
    assert marks[("A", "B")] == ">" and marks[("B", "A")] == "o"
    assert marks[("B", "C")] == "-" and marks[("C", "B")] == "-"
    assert marks[("C", "D")] == ">" and marks[("D", "C")] == ">"


def test_perfect_estimate_scores_perfectly():
    truth = ["A --> B", "B --- C", "C <-> D"]
    m = structural_metrics(truth, truth)
    assert m["skeleton_f1"] == 1.0
    assert m["skeleton_shd"] == 0 and m["endpoint_shd"] == 0
    assert m["arrowhead_precision"] == 1.0 and m["arrowhead_recall"] == 1.0


def test_reversed_edge_keeps_skeleton_but_loses_endpoints():
    m = structural_metrics(["B --> A"], ["A --> B"])
    assert m["skeleton_f1"] == 1.0 and m["skeleton_shd"] == 0
    assert m["arrowhead_precision"] == 0.0
    assert m["endpoint_shd"] == 2       # both ends mismarked


def test_missing_edge_costs_two_endpoints():
    m = structural_metrics([], ["A --> B"])
    assert m["skeleton_recall"] == 0.0
    assert m["endpoint_shd"] == 2


def test_circles_make_no_arrowhead_claim():
    """An all-circles PAG asserts nothing, so it can neither gain nor lose arrowhead precision."""
    m = structural_metrics(["A o-o B"], ["A --> B"])
    assert m["skeleton_f1"] == 1.0
    assert m["arrowhead_precision"] is None      # nothing claimed
    assert m["arrowhead_recall"] == 0.0          # something missed


# ---------------------------------------------------------------------------
# Accuracy
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("algo", ALGOS)
def test_recovers_sparse_structure_at_large_n(port, oracle, algo):
    """
    Sparse graph, 10,000 samples: the regime where the consistency results in
    References/ predict near-exact recovery. Failure here means the algorithm
    is wrong, not that the problem was hard.
    """
    if algo in PAG_ALGOS and oracle is None:
        pytest.skip("PAG ground truth needs the Java oracle")

    cells = _cells(port, oracle, algo, EASY_CONFIGS)
    for cfg in EASY_CONFIGS:
        f1 = _mean(cells, algo, cfg, "skeleton_f1")
        assert f1 is not None, f"no result for {algo}/{cfg.key}"
        assert f1 >= MIN_SKELETON_F1, \
            f"{algo} skeleton F1 {f1:.3f} < {MIN_SKELETON_F1} at {cfg.key}"

        arrow = _mean(cells, algo, cfg, "arrowhead_precision")
        if arrow is not None:
            assert arrow >= MIN_ARROWHEAD_PRECISION, \
                f"{algo} arrowhead precision {arrow:.3f} < " \
                f"{MIN_ARROWHEAD_PRECISION} at {cfg.key}"


@pytest.mark.parametrize("algo", ALGOS)
def test_accuracy_improves_with_sample_size(port, oracle, algo):
    """
    The empirical face of the consistency theorems (Chickering 2002 Thm 15 for
    GES-family, and the corresponding results for the permutation and FCI
    families): more data should not make recovery worse.
    """
    if algo in PAG_ALGOS and oracle is None:
        pytest.skip("PAG ground truth needs the Java oracle")

    configs = [c for pair in SAMPLE_TREND_CONFIGS for c in pair]
    cells = _cells(port, oracle, algo, configs)
    for small, large in SAMPLE_TREND_CONFIGS:
        lo = _mean(cells, algo, small, "skeleton_f1")
        hi = _mean(cells, algo, large, "skeleton_f1")
        assert lo is not None and hi is not None
        assert hi >= lo - TREND_TOL, (
            f"{algo}: skeleton F1 fell from {lo:.3f} at n={small.n_samples} "
            f"to {hi:.3f} at n={large.n_samples} (p={small.n_nodes})")


@pytest.mark.parametrize("algo", ALGOS)
def test_denser_graphs_are_harder(port, oracle, algo):
    """
    A sanity check on the complexity axis: at fixed n, average degree 4 should
    not be *easier* than degree 2. If it were, the density knob would not be
    doing what the sweep reports it as doing.
    """
    if algo in PAG_ALGOS and oracle is None:
        pytest.skip("PAG ground truth needs the Java oracle")

    configs = [c for pair in DENSITY_CONFIGS for c in pair]
    cells = _cells(port, oracle, algo, configs)
    for sparse, dense in DENSITY_CONFIGS:
        s = _mean(cells, algo, sparse, "skeleton_f1")
        d = _mean(cells, algo, dense, "skeleton_f1")
        assert s is not None and d is not None
        assert d <= s + TREND_TOL, (
            f"{algo}: dense graph scored {d:.3f} vs sparse {s:.3f} "
            f"(p={sparse.n_nodes}) -- density axis looks inverted")


@pytest.mark.parametrize("algo", PAG_ALGOS)
def test_fci_variants_handle_latent_confounders(port, require_oracle, algo):
    """
    With confounders hidden, the FCI variants are scored against the true PAG.
    Recovery is genuinely harder here, so the floor is lower than the observed
    case -- the point is that they still find most of the structure rather than
    degenerating.
    """
    cells = _cells(port, require_oracle, algo, LATENT_CONFIGS)
    for cfg in LATENT_CONFIGS:
        f1 = _mean(cells, algo, cfg, "skeleton_f1")
        assert f1 is not None, f"no result for {algo}/{cfg.key}"
        assert f1 >= 0.75, f"{algo} skeleton F1 {f1:.3f} < 0.75 at {cfg.key}"


@pytest.mark.parametrize("algo", ALGOS)
def test_same_seed_gives_same_answer(port, algo):
    """
    Guards a bug this port has had before: containers keyed on ``shared_ptr``
    identity iterate in heap-address order, which varies within a process. Any
    recorded accuracy number is meaningless while that is true.
    """
    sim = make_simulation(15, 2, 2000, n_hidden=0, seed=11)
    runs = {tuple(sorted(run_cpp(port, algo, sim.df, None))) for _ in range(3)}
    assert len(runs) == 1, f"{algo} produced {len(runs)} different results on identical input"


@pytest.mark.parametrize("algo", ALGOS)
def test_rerunning_the_simulation_reproduces_the_data(algo):
    """The whole suite is only reproducible if the simulator is."""
    a = make_simulation(10, 2, 500, n_hidden=1, seed=5)
    b = make_simulation(10, 2, 500, n_hidden=1, seed=5)
    assert a.observed_names == b.observed_names
    assert np.allclose(a.df.values, b.df.values)


# ---------------------------------------------------------------------------
# Slow tier
# ---------------------------------------------------------------------------

@pytest.mark.slow
@pytest.mark.parametrize("n_nodes", [40, 60])
def test_scales_to_larger_graphs(port, oracle, n_nodes):
    """
    Larger graphs at the default tier's easiest density. Not an accuracy claim
    beyond a loose floor -- it exists to catch blowups and pathological output
    (e.g. an empty or near-complete graph) at sizes the default tier skips.
    """
    cfg = Config(n_nodes, 2, 5000, 0)
    for algo in ALGOS:
        if algo in PAG_ALGOS and oracle is None:
            continue
        m = run_cell(port, oracle, algo, cfg, seed=0)
        assert m is not None
        assert m["skeleton_f1"] >= 0.70, f"{algo} at p={n_nodes}: F1 {m['skeleton_f1']}"
        assert 0 < m["est_edge_count"] < n_nodes * (n_nodes - 1) / 4
