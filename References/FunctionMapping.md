# C++ functions mapped to the literature

Every row names a function in `src/` and the exact theorem, lemma, definition,
algorithm or rule it implements. Citation keys are the filenames in `papers/`;
full citations are in `REFERENCES.md`.

**How this was built.** Every anchor below was checked against the text of the
PDF in `papers/`, not recalled. Anchors that could not be confirmed are marked
**NOT FOUND** rather than guessed — most of those are Tetrad-specific machinery
with no paper behind it, but a few are real gaps in the collection (chiefly the
FCI rules; see the FCI section).

**Deviations.** Each section ends with the places where the code knowingly
differs from the paper. These are not bugs to fix on sight: this is a port of
Tetrad 7.6.3, and where Tetrad departs from the literature the port reproduces
Tetrad. The point of recording them is that anyone deriving this code from the
papers will otherwise conclude the port is wrong.

---

## 1. Independence testing

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `IndTestFisherZ::isIndependent` (`ind_test_fisher_z.cpp:42-50`) | raskutti2018 | §3, the display defining `ẑ` and `T_n` | Fisher's z-transform `½ log((1+ρ̂)/(1−ρ̂))`, statistic `√(n−|S|−3)·|ẑ|`, two-sided normal rejection. All three pieces match line for line |
| partial correlation via Cholesky on a correlation submatrix | raskutti2018 | §3, Schur-complement expression for `ρ̂_{j,k|S}` | |

Spirtes (2010) and Colombo & Maathuis (2014) both *use* this test without
stating it; Raskutti & Uhler is the only source here that writes it down.

---

## 2. Meek rules — `src/search/meek_rules.cpp`

