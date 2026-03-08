#include "search/bes_permutation.h"
#include "util/sublist_generator.h"
#include <algorithm>

namespace tetrad {

BesPermutation::BesPermutation(Score& score)
    : score_(score), variables_(score.getVariables()) {}

void BesPermutation::bes(Graph& graph, const std::vector<NodePtr>& order,
                          const std::vector<NodePtr>& suborder) {
    std::unordered_map<NodePtr, int> hashIndices;
    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        hashIndices[order[i]] = i;
    }

    sortedArrows_.clear();
    arrowsMap_.clear();
    arrowIndex_ = 0;

    std::set<NodePtr> orderSet(order.begin(), order.end());
    reevaluateBackward(orderSet, graph, hashIndices);

    while (!sortedArrows_.empty()) {
        Arrow arrow = *sortedArrows_.begin();
        sortedArrows_.erase(sortedArrows_.begin());

        const NodePtr& x = arrow.a;
        const NodePtr& y = arrow.b;

        if (!graph.isAdjacentTo(x, y)) continue;

        Edge edge = graph.getEdge(x, y);
        if (edge.pointsTowards(x)) continue;

        if (getNaYX(x, y, graph) != arrow.naYX) continue;

        auto graphParents = graph.getParents(y);
        std::set<NodePtr> parentSet(graphParents.begin(), graphParents.end());
        if (parentSet != arrow.parents) continue;

        if (!validDelete(x, y, arrow.hOrT, arrow.naYX, graph, suborder)) continue;

        std::set<NodePtr> complement(arrow.naYX);
        for (const auto& h : arrow.hOrT) complement.erase(h);

        double bump = deleteEval(x, y, complement, arrow.parents, hashIndices);

        doDelete(x, y, arrow.hOrT, bump, arrow.naYX, graph);

        std::set<NodePtr> process = revertToCPDAG(graph);
        process.insert(x);
        process.insert(y);
        for (const auto& adj : graph.getAdjacentNodes(x)) process.insert(adj);
        for (const auto& adj : graph.getAdjacentNodes(y)) process.insert(adj);

        reevaluateBackward(process, graph, hashIndices);
    }
}

void BesPermutation::doDelete(const NodePtr& x, const NodePtr& y,
                               const std::set<NodePtr>& H, double /*bump*/,
                               const std::set<NodePtr>& naYX, Graph& graph) {
    std::set<NodePtr> diff(naYX);
    for (const auto& h : H) diff.erase(h);

    Edge oldxy = graph.getEdge(x, y);
    graph.removeEdge(oldxy);

    for (const auto& h : H) {
        if (graph.isParentOf(h, y) || graph.isParentOf(h, x)) continue;

        Edge oldyh = graph.getEdge(y, h);
        graph.removeEdge(oldyh);
        graph.addDirectedEdge(y, h);

        Edge oldxh = graph.getEdge(x, h);
        if (!oldxh.isNull() && isUndirectedEdge(oldxh)) {
            graph.removeEdge(oldxh);
            graph.addDirectedEdge(x, h);
        }
    }
}

double BesPermutation::deleteEval(const NodePtr& x, const NodePtr& y,
                                   const std::set<NodePtr>& complement,
                                   const std::set<NodePtr>& parents,
                                   const std::unordered_map<NodePtr, int>& hashIndices) {
    std::set<NodePtr> set(complement);
    set.insert(parents.begin(), parents.end());
    set.erase(x);
    return -scoreGraphChange(x, y, set, hashIndices);
}

double BesPermutation::scoreGraphChange(const NodePtr& x, const NodePtr& y,
                                         const std::set<NodePtr>& parents,
                                         const std::unordered_map<NodePtr, int>& hashIndices) {
    int xIndex = hashIndices.at(x);
    int yIndex = hashIndices.at(y);

    std::vector<int> parentIndices;
    parentIndices.reserve(parents.size());
    for (const auto& parent : parents) {
        parentIndices.push_back(hashIndices.at(parent));
    }

    return score_.localScoreDiff(xIndex, yIndex, parentIndices);
}

bool BesPermutation::validDelete(const NodePtr& x, const NodePtr& y,
                                  const std::set<NodePtr>& H,
                                  const std::set<NodePtr>& naYX,
                                  Graph& graph,
                                  const std::vector<NodePtr>& suborder) {
    if (existsKnowledge()) {
        for (const auto& h : H) {
            if (knowledge_.isForbidden(x->getName(), h->getName())) return false;
            if (knowledge_.isForbidden(y->getName(), h->getName())) return false;
        }
    }

    std::set<NodePtr> diff(naYX);
    for (const auto& h : H) diff.erase(h);
    if (!isClique(diff, graph)) return false;

    if (existsKnowledge()) {
        Graph graphCopy(graph);
        Edge oldxy = graphCopy.getEdge(x, y);
        graphCopy.removeEdge(oldxy);

        for (const auto& h : H) {
            if (graphCopy.isParentOf(h, y) || graphCopy.isParentOf(h, x)) continue;
            Edge oldyh = graphCopy.getEdge(y, h);
            graphCopy.removeEdge(oldyh);
            graphCopy.addDirectedEdge(y, h);

            Edge oldxh = graphCopy.getEdge(x, h);
            if (!oldxh.isNull() && isUndirectedEdge(oldxh)) {
                graphCopy.removeEdge(oldxh);
                graphCopy.addDirectedEdge(x, h);
            }
        }

        revertToCPDAG(graphCopy);

        // Check valid sink ordering (reverse of suborder)
        std::vector<NodePtr> initialOrder(suborder.rbegin(), suborder.rend());

        while (!initialOrder.empty()) {
            bool foundValid = false;
            for (auto it = initialOrder.begin(); it != initialOrder.end(); ++it) {
                if (!invalidSink(*it, graphCopy)) {
                    graphCopy.removeNode(*it);
                    initialOrder.erase(it);
                    foundValid = true;
                    break;
                }
            }
            if (!foundValid) return false;
        }
    }

    return true;
}

