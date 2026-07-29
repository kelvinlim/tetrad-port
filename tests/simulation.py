"""
Simulation harness: random DAGs of varying size and complexity, linear-Gaussian
data from them, and the ground-truth object each algorithm class is actually
trying to recover.

Why this exists
---------------
``tests/scoreboard.py`` scores C++ against the Java oracle, which answers "did
we port Tetrad faithfully?" but not "is the algorithm right?". Its ground-truth
metrics compare estimates to the raw weighted adjacency ``B``, which penalises
correct output: no algorithm here returns a DAG. PC, FGES, BOSS and GRaSP
return the CPDAG of the true DAG's Markov equivalence class, and the FCI
variants return a PAG over the observed margin. Comparing a CPDAG to a DAG
counts every undirected edge in a correct answer as an orientation error.

So this module supplies:

* ``simulate_dag`` / ``simulate_data`` -- random graphs parameterised by size,
  average degree, sample size and number of latent confounders, so accuracy can
  be traced against each axis rather than measured at one arbitrary point.
* ``dag_to_cpdag`` -- a self-contained DAG -> CPDAG conversion (v-structures
  plus Meek's rules), cross-checked against Tetrad's ``GraphTransforms`` in
  ``test_simulation.py``.
* ``true_graph`` -- the CPDAG or the PAG, whichever the algorithm targets; PAGs
  come from Tetrad's ``DagToPag`` via ``java_oracle`` since deriving them
  requires inducing-path reasoning this port does not implement.
* ``structural_metrics`` -- skeleton precision/recall/F1/SHD plus *endpoint*
  metrics (arrowhead and tail precision/recall, endpoint SHD) computed against
  that target, which ``scoreboard.arrowhead_precision`` cannot do because it
  only knows ``B``.

Edge parsing and adjacency helpers are imported from ``scoreboard`` rather than
redefined -- that module is the single source of truth for edge-string handling.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import pandas as pd

REPO = Path(__file__).resolve().parent.parent
if str(REPO / "tests") not in sys.path:
    sys.path.insert(0, str(REPO / "tests"))

from scoreboard import adjacency, split_edge  # noqa: E402

# Algorithms whose output is a CPDAG vs. a PAG. This decides which ground-truth
# object a result is scored against.
CPDAG_ALGOS = ("pc", "fges", "boss", "grasp")
PAG_ALGOS = ("gfci", "boss_fci", "grasp_fci")

TAIL, ARROW, CIRCLE = "-", ">", "o"


# ---------------------------------------------------------------------------
# Random graphs and data
# ---------------------------------------------------------------------------

def simulate_dag(n_nodes: int, avg_degree: float, seed: int) -> np.ndarray:
    """
    Random DAG as a weighted adjacency matrix; ``b[i, j] != 0`` means ``i -> j``.

    Edges are drawn independently in the upper triangle of a random topological
    order, so every draw is acyclic by construction. ``avg_degree`` is the
    expected number of edges incident to a node, converted to the per-pair
    probability ``avg_degree / (n_nodes - 1)``; weights are drawn from
    +/- U(0.5, 1.5), matching the scheme already used by ``scoreboard.make_dag``.
    """
    if n_nodes < 2:
        raise ValueError("n_nodes must be at least 2")
    rng = np.random.default_rng(seed)
    p_edge = min(1.0, avg_degree / (n_nodes - 1))

    b = np.zeros((n_nodes, n_nodes))
    for i in range(n_nodes):
        for j in range(i + 1, n_nodes):
            if rng.random() < p_edge:
                b[i, j] = rng.uniform(0.5, 1.5) * rng.choice([-1.0, 1.0])

    # Relabel by a random permutation so node index carries no information
    # about causal order -- otherwise an algorithm that happened to favour the
    # data column order would score better than it deserves.
    perm = rng.permutation(n_nodes)
    return b[np.ix_(perm, perm)]


def simulate_data(b: np.ndarray, n_samples: int, seed: int) -> np.ndarray:
    """
    Draw ``n_samples`` rows from the linear-Gaussian SEM ``X_j = sum_i b_ij X_i + e_j``
    with ``e_j ~ N(0, 1)``, respecting the topological order implied by ``b``.
    """
    n = b.shape[0]
    rng = np.random.default_rng(seed)
    order = _topological_order(b)
    data = np.zeros((n_samples, n))
    for j in order:
        data[:, j] = data @ b[:, j] + rng.normal(size=n_samples)
    return data


def _topological_order(b: np.ndarray) -> list[int]:
    """Kahn's algorithm over ``b``; raises if ``b`` is cyclic."""
    n = b.shape[0]
    in_deg = [int(np.count_nonzero(b[:, j])) for j in range(n)]
    ready = [j for j in range(n) if in_deg[j] == 0]
    order: list[int] = []
    while ready:
        j = ready.pop()
        order.append(j)
        for k in range(n):
            if b[j, k] != 0:
                in_deg[k] -= 1
                if in_deg[k] == 0:
                    ready.append(k)
    if len(order) != n:
        raise ValueError("adjacency matrix is cyclic")
    return order


