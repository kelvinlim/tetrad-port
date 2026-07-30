# Suspected bugs in Tetrad 7.6.3, reproduced verbatim

Everything below looked wrong to me while transcribing. **All of it is
reproduced in the C++**, because the port targets bug-for-bug parity with the
7.6.3 JAR. Nothing here has been "fixed".

---

## 1. `useRule4` is permanently `false` — Meek rule R4 never runs

`java/MeekRules.java:47,63-65`

```java
boolean useRule4;
...
public MeekRules() {
    this.useRule4 = !this.knowledge.isEmpty();
}
```

`knowledge` is initialised at line 48 to `new Knowledge()`, which is empty, so
the constructor unconditionally assigns `useRule4 = false`. `setKnowledge`
(lines 123–125) replaces the knowledge but **never recomputes `useRule4`**, and
`grep -rn useRule4` over the supplied 7.6.3 sources finds only the declaration,
the constructor assignment, and the read at line 281. The field is
package-private, so an out-of-package caller could not set it either.

**Intended:** the class Javadoc says "Rule R4 is only performed if knowledge is
nonempty" (lines 37–38) — i.e. `useRule4` should be recomputed in
`setKnowledge`, or the guard at line 281 should read `!this.knowledge.isEmpty()`.

**Actual:** `meekR4` returns `false` on its first line, always. Meek R4 is dead
code in 7.6.3, *including* when the user supplies knowledge — which is the one
situation it exists for.

**Reproduced as:** `out/meek_rules.cpp:39` `const bool USE_RULE4 = false;`, read
at the top of `MeekRules::meekR4`. The rest of R4 is transcribed behind it so
the correspondence stays reviewable. Note the C++ header has no `useRule4_`
member, which is consistent with the value being constant.

---

## 2. Dead code in `meekR2`

`java/MeekRules.java:207-208`

```java
List<Node> adjacentNodes = graph.getAdjacentNodes(c);
adjacentNodes.remove(a);
```

`adjacentNodes` is never read again, and `EdgeListGraph.getAdjacentNodes`
(EdgeListGraph.java:561–573) returns a freshly built `ArrayList`, so the
`remove` mutates a throwaway.

**Intended:** probably an earlier formulation that iterated `adjacentNodes`
instead of `getCommonAdjacents(a, c, graph)`.

**Actual:** two wasted calls. No behavioural effect.

**Reproduced as:** omitted, with the Java quoted in a comment at
`out/meek_rules.cpp:215-220`. This is the one place I dropped Java code rather
than transcribing it; it provably cannot affect the graph.

---

## 3. `direct()` removes the same edge twice

`java/MeekRules.java:304-325`

```java
Edge before = graph.getEdge(a, c);
graph.removeEdge(before);                       // line 309

if (meekPreventCycles && graph.paths().existsDirectedPathFromTo(c, a)) {
    graph.addEdge(before);
    return false;
}

Edge after = Edges.directedEdge(a, c);
visited.add(a);
visited.add(c);

graph.removeEdge(before);                       // line 321 -- already gone
graph.addEdge(after);
```

**Intended:** presumably line 309 was added later (to make the cycle check see
the graph *without* the edge) and line 321 was left behind.

**Actual:** line 321 is a no-op — `before` is not in the graph. Harmless, but it
means the cycle check at line 311 is performed on the edge-removed graph, which
*is* load-bearing: `existsDirectedPathFromTo(c, a)` must not be able to use the
`a—c` edge itself.

**Reproduced as:** both `graph.removeEdge(before)` calls kept,
`out/meek_rules.cpp:345` and `:360`.

---

## 4. `Pc.search(nodes)` ignores `nodes`

`java/Pc.java:158-192`

```java
public Graph search(IFas fas, Set<Node> nodes) {
    ...
    List<Node> allNodes = getIndependenceTest().getVariables();
    if (!new HashSet<>(allNodes).containsAll(nodes)) {
        throw new IllegalArgumentException(...);
    }

    PcCommon search = getPcCommon();
    this.graph = search.search();          // <-- no-arg overload
    ...
}
```

`PcCommon.search()` (PcCommon.java:150–152) is
`return search(getIndependenceTest().getVariables());` — *all* the test's
variables. `nodes` is consumed only by the `containsAll` validation.

