#include "search/meek_rules.h"
#include <algorithm>
#include <iostream>

namespace tetrad {

std::set<NodePtr> MeekRules::orientImplied(Graph& graph) {
    std::set<NodePtr> visited;
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
                std::cout << "Meek R1: " << a->getName() << " --> "
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
                    std::cout << "Meek R2: " << a->getName() << " --> "
                              << b->getName() << " --> " << c->getName() << std::endl;
                }
                return true;
            }
        }
        if (isDirected(graph, c, b) && isDirected(graph, b, a)) {
            if (direct(c, a, graph, visited)) {
                if (verbose_) {
                    std::cout << "Meek R2: " << c->getName() << " --> "
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
                        std::cout << "Meek R3: " << d->getName() << " --> "
                                  << a->getName() << std::endl;
                    }
                    return true;
                }
            }
        }
    }
    return false;
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