def choose_latents(b: np.ndarray, n_hidden: int) -> list[int]:
    """
    Pick ``n_hidden`` node indices to hide, preferring genuine confounders.

    Hiding a leaf or a root with one child changes nothing an FCI variant could
    detect, so the latent cells would silently degenerate into the observed
    case. Nodes with at least two children are preferred, most children first;
    ties and any shortfall fall back to remaining nodes by descending
    out-degree, and the choice is deterministic given ``b``.
    """
    if n_hidden <= 0:
        return []
    out_deg = [int(np.count_nonzero(b[i, :])) for i in range(b.shape[0])]
    confounders = sorted((i for i in range(b.shape[0]) if out_deg[i] >= 2),
                         key=lambda i: (-out_deg[i], i))
    rest = sorted((i for i in range(b.shape[0]) if out_deg[i] < 2),
                  key=lambda i: (-out_deg[i], i))
    return (confounders + rest)[:n_hidden]


@dataclass
class Simulation:
    """A simulated dataset together with everything needed to score against it."""

    b: np.ndarray                  # full DAG including latents
    all_names: list[str]           # names for every column of b
    latent_names: list[str]        # subset of all_names that is hidden
    df: pd.DataFrame               # observed columns only
    n_nodes: int
    avg_degree: float
    n_samples: int
    n_hidden: int
    seed: int
    _cache: dict = field(default_factory=dict, repr=False)

    @property
    def observed_names(self) -> list[str]:
        return list(self.df.columns)

    @property
    def key(self) -> str:
        return (f"p{self.n_nodes}_d{self.avg_degree:g}_n{self.n_samples}"
                f"_h{self.n_hidden}_s{self.seed}")

    def true_graph(self, algo: str, oracle=None) -> list[str]:
        """
        Ground truth for ``algo`` as a list of edge strings.

        CPDAG algorithms are scored against the CPDAG of the true DAG; FCI
        variants against its PAG over the observed margin. Latent cells have no
        CPDAG ground truth at all -- marginalising a DAG does not generally
        yield a DAG -- so requesting one raises rather than silently scoring
        against a sub-DAG that omits the confounding.
        """
        kind = "pag" if algo in PAG_ALGOS else "cpdag"
        if kind == "cpdag" and self.n_hidden:
            raise ValueError(
                f"{algo} returns a CPDAG, which is not defined for a latent-variable "
                f"simulation ({self.n_hidden} hidden); score FCI variants here instead"
            )
        if kind in self._cache:
            return self._cache[kind]

        if kind == "cpdag":
            edges = dag_to_cpdag(self.b, self.all_names)
        else:
            if oracle is None:
                raise RuntimeError("PAG ground truth requires the Java oracle")
            edges = oracle.dag_to_pag(self.b, self.all_names, self.latent_names)
        self._cache[kind] = edges
        return edges


