#include "search/fas.h"
#include "util/choice_generator.h"
#include "util/log_stream.h"
#include <algorithm>

namespace tetrad {

Fas::Fas(IndependenceTest* test) : test_(test) {}

Graph Fas::search() {
    return search(test_->getVariables());
}

Graph Fas::search(const std::vector<NodePtr>& nodes) {
    // Build complete undirected graph
    Graph graph(nodes);
    for (size_t i = 0; i < nodes.size(); i++) {
        for (size_t j = i + 1; j < nodes.size(); j++) {
            // Skip if forbidden by knowledge in both directions
            if (knowledge_.isForbidden(nodes[i]->getName(), nodes[j]->getName()) &&
                knowledge_.isForbidden(nodes[j]->getName(), nodes[i]->getName())) {
                continue;
            }
            graph.addUndirectedEdge(nodes[i], nodes[j]);
        }
    }

    int n = static_cast<int>(test_->getVariables().size());
    int depthCap = (depth_ < 0) ? (n - 1) : depth_;

    for (int d = 0; d <= depthCap; d++) {
        if (verbose_) {
            logStream() << "FAS depth: " << d << std::endl;
        }

        // For stable mode, freeze adjacencies
        Graph checkAdj = stable_ ? Graph(graph) : graph;
        bool anyRemoved = searchAtDepth(checkAdj, graph, d);

        if (!anyRemoved && freeDegree(graph) <= d) {
            break;
        }
    }

    return graph;
}

bool Fas::searchAtDepth(const Graph& checkAdj, Graph& modify, int d) {
    std::vector<EdgeRemoval> removals;

    auto nodes = checkAdj.getNodes();
    for (const auto& x : nodes) {
        auto adjNodes = checkAdj.getAdjacentNodes(x);
        for (const auto& y : adjNodes) {
            // Process each unordered pair once (canonical order: x < y by name)
            if (x->getName() >= y->getName()) continue;
            decideOnePair(checkAdj, d, x, y, removals);
        }
    }

    // Sort removals for deterministic ordering
    std::sort(removals.begin(), removals.end(),
        [](const EdgeRemoval& a, const EdgeRemoval& b) {
            int cmp = a.x->getName().compare(b.x->getName());
            if (cmp != 0) return cmp < 0;
            cmp = a.y->getName().compare(b.y->getName());
            if (cmp != 0) return cmp < 0;
            return a.S.size() < b.S.size();
        });

    bool anyRemoved = false;
    for (const auto& r : removals) {
        if (modify.isAdjacentTo(r.x, r.y)) {
            modify.removeEdge(r.x, r.y);
            sepset_.set(r.x, r.y, r.S);
            anyRemoved = true;
        }
    }

    return anyRemoved;
}

void Fas::decideOnePair(const Graph& checkAdj, int d, const NodePtr& x,
                         const NodePtr& y, std::vector<EdgeRemoval>& removals) {
    // Side X: subsets of adj(x) \ {y}
    auto adjx = checkAdj.getAdjacentNodes(x);
    auto ppx = possibleParents(x, adjx, knowledge_, y);

    if (static_cast<int>(ppx.size()) >= d) {
        ChoiceGenerator gen(static_cast<int>(ppx.size()), d);
        const int* choice;
        while ((choice = gen.next()) != nullptr) {
            std::set<NodePtr> S;
            for (int i = 0; i < d; i++) {
                S.insert(ppx[choice[i]]);
            }

            auto result = test_->checkIndependence(x, y, S);
            if (result.isIndependent()) {
                removals.push_back({x, y, S, result.pValue});
                return;
            }
        }
    }

    // Side Y: subsets of adj(y) \ {x}
    auto adjy = checkAdj.getAdjacentNodes(y);
    auto ppy = possibleParents(y, adjy, knowledge_, x);

    if (static_cast<int>(ppy.size()) >= d) {
        ChoiceGenerator gen(static_cast<int>(ppy.size()), d);
        const int* choice;
        while ((choice = gen.next()) != nullptr) {
            std::set<NodePtr> S;
            for (int i = 0; i < d; i++) {
                S.insert(ppy[choice[i]]);
            }

            auto result = test_->checkIndependence(x, y, S);
            if (result.isIndependent()) {
                removals.push_back({x, y, S, result.pValue});
                return;
            }
        }
    }
}

int Fas::freeDegree(const Graph& graph) const {
    int maxDeg = 0;
    for (const auto& node : graph.getNodes()) {
        int deg = static_cast<int>(graph.getAdjacentNodes(node).size());
        if (deg > 0 && deg - 1 > maxDeg) {
            maxDeg = deg - 1;
        }
    }
    return maxDeg;
}

std::vector<NodePtr> Fas::possibleParents(const NodePtr& x,
    const std::vector<NodePtr>& adjx, const Knowledge& knowledge, const NodePtr& y) {

    std::vector<NodePtr> result;
    for (const auto& z : adjx) {
        if (!z) continue;
        if (*z == *x) continue;
        if (*z == *y) continue;
        // z is a possible parent if it's not forbidden to be a parent of x
        // and x is not required to be a parent of z
        if (!knowledge.isForbidden(z->getName(), x->getName()) &&
            !knowledge.isRequired(x->getName(), z->getName())) {
            result.push_back(z);
        }
    }
    return result;
}

} // namespace tetrad
