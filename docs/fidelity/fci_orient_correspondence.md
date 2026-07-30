# FciOrient: Java 7.6.3 → C++ correspondence

Java source: `java/FciOrient.java` (1254 lines).
C++ output: `out/fci_orient.cpp` (1141 lines), implementing `include/search/fci_orient.h`.

Syntax check: `g++ -std=c++17 -fsyntax-only -I include out/fci_orient.cpp` — clean.

## Method map

| Java method | Java lines | C++ function | C++ lines | Notes |
|---|---|---|---|---|
| `getUcPdPaths(n1,n2,graph)` (static) | 91–103 | `(anon)::getUcPdPaths` | 173–187 | Not in the C++ header; made a file-local free function. |
| `getUcPdPsHelper(...)` (static private) | 117–149 | `(anon)::getUcPdPsHelper` | 134–171 | Recursive DFS, `soFar` mutated + unwound identically. |
| `getUcCirclePaths(n1,n2,graph)` (static) | 161–186 | `(anon)::getUcCirclePaths` | 189–218 | |
| `isArrowheadAllowed(x,y,graph,knowledge)` | 188–212 | `FciOrient::isArrowheadAllowed` | 244–276 | Public static, signature per header. |
| `orient(Graph)` | 220–236 | `FciOrient::orient` | 278–294 | Java returns `Graph`; header returns `void`. |
| `getSepsets()` | 243–245 | — | — | Not in header; `sepsets_` used directly. |
| `setKnowledge` | 252–258 | header inline | — | Header stores by value (Java does `new Knowledge(knowledge)` — same copy semantics). |
| `isCompleteRuleSetUsed` / `setCompleteRuleSetUsed` | 264–274 | header inline | — | |
| `ruleR0(Graph)` | 283–339 | `FciOrient::ruleR0` | 296–353 | |
| `doFinalOrientation(Graph)` | 346–352 | `FciOrient::finalOrientation` | 355–361 | Header's `doFinalOrientation()` is an inline alias to this. |
| `spirtesFinalOrientation(Graph)` | 354–377 | `FciOrient::spirtesFinalOrientation` | 366–384 | |
| `zhangFinalOrientation(Graph)` | 379–422 | `FciOrient::zhangFinalOrientation` | 386–427 | |
| `rulesR1R2cycle(Graph)` | 426–454 | `FciOrient::rulesR1R2cycle` | 429–459 | |
| `ruleR1(a,b,c,graph)` | 458–476 | `FciOrient::ruleR1` | 461–484 | |
| `ruleR2(a,b,c,graph)` | 480–498 | `FciOrient::ruleR2` | 486–512 | |
| `ruleR3(Graph)` | 506–553 | `FciOrient::ruleR3` | 514–578 | |
| `ruleR4B(Graph)` | 568–608 | `FciOrient::ruleR4B` | 580–615 | |
| `ddpOrient(a,b,c,graph)` | 615–678 | `FciOrient::ddpOrient` | 617–696 | Private in header, public in Java. |
| `doDdpOrientation(d,a,b,c,graph)` | 849–898 | `FciOrient::doDdpOrientation` | 698–759 | Private in header, public in Java. |
| `ruleR5(Graph)` | 684–740 | `FciOrient::ruleR5` | 761–811 | |
| `ruleR6R7(Graph)` | 746–807 | `FciOrient::ruleR6R7` | 813–876 | |
| `rulesR8R9R10(Graph)` | 813–843 | `FciOrient::rulesR8R9R10` | 878–905 | |
| `orientTailPath(path,graph)` | 908–922 | `(anon)::orientTailPath` | 220–228 | Free function; see deviation D6. |
| `ruleR8(a,c,graph)` | 935–972 | `FciOrient::ruleR8` | 907–952 | |
| `ruleR9(a,c,graph)` | 985–1015 | `FciOrient::ruleR9` | 966–998 | |
| `Edges.partiallyOrientedEdge` equality test in R9 | 989 | `FciOrient::isPartiallyOrientedEdge` | 954–964 | See deviation D3. |
| `fciOrientbk(bk,graph,variables)` | 1020–1088 | `FciOrient::fciOrientbk` | 1000–1076 | |
| `getMaxPathLength` / `setMaxPathLength` | 1093–1106 | header inline `setMaxDiscriminatingPathLength` | — | Renamed by the header; same field. Java's `< -1` range check dropped (header setter is inline and cannot throw). |
| `setVerbose` | 1113–1115 | header inline | — | |
| `getTruePag` / `setTruePag` | 1120–1131 | — | — | Not in the header; used only by `printWrongColliderMessage`. |
| `isChangeFlag` / `setChangeFlag` | 1138–1149 | — | — | Not in the header; `changeFlag_` is used internally only. |
| `setDoDiscriminatingPath{Collider,Tail}Rule` | 1156–1167 | header inline | — | |
| `ruleR10(a,c,graph)` | 1180–1247 | `FciOrient::ruleR10` | 1078–1139 | Header names the params `alpha`/`gamma`; aliased to `a`/`c` on entry. |
| `printWrongColliderMessage(a,b,c,graph)` | 1249–1253 | — | — | Dropped; needs `truePag`, pure logging, no graph effect. |

