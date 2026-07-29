"""
Sweep every algorithm over simulated graphs of differing size and complexity,
score each run against ground truth, and record the aggregate as JSON.

Where ``scoreboard.py`` answers "does the C++ match the Java?", this answers
"does the algorithm recover the truth, and how does that degrade as the problem
gets harder?" -- across four axes: number of variables, average degree, sample
size, and number of latent confounders.

Usage
-----
    python tests/run_simulation_sweep.py --label baseline
    python tests/run_simulation_sweep.py --grid full --label full-sweep
    python tests/run_simulation_sweep.py --compare scoreboard/a.json scoreboard/b.json

Recording needs only the built C++ extension for CPDAG cells. Latent cells
additionally need Java 21+, jpype and the Tetrad JAR, because the PAG ground
truth comes from Tetrad's ``DagToPag``; without them those cells are skipped.
"""

from __future__ import annotations

import argparse
import json
import platform
import statistics
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
if str(REPO / "tests") not in sys.path:
    sys.path.insert(0, str(REPO / "tests"))

from scoreboard import run_cpp  # noqa: E402
from simulation import (CPDAG_ALGOS, PAG_ALGOS, make_simulation,  # noqa: E402
                        structural_metrics)

ALGOS = list(CPDAG_ALGOS) + list(PAG_ALGOS)

# Metrics aggregated across seeds and reported per cell. Any metric a run
# leaves as None (undefined, e.g. arrowhead precision when nothing was
# oriented) is dropped from that cell's sample rather than counted as zero.
REPORTED = ("skeleton_precision", "skeleton_recall", "skeleton_f1",
            "skeleton_shd", "arrowhead_precision", "arrowhead_recall",
            "arrowhead_f1", "tail_precision", "tail_recall", "endpoint_shd",
            "true_edge_count", "est_edge_count")


@dataclass(frozen=True)
class Config:
    n_nodes: int
    avg_degree: float
    n_samples: int
    n_hidden: int

    @property
    def key(self) -> str:
        return (f"p{self.n_nodes}_d{self.avg_degree:g}"
                f"_n{self.n_samples}_h{self.n_hidden}")


def _grid(nodes, degrees, samples, hiddens) -> list[Config]:
    return [Config(p, d, n, h)
            for p in nodes for d in degrees for n in samples for h in hiddens]


# Default: enough spread on every axis to see a trend, small enough to run in
# a normal test session. Full: adds larger and denser graphs plus a wider
# sample-size range, for release checks and regression baselines.
DEFAULT_GRID = _grid((5, 10, 20), (2, 4), (500, 2000, 10000), (0, 2))
FULL_GRID = _grid((5, 10, 20, 40), (2, 4, 6), (200, 500, 2000, 10000, 50000), (0, 2, 4))

DEFAULT_SEEDS = (0, 1, 2, 3, 4)
FULL_SEEDS = tuple(range(10))


def run_cell(port, oracle, algo: str, cfg: Config, seed: int) -> dict | None:
    """
    Run one (algorithm, configuration, seed) and score it against ground truth.

    Returns None when the cell is not applicable: CPDAG algorithms have no
    ground truth once variables are hidden (marginalising a DAG need not give a
    DAG), and PAG ground truth needs the Java oracle.
    """
    if cfg.n_hidden and algo in CPDAG_ALGOS:
        return None
    if algo in PAG_ALGOS and oracle is None:
        return None

    sim = make_simulation(cfg.n_nodes, cfg.avg_degree, cfg.n_samples,
                          cfg.n_hidden, seed)
    est = run_cpp(port, algo, sim.df, None)
    truth = sim.true_graph(algo, oracle)
    return structural_metrics(est, truth)


