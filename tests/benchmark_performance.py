"""
Performance benchmark: C++ (tetrad_port) vs Java (Tetrad 7.6.8) on fMRI dataset.

Benchmarks all 7 algorithms at appropriate dataset sizes:
- Full 379 variables: PC, FGES (scalable algorithms)
- 50-variable subset: BOSS, GRaSP, GFCI, BOSS-FCI, GRaSP-FCI (expensive algorithms)

Requires:
  - tetrad_port installed (pip install -e ".[dev]")
  - jpype1 installed
  - Tetrad JAR at jars/tetrad-gui-7.6.8-launch.jar
  - Java 21+ on PATH

Usage:
    python tests/benchmark_performance.py              # run all
    python tests/benchmark_performance.py fges pc      # run specific algorithms
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import numpy as np
import pandas as pd

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

FMRI_CSV = Path(__file__).parent.parent / "fastcausal" / "fastcausal" / "data" / "fmri_48451_ptseries.csv"
FMRI_CSV_ALT = Path("/home/kolim/Projects/fastcda/fastcda/data/fmri_48451_ptseries.csv")

# Algorithms that scale to full dataset (379 vars)
FULL_ALGOS = ["pc", "fges"]
# Algorithms that need a reduced dataset
SUBSET_ALGOS = ["boss", "grasp", "gfci", "boss_fci", "grasp_fci"]
ALL_ALGOS = FULL_ALGOS + SUBSET_ALGOS

SUBSET_NVARS = 50  # number of variables for expensive algorithms
N_WARMUP = 1
N_RUNS = 3
PENALTY_DISCOUNT = 1.0
ALPHA = 0.01

ALGO_KWARGS = {
    "pc":        {"alpha": ALPHA},
    "fges":      {"penalty_discount": PENALTY_DISCOUNT},
    "gfci":      {"alpha": ALPHA, "penalty_discount": PENALTY_DISCOUNT},
    "boss":      {"penalty_discount": PENALTY_DISCOUNT},
    "boss_fci":  {"alpha": ALPHA, "penalty_discount": PENALTY_DISCOUNT},
    "grasp":     {"penalty_discount": PENALTY_DISCOUNT},
    "grasp_fci": {"alpha": ALPHA, "penalty_discount": PENALTY_DISCOUNT},
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def load_fmri() -> pd.DataFrame:
    for p in [FMRI_CSV, FMRI_CSV_ALT]:
        if p.exists():
            df = pd.read_csv(p)
            print(f"Loaded fMRI data: {df.shape[0]} obs x {df.shape[1]} vars from {p}")
            return df
    raise FileNotFoundError(f"fMRI CSV not found at {FMRI_CSV} or {FMRI_CSV_ALT}")


def bench_cpp(algo: str, df: pd.DataFrame, n_warmup: int, n_runs: int) -> dict:
    from tetrad_port import TetradPort
    tp = TetradPort()
    kwargs = ALGO_KWARGS.get(algo, {})

    for _ in range(n_warmup):
        tp.run(df, algorithm=algo, **kwargs)

    times = []
    n_edges = 0
    for _ in range(n_runs):
        t0 = time.perf_counter()
        results, graph_info = tp.run(df, algorithm=algo, **kwargs)
        t1 = time.perf_counter()
        times.append(t1 - t0)
        n_edges = len(results["edges"])

    return {
        "times": times,
        "mean": np.mean(times),
        "std": np.std(times),
        "min": np.min(times),
        "n_edges": n_edges,
    }


def bench_java(algo: str, df: pd.DataFrame, n_warmup: int, n_runs: int) -> dict:
    sys.path.insert(0, str(Path(__file__).parent))
    from java_oracle import TetradOracle
    oracle = TetradOracle(jvm_args="-Xmx8g -Djava.util.Arrays.useLegacyMergeSort=true")
    kwargs = ALGO_KWARGS.get(algo, {})

    for _ in range(n_warmup):
        oracle.run(algo, df, **kwargs)

    times = []
    n_edges = 0
    for _ in range(n_runs):
        t0 = time.perf_counter()
        edges = oracle.run(algo, df, **kwargs)
        t1 = time.perf_counter()
        times.append(t1 - t0)
        n_edges = len(edges)

    return {
        "times": times,
        "mean": np.mean(times),
        "std": np.std(times),
        "min": np.min(times),
        "n_edges": n_edges,
    }


def format_time(seconds: float) -> str:
    if seconds < 1.0:
        return f"{seconds*1000:.1f}ms"
    elif seconds < 60:
        return f"{seconds:.2f}s"
    else:
        m, s = divmod(seconds, 60)
        return f"{int(m)}m {s:.1f}s"


def run_benchmark(algo: str, df: pd.DataFrame) -> dict:
    """Run both C++ and Java benchmarks for one algorithm."""
    result = {}

    print(f"  C++ ({N_WARMUP} warmup + {N_RUNS} runs)...")
    try:
        cpp_result = bench_cpp(algo, df, N_WARMUP, N_RUNS)
        result["cpp"] = cpp_result
        print(f"    Mean: {format_time(cpp_result['mean'])} +/- {format_time(cpp_result['std'])}")
        print(f"    Min:  {format_time(cpp_result['min'])}")
        print(f"    Edges: {cpp_result['n_edges']}")
    except Exception as e:
        print(f"    FAILED: {e}")

    print(f"  Java ({N_WARMUP} warmup + {N_RUNS} runs)...")
    try:
        java_result = bench_java(algo, df, N_WARMUP, N_RUNS)
        result["java"] = java_result
        print(f"    Mean: {format_time(java_result['mean'])} +/- {format_time(java_result['std'])}")
        print(f"    Min:  {format_time(java_result['min'])}")
        print(f"    Edges: {java_result['n_edges']}")
    except Exception as e:
        print(f"    FAILED: {e}")

    if "cpp" in result and "java" in result:
        speedup = result["java"]["mean"] / result["cpp"]["mean"] if result["cpp"]["mean"] > 0 else float("inf")
        print(f"  Speedup: {speedup:.1f}x")

    return result


def generate_report(full_results: dict, subset_results: dict,
                    full_shape: tuple, subset_shape: tuple) -> str:
    lines = []
    lines.append("# Performance Benchmark: C++ vs Java (Tetrad 7.6.8)")
    lines.append("")
    lines.append("## Dataset")
    lines.append("")
    lines.append("- **Source**: HCP fMRI parcellated time series (subject 48451)")
    lines.append(f"- **Full dataset**: {full_shape[0]} observations x {full_shape[1]} variables")
    lines.append(f"- **Subset**: {subset_shape[0]} observations x {subset_shape[1]} variables (first {subset_shape[1]} ROIs)")
    lines.append(f"- **Parameters**: penalty_discount={PENALTY_DISCOUNT}, alpha={ALPHA}")
    lines.append(f"- **Timing**: Best of {N_RUNS} runs after {N_WARMUP} warmup; mean +/- std also shown")
    lines.append("")

    # Full dataset results
    lines.append(f"## Full Dataset ({full_shape[0]} x {full_shape[1]})")
    lines.append("")
    lines.append("| Algorithm | C++ Time | Java Time | Speedup | C++ Edges | Java Edges |")
    lines.append("|-----------|----------|-----------|---------|-----------|------------|")

    for algo in FULL_ALGOS:
        if algo not in full_results:
            continue
        r = full_results[algo]
        _append_row(lines, algo, r)

    lines.append("")

    # Subset results
    lines.append(f"## All Algorithms on Subset ({subset_shape[0]} x {subset_shape[1]})")
    lines.append("")
    lines.append("| Algorithm | C++ Time | Java Time | Speedup | C++ Edges | Java Edges |")
    lines.append("|-----------|----------|-----------|---------|-----------|------------|")

    for algo in ALL_ALGOS:
        if algo not in subset_results:
            continue
        r = subset_results[algo]
        _append_row(lines, algo, r)

    lines.append("")
    lines.append("## Analysis")
    lines.append("")

    # Summarize subset results (apples-to-apples comparison at 50 vars)
    subset_speedups = []
    for algo, r in subset_results.items():
        cpp = r.get("cpp")
        java = r.get("java")
        if cpp and java and cpp["mean"] > 0:
            subset_speedups.append((algo, java["mean"] / cpp["mean"]))

    if subset_speedups:
        lines.append("**50-variable comparison (apples-to-apples):**")
        lines.append("")
        faster = [(a, s) for a, s in subset_speedups if s > 1.0]
        slower = [(a, s) for a, s in subset_speedups if s <= 1.0]
        if faster:
            lines.append(f"- **C++ faster**: {', '.join(f'{a.upper().replace(chr(95), chr(45))} ({s:.1f}x)' for a, s in faster)}")
        if slower:
            lines.append(f"- **Java faster**: {', '.join(f'{a.upper().replace(chr(95), chr(45))} ({1/s:.1f}x)' for a, s in slower)}")

    # Full dataset note
    full_speedups = []
    for algo, r in full_results.items():
        cpp = r.get("cpp")
        java = r.get("java")
        if cpp and java and cpp["mean"] > 0:
            full_speedups.append((algo, java["mean"] / cpp["mean"]))

    if full_speedups:
        lines.append("")
        lines.append("**379-variable scaling:**")
        lines.append("")
        for algo, s in full_speedups:
            name = algo.upper().replace("_", "-")
            if s >= 1.0:
                lines.append(f"- {name}: C++ {s:.1f}x faster")
            else:
                lines.append(f"- {name}: Java {1/s:.1f}x faster (C++ scales worse at high p)")

    lines.append("")
    lines.append("## Notes")
    lines.append("")
    lines.append("- C++ implementation uses tetrad_port (nanobind Python bindings to C++)")
    lines.append("- Java implementation uses Tetrad 7.6.8 JAR via jpype")
    lines.append("- Speedup = Java time / C++ time (>1 means C++ is faster)")
    lines.append("- BOSS, GRaSP, and FCI variants use a 50-variable subset because permutation-based")
    lines.append("  and latent-variable algorithms have super-linear complexity in the number of variables")
    lines.append("- Both implementations use SEM BIC score for score-based algorithms and Fisher Z for constraint-based components")
    lines.append("- Edge counts may differ slightly due to non-determinism (BOSS, GRaSP) or minor implementation differences")
    lines.append("")

    return "\n".join(lines)


def _append_row(lines: list, algo: str, r: dict):
    cpp = r.get("cpp")
    java = r.get("java")
    name = algo.upper().replace("_", "-")

    if cpp and java:
        speedup = java["mean"] / cpp["mean"] if cpp["mean"] > 0 else float("inf")
        lines.append(
            f"| {name} "
            f"| {format_time(cpp['mean'])} +/- {format_time(cpp['std'])} "
            f"| {format_time(java['mean'])} +/- {format_time(java['std'])} "
            f"| **{speedup:.1f}x** "
            f"| {cpp['n_edges']} "
            f"| {java['n_edges']} |"
        )
    elif cpp:
        lines.append(
            f"| {name} "
            f"| {format_time(cpp['mean'])} +/- {format_time(cpp['std'])} "
            f"| (failed) | - "
            f"| {cpp['n_edges']} | - |"
        )
    elif java:
        lines.append(
            f"| {name} "
            f"| (failed) "
            f"| {format_time(java['mean'])} +/- {format_time(java['std'])} "
            f"| - | - "
            f"| {java['n_edges']} |"
        )


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    algos_to_run = ALL_ALGOS
    if len(sys.argv) > 1:
        algos_to_run = [a.lower().replace("-", "_") for a in sys.argv[1:]]

    df_full = load_fmri()
    df_subset = df_full.iloc[:, :SUBSET_NVARS].copy()
    print(f"Subset: {df_subset.shape[0]} obs x {df_subset.shape[1]} vars")

    full_results = {}
    subset_results = {}

    for algo in algos_to_run:
        print(f"\n{'='*60}")
        print(f"Benchmarking {algo.upper().replace('_', '-')}")

        if algo in FULL_ALGOS:
            print(f"  (full dataset: {df_full.shape[1]} vars)")
            print(f"{'='*60}")
            full_results[algo] = run_benchmark(algo, df_full)

        # All algorithms also run on subset for fair comparison
        print(f"\n  --- Also on subset ({df_subset.shape[1]} vars) ---")
        subset_results[algo] = run_benchmark(algo, df_subset)

    report = generate_report(full_results, subset_results, df_full.shape, df_subset.shape)
    out_path = Path(__file__).parent.parent / "PerformanceBenchmark.md"
    out_path.write_text(report)
    print(f"\nReport written to {out_path}")
    print(report)


if __name__ == "__main__":
    main()
