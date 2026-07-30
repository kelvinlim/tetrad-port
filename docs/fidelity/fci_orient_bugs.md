# Suspected bugs in `java/FciOrient.java` (7.6.3) — all reproduced verbatim

Ordered roughly by how much I expect each to change output.

---

## 1. `ruleR10`: endpoint accessor arguments swapped — R10 can never fire

`FciOrient.java:1181` and `:1206`

```java
List<Node> intoCArrows = graph.getNodesInTo(c, Endpoint.ARROW);
...
    for (Node d : intoCArrows) {
        ...
        if (!(graph.getEndpoint(d, c) == Endpoint.TAIL)) {
            continue;
        }
        // We know Ao->C and B-->C&lt;--D.
```

`getEndpoint(d, c)` is the endpoint **at `c`** (EdgeListGraph.java:601-613).
Every `d` in `intoCArrows` came from `getNodesInTo(c, ARROW)`, which is exactly
the set of nodes whose edge has an **ARROW at `c`**. So `getEndpoint(d, c)` is
`ARROW` by construction and the `== TAIL` test is unsatisfiable. The `continue`
always fires, the inner body is dead, and **rule R10 never orients anything**.

Intended: `getEndpoint(c, d) == TAIL` — the endpoint at `d`, giving `D-->C`.
That is precisely what the `b` loop twenty lines earlier does correctly
(`FciOrient.java:1192`: `if (!(graph.getEndpoint(c, b) == Endpoint.TAIL))`), so
the asymmetry between the two loops is the tell.

**Reproduced** at `out/fci_orient.cpp:1109-1112`. Because R10 is a no-op, the
whole `getUcPdPaths(a,b)` / `getUcPdPaths(a,d)` double enumeration below it is
also dead code — but I transcribed it anyway, since it would come alive the
moment the accessor is fixed.

---

## 2. `ruleR3`: `return` where `continue` was meant

`FciOrient.java:536-538`

```java
if (!isArrowheadAllowed(d, b, graph, knowledge)) {
    return;
}
```

This is inside four nested loops (`for b : nodes` → choice generator over
`intoBArrows` → `for d : adj`). A single knowledge-forbidden arrowhead abandons
**all remaining R3 work for the entire graph**, not just the current triple.
Every sibling rule in this file uses `continue` in the analogous position
(`ruleR0` at :321-327, `ruleR1` at :464-466 which is a per-triple method so
`return` is right there). Intended: `continue`.

Impact is nil when knowledge is empty (`isArrowheadAllowed` only returns false
for a TAIL-at-`b` edge, and the enclosing `getEndpoint(d, b) == CIRCLE` guard
excludes that) — but with tiered/forbidden knowledge, which is the normal case
for this project, it truncates R3.

**Reproduced** at `out/fci_orient.cpp:552-556`.

---

## 3. `fciOrientbk`: two more `return`s where `continue` was meant

`FciOrient.java:1045-1047` (forbidden loop) and `:1075-1077` (required loop)

```java
if (!isArrowheadAllowed(to, from, graph, knowledge)) {
    return;
}
```

The first one is the worse of the two: a single unorientable forbidden edge
aborts not only the rest of the forbidden-edge loop but **the entire required-edge
loop as well**, so required background knowledge silently stops being applied.
Intended: `continue` in both places.

Because `ruleR0` calls `fciOrientbk` immediately after
`reorientAllWith(Endpoint.CIRCLE)`, every edge is `o-o` at that moment and
`isArrowheadAllowed` returns `true` (last line, `getEndpoint(x,y) == CIRCLE`)
unless a knowledge-forbidden `x--o y` case is hit — so this fires only in
combination with bug #4's fourth block.

**Reproduced** at `out/fci_orient.cpp:1027-1031` and `:1058-1061`.

---

## 4. `isArrowheadAllowed`: forbidden knowledge returns `true`

`FciOrient.java:199-203`

```java
if (graph.getEndpoint(y, x) == Endpoint.ARROW && graph.getEndpoint(x, y) == Endpoint.CIRCLE) {
    if (knowledge.isForbidden(x.getName(), y.getName())) {
        return true;
    }
}
```