**Intended:** `search.search(new ArrayList<>(nodes))` — `PcCommon.search(List)`
exists and takes exactly that. The Javadoc at Pc.java:136 says
"@param nodes The sublist of nodes to search over."

**Actual:** asking PC to search a subset silently searches everything. Only
observable when a caller passes a strict subset.

**Reproduced as:** `out/pc.cpp:92-110` — `nodes` is validated, then
`searchNodes = test_->getVariables()` is what the rest of the search uses. A
prominent comment marks it.

---

## 5. `Pc` builds a `Fas` it never uses, then reads sepsets out of it

`java/Pc.java:143-145, 179-181`

```java
IFas fas = new Fas(getIndependenceTest());
fas.setVerbose(this.verbose);
return search(fas, nodes);
...
this.graph = search.search();          // PcCommon builds its OWN Fas
this.sepsets = fas.getSepsets();       // ...but sepsets come from this one
this.numIndependenceTests = fas.getNumIndependenceTests();
```

The `fas` object here never has `search()` called on it. It also never receives
the knowledge or the depth (Pc.java:144 sets only `verbose`).

**Intended:** either pass `fas` into `PcCommon`, or read the sepsets back out of
the `PcCommon` instance.

**Actual:** `Pc.getSepsets()` returns the un-run Fas's sepset map (null / empty)
and `Pc.getNumIndependenceTests()` returns 0, for every run. The returned graph
is unaffected.

**Reproduced as:** **not** reproduced — this is the one deliberate divergence.
The C++ header's `getSepsets()` returns `fas_->getSepsets()`, so `out/pc.cpp`
keeps a single `Fas` (`fas_`), runs it, and returns its real sepsets. Copying
the bug would mean carrying a second dead `Fas` purely to hand back an empty
map. The search graph is identical either way. Flagged here so the divergence is
on the record.

---

## 6. `PcCommon.orientCollider` under `ORIENT_BIDIRECTED` prints to stdout

`java/PcCommon.java:95` — `System.out.println("Orienting " + ...)`, not gated on
`verbose`. Unreachable from `Pc` (which uses `PRIORITIZE_EXISTING`), so not
ported; noted only because it would surface if the conflict rule were ever
exposed.

---

## 7. `PRIORITIZE_EXISTING` does not actually prioritise existing orientations

`java/PcCommon.java:83-90`

```java
if (conflictRule == ConflictRule.PRIORITIZE_EXISTING) {
    if (!(graph.getEndpoint(x, y) == Endpoint.ARROW && graph.getEndpoint(z, y) == Endpoint.ARROW)) {
        graph.removeEdge(x, y);
        graph.removeEdge(z, y);
        graph.addDirectedEdge(x, y);
        graph.addDirectedEdge(z, y);
    }
}
```

The guard only declines to act when the triple is *already* a collider at `y`.
If exactly one of the two arrowheads is already present — the actual conflict
case, e.g. `y --> x` was oriented by an earlier triple — the branch is entered
and **overwrites** it with `x --> y`. So "prioritise existing" behaves like
"overwrite existing" for every single-arrowhead conflict; the two enum values
differ only when both arrowheads are already in place.

**Intended:** presumably per-endpoint checks, e.g. orient `x *-> y` only if
`getEndpoint(x, y) != TAIL`.

