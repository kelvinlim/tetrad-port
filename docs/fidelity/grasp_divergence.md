# Why GRaSP does not match Java, and why it may not be fixable

Status: root cause identified. One contributing bug fixed; the residual is a
one-ULP floating-point difference amplified by GRaSP's control flow.

## What was ruled out

Earlier investigations suspected iteration order and named two candidates:
tie-breaking inside `GrowShrinkTree` grow/shrink, and edge insertion order in
`TeyssierScorer::getGraph`. Both are ruled out by direct measurement.

Scoring eight independently generated tier-valid permutations of the Boston
lagged data on both sides gave **identical edge counts on every one** (30/30,
31/31, 31/31, 30/30, 30/30, 26/26, 28/28, 29/29). Identical scores and identical
edge counts for a *given* permutation mean `GrowShrinkTree` finds the same parent
sets and `getGraph` builds the same graph. The divergence is in which permutation
the search arrives at, not in what it does with one.

`bestOrder`, the `grasp()` driver loop, `makeValidKnowledgeOrder`, and the body of
`graspDfs` were each compared against `Grasp.java` and match structurally,
including the `do/while` with the `first` flag and the `Z` snapshot taken before
`moveTo`.

## Contributing bug (fixed): the score was not Java's

`SemBicScore::localScore` computed the full Gaussian log-likelihood and doubled it:

```
    lik   = -0.5 * n * (log(2*pi*sigma^2) + 1)
    score = 2*lik - c*k*logN - structurePrior
```

Java computes (`SemBicScore.java:344-372`):

```
    lik   = -(n / 2.0) * log(varRy)
    score = lik - c*(k/2.0)*logN - structurePrior
```

so `cpp = 2*java - n*(log(2*pi) + 1)` per variable. Measured across eight
permutations, `cpp = 2*java - 25427.3785` held to 8e-7 — a positive affine
transform.

That transform preserves every score *comparison*, which is why FGES, BOSS and
GFCI matched Java exactly despite it. GRaSP is the exception: `graspDfs` branches
on **exact floating-point equality**.

```cpp
double sNew = scorer.score();
if (sNew > sOld) { ...accept, return... }
if (sNew == sOld && currentDepth < depth[0]) { ...recurse one level deeper... }
```

Scaling and offsetting the score moves near-ties across that knife-edge. Now
fixed: the C++ score reproduces Java's value exactly (`-6787.150588` on both
sides for the same permutation).

## Residual: one ULP

With the score aligned, the two searches take **identical tucks with identical
score improvements** — matching to six decimals — and then diverge at step 3:

```
java  1 {TST_lag,TIB_lag}      0.047187
cpp   1 {TIB_lag,TST_lag}      0.0471872     same
java  2 {ws_lag,NA_lag}        1.415583
cpp   2 {NA_lag,ws_lag}        1.41558       same
java  3 {ws_lag,ab_lag}        6.277528
cpp   3 {TST_lag,PANAS_PA_lag} 9.09495e-13   <-- extra tuck, not in Java
cpp   4 {ab_lag,ws_lag}        6.27753
```

C++ accepts a tuck whose "improvement" is 9.09495e-13. The running score is about
6787, and one ULP at that magnitude is ~9.09e-13. **The improvement is exactly one
unit in the last place.** Java computes the same tuck as an exact tie and takes
the `sNew == sOld` branch, recursing instead of accepting.

So the residual is not a logic error. It is floating-point non-associativity —
different summation order in `score()`, and Eigen versus Apache Commons Math in
the residual-variance computation — amplified by an exact-equality branch into a
completely different search trajectory.

## Consequences

- C++ GRaSP terminates at a local optimum its own scorer rates **1.129 worse**
  than Java's answer. It is not finding a different-but-equal solution; it is
  stuck somewhere it can itself recognise as worse.
- Matching Java would require bit-identical linear algebra with Apache Commons
  Math. That is not realistically achievable with Eigen.
- GRaSP results are therefore *chaotically* sensitive to arithmetic. Small,
  legitimate changes elsewhere in the port will keep shuffling GRaSP and
  GRaSP-FCI numbers by a couple of percent in either direction. Treat movement in
  those cells as noise unless it is large or accompanied by a traced cause.

## The open decision

`graspDfs` could compare with a tolerance (`fabs(sNew - sOld) < eps` treated as
equal) instead of exact equality. That would very likely converge the search onto
Java's trajectory, because it restores the tie that Java sees.

It is a deliberate deviation from Java's literal code, which this port otherwise
avoids — so it is recorded here rather than applied. Note the deviation is
arguably *closer to Java's intent*: Java's `==` is comparing two sums that are
mathematically equal and differ only by accumulation error, and the tolerance is
what a numerically careful author would have written.

## Reproducing

The probe used here builds against `libtetrad_cpp.a`, runs GRaSP with
`setVerbose(true)` to emit the accepted-tuck trace, and can score an arbitrary
permutation for comparison. The Java side needs only `grasp.setVerbose(True)` via
jpype — `Grasp.java:505-508` already prints each accepted tuck, and the C++ side
now prints in the same format so the two traces diff directly.
