# Performance Benchmark: C++ vs Java (Tetrad 7.6.3)

## Dataset

- **Source**: HCP fMRI parcellated time series (subject 48451)
- **Full dataset**: 520 observations x 379 variables
- **Subset**: 520 observations x 50 variables (first 50 ROIs)
- **Parameters**: penalty_discount=1.0, alpha=0.01
- **Timing**: Best of 3 runs after 1 warmup; mean +/- std also shown

## Full Dataset (520 x 379)

| Algorithm | C++ Time | Java Time | Speedup | C++ Edges | Java Edges |
|-----------|----------|-----------|---------|-----------|------------|
| PC | 10.42s +/- 64.7ms | 5.37s +/- 217.1ms | **0.5x** | 799 | 799 |
| FGES | 2m 39.3s +/- 252.8ms | 35.74s +/- 684.1ms | **0.2x** | 1255 | 1253 |

## All Algorithms on Subset (520 x 50)

| Algorithm | C++ Time | Java Time | Speedup | C++ Edges | Java Edges |
|-----------|----------|-----------|---------|-----------|------------|
| PC | 27.3ms +/- 0.1ms | 42.1ms +/- 8.1ms | **1.5x** | 93 | 93 |
| FGES | 230.6ms +/- 0.8ms | 175.2ms +/- 3.4ms | **0.8x** | 155 | 155 |
| BOSS | 121.8ms +/- 1.1ms | 288.9ms +/- 1.9ms | **2.4x** | 269 | 269 |
| GRASP | 1.51s +/- 138.8ms | 800.4ms +/- 4.9ms | **0.5x** | 259 | 259 |
| GFCI | 243.3ms +/- 0.2ms | 516.4ms +/- 5.5ms | **2.1x** | 91 | 80 |
| BOSS-FCI | 306.5ms +/- 0.5ms | 434.8ms +/- 18.1ms | **1.4x** | 88 | 86 |
| GRASP-FCI | 2.17s +/- 165.6ms | 1.10s +/- 76.9ms | **0.5x** | 87 | 91 |

## Analysis

**50-variable comparison (apples-to-apples):**

- **C++ faster**: PC (1.5x), BOSS (2.4x), GFCI (2.1x), BOSS-FCI (1.4x)
- **Java faster**: FGES (1.3x), GRASP (1.9x), GRASP-FCI (2.0x)

**379-variable scaling:**

- PC: Java 1.9x faster (C++ scales worse at high p)
- FGES: Java 4.5x faster (C++ scales worse at high p)

## Notes

- C++ implementation uses tetrad_port (nanobind Python bindings to C++)
- Java implementation uses Tetrad 7.6.3 JAR via jpype
- Speedup = Java time / C++ time (>1 means C++ is faster)
- BOSS, GRaSP, and FCI variants use a 50-variable subset because permutation-based
  and latent-variable algorithms have super-linear complexity in the number of variables
- Both implementations use SEM BIC score for score-based algorithms and Fisher Z for constraint-based components
- Edge counts may differ slightly due to non-determinism (BOSS, GRaSP) or minor implementation differences