def make_simulation(n_nodes: int, avg_degree: float, n_samples: int,
                    n_hidden: int = 0, seed: int = 0) -> Simulation:
    """
    Build a simulation with ``n_nodes`` observed variables.

    ``n_hidden`` extra variables are generated and then dropped, so the observed
    dimension is ``n_nodes`` regardless of how many latents are requested and
    cells stay comparable across the ``n_hidden`` axis.
    """
    total = n_nodes + n_hidden
    b = simulate_dag(total, avg_degree, seed)
    all_names = [f"V{i}" for i in range(total)]
    latent_idx = choose_latents(b, n_hidden)
    latent_names = [all_names[i] for i in latent_idx]

    data = simulate_data(b, n_samples, seed + 1_000_000)
    df = pd.DataFrame(data, columns=all_names)
    observed = [n for n in all_names if n not in set(latent_names)]

    return Simulation(
        b=b, all_names=all_names, latent_names=latent_names,
        df=df[observed], n_nodes=n_nodes, avg_degree=avg_degree,
        n_samples=n_samples, n_hidden=n_hidden, seed=seed,
    )


# ---------------------------------------------------------------------------
# DAG -> CPDAG
# ---------------------------------------------------------------------------
#
# v-structures plus Meek's rules R1-R3, which are what the knowledge-free
# direction needs; R4 only bites when background knowledge has already oriented
# an edge no v-structure implies, and the simulations carry no knowledge. The
# guarantee that this is right is empirical, not asserted: test_simulation.py
# checks the output against Tetrad's own GraphTransforms.cpdagForDag over many
# random DAGs.

def dag_to_cpdag(b: np.ndarray, names: list[str]) -> list[str]:
    """CPDAG of the DAG ``b`` as edge strings (``A --> B`` / ``A --- B``)."""
    n = b.shape[0]
    adj = [[False] * n for _ in range(n)]
    for i in range(n):
        for j in range(n):
            if b[i, j] != 0:
                adj[i][j] = adj[j][i] = True

    directed: set[tuple[int, int]] = set()

    # v-structures: a -> c <- b with a, b non-adjacent
    for c in range(n):
        parents = [i for i in range(n) if b[i, c] != 0]
        for x_pos, a in enumerate(parents):
            for bb in parents[x_pos + 1:]:
                if not adj[a][bb]:
                    directed.add((a, c))
                    directed.add((bb, c))

    def is_undirected(x: int, y: int) -> bool:
        return adj[x][y] and (x, y) not in directed and (y, x) not in directed

    changed = True
    while changed:
        changed = False
        for a in range(n):
            for c in range(n):
                if not is_undirected(a, c):
                    continue
                if _meek_r1(a, c, n, adj, directed) or \
                   _meek_r2(a, c, n, adj, directed) or \
                   _meek_r3(a, c, n, adj, directed, is_undirected):
                    directed.add((a, c))
                    changed = True

    edges = []
    for i in range(n):
        for j in range(i + 1, n):
            if not adj[i][j]:
                continue
            if (i, j) in directed:
                edges.append(f"{names[i]} --> {names[j]}")
            elif (j, i) in directed:
                edges.append(f"{names[j]} --> {names[i]}")
            else:
                edges.append(f"{names[i]} --- {names[j]}")
    return edges


def _meek_r1(a: int, c: int, n: int, adj, directed) -> bool:
    """x -> a, a - c, x not adjacent c  =>  a -> c."""
    return any((x, a) in directed and not adj[x][c] for x in range(n) if x != c)


def _meek_r2(a: int, c: int, n: int, adj, directed) -> bool:
    """a -> x -> c and a - c  =>  a -> c (else the a - c edge would cycle)."""
    return any((a, x) in directed and (x, c) in directed
               for x in range(n) if x not in (a, c))


def _meek_r3(a: int, c: int, n: int, adj, directed, is_undirected) -> bool:
    """a - x -> c, a - y -> c, x and y non-adjacent, a - c  =>  a -> c."""
    kites = [x for x in range(n)
             if x not in (a, c) and is_undirected(a, x) and (x, c) in directed]
    return any(not adj[x][y] for i, x in enumerate(kites) for y in kites[i + 1:])


