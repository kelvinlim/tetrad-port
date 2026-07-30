# Java → C++ correspondence

Derived solely from `blind_pc/java/` and `blind_pc/include/`. No existing C++
implementation of MeekRules or Pc was consulted, searched for, or opened.

Line ranges are inclusive. Java ranges refer to the copies in `blind_pc/java/`.

---

## 1. `out/meek_rules.cpp` ← `java/MeekRules.java`

| Java method | Java lines | C++ function | C++ lines |
|---|---|---|---|
| field `useRule4` + ctor `MeekRules()` | 47, 63–65 | `USE_RULE4` file constant | 22–39 |
| `log(String)` | 355–359 | `logMsg(bool, const std::string&)` | 41–46 |
| (Java `p != q` reference test) | 335 | `sameNode` | 52–56 |
| (Java `Collection.contains`) | — | `containsNode` | 58–63 |
| `revertToUnshieldedColliders(Node, Graph, Set)` | 327–353 | `revertToUnshieldedCollidersAt` | 65–115 |
| `revertToUnshieldedColliders(List, Graph, Set)` | 173–185 | `revertToUnshieldedCollidersAll` | 117–131 |
| `orientImplied(Graph)` | 80–115 | `MeekRules::orientImplied` | 135–185 |
| `meekR1(b, c, graph, visited)` | 190–201 | `MeekRules::meekR1` | 186–208 |
| `meekR2(a, c, graph, visited)` + `r2Helper` | 206–234 | `MeekRules::meekR2` | 210–248 |
| `meekR3(d, a, graph, visited)` + `r3Helper` | 239–278 | `MeekRules::meekR3` | 250–287 |
| `meekR4(a, b, graph, visited)` | 280–302 | `MeekRules::meekR4` | 289–329 |
| `direct(a, c, graph, visited)` | 304–325 | `MeekRules::direct` | 331–364 |
| `isArrowheadAllowed(from, to, k)` | 68–72 | `MeekRules::isArrowheadAllowed` | 366–375 |
| `Edges.isUndirectedEdge(edge)` | (Edges util) | `MeekRules::isUndirectedEdge` | 377–384 |
| `graph.paths().isUndirectedFromTo(x, y)` | (Paths util) | `MeekRules::isUndirected` | 386–392 |
| `graph.paths().isDirectedFromTo(x, y)` | (Paths util) | `MeekRules::isDirected` | 394–399 |
| `getCommonAdjacents(x, y, graph)` | 361–365 | `MeekRules::getCommonAdjacents` | 401–435 |
| `setKnowledge` / `setMeekPreventCycles` / `setVerbose` / `setRevertToUnshieldedColliders` | 123–164 | inline in `meek_rules.h` | — |
| `getChangedEdges()` / `changedEdges` field | 45, 143–145 | **not ported** | — |

### Deviations from a literal transcription (MeekRules)

1. **`r2Helper` / `r3Helper` inlined.** The header declares neither, and both are
   two-liners (`direct(...)` + `log(...)`). Semantics preserved exactly,
   including the fact that `r2Helper` **logs even when `direct` returned false**
   (`boolean directed = direct(...); log(...); return directed;`, MeekRules.java:229–234).

2. **`revertToUnshieldedColliders` moved to file-scope helpers.** The header
   declares no member for it, so `knowledge_` is passed as a parameter. Body is
   otherwise line-for-line, including the labelled-`continue` emulation
   (`P: for (...) { for (...) { ... continue P; } }`, MeekRules.java:332–338)
   via a `skip` flag + `break`.

3. **`meekR2`'s dead code dropped.** MeekRules.java:207–208
   ```java
   List<Node> adjacentNodes = graph.getAdjacentNodes(c);
   adjacentNodes.remove(a);
   ```
   `adjacentNodes` is never read again and `getAdjacentNodes` returns a fresh
   `ArrayList` (EdgeListGraph.java:561–573), so removing `a` from it has no
   effect on the graph. Omitted; recorded in SUSPECTED_BUGS.md #2.