### Supporting Java pulled in from other files

| Java | Where | C++ | C++ lines |
|---|---|---|---|
| `EdgeListGraph.getNodesOutTo(node, ep)` | `EdgeListGraph.java:657-668` | `(anon)::getNodesOutTo` | 98–109 | Not on the C++ `Graph`; transcribed literally from the Java. |
| `GraphUtils.asList(int[], List<Node>)` | `GraphUtils.java:652-660` | `(anon)::asList` | 119–128 | |
| `GraphSearchUtils.translate(String, List<Node>)` | referenced at `FciOrient.java:1034` | `(anon)::translate` | 111–117 | Source not staged; implemented as first name match, which is what the call sites require. |
| `EdgeListGraph.getAdjacentNodes` HashSet order | `EdgeListGraph.java:561-577` | `(anon)::adjacentNodesJavaOrder` | 84–89 | See deviation D1. |

## Deviations from a literal transcription

### D1 — Java `HashSet<Node>` order applied at `getAdjacentNodes()` sites *(behavioural)*

`EdgeListGraph.getAdjacentNodes` (EdgeListGraph.java:561-577) is:

```java
Set<Edge> edges = this.edgeLists.get(node);
Set<Node> adj = new HashSet<>();
for (Edge edge : edges) { ... adj.add(edge.getDistalNode(node)); }
return new ArrayList<>(adj);
```

so the returned `List` is in `java.util.HashSet` bucket order keyed on
`GraphNode.hashCode()`, which is `getName().hashCode()` (GraphNode.java:203-205) —
**not** insertion order. `include/util/java_hash.h::sortByJavaHashOrder` models
precisely this (Java `String.hashCode`, HashMap's `h ^ h>>>16` perturbation,
`(cap-1) & h` bucketing, `n/0.75+1` rounded up to a power of two, floor 16). I
judged it applicable and correct at every `getAdjacentNodes()` call site and
routed all of them through `adjacentNodesJavaOrder()`:

- `ruleR0` (Java 294) — order decides which unshielded collider is oriented first, and earlier orientations change later `isArrowheadAllowed`/`isDefCollider` outcomes.
- `rulesR1R2cycle` (Java 434).
- `ruleR3` (Java 527) — only the `a`-side list, since the `c`-side list is consumed by `retainAll` where only membership matters.
- `ruleR6R7` (Java 754).
- `getUcPdPaths` (Java 97) and `getUcPdPsHelper` (Java 142) — path *enumeration* order feeds "first match wins" logic in R5/R9/R10.

Caveat, stated in SUSPECTED_BUGS.md: within-bucket ordering is only approximated.