The next block (`:205-209`) is the same shape but returns `false`. Reading
`isForbidden(...) → return true` as "allowed" is at minimum jarring. A charitable
reading: the configuration is `x <-o y`, so putting an arrowhead at `y` produces
`x <-> y`, not `x --> y`, and `x --> y` is what the knowledge forbids — so it is
arguably deliberate. I flag it because the two adjacent blocks disagree on the
sign of the same predicate and only one of them carries any justification.

Also note both blocks re-test `graph.getEndpoint(x, y) == Endpoint.CIRCLE`, which
is already implied: the two early returns at `:191-197` have eliminated `ARROW`
and `TAIL`, leaving only `CIRCLE` or `null`. Harmless, but dead.

**Reproduced** at `out/fci_orient.cpp:255-266`.

---

## 5. `ddpOrient`: BFS distance counter never advances — `maxPathLength` is dead

`FciOrient.java:638-644`

```java
Node t = Q.poll();

if (e == null || e == t) {
    e = t;
    distance++;
    if (distance > 0 && distance > (this.maxPathLength == -1 ? 1000 : this.maxPathLength)) {
        return;
    }
}
```

The standard Tetrad layer-counting idiom sets `e` to the *last node enqueued in
the next layer* and clears it (`e = null`) when that layer boundary is polled.
Here `e = t` sets it to the node just dequeued. Trace it: first poll `e == null`
→ `e = a`, `distance = 1`. Every later poll dequeues something other than `a`
(`a` is in `V` and is never re-offered), so `e == t` is false forever and
`distance` stays pinned at 1.

Consequences: the discriminating-path length limit is **never enforced** for any
`maxPathLength >= 1` or `-1`; and `maxPathLength == 0` degenerates to returning
on the very first poll, disabling R4 entirely. The `distance > 0` conjunct is
also trivially redundant given `distance > (>= 0)`.

**Reproduced** at `out/fci_orient.cpp:643-655`.

---

## 6. `ddpOrient`: `previous` written before the collider test and never rolled back

`FciOrient.java:657` and `:664`

```java
previous.put(d, t);          // <-- line 657
Node p = previous.get(t);

if (!graph.isDefCollider(d, t, p)) {
    continue;               // <-- previous[d] is left pointing at t anyway
}

previous.put(d, t);          // <-- line 664, identical, redundant
```

Line 657 is pure damage: it records a predecessor for a `d` that is then rejected
and left unvisited, so a later iteration that legitimately reaches `d` will read
a stale `previous[d]` when computing its own `p`. Line 664 then repeats the same
assignment for the accepted case. Almost certainly line 657 was meant to be
deleted when 664 was added (or vice versa).

**Reproduced** at `out/fci_orient.cpp:668` and `:684`, with the redundant second
write kept.

Related: `Node p = previous.get(t)` can be `null` for a `t` that entered `Q`
without a `previous` entry. In practice every enqueued node gets one, and Java's
`isDefCollider` returns `false` for null args (EdgeListGraph.java:273), so I
guard explicitly with `p == nullptr || !isDefCollider(...)` to keep the same
outcome without dereferencing null.

---

## 7. `ruleR6R7`: the `(a, c)` pair is not symmetrized

`FciOrient.java:760-775`

`ChoiceGenerator(adjacents.size(), 2)` only yields index pairs with
`choice[0] < choice[1]`, so each unordered pair `{a, c}` is visited once. But the
guards are asymmetric in `a` and `c`:

```java
if (!(graph.getEndpoint(b, a) == Endpoint.TAIL)) continue;   // tail at a
if (!(graph.getEndpoint(c, b) == Endpoint.CIRCLE)) continue; // circle at b, from c
```