# ---------------------------------------------------------------------------
# Endpoint-level metrics
# ---------------------------------------------------------------------------

_MARKS = {
    "-->": (TAIL, ARROW),
    "<--": (ARROW, TAIL),
    "---": (TAIL, TAIL),
    "<->": (ARROW, ARROW),
    "o->": (CIRCLE, ARROW),
    "<-o": (ARROW, CIRCLE),
    "o-o": (CIRCLE, CIRCLE),
}


def endpoint_marks(edges: list[str]) -> dict[tuple[str, str], str]:
    """
    Map each ordered adjacent pair ``(x, y)`` to the edge mark *at y*.

    ``A o-> B`` contributes ``(A, B) -> '>'`` and ``(B, A) -> 'o'``. This is the
    representation both PAG and CPDAG comparisons need: a CPDAG's ``A --- B``
    is two tails, and only arrowheads and tails make a claim a circle does not.
    """
    marks: dict[tuple[str, str], str] = {}
    for e in edges:
        parts = split_edge(e)
        if parts is None:
            continue
        a, sep, c = parts
        if sep not in _MARKS:
            continue
        at_a, at_c = _MARKS[sep]
        marks[(c, a)] = at_a
        marks[(a, c)] = at_c
    return marks


def _prf(tp: int, fp: int, fn: int) -> tuple[float | None, float | None, float | None]:
    precision = tp / (tp + fp) if (tp + fp) else None
    recall = tp / (tp + fn) if (tp + fn) else None
    if precision is None or recall is None or (precision + recall) == 0:
        f1 = 0.0 if (precision is not None and recall is not None) else None
    else:
        f1 = 2 * precision * recall / (precision + recall)
    return precision, recall, f1


def structural_metrics(est_edges: list[str], true_edges: list[str]) -> dict:
    """
    Score an estimated graph against a ground-truth CPDAG or PAG.

    Skeleton metrics ignore orientation. Endpoint metrics compare the mark at
    each ordered adjacency: an arrowhead is scored correct only when the truth
    also has an arrowhead there, so an algorithm cannot inflate its score by
    orienting everything. ``endpoint_shd`` is the standard structural Hamming
    distance over endpoints -- an edge present in one graph and absent in the
    other costs 2, and a present-but-mismarked endpoint costs 1.
    """
    est_adj, true_adj = adjacency(est_edges), adjacency(true_edges)
    tp = len(est_adj & true_adj)
    fp = len(est_adj - true_adj)
    fn = len(true_adj - est_adj)
    s_prec, s_rec, s_f1 = _prf(tp, fp, fn)

    est_marks, true_marks = endpoint_marks(est_edges), endpoint_marks(true_edges)

    def mark_counts(mark: str) -> tuple[int, int, int]:
        est_set = {k for k, v in est_marks.items() if v == mark}
        true_set = {k for k, v in true_marks.items() if v == mark}
        return (len(est_set & true_set), len(est_set - true_set),
                len(true_set - est_set))

    a_prec, a_rec, a_f1 = _prf(*mark_counts(ARROW))
    t_prec, t_rec, _ = _prf(*mark_counts(TAIL))

    endpoint_shd = 0
    for pair in est_adj | true_adj:
        x, y = sorted(pair)
        for ordered in ((x, y), (y, x)):
            if est_marks.get(ordered) != true_marks.get(ordered):
                endpoint_shd += 1

    def r(v):
        return round(v, 4) if isinstance(v, float) else v

    return {
        "skeleton_precision": r(s_prec),
        "skeleton_recall": r(s_rec),
        "skeleton_f1": r(s_f1),
        "skeleton_shd": fp + fn,
        "arrowhead_precision": r(a_prec),
        "arrowhead_recall": r(a_rec),
        "arrowhead_f1": r(a_f1),
        "tail_precision": r(t_prec),
        "tail_recall": r(t_rec),
        "endpoint_shd": endpoint_shd,
        "true_edge_count": len(true_adj),
        "est_edge_count": len(est_adj),
    }