I did **not** apply it to `getNodesInTo`, `getNodesOutTo`, `getParents`, or
`getEdges(node)`. Those iterate a `HashSet<Edge>`, not a `HashSet<Node>`; the
node-keyed sorter is the wrong model, and `sortEdgesByJavaHashOrder` takes a
whole-graph edge count that does not describe a per-node edge set. Details in
SUSPECTED_BUGS.md.

### D2 — Null-endpoint / null-edge idioms

Java's `getEndpoint` returns `null` when there is no edge, and `getEdge` returns
`null`. The C++ `Graph` has no nullable return. Wherever Java tested
`graph.getEdge(x, y) == null` I used `!graph.isAdjacentTo(x, y)`, which is
exactly equivalent (`fciOrientbk`, Java 1041 and 1071; `ruleR9`, Java 988).
Endpoint comparisons rely on `Endpoint::NULL_EP` never equalling
`ARROW`/`TAIL`/`CIRCLE`, matching Java's `null != Endpoint.X`.

### D3 — `Edges.partiallyOrientedEdge` in R9

Java 989: `if (!e.equals(Edges.partiallyOrientedEdge(a, c))) return false;`
`Edges.partiallyOrientedEdge(a, c)` is `new Edge(a, c, Endpoint.CIRCLE, Endpoint.ARROW)`
(i.e. `a o-> c`; the `Edges` class was not staged, so this is asserted from
Tetrad's standard definition), and `Edge.equals` (Edge.java:328-350) compares the
endpoints proximal to each node. I expressed the same predicate directly on the
graph in `isPartiallyOrientedEdge` — endpoint AT `a` is `CIRCLE`, endpoint AT `c`
is `ARROW` — rather than constructing an `Edge` and relying on the C++
`Edge::operator==`, whose implementation I do not have. Semantically identical.
This also fills the private static declared in the header.

### D4 — Node comparison

Java mixes reference identity (`d == a`, `b == c`, `e == t`) with
`.equals()`/`List.contains`/`Set.contains` (name equality, GraphNode.java:210-214).
All nodes involved come from the same `Graph`, so the two coincide. I used name
equality (`sameNode`, `listContains`, `setContains`) everywhere except `e == t`
in `ddpOrient`, where I kept `shared_ptr` identity to mirror Java's `==`.

`setContains` matters: `sepset->count(b)` would not work, because
`std::set<NodePtr>` orders by *raw pointer*, making `count` an identity test,
whereas Java's `Set.contains` is name-based.

### D5 — `doDdpOrientation` guard

Java 850-852 throws `IllegalArgumentException` when `isAdjacentTo(d, c)`. The
only caller (`ddpOrient`, Java 666) already guarantees non-adjacency, so the
throw is unreachable; I `return false` instead of throwing.

### D6 — `orientTailPath` as a free function

Java's `orientTailPath` sets `this.changeFlag = true` once per edge. The C++
header does not declare it, and I did not want to modify the header, so it is a
file-local free function. Its only caller, `ruleR5`, already sets
`changeFlag_ = true` on the very next line (Java 736), so the net effect is
identical.

### D7 — Thread-interruption checks

Every `Thread.currentThread().isInterrupted()` guard (Java 290, 304, 359, 383,
408, 416, 430, 443, 510, 574, 584, 589, 632, 649, 688, 695, 707, 750, 762, 817,
824, 1027, 1057, 1184, 1198, 1214, 1220) is dropped. There is no equivalent in
the C++ contract; they only ever `break`/`return` early on cancellation, which
never happens in a normal run.

### D8 — Logging

`TetradLogger` / `LogUtilsSearch` / `GraphUtils.pathString` are replaced by
`logStream()` from `util/log_stream.h` with plainer messages. Two of Java's
`forceLogMessage` calls in `fciOrientbk` (Java 1052, 1082) are *not* gated on
`verbose`; I gated them, since unconditional stdout writes from a library
function are not something the C++ side does. No graph effect.

### D9 — `zhangFinalOrientation`'s redundant re-test

Java 399 re-tests `isCompleteRuleSetUsed()` inside a method that is only reached
when it is true. Kept (as `if (completeRuleSetUsed_)`) to preserve shape; it is
always taken.