`rulesR1R2cycle` faces exactly the same situation and explicitly compensates
(`FciOrient.java:447-451`, comment: *"choice gen doesnt do diff orders, so must
switch A & C around"*), calling `ruleR1(A,B,C)` and `ruleR1(C,B,A)`. R6/R7 has no
such swap, so roughly half the eligible configurations are never tested in a
given pass. (R6/R7 is run to fixpoint by `zhangFinalOrientation`, which may
recover some but not all of it.)

**Reproduced** at `out/fci_orient.cpp:829-874` — no swap added.

---

## 8. `ruleR3`: `if (d == a) continue;` is unreachable

`FciOrient.java:531`

`d` ranges over `adj(a) ∩ adj(c)`, and no graph here has self-loops, so `a ∉ adj(a)`
and `d == a` is impossible. The check that *would* matter — `d == b` — is absent,
though it is neutralised by the `getEndpoint(a, d) == CIRCLE` guard (`a *-> b` has
an ARROW at `b`, not a circle). Dead code, kept.

**Reproduced** at `out/fci_orient.cpp:544`.

---

## 9. `ruleR8`: `b == a` is not excluded

`FciOrient.java:938`

`b` ranges over `getNodesInTo(c, ARROW)`, which contains `a` (the caller
guarantees `a o-> c`). Nothing rejects `b == a`; it is only accidentally
filtered by `!graph.isAdjacentTo(a, b)` returning false for a self-pair. `ruleR10`
does guard this explicitly (`:1188`), which shows the omission is unintentional.
Kept as-is.

---

## 10. `doDdpOrientation`: collider branch falls through to `return false`

`FciOrient.java:867-897`

The collider branch orients `a *-> b <-* c` and sets `changeFlag`, then falls out
of the `if/else` to `return false` at `:897`, so `ddpOrient` **keeps searching**
after a successful collider orientation. The tail branch returns `true` at `:894`
and stops the BFS. The asymmetry may well be deliberate, but combined with the
fact that the BFS's `V`/`previous` state was built against a now-mutated graph,
it is at least fragile. Reproduced exactly
(`out/fci_orient.cpp:748-753` comment marks the fall-through).

---

## 11. `getUcPdPsHelper`: `List.remove(Object)` used to pop the tail

`FciOrient.java:148`

```java
soFar.remove(soFar.get(soFar.size() - 1)); // For other recursive calls.
```

`LinkedList.remove(Object)` removes the **first** element equal to the argument,
not the last. It happens to be correct only because the `soFar.contains(curr)`
guard at `:120` keeps `soFar` duplicate-free. Written as `soFar.pop_back()`
(`out/fci_orient.cpp:170`), which is what it always does.

---

## 12. `zhangFinalOrientation`: redundant `isCompleteRuleSetUsed()`

`FciOrient.java:399` — the method is only reachable from `doFinalOrientation`
when `completeRuleSetUsed` is true, so the test always passes. Kept.

---

# Iteration order I could NOT faithfully reproduce

## O1. `getNodesInTo` / `getNodesOutTo` / `getParents` / `getEdges(node)` — `HashSet<Edge>` order

All four walk `this.edgeLists.get(node)`, which is a `Set<Edge>`
(EdgeListGraph.java:61 `Map<Node, Set<Edge>>`, populated with `new HashSet<>()` at
:740). So Java returns them in `HashSet<Edge>` bucket order, keyed on
`Edge.hashCode() = node1.hashCode() + node2.hashCode()` (Edge.java:321-323).

The C++ `Graph` backs this with `std::unordered_map<std::string, std::vector<Edge>>`
(graph.h:95) — a `vector`, i.e. **insertion order**. I cannot change that from
`fci_orient.cpp`, and `java_hash.h` does not give me a correct tool for it either:

- `sortByJavaHashOrder` is keyed on `Node`, not `Edge` — wrong hash entirely.
- `sortEdgesByJavaHashOrder(edges, edgesSetSize)` takes the **whole graph's** edge
  count to size the table, but a per-node edge list is its own `HashSet` sized by
  that node's degree. Using it here would model the wrong capacity.

**What I did:** used the C++ `Graph`'s native order unchanged, at these sites:
`ruleR3` (`getNodesInTo(b, ARROW)`, Java :514), `ruleR4B`
(`getNodesOutTo(b, ARROW)` / `getNodesInTo(b, CIRCLE)`, Java :580-581), `ddpOrient`
(`getNodesInTo(t, ARROW)` :646, `getParents(c)` :624), `ruleR5`
(`getNodesInTo(a, CIRCLE)` :692), `rulesR8R9R10` (`getNodesInTo(c, ARROW)` :821),
`ruleR8` (:936), `ruleR10` (:1181).

This is a real, unquantified divergence. `ruleR3` and `rulesR8R9R10` are the most
order-sensitive of these (both mutate the graph while iterating a stale snapshot,
and R8/R9/R10 is "first rule that fires wins"). Note that for any node of degree
≤ 12 the Java table capacity is the default 16, so a correct fix is tractable —
it would need either a per-node edge-hash sort helper or a change to `Graph`.

## O2. `getAdjacentNodes` — `HashSet<Node>` order, modelled but only approximately

`EdgeListGraph.getAdjacentNodes` returns `new ArrayList<>(new HashSet<Node>(...))`
(EdgeListGraph.java:561-577), i.e. bucket order on `name.hashCode()`.
`sortByJavaHashOrder` models the bucket computation exactly (`String.hashCode`,
`h ^ h>>>16`, `(cap-1) & h`, capacity `ceil2(n/0.75 + 1)` floored at 16 — which
matches Java's resize-at-75%-load behaviour). **I applied it** at all six
`getAdjacentNodes` sites (see CORRESPONDENCE.md D1).

The residual gap is **within-bucket order**. In Java, colliding nodes appear in
the order they were inserted into the `HashSet`, which is the iteration order of
the per-node `HashSet<Edge>` — i.e. it inherits problem O1. `java_hash.h`
substitutes a heuristic tiebreaker (its own comment: *"For simplicity, use the raw
edge hash as tiebreaker"*) comparing `perturb(hash(query) + hash(candidate))`,
which is not the same thing. With 7 variables in a 16-bucket table, bucket
collisions are likely rather than rare, so this matters.

Because `sortByJavaHashOrder` uses `std::stable_sort`, candidates that also tie on
the heuristic fall back to the C++ `Graph`'s native adjacency order.

**Judgement:** applying the sort is strictly closer to Java than not applying it
(the bucket-level ordering is exactly right, and it is the dominant term), so I
applied it. But it is a deviation from a literal transcription and it is the one
change in this file most likely to be worth A/B-testing against the JAR — it is
isolated in a single 6-line helper, `adjacentNodesJavaOrder`
(`out/fci_orient.cpp:84-89`), and can be neutered by deleting one line.

## O3. `fciOrientbk` — `HashSet<KnowledgeEdge>` order

`Knowledge.forbiddenEdgesIterator()` (Knowledge.java:302-305) delegates to
`getListOfForbiddenEdges()`, which ends `return new ArrayList<>(edges);` over a
`Set<KnowledgeEdge>` (Knowledge.java:764) — hash order.
`requiredEdgesIterator()` (Knowledge.java:521-531) likewise returns
`edges.iterator()` on a fresh `HashSet<KnowledgeEdge>`.

`KnowledgeEdge` is not among the staged sources, so I do not know its `hashCode`
and cannot model the bucket order at all. The C++ `Knowledge::getListOf*Edges()`
returns a `std::vector` in whatever order its own implementation produces.

**What I did:** used the C++ order as-is. This interacts badly with bug #3 above:
the loop `return`s on the first disallowed arrowhead, so *which* knowledge edge
comes first can decide how much background knowledge gets applied at all.

## O4. Snapshot-vs-live collections (reproduced correctly, but worth recording)

Several rules take a collection *before* mutating the graph and then iterate the
stale copy. I preserved all of these, but they are the places where any residual
ordering difference gets amplified:

- `ruleR3` :514 — `intoBArrows` snapshotted, then arrowheads are set at `b` inside the loop.
- `ruleR3` :527-528 — `adj` (= `adj(a) ∩ adj(c)`) computed once per `(a,c)` pair.
- `ruleR4B` :580-581 — `possA`/`possC` snapshotted; `ddpOrient` mutates `b`'s and `c`'s endpoints inside.
- `ruleR5` :692, :704 — `adjacents` and the whole `ucCirclePaths` list are computed before `orientTailPath` rewrites the graph; iteration then continues over paths that may no longer be circle paths.
- `rulesR8R9R10` :821 — `intoCArrows` snapshotted while R8/R9/R10 flip endpoints at `a`.
- `ddpOrient` :624 — `cParents` snapshotted before the BFS mutates the graph via `doDdpOrientation`.