**Actual:** as described. This makes the *order* of the triple loop
(SUSPECTED_BUGS "iteration order" #B below) directly affect the output.

**Reproduced as:** `Pc::canOrientCollider` (`out/pc.cpp:319-323`) is the literal
negated conjunction; `Pc::orientCollider` (`:326-332`) is the literal body.

---

## 8. `pcOrientbk` is asymmetric: `removeEdge` vs `removeEdges`

`java/GraphSearchUtils.java:70` (forbidden loop) uses `graph.removeEdge(from, to)`,
which throws `IllegalStateException` if there is more than one edge between the
pair (EdgeListGraph.java:580–590); `java/GraphSearchUtils.java:90` (required
loop) uses `graph.removeEdges(from, to)`, which does not.

**Intended:** the same call in both loops.

**Actual:** with a single-edge graph (which is what FAS produces) the two are
equivalent, so this never bites in the `Pc` pipeline. Reproduced anyway —
`out/pc.cpp:178` uses `removeEdge`, `out/pc.cpp:197` uses `removeEdges`.

---

## 9. Knowledge orientations are (mostly) thrown away immediately afterwards

Not a coding error, but surprising enough to flag. `PcCommon.search` calls
`pcOrientbk` at line 203, then `MeekRules.orientImplied` at line 235 with
`revertToUnshieldedColliders` left at its default `true` (MeekRules.java:58).
`revertToUnshieldedColliders` (MeekRules.java:327–353) reverts every directed
edge that is not part of an unshielded collider back to undirected — which
includes the knowledge orientations from `pcOrientbk`, except those saved by the
knowledge check at MeekRules.java:340–341. Reproduced as-is.

---

# Iteration order I could not faithfully reproduce

## A. `graph.getParents(node)` — `meekR1`, `meekR4`, `revertToUnshieldedColliders`

`EdgeListGraph.java:366-390` builds the parent list by walking
`edgeLists.get(node)`, a **per-node `HashSet<Edge>`** created with the default
capacity 16 (EdgeListGraph.java:740) and grown by rehashing as the node's degree
increases. The resulting order depends on `Edge.hashCode()` bucketing *and* on
the rehash history of that particular node's set. The list is then memoised in
`parentsHash`, so the order also depends on when it was first requested.

I did not attempt to model this. `out/meek_rules.cpp` uses whatever order the
C++ `Graph::getParents` returns.

**Why this is (I believe) harmless:** all three consumers are
order-insensitive in outcome.
* `meekR1(b, c)`: the loop body's effect, `direct(b, c, ...)`, does not mention
  `a` at all. The method returns true iff *some* parent of `b` is non-adjacent
  to `c` and `direct(b, c)` succeeds — and `direct` is idempotent in the sense
  that it either succeeds on the first attempt or fails on every attempt within
  a single call (nothing in the loop changes its inputs). Only the log message
  names a different `a`.
* `meekR4`: same shape — `direct(a, b, ...)` is independent of `c` and `d`. And
  it never executes at all (bug #1).
* `revertToUnshieldedColliders(y, ...)`: the parent list is snapshotted before
  any mutation (Java returns the cached `ArrayList`; `removeEdge` evicts the
  cache entry but does not touch the list object, so the loop keeps iterating the
  pre-mutation list — C++ `getParents` returning by value gives the same
  snapshot). The per-`p` decision reads only that snapshot plus `isAdjacentTo`,
  which orientation changes cannot affect. So every `p` is decided independently.

## B. `graph.getAdjacentNodes(node)` and `getCommonAdjacents` — `meekR2`, `meekR3`, `collectUnshieldedTriples`

`EdgeListGraph.java:561-573` funnels the neighbours through a
`HashSet<Node>` (default capacity 16) before returning an `ArrayList`, so the
list is in Java hash-bucket order, *not* insertion order.
`MeekRules.getCommonAdjacents` (MeekRules.java:361-365) then re-hashes that list
into a second `HashSet<Node>` whose capacity is fixed by `|adj(x)|` — the
constructor argument — and `retainAll` shrinks the contents without changing the
capacity or the relative order of survivors.

**What I did:** used `include/util/java_hash.h`.

* `MeekRules::getCommonAdjacents` sorts `adj(x)` with `sortByJavaHashOrder(adjX, x)`
  **before** filtering against `adj(y)`. That makes the helper compute the
  capacity from `|adj(x)|`, which is the correct capacity for the
  `new HashSet<>(graph.getAdjacentNodes(x))` at MeekRules.java:362. Sorting after
  filtering would have used the retained size and been wrong whenever
  `|adj(x)| > 12`.
* `Pc::collectUnshieldedTriples` sorts the adjacency list with
  `sortByJavaHashOrder(adjacentNodes, b)` before handing it to `ChoiceGenerator`.

**Residual inaccuracies, stated plainly:**

1. `javaHashSetCapacity(n)` has a floor of 16, and `getAdjacentNodes`'s internal
   set is a default-capacity-16 `HashSet` that resizes once the node's degree
   exceeds 12. For degree ≤ 12 the two capacities coincide and the model is
   exact. For degree > 12 the modelled capacity is the post-hoc
   `tableSizeFor(n/0.75+1)` rather than the actual resize-history capacity — the
   two agree for the `new HashSet<>(collection)` at MeekRules.java:362 but not
   necessarily for the incrementally-grown set at EdgeListGraph.java:562.
2. The within-bucket tie-break in `sortByJavaHashOrder` is a heuristic (perturbed
   `queryHash + nodeHash`, i.e. a proxy for the per-node edge `HashSet` order).
   Java's true within-bucket order is linked-list insertion order, which for
   `getAdjacentNodes` comes from iterating the per-node `HashSet<Edge>` — the
   same unmodelled thing as (A). Distinct variable names colliding in the same
   bucket is uncommon, but it is not impossible, and this is where a divergence
   would show up first.

**Where this actually changes results:**
* `meekR2` — genuinely order-sensitive. Different `b` in `common` can fire
  opposite orientations (`a-->c` from `a-->b-->c`, versus `c-->a` from
  `c-->b-->a`); whichever `b` comes first wins.
* `collectUnshieldedTriples` — the adjacency order decides both the sequence in
  which unshielded triples are visited and which of the pair is `a` versus `c`.
  Combined with bug #7 (later collider orientations overwrite earlier
  single-arrowhead ones), this directly moves arrowheads.
* `meekR3` — order-**in**sensitive in outcome: every `(b, c)` pair that passes
  leads to the identical `direct(d, a, ...)` call, so only the log message
  changes.

## C. `graph.getEdges()` — the `orientImplied` main loop

`EdgeListGraph.java:755-757` returns `new HashSet<>(this.edgesSet)`, whose
capacity derives from the edge count. Modelled with
`sortEdgesByJavaHashOrder(edges, graph.getNumEdges())`, which is exactly that
construction, so this one I believe is faithful — modulo the same
within-bucket tie-break caveat (the helper does not break ties at all; it is a
`stable_sort`, so equal-bucket edges keep the C++ `Graph::getEdges()` order).

This order matters: `orientImplied` fires at most one rule per edge per sweep
(the `else if` chain), so the sweep order determines which rule wins on the
first pass. The loop iterates to a fixpoint, which damps but does not eliminate
the effect.

Note also the snapshot semantics, which I did reproduce: the `Edge` objects come
from a copy taken before the sweep, so `Edges.isUndirectedEdge(edge)` at
MeekRules.java:96 inspects the **stale** endpoints. An edge oriented earlier in
the same sweep is still seen as undirected here; it is `direct()`'s own
`isUndirectedEdge(graph.getEdge(a, c))` check (line 306, on the *live* graph)
that stops it being re-oriented.

## D. `Knowledge.forbiddenEdgesIterator()` / `requiredEdgesIterator()` — `pcOrientbk`

`Knowledge.java:302-305` returns `getListOfForbiddenEdges().iterator()`, and
`getListOfForbiddenEdges` (Knowledge.java:726+) is
`new ArrayList<>(HashSet<KnowledgeEdge>)`. `requiredEdgesIterator`
(Knowledge.java:521-531) iterates a `HashSet<KnowledgeEdge>` directly. Both are
in `KnowledgeEdge` hash order.

The C++ `Knowledge` in `include/data/knowledge.h` is an independent, simplified
reimplementation (`std::vector<RulePair>` + `std::set<std::string>`) whose
`getListOfForbiddenEdges` / `getListOfRequiredEdges` ordering I neither control
nor can match. I did nothing; `out/pc.cpp` iterates them in whatever order they
come back.

**When this matters:** only when two knowledge edges touch the same node pair in
opposite directions — e.g. a tier with `setTierForbiddenWithin(i, true)`, which
emits both `(x, y)` and `(y, x)` as forbidden (Knowledge.java:729-740). The
forbidden loop orients `to --> from` unconditionally, so processing `(x, y)` and
then `(y, x)` leaves `x --> y`, and the reverse order leaves `y --> x`. Ordinary
tier knowledge (later tier cannot cause earlier tier) is one-directional per
pair and so is unaffected.