def run_grid(port, oracle, grid: list[Config], seeds, algos: list[str],
             progress=None) -> dict:
    """
    Run the whole grid and aggregate across seeds.

    Returns ``{"algo/config_key": {metric: {mean, sd, n, values}}}``. Keeping
    the raw per-seed values makes it possible to tell a genuinely worse
    algorithm from a noisy one when diffing two recordings.
    """
    results: dict[str, dict] = {}
    for algo in algos:
        for cfg in grid:
            runs = []
            for seed in seeds:
                try:
                    m = run_cell(port, oracle, algo, cfg, seed)
                except Exception as e:  # a crash is a result worth recording
                    results[f"{algo}/{cfg.key}"] = {
                        "error": f"{type(e).__name__}: {e}"}
                    runs = []
                    break
                if m is not None:
                    runs.append(m)
            if not runs:
                continue
            cell = {}
            for metric in REPORTED:
                vals = [r[metric] for r in runs if r.get(metric) is not None]
                if not vals:
                    continue
                cell[metric] = {
                    "mean": round(statistics.fmean(vals), 4),
                    "sd": round(statistics.stdev(vals), 4) if len(vals) > 1 else 0.0,
                    "n": len(vals),
                    "values": [round(v, 4) if isinstance(v, float) else v
                               for v in vals],
                }
            key = f"{algo}/{cfg.key}"
            results[key] = cell
            if progress:
                progress(key, cell)
    return results


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def to_markdown(payload: dict) -> str:
    """Readable per-algorithm tables; the JSON stays the machine-readable form."""
    meta = payload["metadata"]
    lines = [
        f"# Simulation sweep -- {meta['label']}",
        "",
        f"- Recorded: {meta['timestamp']}",
        f"- Platform: {meta['platform']}, Python {meta['python']}, "
        f"tetrad-port {meta['tetrad_port_version']}",
        f"- alpha={meta['alpha']}, penalty_discount={meta['penalty_discount']}, "
        f"seeds={meta['seeds']}",
        "",
        "Each row is the mean over seeds. CPDAG algorithms (pc, fges, boss, grasp) "
        "are scored against the true CPDAG; FCI variants against the true PAG over "
        "the observed margin. `p` = variables, `d` = average degree, `n` = sample "
        "size, `h` = latent confounders.",
        "",
    ]
    by_algo: dict[str, list[tuple[str, dict]]] = {}
    for key, cell in payload["results"].items():
        algo, cfg = key.split("/", 1)
        by_algo.setdefault(algo, []).append((cfg, cell))

    for algo in sorted(by_algo):
        lines += [f"## {algo}", "",
                  "| config | skel P | skel R | skel F1 | arrow P | arrow R | "
                  "endpoint SHD | edges (est/true) |",
                  "|---|---|---|---|---|---|---|---|"]

        def fmt(cell, metric, prec=3):
            v = cell.get(metric)
            return "--" if v is None else f"{v['mean']:.{prec}f}"

        for cfg, cell in sorted(by_algo[algo]):
            if "error" in cell:
                lines.append(f"| {cfg} | ERROR: {cell['error'][:60]} |||||||")
                continue
            lines.append(
                f"| {cfg} | {fmt(cell, 'skeleton_precision')} | "
                f"{fmt(cell, 'skeleton_recall')} | {fmt(cell, 'skeleton_f1')} | "
                f"{fmt(cell, 'arrowhead_precision')} | "
                f"{fmt(cell, 'arrowhead_recall')} | "
                f"{fmt(cell, 'endpoint_shd', 1)} | "
                f"{fmt(cell, 'est_edge_count', 1)} / "
                f"{fmt(cell, 'true_edge_count', 1)} |")
        lines.append("")
    return "\n".join(lines)


