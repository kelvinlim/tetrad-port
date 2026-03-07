#include "search/fges.h"
#include "search/meek_rules.h"
#include "util/sublist_generator.h"
#include <algorithm>
#include <queue>
#include <limits>
#include <cmath>

namespace tetrad {

Fges::Fges(Score& score) : score_(score) {
    variables_.clear();
    for (const auto& node : score.getVariables()) {
        if (node->getNodeType() == NodeType::MEASURED) {
            variables_.push_back(node);
        }
    }

    for (int i = 0; i < static_cast<int>(variables_.size()); i++) {
        hashIndices_[variables_[i]->getName()] = i;
    }

    maxDegree_ = score.getMaxDegree();
}

Graph Fges::search() {
    graph_ = Graph(variables_);

    addRequiredEdges();
    initializeEffectEdges();

    mode_ = Mode::heuristicSpeedup;
    fes();
    bes();

    mode_ = Mode::coverNoncolliders;
    fes();
    bes();

    if (!faithfulnessAssumed_) {
        mode_ = Mode::allowUnfaithfulness;
        fes();
        bes();
    }

    modelScore_ = scoreDag(graph_);
    return graph_;
}

// --- Forward Equivalence Search ---

void Fges::initializeEffectEdges() {
    effectEdgesGraph_ = Graph(variables_);

    for (int i = 0; i < static_cast<int>(variables_.size()); i++) {
        for (int j = i + 1; j < static_cast<int>(variables_.size()); j++) {
            const auto& x = variables_[i];
            const auto& y = variables_[j];

            if (boundGraph_ && !boundGraph_->isAdjacentTo(x, y)) continue;

            double bump = score_.localScoreDiff(indexOf(x), indexOf(y), {});
            if (score_.isEffectEdge(bump)) {
                effectEdgesGraph_.addUndirectedEdge(x, y);
            }

            // Check reverse direction too
            double bumpRev = score_.localScoreDiff(indexOf(y), indexOf(x), {});
            if (score_.isEffectEdge(bumpRev) && !effectEdgesGraph_.isAdjacentTo(x, y)) {
                effectEdgesGraph_.addUndirectedEdge(x, y);
            }
        }
    }
}

void Fges::fes() {
    int maxDeg = maxDegree_ == -1 ? 1000 : maxDegree_;

    sortedArrows_.clear();
    arrowsMap_.clear();
    arrowIndex_ = 0;

    reevaluateForward(std::set<NodePtr>(variables_.begin(), variables_.end()));

    while (!sortedArrows_.empty()) {
        Arrow arrow = *sortedArrows_.begin();
        sortedArrows_.erase(sortedArrows_.begin());

        const auto& x = arrow.a;
        const auto& y = arrow.b;

        if (graph_.isAdjacentTo(x, y)) continue;
        if (graph_.getDegree(x) > maxDeg - 1) continue;
        if (graph_.getDegree(y) > maxDeg - 1) continue;

        if (getNaYX(x, y) != arrow.naYX) continue;

        auto tNeighVec = getTNeighbors(x, y);
        std::set<NodePtr> tNeighSet(tNeighVec.begin(), tNeighVec.end());
        if (tNeighSet != arrow.tNeighbors) continue;

        auto parentsVec = graph_.getParents(y);
        std::set<NodePtr> currentParents(parentsVec.begin(), parentsVec.end());
        if (currentParents != arrow.parents) continue;

        if (!validInsert(x, y, arrow.hOrT, getNaYX(x, y))) continue;

        insert(x, y, arrow.hOrT, arrow.bump);

        std::set<NodePtr> process = revertToCpdag();
        process.insert(x);
        process.insert(y);
        auto common = getCommonAdjacents(x, y);
        process.insert(common.begin(), common.end());

        reevaluateForward(process);
    }
}

void Fges::reevaluateForward(const std::set<NodePtr>& nodes) {
    for (const auto& y : nodes) {
        std::vector<NodePtr> adj;

        if (mode_ == Mode::heuristicSpeedup) {
            adj = effectEdgesGraph_.getAdjacentNodes(y);
        } else if (mode_ == Mode::coverNoncolliders) {
            std::set<NodePtr> g;
            for (const auto& n : graph_.getAdjacentNodes(y)) {
                for (const auto& m : graph_.getAdjacentNodes(n)) {
                    if (graph_.isAdjacentTo(y, m)) continue;
                    if (graph_.isDefCollider(m, n, y)) continue;
                    g.insert(m);
                }
            }
            adj.assign(g.begin(), g.end());
        } else { // allowUnfaithfulness
            adj = variables_;
        }

        for (const auto& x : adj) {
            if (boundGraph_ && !boundGraph_->isAdjacentTo(x, y)) continue;
            calculateArrowsForward(x, y);
        }
    }
}

void Fges::calculateArrowsForward(const NodePtr& a, const NodePtr& b) {
    if (boundGraph_ && !boundGraph_->isAdjacentTo(a, b)) return;
    if (*a == *b) return;
    if (graph_.isAdjacentTo(a, b)) return;

    if (!knowledge_.isEmpty()) {
        if (knowledge_.isForbidden(a->getName(), b->getName())) return;
    }

    std::set<NodePtr> naYX = getNaYX(a, b);
    std::vector<NodePtr> tNeighbors = getTNeighbors(a, b);
    auto parentsVec = graph_.getParents(b);
    std::set<NodePtr> parents(parentsVec.begin(), parentsVec.end());

    std::set<NodePtr> tNeighSet(tNeighbors.begin(), tNeighbors.end());
    std::string key = a->getName() + "->" + b->getName();
    ArrowConfig config{tNeighSet, naYX, parents};
    auto it = arrowsMap_.find(key);
    if (it != arrowsMap_.end() && it->second == config) return;
    arrowsMap_[key] = config;

    int depth = std::min(10000, static_cast<int>(tNeighbors.size()));

    // Generate all subsets of tNeighbors of sizes 0..depth
    std::set<NodePtr> maxT;
    double maxBump = -std::numeric_limits<double>::infinity();

    SublistGenerator gen(static_cast<int>(tNeighbors.size()), depth);
    bool valid;
    while (true) {
        const auto& choice = gen.next(valid);
        if (!valid) break;

        std::set<NodePtr> T;
        for (int idx : choice) {
            T.insert(tNeighbors[idx]);
        }

        double bump = insertEval(a, b, T, naYX, parents);
        if (bump > maxBump) {
            maxT = T;
            maxBump = bump;
        }
    }

    if (maxBump > 0) {
        Arrow arrow;
        arrow.bump = maxBump;
        arrow.a = a;
        arrow.b = b;
        arrow.hOrT = maxT;
        arrow.tNeighbors = tNeighSet;
        arrow.naYX = naYX;
        arrow.parents = parents;
        arrow.index = arrowIndex_++;
        sortedArrows_.insert(arrow);
    }
}

double Fges::insertEval(const NodePtr& x, const NodePtr& y,
                         const std::set<NodePtr>& T, const std::set<NodePtr>& naYX,
                         const std::set<NodePtr>& parents) {
    std::set<NodePtr> set = naYX;
    set.insert(T.begin(), T.end());
    set.insert(parents.begin(), parents.end());
    return scoreGraphChange(x, y, set);
}

void Fges::insert(const NodePtr& x, const NodePtr& y,
                   const std::set<NodePtr>& T, double /*bump*/) {
    graph_.addDirectedEdge(x, y);

    for (const auto& t : T) {
        graph_.removeEdge(t, y);
        graph_.addDirectedEdge(t, y);
    }
}

bool Fges::validInsert(const NodePtr& x, const NodePtr& y,
                        const std::set<NodePtr>& T, const std::set<NodePtr>& naYX) {
    bool violatesKnowledge = false;

    if (!knowledge_.isEmpty()) {
        if (knowledge_.isForbidden(x->getName(), y->getName())) {
            violatesKnowledge = true;
        }
        for (const auto& t : T) {
            if (knowledge_.isForbidden(t->getName(), y->getName())) {
                violatesKnowledge = true;
            }
        }
    }

    std::set<NodePtr> unionSet = T;
    unionSet.insert(naYX.begin(), naYX.end());

    return isClique(unionSet) && semidirectedPathCondition(y, x, unionSet) && !violatesKnowledge;
}

// --- Backward Equivalence Search ---

void Fges::bes() {
    sortedArrowsBack_.clear();
    arrowsMapBack_.clear();
    arrowIndexBack_ = 0;

    reevaluateBackward(std::set<NodePtr>(variables_.begin(), variables_.end()));

    while (!sortedArrowsBack_.empty()) {
        Arrow arrow = *sortedArrowsBack_.begin();
        sortedArrowsBack_.erase(sortedArrowsBack_.begin());

        const auto& x = arrow.a;
        const auto& y = arrow.b;

        if (!graph_.isAdjacentTo(x, y)) continue;

        Edge edge = graph_.getEdge(x, y);
        if (edge.pointsTowards(x)) continue;

        if (getNaYX(x, y) != arrow.naYX) continue;

        auto parentsVecB = graph_.getParents(y);
        std::set<NodePtr> currentParents(parentsVecB.begin(), parentsVecB.end());
        if (currentParents != arrow.parents) continue;

        if (!validDelete(x, y, arrow.hOrT, arrow.naYX)) continue;

        std::set<NodePtr> complement = arrow.naYX;
        for (const auto& h : arrow.hOrT) complement.erase(h);

        double bump = deleteEval(x, y, complement, arrow.parents);

        doDelete(x, y, arrow.hOrT, bump, arrow.naYX);

        std::set<NodePtr> process = revertToCpdag();
        process.insert(x);
        process.insert(y);
        auto adjX = graph_.getAdjacentNodes(x);
        auto adjY = graph_.getAdjacentNodes(y);
        process.insert(adjX.begin(), adjX.end());
        process.insert(adjY.begin(), adjY.end());

        reevaluateBackward(process);
    }
}

void Fges::reevaluateBackward(const std::set<NodePtr>& nodes) {
    for (const auto& r : nodes) {
        for (const auto& w : nodes) {
            Edge e = graph_.getEdge(w, r);
            if (e.isNull()) continue;

            if (e.pointsTowards(r)) {
                calculateArrowsBackward(w, r);
            } else if (e.pointsTowards(w)) {
                calculateArrowsBackward(r, w);
            } else {
                calculateArrowsBackward(w, r);
                calculateArrowsBackward(r, w);
            }
        }
    }
}

void Fges::calculateArrowsBackward(const NodePtr& a, const NodePtr& b) {
    if (!knowledge_.isEmpty()) {
        if (!knowledge_.noEdgeRequired(a->getName(), b->getName())) return;
    }

    std::set<NodePtr> naYX = getNaYX(a, b);
    auto parentsVecBack = graph_.getParents(b);
    std::set<NodePtr> parents(parentsVecBack.begin(), parentsVecBack.end());

    std::string key = a->getName() + "->back->" + b->getName();
    ArrowConfig config{{}, naYX, parents};
    auto it = arrowsMapBack_.find(key);
    if (it != arrowsMapBack_.end() && it->second == config) return;
    arrowsMapBack_[key] = config;

    std::vector<NodePtr> naYXList(naYX.begin(), naYX.end());
    int depth = std::min(4, static_cast<int>(naYXList.size()));

    std::set<NodePtr> maxComplement;
    double maxBump = -std::numeric_limits<double>::infinity();

    SublistGenerator gen(static_cast<int>(naYXList.size()), depth);
    bool valid;
    while (true) {
        const auto& choice = gen.next(valid);
        if (!valid) break;

        std::set<NodePtr> complement;
        for (int idx : choice) {
            complement.insert(naYXList[idx]);
        }

        double bump = deleteEval(a, b, complement, parents);
        if (bump > maxBump) {
            maxBump = bump;
            maxComplement = complement;
        }
    }

    if (maxBump > 0) {
        std::set<NodePtr> H = naYX;
        for (const auto& c : maxComplement) H.erase(c);

        Arrow arrow;
        arrow.bump = maxBump;
        arrow.a = a;
        arrow.b = b;
        arrow.hOrT = H;
        arrow.naYX = naYX;
        arrow.parents = parents;
        arrow.index = arrowIndexBack_++;
        sortedArrowsBack_.insert(arrow);
    }
}

double Fges::deleteEval(const NodePtr& x, const NodePtr& y,
                         const std::set<NodePtr>& complement,
                         const std::set<NodePtr>& parents) {
    std::set<NodePtr> set = complement;
    set.insert(parents.begin(), parents.end());
    set.erase(x);
    return -scoreGraphChange(x, y, set);
}

void Fges::doDelete(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& H,
                     double /*bump*/, const std::set<NodePtr>& naYX) {
    std::set<NodePtr> diff = naYX;
    for (const auto& h : H) diff.erase(h);

    Edge oldxy = graph_.getEdge(x, y);
    graph_.removeEdge(oldxy);

    for (const auto& h : H) {
        if (graph_.isParentOf(h, y) || graph_.isParentOf(h, x)) continue;

        Edge oldyh = graph_.getEdge(y, h);
        if (oldyh.isNull()) continue;

        if (isUndirectedEdge(oldyh)) {
            graph_.removeEdge(oldyh);
            graph_.addDirectedEdge(y, h);
        }

        Edge oldxh = graph_.getEdge(x, h);
        if (oldxh.isNull()) continue;

        if (isUndirectedEdge(oldxh)) {
            graph_.removeEdge(oldxh);
            graph_.addDirectedEdge(x, h);
        }
    }
}

bool Fges::validDelete(const NodePtr& x, const NodePtr& y,
                        const std::set<NodePtr>& H, const std::set<NodePtr>& naYX) {
    bool violatesKnowledge = false;

    if (!knowledge_.isEmpty()) {
        for (const auto& h : H) {
            if (knowledge_.isForbidden(x->getName(), h->getName())) {
                violatesKnowledge = true;
            }
            if (knowledge_.isForbidden(y->getName(), h->getName())) {
                violatesKnowledge = true;
            }
        }
    }

    std::set<NodePtr> diff = naYX;
    for (const auto& h : H) diff.erase(h);

    return isClique(diff) && !violatesKnowledge;
}

// --- Utility methods ---

std::set<NodePtr> Fges::getNaYX(const NodePtr& x, const NodePtr& y) const {
    std::set<NodePtr> nayx;
    auto adj = graph_.getAdjacentNodes(y);

    for (const auto& z : adj) {
        if (*z == *x) continue;
        Edge yz = graph_.getEdge(y, z);
        if (!isUndirectedEdge(yz)) continue;
        if (!graph_.isAdjacentTo(z, x)) continue;
        nayx.insert(z);
    }

    return nayx;
}

std::vector<NodePtr> Fges::getTNeighbors(const NodePtr& x, const NodePtr& y) const {
    std::vector<NodePtr> tNeighbors;
    auto yEdges = graph_.getEdges(y);

    for (const auto& edge : yEdges) {
        if (!isUndirectedEdge(edge)) continue;
        auto z = edge.getDistalNode(y);
        if (!z || graph_.isAdjacentTo(z, x)) continue;
        tNeighbors.push_back(z);
    }

    return tNeighbors;
}

std::set<NodePtr> Fges::getCommonAdjacents(const NodePtr& x, const NodePtr& y) const {
    auto adjX = graph_.getAdjacentNodes(x);
    std::set<std::string> adjXNames;
    for (const auto& n : adjX) adjXNames.insert(n->getName());

    std::set<NodePtr> common;
    for (const auto& n : graph_.getAdjacentNodes(y)) {
        if (adjXNames.count(n->getName())) {
            common.insert(n);
        }
    }
    return common;
}

bool Fges::isClique(const std::set<NodePtr>& nodes) const {
    std::vector<NodePtr> v(nodes.begin(), nodes.end());
    for (size_t i = 0; i < v.size(); i++) {
        for (size_t j = i + 1; j < v.size(); j++) {
            if (!graph_.isAdjacentTo(v[i], v[j])) return false;
        }
    }
    return true;
}

bool Fges::semidirectedPathCondition(const NodePtr& from, const NodePtr& to,
                                      const std::set<NodePtr>& cond) const {
    // Returns true if there is NO semidirected path from 'from' to 'to'
    // that avoids 'cond'. (If such a path exists, insertion creates cycle.)
    std::queue<NodePtr> Q;
    std::set<std::string> visited;

    Q.push(from);
    visited.insert(from->getName());

    while (!Q.empty()) {
        NodePtr t = Q.front();
        Q.pop();

        // Skip conditioned nodes
        bool inCond = false;
        for (const auto& c : cond) {
            if (*c == *t) { inCond = true; break; }
        }
        if (inCond) continue;

        if (*t == *to) return false;

        for (const auto& adj : graph_.getAdjacentNodes(t)) {
            Edge edge = graph_.getEdge(t, adj);
            NodePtr c = traverseSemiDirected(t, edge);
            if (!c) continue;
            if (!visited.count(c->getName())) {
                visited.insert(c->getName());
                Q.push(c);
            }
        }
    }

    return true;
}

NodePtr Fges::traverseSemiDirected(const NodePtr& node, const Edge& edge) {
    if (*node == *edge.getNode1()) {
        if (edge.getEndpoint1() == Endpoint::TAIL) {
            return edge.getNode2();
        }
    } else if (*node == *edge.getNode2()) {
        if (edge.getEndpoint2() == Endpoint::TAIL) {
            return edge.getNode1();
        }
    }
    return nullptr;
}

double Fges::scoreGraphChange(const NodePtr& x, const NodePtr& y,
                               const std::set<NodePtr>& parents) const {
    int xIndex = indexOf(x);
    int yIndex = indexOf(y);

    std::vector<int> parentIndices;
    parentIndices.reserve(parents.size());
    for (const auto& p : parents) {
        parentIndices.push_back(indexOf(p));
    }

    return score_.localScoreDiff(xIndex, yIndex, parentIndices);
}

std::set<NodePtr> Fges::revertToCpdag() {
    MeekRules rules;
    rules.setKnowledge(knowledge_);
    rules.setMeekPreventCycles(true);
    rules.setVerbose(false);
    return rules.orientImplied(graph_);
}

void Fges::addRequiredEdges() {
    if (knowledge_.isEmpty()) return;

    for (const auto& ke : knowledge_.getListOfRequiredEdges()) {
        NodePtr nodeA = graph_.getNode(ke.from);
        NodePtr nodeB = graph_.getNode(ke.to);
        if (!nodeA || !nodeB) continue;

        if (!graph_.existsDirectedPath(nodeB, nodeA)) {
            graph_.removeEdges(nodeA, nodeB);
            graph_.addDirectedEdge(nodeA, nodeB);
        }
    }
}

double Fges::scoreDag(const Graph& dag) const {
    double total = 0.0;
    for (const auto& node : variables_) {
        auto parents = dag.getParents(node);
        std::vector<int> parentIndices;
        for (const auto& p : parents) {
            parentIndices.push_back(indexOf(p));
        }
        total += score_.localScore(indexOf(node), parentIndices);
    }
    return total;
}

bool Fges::isUndirectedEdge(const Edge& e) {
    return e.getEndpoint1() == Endpoint::TAIL && e.getEndpoint2() == Endpoint::TAIL;
}

int Fges::indexOf(const NodePtr& node) const {
    auto it = hashIndices_.find(node->getName());
    if (it != hashIndices_.end()) return it->second;
    return -1;
}

} // namespace tetrad