4. **`useRule4` modelled as a compile-time-visible constant** rather than a
   member, because the header has no member for it and the Java value can never
   be anything but `false` (see SUSPECTED_BUGS.md #1). `meekR4` is still fully
   transcribed behind the flag.

5. **`Edges.isUndirectedEdge(null)` NPE avoided.** MeekRules.java:306 passes
   `graph.getEdge(a, c)` straight into `Edges.isUndirectedEdge`, which throws if
   the pair is not adjacent. `MeekRules::isUndirected` checks `isAdjacentTo`
   first and returns `false`. Every call site only reaches `direct` with an
   adjacent pair, so no behavioural difference; it just cannot crash.

6. **`graph.getNodes()` copied** before the revert loop (`orientImplied`),
   because the C++ accessor returns a reference into the graph being mutated.
   Java's `EdgeListGraph.getNodes()` already hands back a copy. No semantic
   change — the node list is not modified by any Meek rule.

7. **Log message text is paraphrased.** `LogUtilsSearch.edgeOrientedMsg` and
   `TetradLogger` are not part of the supplied reference set; messages are
   emitted to `logStream()` only when `verbose_`, and carry the same operands.

### Assumptions (source not supplied)

`Paths.java` and `Edges.java` are not in `blind_pc/java/`. Two semantics were
assumed and are documented at the C++ definitions:

* `paths().isDirectedFromTo(x, y)` — the edge between `x` and `y` exists and
  `Edge.pointsTowards(y)` (Edge.java:201–205: arrowhead proximal to `y`, TAIL or
  CIRCLE distal). Used by `MeekRules::isDirected`.
* `paths().isUndirectedFromTo(x, y)` — the edge exists and is TAIL–TAIL, matching
  `Edges.isUndirectedEdge`. Used by `MeekRules::isUndirected`.
* `paths().existsDirectedPathFromTo(c, a)` → `Graph::existsDirectedPath(c, a)`.

If those differ in 7.6.3, `meekR2`/`meekR3` and cycle prevention are the places
to re-check.

---

## 2. `out/pc.cpp` ← `java/Pc.java`, `java/PcCommon.java`, `java/GraphSearchUtils.java`

| Java method | Java file:lines | C++ function | C++ lines |
|---|---|---|---|
| `GraphSearchUtils.translate` | GraphSearchUtils.java:766–774 | `translate` | 35–43 |
| (`Set<Node>.contains`) | — | `setContainsNode` | 45–54 |
| `Pc(IndependenceTest)` | Pc.java:101–107 | `Pc::Pc` | 58–69 |
| `Pc.search()` | Pc.java:122–125 | `Pc::search()` | 71–77 |
| `Pc.search(Set<Node>)` → `Pc.search(IFas, Set<Node>)` → `PcCommon.search()` / `PcCommon.search(List)` | Pc.java:140–192; PcCommon.java:150–247 | `Pc::search(const std::vector<NodePtr>&)` | 79–152 |
| `Pc.getPcCommon()` (option wiring) | Pc.java:194–218 | folded into `Pc::search` | 112–121 |
| `GraphSearchUtils.pcOrientbk` | GraphSearchUtils.java:52–97 | `Pc::pcOrientbk` | 154–200 |
| `PcCommon.orientCollidersUsingSepsets` (enumeration half) | PcCommon.java:522–546 | `Pc::collectUnshieldedTriples` | 202–250 |
| `PcCommon.orientCollidersUsingSepsets` (orientation half) | PcCommon.java:547–574 | `Pc::orientUnshieldedTriples` | 252–288 |
| `PcCommon.colliderAllowed` (and its inline copy at 552–562) | PcCommon.java:496–508 | `Pc::colliderAllowed` | 290–306 |
| `PcCommon.orientCollider` PRIORITIZE_EXISTING guard | PcCommon.java:84–85 | `Pc::canOrientCollider` | 308–323 |
| `PcCommon.orientCollider` PRIORITIZE_EXISTING body | PcCommon.java:86–89 | `Pc::orientCollider` | 325–332 |
| MeekRules wiring | PcCommon.java:231–235 | `Pc::applyMeekRules` | 334–357 |
| `PcCommon.orientUnshieldedTriplesConservative`, `getSepsets`, `isColliderSepset`, `isNoncolliderSepset`, MAX_P branch, `logTriples`, ORIENT_BIDIRECTED / OVERWRITE_EXISTING | PcCommon.java:91–101, 387–494, 207–227, 366–385 | **not ported** — unreachable from `Pc` with its fixed defaults, and the C++ header exposes no setters to reach them | — |

### Deviations from a literal transcription (Pc)

1. **One `Fas`, not two.** `Pc.search(Set)` (Pc.java:143–145) builds an `IFas` and
   passes it to `Pc.search(IFas, Set)`, which then **ignores it** and lets
   `PcCommon.search()` build its own `Fas` (PcCommon.java:180–189). The header's
   `getSepsets()` returns `fas_->getSepsets()`, so `fas_` here is the one that
   actually runs. Consequence: C++ `getSepsets()` returns the real sepsets,
   whereas Java `Pc.getSepsets()` returns the *unused* Fas's sepsets — i.e. null.
   Same graph either way. See SUSPECTED_BUGS.md #5.

2. **The `nodes` argument of `search(nodes)` is honoured only as a domain check**,
   exactly as in Java (Pc.java:170–178). The search itself runs over
   `test_->getVariables()`. This is faithful, and it is a bug — SUSPECTED_BUGS.md #4.

3. **`existsDirectedCycle` guards not ported** (PcCommon.java:200–201, 212–213).
   FAS produces an undirected skeleton, so neither can fire; they are pure cost.

4. **`GraphUtils.replaceNodes(graph, nodes)` not ported** (PcCommon.java:229). It
   substitutes same-named nodes from `nodes` into the graph; here the graph is
   already built from the test's own `NodePtr`s, which *are* `nodes`. No-op.

5. **`colliderAllowed` written as a flat conjunction.** Java (PcCommon.java:496–508)
   uses a `knowledge != null` guard plus an early `return false`. `knowledge_` is a
   value member and never null, so the short-circuit structure is exactly the
   4-clause conjunction implemented.

6. **`orientCollider` split into `canOrientCollider` + `orientCollider`** to match
   the header, which declares the predicate separately and makes `orientCollider`
   `static` (so it cannot read the conflict rule). Composition at the call site
   reproduces PcCommon.java:84–90 exactly.

7. **`meekPreventCycles` hard-coded to `true`** in `applyMeekRules`. `PcCommon`'s
   own field default is `false` (PcCommon.java:54), but `Pc` defaults it to `true`
   (Pc.java:89) and always forwards it (Pc.java:198). The C++ `Pc` header has no
   setter, so `true` — the only value `Pc` ever produces — is inlined. This also
   agrees with `meek_rules.h`'s `meekPreventCycles_ = true` default.

8. **`setPcHeuristicType(NONE)` / `setMaxPathLength(3)` dropped** — `NONE` is the
   no-op heuristic and `maxPathLength` is read only by the MAX_P branch, which is
   unreachable. The C++ `Fas` has no `setPcHeuristicType`.

9. **Depth default differs.** Java `Pc.depth = 1000` (Pc.java:75); the C++ header
   defaults `depth_ = -1`. Unchanged (it's a header decision, not mine); with the
   variable counts these searches run on, "unlimited" and "1000" coincide. Worth
   a glance if `Fas` treats `-1` as something other than unlimited.