def compare(path_a: Path, path_b: Path, metric: str = "skeleton_f1",
            tol: float = 0.02) -> int:
    """
    Diff two recordings on one metric. Exit 1 if any cell regressed beyond ``tol``.

    The tolerance is not decoration: BOSS and GRaSP use randomised restarts, so
    a cell can move by a little without anything having changed in the code.
    """
    a = json.loads(path_a.read_text())
    b = json.loads(path_b.read_text())
    ra, rb = a["results"], b["results"]

    print(f"A: {a['metadata'].get('label')}  ({a['metadata'].get('timestamp')})")
    print(f"B: {b['metadata'].get('label')}  ({b['metadata'].get('timestamp')})")
    print(f"metric: {metric}, tolerance: {tol}\n")
    header = f"{'cell':<34} {'A':>8} {'B':>8} {'delta':>8}  status"
    print(header)
    print("-" * len(header))

    regressions = improvements = 0
    for key in sorted(set(ra) | set(rb)):
        ca, cb = ra.get(key), rb.get(key)
        if ca is None or cb is None or metric not in (ca or {}) or metric not in (cb or {}):
            continue
        va, vb = ca[metric]["mean"], cb[metric]["mean"]
        d = vb - va
        if d < -tol:
            status, regressions = "REGRESSED", regressions + 1
        elif d > tol:
            status, improvements = "improved", improvements + 1
        else:
            status = "same"
        if status != "same":
            print(f"{key:<34} {va:>8.3f} {vb:>8.3f} {d:>+8.3f}  {status}")

    print(f"\n{improvements} improved, {regressions} regressed "
          f"(cells within +/-{tol} not shown)")
    return 1 if regressions else 0


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--label", default="simulation",
                    help="label stored in the JSON metadata and used for the filename")
    ap.add_argument("--out", type=Path,
                    help="output JSON path (default scoreboard/simulation-<label>.json)")
    ap.add_argument("--grid", choices=("default", "full"), default="default")
    ap.add_argument("--seeds", type=int, help="number of seeds (default 5 / 10 for full)")
    ap.add_argument("--algo", action="append", choices=ALGOS,
                    help="restrict to these algorithms (repeatable)")
    ap.add_argument("--no-markdown", action="store_true")
    ap.add_argument("--compare", nargs=2, type=Path, metavar=("A", "B"))
    ap.add_argument("--metric", default="skeleton_f1", help="metric for --compare")
    ap.add_argument("--tolerance", type=float, default=0.02, help="for --compare")
    args = ap.parse_args()

    if args.compare:
        return compare(args.compare[0], args.compare[1], args.metric, args.tolerance)

    import tetrad_port

    grid = FULL_GRID if args.grid == "full" else DEFAULT_GRID
    seeds = tuple(range(args.seeds)) if args.seeds else (
        FULL_SEEDS if args.grid == "full" else DEFAULT_SEEDS)
    algos = args.algo or ALGOS

    port = tetrad_port.TetradPort()
    try:
        from java_oracle import TetradOracle
        oracle = TetradOracle()
    except Exception as e:
        oracle = None
        print(f"Java oracle unavailable ({type(e).__name__}); "
              f"skipping latent-variable cells and all FCI variants.", file=sys.stderr)

    def progress(key, cell):
        f1 = cell.get("skeleton_f1", {}).get("mean")
        ap_ = cell.get("arrowhead_precision", {}).get("mean")
        print(f"  {key:<34} skelF1={f1 if f1 is None else f'{f1:.3f}'} "
              f"arrowP={ap_ if ap_ is None else f'{ap_:.3f}'}", file=sys.stderr)

    results = run_grid(port, oracle, grid, seeds, algos, progress)

    import scoreboard
    payload = {
        "metadata": {
            "label": args.label,
            "grid": args.grid,
            "seeds": list(seeds),
            "algorithms": algos,
            "configs": [c.key for c in grid],
            "platform": platform.platform(),
            "python": platform.python_version(),
            "tetrad_port_version": getattr(tetrad_port, "__version__", "unknown"),
            "alpha": scoreboard.ALPHA,
            "penalty_discount": scoreboard.PENALTY_DISCOUNT,
            "pag_ground_truth": "tetrad DagToPag" if oracle else "unavailable",
            "timestamp": datetime.now(timezone.utc).isoformat(),
        },
        "results": results,
    }

    out = args.out or REPO / "scoreboard" / f"simulation-{args.label}.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2, sort_keys=True))
    print(f"\nWrote {out}", file=sys.stderr)

    if not args.no_markdown:
        md = out.with_suffix(".md")
        md.write_text(to_markdown(payload))
        print(f"Wrote {md}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