bool BesPermutation::invalidSink(const NodePtr& x, const Graph& graph) {
    std::vector<NodePtr> neighbors;

    for (const auto& edge : graph.getEdges(x)) {
        if (edge.getDistalEndpoint(x) == Endpoint::ARROW) return true;
        if (edge.getEndpoint(x) == Endpoint::TAIL) {
            neighbors.push_back(edge.getDistalNode(x));
        }
    }

    for (int i = 0; i < static_cast<int>(neighbors.size()); i++) {
        for (int j = i + 1; j < static_cast<int>(neighbors.size()); j++) {
            if (!graph.isAdjacentTo(neighbors[i], neighbors[j])) return true;
        }
    }

    return false;
}

std::set<NodePtr> BesPermutation::getNaYX(const NodePtr& x, const NodePtr& y,
                                            const Graph& graph) {
    std::set<NodePtr> nayx;
    for (const auto& z : graph.getAdjacentNodes(y)) {
        if (z == x) continue;
        Edge yz = graph.getEdge(y, z);
        if (!isUndirectedEdge(yz)) continue;
        if (!graph.isAdjacentTo(z, x)) continue;
        nayx.insert(z);
    }
    return nayx;
}

void BesPermutation::reevaluateBackward(const std::set<NodePtr>& toProcess,
                                         Graph& graph,
                                         const std::unordered_map<NodePtr, int>& hashIndices) {
    for (const auto& r : toProcess) {
        for (const auto& w : toProcess) {
            if (r == w) continue;
            Edge e = graph.getEdge(w, r);
            if (e.isNull()) continue;

            if (e.pointsTowards(r)) {
                calculateArrowsBackward(w, r, graph, hashIndices);
            } else if (e.pointsTowards(w)) {
                calculateArrowsBackward(r, w, graph, hashIndices);
            } else {
                calculateArrowsBackward(w, r, graph, hashIndices);
                calculateArrowsBackward(r, w, graph, hashIndices);
            }
        }
    }
}

void BesPermutation::calculateArrowsBackward(const NodePtr& a, const NodePtr& b,
                                               Graph& graph,
                                               const std::unordered_map<NodePtr, int>& hashIndices) {
    if (existsKnowledge()) {
        if (!knowledge_.noEdgeRequired(a->getName(), b->getName())) return;
    }

    std::set<NodePtr> naYX = getNaYX(a, b, graph);
    auto graphParents = graph.getParents(b);
    std::set<NodePtr> parents(graphParents.begin(), graphParents.end());

    // Check if config changed
    std::string key = a->getName() + "->" + b->getName();
    ArrowConfig config{naYX, parents};
    auto it = arrowsMap_.find(key);
    if (it != arrowsMap_.end() && it->second == config) return;
    arrowsMap_[key] = config;

    std::vector<NodePtr> naYXList(naYX.begin(), naYX.end());
    int depth = static_cast<int>(naYXList.size());
    SublistGenerator gen(static_cast<int>(naYXList.size()), depth);

    std::set<NodePtr> maxComplement;
    double maxBump = -std::numeric_limits<double>::infinity();

    bool valid;
    while (true) {
        const auto& choice = gen.next(valid);
        if (!valid) break;

        std::set<NodePtr> complement;
        for (int idx : choice) {
            complement.insert(naYXList[idx]);
        }

        double bump = deleteEval(a, b, complement, parents, hashIndices);
        if (bump > maxBump) {
            maxBump = bump;
            maxComplement = complement;
        }
    }

    if (maxBump > 0) {
        std::set<NodePtr> H(naYX);
        for (const auto& c : maxComplement) H.erase(c);

        Arrow arrow;
        arrow.bump = maxBump;
        arrow.a = a;
        arrow.b = b;
        arrow.hOrT = H;
        arrow.naYX = naYX;
        arrow.parents = parents;
        arrow.index = arrowIndex_++;
        sortedArrows_.insert(arrow);
    }
}

bool BesPermutation::isClique(const std::set<NodePtr>& nodes, const Graph& graph) {
    std::vector<NodePtr> nodeList(nodes.begin(), nodes.end());
    for (int i = 0; i < static_cast<int>(nodeList.size()); i++) {
        for (int j = i + 1; j < static_cast<int>(nodeList.size()); j++) {
            if (!graph.isAdjacentTo(nodeList[i], nodeList[j])) return false;
        }
    }
    return true;
}

std::set<NodePtr> BesPermutation::revertToCPDAG(Graph& graph) {
    MeekRules rules;
    rules.setKnowledge(knowledge_);
    rules.setMeekPreventCycles(false);
    rules.setVerbose(verbose_);
    return rules.orientImplied(graph);
}

bool BesPermutation::isUndirectedEdge(const Edge& e) {
    return e.getEndpoint1() == Endpoint::TAIL && e.getEndpoint2() == Endpoint::TAIL;
}

} // namespace tetrad