Meek presents the rules **only as schematics in Figure 1** ("Orientation rules
for patterns"), not as numbered definitions. The schematics are image content
and are absent from the extracted text; they were read from the rendered PDF
page. Their verbal justifications are in the proof of Theorem 2.

| C++ symbol | Paper | Anchor | Rule content as printed |
|---|---|---|---|
| `MeekRules::meekR1` | meek1995 | Figure 1, **R1**; justification in proof of Thm 2 | `a→b`, `b−c`, `a,c` non-adjacent ⟹ `b→c`. "If the edge were oriented in the opposite direction there would be a new unshielded collider" |
| `MeekRules::meekR2` | meek1995 | Figure 1, **R2** | `a→b→c`, `a−c` ⟹ `a→c`. "…there would be a cycle" |
| `MeekRules::meekR3` | meek1995 | Figure 1, **R3** | `T−L`, `T−R`, `L→B`, `R→B`, `L,R` non-adjacent, `T−B` ⟹ `T→B` |
| `MeekRules::meekR4` | meek1995 | Figure 1, **R4**; **Theorem 4** | `T−L`, `T−R`, `R→B`, `B→L`, `L,R` non-adjacent ⟹ `T→L`. R4 appears **only** in the background-knowledge setting, which is why the C++ gate on non-empty knowledge is correct rather than a deviation |
| `MeekRules::orientImplied` (closure loop) | meek1995 | **Phase II′, step S1** | "Orient every edge which can be oriented by successive applications of rules R1, R2 and R3" |
| soundness of all four | meek1995 | **Theorem 2** | "The four orientation rules given in Figure 1 are sound" |
| completeness without knowledge | meek1995 | **Theorem 3** | R1–R3 alone suffice: "The result of applying rules R1, R2 and R3 to a pattern … is a maximally oriented graph" |
| completeness with knowledge | meek1995 | **Theorem 4** | R1–R4 with respect to knowledge `K` |
| `revertToUnshieldedColliders_` branch | ramsey2015 | §4, §5 | "eliminates all orientations … that do not directly participate in unshielded colliders and then applies the Meek orientation rules" |

### Deviations

- **`meekPreventCycles_`** has no anchor. Theorem 2 proves the rules cannot
  create a cycle on a genuine pattern, so the guard is a no-op on valid input
  and only bites on malformed graphs.

---

## 3. Scoring — `src/search/sem_bic_score.cpp`

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `SemBicScore::localScore` | ramsey2015 | §4 | "BIC2 = 2 L − c k ln n", `c` the penalty discount |
| BIC consistency | chickering2002 | §4.1, Eq. (4); **Lemma 7** | "The Bayesian scoring criterion is locally consistent" |
| | schwarz1978 | whole paper (bibliographic only — scanned, no text layer) | |
| `SemBicScore::localScoreDiff` | chickering2002 | §2.3, Eq. (2) (decomposability) + **Cor. 16/18** | The `s(·)`-difference the GES operators evaluate |
| `SemBicScore::getResidualVariance` | ramsey2015 | §4, §5 | "residual variance after regressing X onto its parents"; "calculating regressions directly from the covariance matrix" |
| `SemBicScore::getLikelihood` | ramsey2015 | §4 | See deviation below — the paper's own statement is inconsistent |
| `getStructurePrior(numParents)` | — | **NOT FOUND** | The formula `−(k ln p + (V−k) ln(1−p))` is in neither paper; it matches Java `SemBicScore.java:581-588`, so it is a Tetrad construction |

### Deviations

- **Penalty discount default.** C++ defaults to `1.0` (matching Java); Ramsey
  §4 says "by default 2" and §8 reports using 4.
- **Degrees of freedom.** C++ uses `k = parents.size()`; Ramsey §4 states
  `k = 2p + 1`. Per added parent the C++ penalty is `c·ln n`, Ramsey's `2c·ln n`.
  Inherited from Java.
- **Factor-of-2 asymmetry with the structure prior.** C++ doubles both the
  likelihood and the penalty relative to Java but not `getStructurePrior(k)`.
  With the default `structurePrior_ = 0` the two agree up to an additive
  constant; with a non-zero structure prior the C++ under-weights the prior by
  a factor of two. Worth a regression test if structure priors are ever exposed
  through the bindings.
- **Ramsey §4's likelihood statement** ("−n ln s + C") is inconsistent with the
  BIC line in the same paragraph, which needs `L = −(n/2) ln s + C`. The code
  uses the latter and is right.

---

## 4. Adjacency search and PC — `src/search/fas.cpp`, `pc.cpp`

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `Fas::search` (depth loop) | colombo2014 | **Algorithm 4.1** (stable); plain variant is **Algorithm 3.2** | |
| `Fas::searchAtDepth` | colombo2014 | **Algorithm 4.1**, pseudocode lines 5–7 | `Graph checkAdj = Graph(graph)` is the frozen `a(X_i)`; batched removal matches "removes these edges only when it goes to the next value of ℓ" |
| `Fas::possibleParents` | colombo2014 | **Algorithm 4.1**, line 11 (`S ⊆ a(X_i) \ {X_j}`) | The knowledge filtering is a Tetrad addition — **NOT FOUND** in the paper |
| `Fas::setStable` | colombo2014 | §4.1; **Theorem 3** (order-independence of the stable skeleton) | See deviation |
| `Pc::collectUnshieldedTriples`, `orientUnshieldedTriples` | ramsey2006 | **step S3** | "For each unshielded triple ⟨A,B,C⟩ in P, orient it as A → B ← C iff B is not in Sepset(A,C)" |
| adjacency-faithfulness | ramsey2006 | **Implication 1** (not a numbered definition) | "if two variables X, Y are adjacent in G, then they are dependent conditional on any subset of V\\{X,Y}" |
| conservative variant (not implemented) | ramsey2006 | **step S3′**; correctness **Theorem 1** | |
| `Pc::applyMeekRules` | colombo2014 | **Algorithm 3.1**, step 3 | Primary anchor is Meek 1995, §2 above |
| PC correctness | colombo2014 | **Theorem 2** | |
| `Pc::canOrientCollider` | — | **NOT FOUND** | Despite the name, the body is only the `PRIORITIZE_EXISTING` conflict guard |
| `Pc::colliderAllowed`, `Pc::pcOrientbk` | — | **NOT FOUND** | Knowledge handling; Tetrad extensions |

### Deviations

- **`setStable(false)` does nothing.** `fas.cpp:37` writes
  `Graph checkAdj = stable_ ? Graph(graph) : graph;`, but `checkAdj` is a
  `Graph` by value and `Graph` has a user-declared copy constructor, so both
  branches deep-copy. Removals are additionally buffered until the end of the
  depth sweep. The search is therefore always Algorithm 4.1 (stable). The
  default is `true`, so behaviour is correct — but the unstable configuration
  is unreachable, which matters because the BOSS paper's PC baseline used it.
- **PC is stable but not conservative.** The port implements Colombo &
  Maathuis's PC-stable skeleton (order-independent by Theorem 3) with the plain
  collider rule, so v-structures and orientation rules remain order-dependent —
  the paper's Theorems 5 and 7 do not apply. The name-sorting in
  `collectUnshieldedTriples` is a determinism device, not the paper's fix.
- **Termination condition.** `fas.cpp:40` requires `!anyRemoved && freeDegree <= d`;
  Algorithm 4.1 terminates on the degree condition alone. Strictly more
  conservative — it may run extra depth levels, never fewer.

---

## 5. FGES — `src/search/fges.cpp`

The operator definitions, their validity conditions and their score changes are
all in Chickering (2002) §5, summarised there in **Table 1** ("Necessary and
sufficient validity conditions and (local) change in score for each operator").

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `Fges::fes` | chickering2002 | §4.2, first phase | "repeatedly replace E with the member of EE⁺(E) that has the highest score" |
| `Fges::bes` | chickering2002 | §4.2, second phase | |
| `Fges::insert(x,y,T,bump)` | chickering2002 | **Definition 12** | "(1) inserting the directed edge X → Y, and (2) for each T ∈ T, directing the previously undirected edge between T and Y as T → Y" |
| `Fges::validInsert` | chickering2002 | **Theorem 15** | "1. NA_{Y,X} ∪ T is a clique  2. Every semi-directed path from Y to X contains a node in NA_{Y,X} ∪ T" |
| `Fges::insertEval` | chickering2002 | **Corollary 16** | `s(Y, NA∪T∪Pa_Y+X) − s(Y, NA∪T∪Pa_Y)` |
| `Fges::doDelete(x,y,H,…)` | chickering2002 | **Definition 13** | |
| `Fges::validDelete` | chickering2002 | **Theorem 17** | "There exists a consistent extension G … if and only if NA_{Y,X} \ H is a clique" — a single clique test, no path condition |
| `Fges::deleteEval` | chickering2002 | **Corollary 18** | |
| `Fges::getNaYX` | chickering2002 | §5, notation preceding Thm 15 | "neighbors of node Y and are adjacent to node X" |
| `Fges::getTNeighbors` | chickering2002 | **Definition 12** | "any subset T of the neighbors of Y that are not adjacent to X" |
| `Fges::isClique` | chickering2002 | **Theorem 15** cond. 1; **Theorem 17** | |
| `Fges::semidirectedPathCondition`, `traverseSemiDirected` | chickering2002 | **Definition 14** + **Thm 15** cond. 2 | "each edge is either undirected or directed away from Y" |
| `Fges::revertToCpdag` | ramsey2015 | §4 | "rendering as undirected all edges not involved in unshielded colliders and then applying the Meek rules". Chickering's own conversion (Appendix C, Figures 13–14) is a different procedure |
| `Fges::initializeEffectEdges` | ramsey2015 | §6 | "precalculate the 'effect edges' … in search of the single best edge to add" |
| `Arrow`, `sortedArrows_` | ramsey2015 | §5 | "A = <d, X->Y, S, NaYX> for the forward step" |
| `ArrowConfig` staleness cache | chickering2002 | §5 | "if the neighbors of Y have not changed, the first validity condition must still hold" |
| `setFaithfulnessAssumed` / `Mode::heuristicSpeedup` | ramsey2015 | §5 | "strengthened to a weak version of faithfulness when doing regressions for scoring" |
| GES optimality | chickering2002 | **Lemma 10** (with **Prop. 8**, **Lemma 9**, and **Theorem 4**, the Meek Conjecture) | "in the limit of large m, E is a perfect map of p". There is no single numbered "GES is correct" theorem |
| `Mode::coverNoncolliders`, `Mode::allowUnfaithfulness` | — | **NOT FOUND** | |

### Deviations

- **BES subset depth is capped at 4.** `fges.cpp:328` uses
  `min(4, |NaYX|)`, so `H` is restricted to `|H| ≥ |NA_{Y,X}| − 4`; Chickering
  §5 prescribes the full power set. This is a faithful port —
  `Bes.java:48` has `depth = 4` — so Tetrad, and therefore this port, deviates
  from Chickering here.
- **Three-pass search.** `Fges::search()` runs `fes(); bes();` three times under
  the three `Mode` values. Neither paper describes more than one forward pass
  followed by one backward pass.
- **`Fges::doDelete` extra guard.** `if (graph_.isParentOf(h, y) || graph_.isParentOf(h, x)) continue;`
  (`fges.cpp:385`) has no counterpart in Definition 13, which directs `Y→H` and
  `X→H` for every `H ∈ H` unconditionally.
- **Cycle condition follows Chickering, not Ramsey.** Ramsey §4 states the test
  as "no semi-directed path from Y to X"; Chickering Theorem 15 only requires
  every such path to be *blocked* by `NA_{Y,X} ∪ T`. The code implements
  Chickering's weaker, correct condition. Ramsey §4 also has a clique-condition
  typo ("must not be a clique") and swaps the definitions of `NaYX` and `T`
  between §4 and §5; the code follows §4 and Chickering, correctly.
- **`initializeEffectEdges` scores both directions.** Ramsey §4 notes "the
  difference in score for X->Y is the same as … Y->X; one only needs to test
  one of these". Roughly double the work in the most expensive step.
- **Arrow tie-break.** C++ uses a monotone counter; Ramsey §5 subtracts hash
  codes and concedes that "does not produce a complete ordering". The C++
  choice is strictly more deterministic — an improvement on the paper.
- **`Score::getMaxDegree() = ceil(log n)`** is not prescribed by either paper;
  Chickering §5 sanctions a parent bound in principle only.

---

## 6. FCI orientation — `src/search/fci_orient.cpp`

**This is the weakest-sourced part of the collection.** The primary reference,
Zhang (2008) in *Artificial Intelligence*, is paywalled and not in `papers/`.
Rules R1–R4 and R8–R10 are recoverable verbatim from Wang et al. (2024)
Appendix A.2, which is included for that purpose. **R5, R6 and R7 — the
selection-bias rules, both implemented here — have no verifiable statement in
this collection.** Anyone deriving `fci_orient.cpp` from first principles needs
that paper.

| C++ symbol | Paper | Anchor | Rule content as printed |
|---|---|---|---|
| `FciOrient::ruleR0` | ogarrio2016 | **Algorithm 1**, steps C′/F′ | "orient it as X◦→ Y ←◦Z if it is an unshielded collider in PAT" |
| `FciOrient::ruleR1` | wang2024 (substitute) | Appendix A.2, **R1** | "If A∗→ B ◦−∗ R, and A and R are not adjacent, then orient the triple as A∗→ B → R" |
| `FciOrient::ruleR2` | wang2024 (substitute) | Appendix A.2, **R2** | "If A → B∗→ R or A∗→ B → R, and A ∗−◦ R, then orient A ∗−◦ R as A∗→ R" |
| `FciOrient::ruleR3` | wang2024 (substitute) | Appendix A.2, **R3** | "If A∗→ B ←∗R, A ∗−◦ D ◦−∗ R, A and R are not adjacent, and D ∗−◦ B, then orient D ∗−◦ B as D∗→ B" |
| `FciOrient::ruleR4B`, `ddpOrient`, `doDdpOrientation` | wang2024 (substitute); colombo2012 | Appendix A.2, **R4**; **Lemma 3.2** | "If ⟨K,…,A,B,R⟩ is a discriminating path between K and R for B … then if B ∈ Sepset(K,R), orient B ◦−∗ R as B → R; otherwise orient the triple as A ↔ B ↔ R". Colombo's Lemma 3.2 is RFCI's *modified* independence-based version, explicitly not Zhang's; the code implements Zhang's sepset-membership form |
| `FciOrient::ruleR5`, `ruleR6R7` | — | **NOT FOUND** | Selection-bias rules; absent from every paper here. Wang et al. omit them explicitly: "Since R5 − R7 are triggered only if the selection bias is involved … we omit these three rules" |
| `FciOrient::ruleR8` | wang2024 (substitute) | Appendix A.2, **R8** | "If A → B → R, and A◦→ R, orient A◦→ R as A → R" |
| `FciOrient::ruleR9` | wang2024 (substitute) | Appendix A.2, **R9** | "If A◦→ R, and p = ⟨A,B,D,…,R⟩ is an uncovered possible directed path from A to R such that R and B are not adjacent, then orient A◦→ R as A → R" |
| `FciOrient::ruleR10` | wang2024 (substitute) | Appendix A.2, **R10** | "Suppose A◦→ R, B → R ← D, p1 … p2 are uncovered possible directed paths … If U and W are distinct, and are not adjacent, then orient A◦→ R as A → R" |
| `zhangFinalOrientation` vs `spirtesFinalOrientation`, `setCompleteRuleSetUsed` | colombo2012; kalisch2012 | **Algorithm 3.1**, step 5; §2.1 | "Use rules (R1)–(R10) of [Zhang 2008] to orient as many edge marks as possible"; "The orientation rules … were slightly extended and proven to be complete in Zhang (2008)" |
| `existsSemiDirectedPath` (used by R9/R10) | spirtes1995 | Appendix | "A semi-directed path from A to B … in which no edge contains an arrowhead pointing towards A" |
| uncovered path | zhang2008-ancestral-reasoning | **footnote 26** | "A path is called uncovered if every consecutive triple on the path is unshielded" |
| possibly/potentially directed path | zhang2008-ancestral-reasoning | §2.3; **footnote 11** for the synonym | "for every 0 < i ≤ n, the edge between V_{i−1} and V_i is not into V_{i−1}" |
| circle path (R5) | — | **NOT FOUND** | |
| discriminating path (graphical definition) | — | **NOT FOUND** | Used but never defined in any paper here. Colombo's Lemma 3.2 conditions are an independence-based surrogate |
| `FciOrient::fciOrientbk`, `isArrowheadAllowed` | — | **NOT FOUND** | Knowledge gating; Tetrad-specific |
| MAG semantics | zhang2008-ancestral-reasoning | **Definition 1** | "no directed or almost directed cycles (ancestral); and … no inducing path between any two non-adjacent vertices (maximal)" |
| PAG semantics — what the output *means* | zhang2008-ancestral-reasoning | **Definition 3** | "A mark of arrowhead is in P_[M] if and only if it is shared by all MAGs in [M]" |
| endpoint marks (`Endpoint::TAIL/ARROW/CIRCLE`) | zhang2008-ancestral-reasoning | §2 | "three kinds of end marks for edges: arrowhead (>), tail (−) and circle (◦)" |
| ancestral graph, arrowhead meaning | richardson2002 | **§3.1**; maximality **§3.7**, **Prop. 3.20** | "if α and β are joined by an edge with an arrowhead at α, then α is not anterior to β" |
| m-separation, collider on a path | richardson2002 | **§3.4** | |
| PAG under selection bias (tails ⇒ ancestor) | colombo2012 | **Definition 3.1** (FCI-PAG) | |

### Deviations

Two bugs are **intentionally preserved** from Tetrad 7.6.3, both confirmed
present in the code:

- **R3 aborts the whole pass.** `fci_orient.cpp:341-342` uses `return`, not
  `continue`, when `isArrowheadAllowed` fails. Because that sits in the
  innermost loop, one knowledge-forbidden arrowhead ends R3 for *every*
  remaining node, not just the current candidate. With empty knowledge the
  branch is unreachable, so the bug is dormant; it activates whenever
  background knowledge is supplied — which includes the tiered-knowledge Boston
  configuration used in the comparison tests.
- **R10 can never fire.** `fci_orient.cpp:631-635` tests
  `getEndpoint(theta, gamma) != TAIL`. `theta` is drawn from
  `getNodesInTo(gamma, ARROW)`, so that endpoint is an arrowhead by
  construction, the guard is unconditionally true, and the body is unreachable.
  The structurally parallel `beta` guard eight lines earlier is written the
  other way round (`getEndpoint(gamma, beta)`), which is the signature of the
  slip. Consequence: `setCompleteRuleSetUsed(true)` delivers R1–R9, not R1–R10,
  and `rulesR8R9R10` is effectively R8-then-R9.

---

## 7. GFCI and the \*-FCI template — `src/search/gfci.cpp`, `star_fci.cpp`, `sepsets_greedy.cpp`

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `Gfci::search` pipeline | ogarrio2016 | **Algorithm 1** (steps are lettered A′–G, not numbered) | |
| `Gfci::getMarkovCpdag` | ogarrio2016 | **Algorithm 1**, line 1; **Lemma 3** | "Run FGS on data and obtain output pattern PAT" |
| `gfciExtraEdgeRemovalStep` | ogarrio2016 | **Algorithm 1**, step **B** | "remove an adjacency between X and Y if there is an independence … conditioning on some set M that is a subset of adjacencies of X or … Y" |
| `Gfci::gfciR0` / `StarFci::gfciR0` | ogarrio2016 | **Algorithm 1**, steps **E** + **F′** | "Apply step E of FCI to unorient all of the edges in Q that remain" |
| `SepsetsGreedy::getSepsetGreedy` | ogarrio2016 | §4.3 preamble | "Sepset(A,B) is set to the results of a search for a conditioning set" |
| `SepsetsGreedy::isUnshieldedCollider` | ogarrio2016 | **Lemma 5** | "If ⟨A,B,C⟩ is an unshielded or discriminated collider (non-collider) … every subset … that d-separates A and C does not (does) contain B" |
| final orientation | ogarrio2016 | **Algorithm 1**, step **G** | Delegates to §6 above |
| GFCI correctness | ogarrio2016 | **Theorem 7** | See the deviation — this guarantee does **not** transfer |
| `StarFci` (the template), `getMarkovCpdag()` hook | — | **NOT FOUND** | A Tetrad refactor generalising Algorithm 1 over the source of the initial CPDAG; `BossFci`, `GraspFci` and `GenericFci` are instantiations with no separate papers |
| Possible-D-SEP (if ever implemented) | colombo2012 | **Definition 3.3** | |

### Deviations

- **The Possible-D-SEP step is omitted.** Ogarrio's Algorithm 1 step D reads:
  "For all adjacencies in Q, apply step D of FCI to remove an adjacency between
  X and Z if there is an independence between X and Z conditioning on a subset
  M of Possible-D-Sep(X,Z) or Possible-D-Sep(Z,X)". There is no
  Possible-D-SEP code anywhere in `src/`. Step C′ is omitted with it, since C′
  exists only to supply the orientations D needs.

  This is not cosmetic. The proof of **Theorem 7** relies on it: "The FCI part
  of GFCI does not remove too few edges, because GFCI considers all subsets of
  adjacencies to A, adjacencies to B, Possible-D-Sep(A,B), and
  Possible-D-Sep(B,A)". **The paper's asymptotic-correctness guarantee therefore
  does not apply to this implementation as written.** It is a faithful port of
  Tetrad 7.6.3, which also omits the step, and it is a recognised variation
  axis — pcalg exposes it as `doPdsep`, noting that with it off "the algorithm
  simplifies to the modified PC algorithm of Spirtes et al. (2000)". But
  `Gfci` and `StarFci` are better described as "the Tetrad 7.6.3 GFCI variant"
  than as Ogarrio et al.'s algorithm.

---

## 8. BOSS and permutation search — `src/search/boss.cpp`, `grow_shrink_tree.cpp`, `permutation_search.cpp`, `bes_permutation.cpp`

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `Boss::searchSuborder` | andrews2023 | **Algorithm 4** `BOSS(X, π, δ)` | "repeat best ← T.score(π); foreach v ∈ π do π ← best-move(T, π, v); until best = T.score(π)" |
| `Boss::betterMutation` | andrews2023 | **Algorithm 5** `best-move(T, π, v)` | Same argmax over insertion positions |
| `Boss::update` | andrews2023 | **Algorithm 3** `project(X, π)` | "foreach v ∈ π do Z ← pre_π(v); W ← grow(X, v, Z); W ← shrink(X, v, W)" |
| `Boss::bes`, `setUseBes` | andrews2023 | **Algorithm 4** (the `δ` argument) | "The BES step is optional but guarantees asymptotic correctness if executed" |
| `GrowShrinkTree::trace`, `GSTNode::trace` | andrews2023 | **§3** (prose; the tree itself has no algorithm number) | "the first child in the sorted list that is also in the prefix is chosen and the corresponding edge traversed" |
| `GSTNode::grow` | andrews2023; margaritis1999 | **Algorithm 1** `grow(X,v,Z)`; MT99 **Figure 2**, growing phase | |
| `GSTNode::shrink` | andrews2023; margaritis1999 | **Algorithm 2** `shrink(X,v,W)`; MT99 **Figure 2**, shrinking phase | "repeat w ← argmax_w BIC(X_v, X_{W\w}); if w ≠ ∅ then W ← W\w; until w = ∅" |
| BIC maximisation rationale | andrews2023 | **Proposition 1** (Haughton) | |
| BOSS correctness | andrews2023 | **Proposition 2** | "If X ∼ P then BOSS(X, π, true) returns the MEC of the causal DAG for all initial permutation π in the large sample limit" — note the `true`: correctness is conditional on BES |
| `BesPermutation::bes` | chickering2002 | §4.2 second phase; **Thm 17**, **Cor. 18** | BOSS §4: "BES … is exactly second phase of GES" |
| `PermutationSearch::getGraph` | andrews2023 | **Algorithm 4** | Paper calls Chickering's `find-compelled`; C++ uses `MeekRules::orientImplied` |
| sparsest-permutation background | raskutti2018 | **Definition 2.2** (SMR), **Theorem 2.3** | The SP algorithm has no algorithm number |
| `PermutationSearch` tiered-knowledge decomposition | — | **NOT FOUND** | Tetrad extension |
| `BesPermutation::validDelete`, `invalidSink` | — | **NOT FOUND** | Clique validity traces to Chickering Thm 17; `invalidSink` is permutation-specific with no anchor |

### Deviations

- **`setUseBes` defaults to off, so Proposition 2 does not apply.** The paper's
  correctness result is stated for `BOSS(X, π, true)`.
- **BES placement.** Algorithm 4 runs BES once at the very end, after
  `find-compelled`. The C++ runs `bes()` inside the restart loop, then
  topologically re-sorts the CPDAG back into a permutation (`boss.cpp:210-246`)
  — a step Algorithm 4 has no counterpart for, needed only because the C++
  carries a permutation across restarts.
- **Restarts.** `setNumStarts` has no counterpart in Algorithm 4; it appears
  only in the supplement's tuning list.
- **`betterMutation` is not literally Algorithm 5.** Algorithm 5 does `|π|` full
  rescorings with revert-on-failure; the C++ does one forward and one backward
  incremental sweep and a single final move. Same argmax, much cheaper. The
  `1e-6` tie tolerance and the index fixup at `boss.cpp:134` have no counterpart.
- **`grow` prunes more than §3 describes.** §3 says a child is added "for each
  possible addition"; `grow_shrink_tree.cpp:111` keeps a branch only when
  `branch->growScore >= this->growScore`. Consistent with Algorithm 1's
  argmax-with-stopping, but it changes which branches are cached for later
  prefixes.
- **`find-compelled` → Meek.** Same CPDAG in the noiseless case, different
  procedure.

---

## 9. GRaSP — `src/search/grasp.cpp`, `teyssier_scorer.cpp`

| C++ symbol | Paper | Anchor | Note |
|---|---|---|---|
| `Grasp::grasp` | lam2022 | **Algorithm 2** `GRaSP_t: grasp(P, π, d, t)` | "do π←τ; τ ← dfs(P, π, d, 1, t); while score(τ) > score(π)" |
| `Grasp::graspDfs` | lam2022 | **Algorithm 1** `dfs(P, π, d, d_cur, t)` | "foreach (j → k) ∈ E_t(G_π) do τ ← tuck(π, j, k); if score(τ) = score(π) and d_cur < d then …" |
| `TeyssierScorer::tuck` | lam2022 | **Definition 4.1 (Tuck)** | See deviation — `Grasp` does not call it |
| covered edge | lam2022 | §3, unnumbered prose | "A directed edge j → k is covered in G if Pa(j,G) = Pa(k,G) \ {j}" |
| `Grasp::setNonSingularDepth` | lam2022 | **Definition 4.2**; footnote 11 | "a directed edge (j → k) ∈ E(G) is said to be singular if there exists no directed path from j to k in G except j → k" |
| `Grasp::setUncoveredDepth` | lam2022 | **Definition 4.2** (t = 0 case); footnote 11 | The parameter names appear verbatim in footnote 11 |
| `Grasp::setDepth` (default 3) | lam2022 | **Algorithm 2** input (c); §5.2 | "we allow tucks of covered edges up to depth 3" |
| `Grasp::setOrdered` | lam2022 | **Algorithm 2**, lines 1–2 | "if t ≠ 0 then π = grasp(P, π, d, t−1)" |
| ct-sequences | lam2022 | **Definition 4.3** | |
| GRaSP correctness / hierarchy | lam2022 | **Lemma 4.4**, **Theorem 4.5**, **Corollary 4.6**, **Theorem 4.7**, **Theorem 4.8**, **Corollary 4.9** | Cor. 4.6: "Unbounded GRaSP0, GRaSP1, and GRaSP2 are correct and pointwise consistent under faithfulness" |
| `TeyssierScorer::score` | teyssier2005 | §3.1, Eqs. (2)–(3) | "the score of an ordering is the score of the best network consistent with it". The paper has no numbered theorems or algorithms |
| `TeyssierScorer::swap` | teyssier2005 | **Eq. (4)** | The adjacent-swap operator — TK05's only operator |
| `TeyssierScorer::getGrowShrinkScore` | lam2022; margaritis1999 | §4 (VP) | "we can estimate the unique Markov boundary by the Grow-Shrink (GS) algorithm from [Margaritis and Thrun, 1999] using BIC scores" |
| `TeyssierScorer::moveTo` | — | **NOT FOUND** | TK05's operator is the adjacent swap, matching `swap`, not `moveTo` |
| `TeyssierScorer::bookmark`/`goToBookmark` | — | **NOT FOUND** | The paper's DFS backtracks functionally; bookmarking is the imperative equivalent |

### Deviations

- **The tier cascade is off by default.** Algorithm 2 lines 1–2 recurse to tier
  `t−1` unconditionally; `grasp.cpp:71-83` does so only when `ordered_`, which
  defaults to `false`. §5.2 says the paper's own simulations always ran lower
  tiers first, so the default does not reproduce the paper's protocol.
- **`TeyssierScorer::tuck` is dead code.** `graspDfs` inlines the tuck via
  `moveTo` (`grasp.cpp:142-170`), and that inline version is faithful to
  Definition 4.1. But `TeyssierScorer::tuck` itself uses inverted argument
  conventions and is unreachable from `Grasp` — a trap for anyone assuming it
  is the operative implementation.
- **No in-degree bound.** TK05's Eq. (2) restricts `|U| ≤ k` and its entire
  complexity argument depends on that; `getGrowShrinkScore` runs unbounded
  grow-shrink over the full prefix. Justified — GS with BIC replaces the bound,
  per Lam et al. §4 — but it means the C++ is not TK05's formulation, only its
  ordering-space framing.
- **Iteration order was heap-address-dependent.** `graspDfs` iterated
  `std::set<NodePtr>`, which orders by pointer address, and returns on the first
  improving tuck — so the result varied between runs in the same process. Java
  iterates a `HashSet<Node>` in name-hash order: arbitrary but deterministic.
  `tests/test_simulation.py::test_same_seed_gives_same_answer` covers this.

---

## 10. Citation errors in source comments

Worth correcting when those files are next touched:

- `src/search/boss.h` and the other permutation headers date the BOSS paper to
  **2024**; it is NeurIPS **2023**.
- `src/search/teyssier_scorer.h` cites Teyssier & Koller **(2012)**; the paper
  is UAI **2005**.
- `src/search/fci_orient.h` cites Zhang (2008) correctly, but that paper is not
  held locally — see §6.
