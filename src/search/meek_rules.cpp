#include "search/meek_rules.h"
#include "util/log_stream.h"
#include <algorithm>

namespace tetrad {

std::set<NodePtr> MeekRules::orientImplied(Graph& graph) {
    std::set<NodePtr> visited;

    if (revertToUnshieldedColliders_) {
        // Step 1: Collect unshielded colliders from current directed structure.
        // A triple A→B←C where A-C are not adjacent is an unshielded collider.
        struct Triple { NodePtr a, b, c; };
        std::vector<Triple> colliders;

        for (const auto& b : graph.getNodes()) {
            auto parents = graph.getParents(b);
            for (size_t i = 0; i < parents.size(); i++) {
                for (size_t j = i + 1; j < parents.size(); j++) {
                    const auto& a = parents[i];
                    const auto& c = parents[j];
                    if (!graph.isAdjacentTo(a, c)) {
                        colliders.push_back({a, b, c});
                    }
                }
            }
        }

        // Step 2: Un-direct all directed edges (convert to undirected),
        // except those pinned by knowledge.
        // Mirrors Java MeekRules.revertToUnshieldedColliders():
        //   skip undirecting z→y if knowledge.isForbidden(y,z) [reverse is forbidden]
        //                        OR knowledge.isRequired(z,y)   [this direction is required]
        std::vector<Edge> toUndirect;
        for (const auto& e : graph.getEdges()) {
            if (!isUndirectedEdge(e)) {
                // Only un-direct TAIL→ARROW (standard directed) edges
                if (e.getEndpoint1() == Endpoint::TAIL && e.getEndpoint2() == Endpoint::ARROW) {
                    const auto& tail = e.getNode1();
                    const auto& head = e.getNode2();
                    if (knowledge_.isEmpty() ||
                        (!knowledge_.isRequired(tail->getName(), head->getName()) &&
                         !knowledge_.isForbidden(head->getName(), tail->getName()))) {
                        toUndirect.push_back(e);
                    }
                } else if (e.getEndpoint1() == Endpoint::ARROW && e.getEndpoint2() == Endpoint::TAIL) {
                    const auto& tail = e.getNode2();
                    const auto& head = e.getNode1();
                    if (knowledge_.isEmpty() ||
                        (!knowledge_.isRequired(tail->getName(), head->getName()) &&
                         !knowledge_.isForbidden(head->getName(), tail->getName()))) {
                        toUndirect.push_back(e);
                    }
                }
            }
        }
        for (const auto& e : toUndirect) {
            graph.removeEdge(e);
            graph.addUndirectedEdge(e.getNode1(), e.getNode2());
        }

        // Step 3: Re-orient the unshielded colliders A→B←C.
        for (const auto& t : colliders) {
            if (!graph.isAdjacentTo(t.a, t.b) || !graph.isAdjacentTo(t.c, t.b)) continue;
            if (!isArrowheadAllowed(t.a, t.b, knowledge_)) continue;
            if (!isArrowheadAllowed(t.c, t.b, knowledge_)) continue;

            Edge ab = graph.getEdge(t.a, t.b);
            if (!ab.isNull() && isUndirectedEdge(ab)) {
                graph.removeEdge(ab);
                graph.addDirectedEdge(t.a, t.b);
                visited.insert(t.a);
                visited.insert(t.b);
            }
            Edge cb = graph.getEdge(t.c, t.b);
            if (!cb.isNull() && isUndirectedEdge(cb)) {
                graph.removeEdge(cb);
                graph.addDirectedEdge(t.c, t.b);
                visited.insert(t.c);
                visited.insert(t.b);
            }
        }
    }

    // Step 4: Propagate orientation via Meek rules R1–R4.
    bool oriented = true;
    while (oriented) {
        oriented = false;

        // Collect undirected edges
        std::vector<Edge> undirected;
        for (const auto& e : graph.getEdges()) {
            if (isUndirectedEdge(e)) {
                undirected.push_back(e);
            }
        }

        for (const auto& edge : undirected) {
            auto x = edge.getNode1();
            auto y = edge.getNode2();

            // Check if this edge is still undirected
            Edge cur = graph.getEdge(x, y);
            if (cur.isNull() || !isUndirectedEdge(cur)) continue;

            if (meekR1(x, y, graph, visited)) { oriented = true; }
            else if (meekR1(y, x, graph, visited)) { oriented = true; }
            else if (meekR2(x, y, graph, visited)) { oriented = true; }
            else if (meekR2(y, x, graph, visited)) { oriented = true; }
            else if (meekR3(x, y, graph, visited)) { oriented = true; }
            else if (meekR3(y, x, graph, visited)) { oriented = true; }
            else if (meekR4(x, y, graph, visited)) { oriented = true; }
            else if (meekR4(y, x, graph, visited)) { oriented = true; }
        }
    }

    return visited;
}

// R1: if a→b, b--c, and a not adj c, then b→c
bool MeekRules::meekR1(const NodePtr& b, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited) {
    for (const auto& a : graph.getParents(b)) {
        if (graph.isAdjacentTo(c, a)) continue;
        if (direct(b, c, graph, visited)) {
            if (verbose_) {
                logStream() << "Meek R1: " << a->getName() << " --> "
                          << b->getName() << " --- " << c->getName() << std::endl;
            }
            return true;
        }
    }
    return false;
}

// R2: if a→b→c, a--c, then a→c
bool MeekRules::meekR2(const NodePtr& a, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited) {
    auto common = getCommonAdjacents(a, c, graph);

    for (const auto& b : common) {
        if (isDirected(graph, a, b) && isDirected(graph, b, c)) {
            if (direct(a, c, graph, visited)) {
                if (verbose_) {
                    logStream() << "Meek R2: " << a->getName() << " --> "
                              << b->getName() << " --> " << c->getName() << std::endl;
                }
                return true;
            }
        }
        if (isDirected(graph, c, b) && isDirected(graph, b, a)) {
            if (direct(c, a, graph, visited)) {
                if (verbose_) {
                    logStream() << "Meek R2: " << c->getName() << " --> "
                              << b->getName() << " --> " << a->getName() << std::endl;
                }
                return true;
            }
        }
    }
    return false;
}

// R3: if d--a, d--b, d--c, b→a, c→a, b not adj c, then d→a
bool MeekRules::meekR3(const NodePtr& d, const NodePtr& a, Graph& graph, std::set<NodePtr>& visited) {
    auto common = getCommonAdjacents(a, d, graph);

    if (common.size() < 2) return false;

    for (size_t i = 0; i < common.size(); i++) {
        for (size_t j = i + 1; j < common.size(); j++) {
            auto b = common[i];
            auto c = common[j];

            if (graph.isAdjacentTo(b, c)) continue;

            // Check: d--a undirected, d--b undirected, d--c undirected, b→a, c→a
            if (isUndirected(graph, d, a) &&
                isUndirected(graph, d, b) &&
                isUndirected(graph, d, c) &&
                isDirected(graph, b, a) &&
                isDirected(graph, c, a)) {
                if (direct(d, a, graph, visited)) {
                    if (verbose_) {
                        logStream() << "Meek R3: " << d->getName() << " --> "
                                  << a->getName() << std::endl;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// R4: if a--b, c→b, d→c, a--d, b not adj d, then a→b
// Only active when knowledge is non-empty.
bool MeekRules::meekR4(const NodePtr& a, const NodePtr& b, Graph& graph, std::set<NodePtr>& visited) {
    if (knowledge_.isEmpty()) return false;

    bool oriented = false;

    for (const auto& c : graph.getParents(b)) {
        auto adj = getCommonAdjacents(a, c, graph);

        for (const auto& d : adj) {
            if (*d == *b) continue;
            if (graph.isAdjacentTo(b, d)) continue;

            Edge dc = graph.getEdge(d, c);
            if (dc.isNull()) continue;
            // d→c: check pointsTowards c
            if (!dc.pointsTowards(c)) continue;

            Edge ad = graph.getEdge(a, d);
            if (ad.isNull()) continue;
            // a--d must be undirected
            if (!isUndirectedEdge(ad)) continue;

            if (direct(a, b, graph, visited)) {
                if (verbose_) {
                    logStream() << "Meek R4: " << a->getName() << " --> "
                              << b->getName() << " using " << c->getName()
                              << ", " << d->getName() << std::endl;
                }
                oriented = true;
            }
        }
    }

    return oriented;
}

bool MeekRules::direct(const NodePtr& a, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited) {
    if (!isArrowheadAllowed(a, c, knowledge_)) return false;

    Edge e = graph.getEdge(a, c);
    if (e.isNull()) return false;
    if (!isUndirectedEdge(e)) return false;

    // Cycle prevention: don't orient if it would create a directed cycle
    if (meekPreventCycles_ && graph.existsDirectedPath(c, a)) {
        return false;
    }

    graph.removeEdge(e);
    graph.addDirectedEdge(a, c);

    visited.insert(a);
    visited.insert(c);
    return true;
}

bool MeekRules::isUndirectedEdge(const Edge& e) {
    return e.getEndpoint1() == Endpoint::TAIL && e.getEndpoint2() == Endpoint::TAIL;
}

bool MeekRules::isUndirected(const Graph& g, const NodePtr& x, const NodePtr& y) {
    Edge e = g.getEdge(x, y);
    if (e.isNull()) return false;
    return isUndirectedEdge(e);
}

bool MeekRules::isDirected(const Graph& g, const NodePtr& x, const NodePtr& y) {
    return g.isDirectedFromTo(x, y);
}

std::vector<NodePtr> MeekRules::getCommonAdjacents(const NodePtr& x, const NodePtr& y, const Graph& graph) {
    auto adjx = graph.getAdjacentNodes(x);
    auto adjy = graph.getAdjacentNodes(y);

    std::vector<NodePtr> common;
    for (const auto& n : adjx) {
        for (const auto& m : adjy) {
            if (*n == *m) {
                common.push_back(n);
                break;
            }
        }
    }
    return common;
}

bool MeekRules::isArrowheadAllowed(const NodePtr& from, const NodePtr& to, const Knowledge& knowledge) {
    if (knowledge.isEmpty()) return true;
    return !knowledge.isRequired(to->getName(), from->getName()) &&
           !knowledge.isForbidden(from->getName(), to->getName());
}

} // namespace tetrad
