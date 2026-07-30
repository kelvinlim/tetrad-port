// Port of edu.cmu.tetrad.search.utils.MeekRules (Tetrad 7.6.3).
//
// This is a deliberately literal, bug-for-bug transcription of MeekRules.java.
// Where the Java looks wrong, the Java behaviour is reproduced and the issue is
// recorded in SUSPECTED_BUGS.md. Do not "fix" anything here without also
// changing the reference expectations.

#include "search/meek_rules.h"

#include "util/java_hash.h"
#include "util/log_stream.h"

#include <algorithm>
#include <string>
#include <vector>

namespace tetrad {

namespace {

// ---------------------------------------------------------------------------
// MeekRules.java:47   boolean useRule4;
// MeekRules.java:63-65
//     public MeekRules() {
//         this.useRule4 = !this.knowledge.isEmpty();
//     }
//
// `knowledge` is initialised at MeekRules.java:48 to `new Knowledge()`, which is
// empty, so the constructor always assigns useRule4 = false.  setKnowledge()
// (MeekRules.java:123-125) never recomputes it, and nothing else in the 7.6.3
// tree assigns to it (it is package-private but unreferenced elsewhere).
// Therefore useRule4 is permanently false and meekR4 never does anything.
//
// The C++ header has no useRule4_ member, which is consistent with the value
// being a constant.  It is modelled here as a file-scope constant so that the
// transcription of meekR4 below stays visible and reviewable.
// See SUSPECTED_BUGS.md #1.
// ---------------------------------------------------------------------------
const bool USE_RULE4 = false;

void logMsg(bool verbose, const std::string& message) {
    // MeekRules.java:355-359 (log): TetradLogger only when this.verbose.
    if (verbose) {
        logStream() << message << "\n";
    }
}

// Java: `p != q` in revertToUnshieldedColliders is *reference* comparison on
// Node objects.  Within a single graph each variable is a single Node instance,
// so reference identity == name identity.  NodePtr comparison is the literal
// analogue.
bool sameNode(const NodePtr& a, const NodePtr& b) {
    if (a == b) return true;          // pointer identity, as in Java
    if (!a || !b) return false;
    return *a == *b;                  // Node::operator== compares names
}

bool containsNode(const std::vector<NodePtr>& v, const NodePtr& n) {
    for (const NodePtr& m : v) {
        if (sameNode(m, n)) return true;
    }
    return false;
}

// MeekRules.java:327-353
//     private boolean revertToUnshieldedColliders(Node y, Graph graph, Set<Node> visited)
//
// Free function because the C++ header does not declare it as a member; it needs
// only the knowledge and the verbose-independent state, both passed in.
bool revertToUnshieldedCollidersAt(const NodePtr& y, Graph& graph,
                                   std::set<NodePtr>& visited,
                                   const Knowledge& knowledge) {
    bool did = false;

    // Java: `List<Node> parents = graph.getParents(y);` -- EdgeListGraph returns
    // the *cached* ArrayList out of parentsHash (EdgeListGraph.java:366-390).
    // graph.removeEdge(p, y) evicts the cache entry but does not touch the list
    // object, so the loop below runs over a snapshot taken before any mutation.
    // C++ getParents() returns by value, which is the same snapshot semantics.
    //
    // ITERATION ORDER: the Java list is built by walking edgeLists.get(y), a
    // per-node HashSet<Edge>; we cannot reproduce that order.  See
    // SUSPECTED_BUGS.md "iteration order" #A.  The outcome of this method is
    // order-independent (see the note there), so this is benign.
    std::vector<NodePtr> parents = graph.getParents(y);

    // Java's labelled `P: for (...) { for (...) { ... continue P; } ... }`.
    for (const NodePtr& p : parents) {
        bool skip = false;
        for (const NodePtr& q : parents) {
            if (!sameNode(p, q) && !graph.isAdjacentTo(p, q)) {
                skip = true;  // `continue P` -- p heads an unshielded collider
                break;
            }
        }
        if (skip) continue;

        if (knowledge.isForbidden(y->getName(), p->getName()) ||
            knowledge.isRequired(p->getName(), y->getName())) {
            continue;
        }

        graph.removeEdge(p, y);
        graph.addUndirectedEdge(p, y);

        visited.insert(p);
        visited.insert(y);

        did = true;
    }

    return did;
}

// MeekRules.java:173-185
//     private void revertToUnshieldedColliders(List<Node> nodes, Graph graph, Set<Node> visited)
void revertToUnshieldedCollidersAll(const std::vector<NodePtr>& nodes, Graph& graph,
                                    std::set<NodePtr>& visited,
                                    const Knowledge& knowledge) {
    bool reverted = true;

    while (reverted) {
        reverted = false;

        for (const NodePtr& node : nodes) {
            if (revertToUnshieldedCollidersAt(node, graph, visited, knowledge)) {
                reverted = true;
            }
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// MeekRules.java:80-115  public Set<Node> orientImplied(Graph graph)
// ---------------------------------------------------------------------------
std::set<NodePtr> MeekRules::orientImplied(Graph& graph) {
    // Java: `Set<Node> visited = new HashSet<>();`  Only membership matters; the
    // C++ header fixes this to std::set<NodePtr> (ordered by pointer address).
    std::set<NodePtr> visited;

    if (revertToUnshieldedColliders_) {
        // Java passes graph.getNodes() directly; copy here because the C++
        // accessor returns a reference into the graph we are about to mutate.
        std::vector<NodePtr> nodes = graph.getNodes();
        revertToUnshieldedCollidersAll(nodes, graph, visited, knowledge_);
    }

    bool oriented = true;

    while (oriented) {
        oriented = false;

        // Java: `for (Edge edge : graph.getEdges())`.  EdgeListGraph.getEdges()
        // (EdgeListGraph.java:755-757) returns `new HashSet<>(this.edgesSet)`,
        // i.e. a fresh snapshot iterated in HashSet bucket order with a capacity
        // derived from the number of edges.  Because it is a snapshot, edges
        // mutated by the rules below are still visited with their *old*
        // endpoints -- this matters for the isUndirectedEdge() test just below,
        // which therefore inspects the stale edge object, not the live graph.
        std::vector<Edge> edges = graph.getEdges();
        sortEdgesByJavaHashOrder(edges, graph.getNumEdges());

        for (const Edge& edge : edges) {
            if (!isUndirectedEdge(edge)) continue;

            const NodePtr x = edge.getNode1();
            const NodePtr y = edge.getNode2();

            // The else-if chain is significant: at most one rule fires per edge
            // per sweep, and the rules are tried in this exact order.
            if (meekR1(x, y, graph, visited)) oriented = true;
            else if (meekR1(y, x, graph, visited)) oriented = true;
            else if (meekR2(x, y, graph, visited)) oriented = true;
            else if (meekR2(y, x, graph, visited)) oriented = true;
            else if (meekR3(x, y, graph, visited)) oriented = true;
            else if (meekR3(y, x, graph, visited)) oriented = true;
            else if (meekR4(x, y, graph, visited)) oriented = true;
            else if (meekR4(y, x, graph, visited)) oriented = true;
        }
    }

    return visited;
}

// ---------------------------------------------------------------------------
// MeekRules.java:190-201  meekR1: if a-->b, b---c, and a not adj to c, then b-->c
// ---------------------------------------------------------------------------
bool MeekRules::meekR1(const NodePtr& b, const NodePtr& c, Graph& graph,
                       std::set<NodePtr>& visited) {
    // ITERATION ORDER: graph.getParents(b) -- see SUSPECTED_BUGS.md #A.  The
    // result of this method does not depend on the order (the direct() call is
    // independent of `a`), only the log message does.
    std::vector<NodePtr> parents = graph.getParents(b);

    for (const NodePtr& a : parents) {
        if (graph.isAdjacentTo(c, a)) continue;
        if (direct(b, c, graph, visited)) {
            logMsg(verbose_, "Meek R1 triangle (" + a->getName() + "-->" + b->getName() +
                                 "---" + c->getName() + ") oriented " +
                                 graph.getEdge(b, c).toString());
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// MeekRules.java:206-234  meekR2 (+ r2Helper): if a-->b-->c and a--c, then a-->c
// ---------------------------------------------------------------------------
bool MeekRules::meekR2(const NodePtr& a, const NodePtr& c, Graph& graph,
                       std::set<NodePtr>& visited) {
    // MeekRules.java:207-208
    //     List<Node> adjacentNodes = graph.getAdjacentNodes(c);
    //     adjacentNodes.remove(a);
    // `adjacentNodes` is never read afterwards and getAdjacentNodes() returns a
    // fresh ArrayList, so these two lines are pure dead code with no side
    // effect on the graph.  Omitted.  See SUSPECTED_BUGS.md #2.

    // ITERATION ORDER MATTERS HERE: different `b` can trigger opposite
    // orientations (a-->c vs c-->a).  Java iterates a HashSet<Node>; see
    // getCommonAdjacents() below for how that order is modelled.
    std::vector<NodePtr> common = getCommonAdjacents(a, c, graph);

    for (const NodePtr& b : common) {
        if (isDirected(graph, a, b) && isDirected(graph, b, c)) {
            // r2Helper(a, b, c): logs unconditionally, even when direct() failed.
            bool directed = direct(a, c, graph, visited);
            logMsg(verbose_, "Meek R2 triangle (" + a->getName() + "-->" + b->getName() +
                                 "-->" + c->getName() + ", " + a->getName() + "---" +
                                 c->getName() + ")");
            if (directed) return true;
        }

        if (isDirected(graph, c, b) && isDirected(graph, b, a)) {
            // r2Helper(c, b, a)
            bool directed = direct(c, a, graph, visited);
            logMsg(verbose_, "Meek R2 triangle (" + c->getName() + "-->" + b->getName() +
                                 "-->" + a->getName() + ", " + c->getName() + "---" +
                                 a->getName() + ")");
            if (directed) return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// MeekRules.java:239-278  meekR3 (+ r3Helper):
//     if d--a, d--b, d--c, b-->a, c-->a and b,c nonadjacent, then d-->a
// ---------------------------------------------------------------------------
bool MeekRules::meekR3(const NodePtr& d, const NodePtr& a, Graph& graph,
                       std::set<NodePtr>& visited) {
    // Note the argument order: getCommonAdjacents(a, d, ...) -- `a` first.
    std::vector<NodePtr> adjacentNodes = getCommonAdjacents(a, d, graph);

    if (adjacentNodes.size() < 2) {
        return false;
    }

    for (std::size_t i = 0; i < adjacentNodes.size(); i++) {
        for (std::size_t j = i + 1; j < adjacentNodes.size(); j++) {
            const NodePtr& b = adjacentNodes[i];
            const NodePtr& c = adjacentNodes[j];

            if (!graph.isAdjacentTo(b, c)) {
                // r3Helper(a, d, b, c, graph, visited)
                bool b4 = isUndirected(graph, d, a);
                bool b5 = isUndirected(graph, d, b);
                bool b6 = isUndirected(graph, d, c);
                bool b7 = isDirected(graph, b, a);
                bool b8 = isDirected(graph, c, a);

                if (b4 && b5 && b6 && b7 && b8) {
                    bool oriented = direct(d, a, graph, visited);
                    logMsg(verbose_, "Meek R3 " + d->getName() + "--" + a->getName() +
                                         ", " + b->getName() + ", " + c->getName());
                    if (oriented) return true;
                }
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// MeekRules.java:280-302  meekR4
// Gated on useRule4, which is permanently false -- see USE_RULE4 above.
// ---------------------------------------------------------------------------
bool MeekRules::meekR4(const NodePtr& a, const NodePtr& b, Graph& graph,
                       std::set<NodePtr>& visited) {
    if (!USE_RULE4) {
        return false;
    }

    // ITERATION ORDER: graph.getParents(b) -- see SUSPECTED_BUGS.md #A.
    std::vector<NodePtr> parents = graph.getParents(b);

    for (const NodePtr& c : parents) {
        std::vector<NodePtr> adj = getCommonAdjacents(a, c, graph);

        // Java: adj.remove(b)  -- removes at most one element (Set.remove).
        for (auto it = adj.begin(); it != adj.end(); ++it) {
            if (sameNode(*it, b)) {
                adj.erase(it);
                break;
            }
        }

        for (const NodePtr& d : adj) {
            if (graph.isAdjacentTo(b, d)) continue;

            // d is a common adjacent of a and c, so both edges below exist.
            Edge dc = graph.getEdge(d, c);
            if (!dc.pointsTowards(c)) continue;
            if (graph.getEdge(a, d).isDirected()) continue;

            if (direct(a, b, graph, visited)) {
                logMsg(verbose_, "Meek R4 using " + c->getName() + ", " + d->getName());
                return true;
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// MeekRules.java:304-325  direct(a, c): orient a---c as a-->c
// ---------------------------------------------------------------------------
bool MeekRules::direct(const NodePtr& a, const NodePtr& c, Graph& graph,
                       std::set<NodePtr>& visited) {
    if (!MeekRules::isArrowheadAllowed(a, c, knowledge_)) return false;

    // Java: `if (!Edges.isUndirectedEdge(graph.getEdge(a, c))) return false;`
    // getEdge() returns null when a and c are not adjacent, which would NPE in
    // Java; isUndirected() guards adjacency first and returns false instead.
    // Callers only ever reach here for adjacent pairs, so the behaviour agrees.
    if (!MeekRules::isUndirected(graph, a, c)) return false;

    Edge before = graph.getEdge(a, c);
    graph.removeEdge(before);

    if (meekPreventCycles_ && graph.existsDirectedPath(c, a)) {
        graph.addEdge(before);
        return false;
    }

    Edge after = directedEdge(a, c);

    visited.insert(a);
    visited.insert(c);

    // MeekRules.java:321 removes `before` a second time; it was already removed
    // at line 309 and the graph no longer contains it, so this is a no-op.
    // Reproduced verbatim.  See SUSPECTED_BUGS.md #3.
    graph.removeEdge(before);
    graph.addEdge(after);

    return true;
}

// ---------------------------------------------------------------------------
// MeekRules.java:68-72  isArrowheadAllowed(from, to, knowledge)
// ---------------------------------------------------------------------------
bool MeekRules::isArrowheadAllowed(const NodePtr& from, const NodePtr& to,
                                   const Knowledge& knowledge) {
    if (knowledge.isEmpty()) return true;
    // Java uses Node.toString(); GraphNode.toString() returns getName().
    return !knowledge.isRequired(to->getName(), from->getName()) &&
           !knowledge.isForbidden(from->getName(), to->getName());
}

// ---------------------------------------------------------------------------
// Edges.isUndirectedEdge(edge): both endpoints TAIL.
// A "null" Edge (no such edge in the graph) has NULL_EP endpoints and so
// returns false here, where Java would throw a NullPointerException.
// ---------------------------------------------------------------------------
bool MeekRules::isUndirectedEdge(const Edge& e) {
    return e.getEndpoint1() == Endpoint::TAIL && e.getEndpoint2() == Endpoint::TAIL;
}

// graph.paths().isUndirectedFromTo(x, y): the edge between x and y exists and is
// TAIL--TAIL.  (Paths.java is not part of the supplied reference set; this is
// the documented 7.6.3 semantics -- see CORRESPONDENCE.md, "assumptions".)
bool MeekRules::isUndirected(const Graph& g, const NodePtr& x, const NodePtr& y) {
    if (!g.isAdjacentTo(x, y)) return false;
    return isUndirectedEdge(g.getEdge(x, y));
}

// graph.paths().isDirectedFromTo(x, y): the (unique) edge between x and y points
// towards y, i.e. Edge.pointsTowards(y) -- arrowhead at y, tail-or-circle at x.
bool MeekRules::isDirected(const Graph& g, const NodePtr& x, const NodePtr& y) {
    if (!g.isAdjacentTo(x, y)) return false;
    return g.getEdge(x, y).pointsTowards(y);
}

// ---------------------------------------------------------------------------
// MeekRules.java:361-365
//     private Set<Node> getCommonAdjacents(Node x, Node y, Graph graph) {
//         Set<Node> adj = new HashSet<>(graph.getAdjacentNodes(x));
//         adj.retainAll(graph.getAdjacentNodes(y));
//         return adj;
//     }
//
// The returned collection is a HashSet whose *capacity* is fixed by
// |adj(x)| (the constructor argument), not by the retained size; retainAll
// preserves the relative order of the survivors.  So the iteration order is:
//   bucket order under capacity = tableSizeFor(|adj(x)|/0.75 + 1), min 16,
//   with ties broken by insertion order, which is itself the HashSet<Node>
//   order produced by EdgeListGraph.getAdjacentNodes (EdgeListGraph.java:561-573).
//
// sortByJavaHashOrder() computes the capacity from the vector it is handed, so
// we sort adj(x) *before* filtering, which reproduces the capacity correctly.
// The remaining approximation is the within-bucket tie-break; see
// SUSPECTED_BUGS.md "iteration order" #B.
// ---------------------------------------------------------------------------
std::vector<NodePtr> MeekRules::getCommonAdjacents(const NodePtr& x, const NodePtr& y,
                                                   const Graph& graph) {
    std::vector<NodePtr> adjX = graph.getAdjacentNodes(x);
    sortByJavaHashOrder(adjX, x);

    std::vector<NodePtr> adjY = graph.getAdjacentNodes(y);

    std::vector<NodePtr> common;
    common.reserve(adjX.size());
    for (const NodePtr& n : adjX) {
        if (containsNode(adjY, n)) common.push_back(n);
    }

    return common;
}

}  // namespace tetrad
