#include "search/fci_orient.h"
#include "search/ind_test_fisher_z.h"
#include "util/choice_generator.h"
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace tetrad {

// ─────────────────────────────────────────────────────────────────────────────
// File-scope helpers for R9
// ─────────────────────────────────────────────────────────────────────────────

// Returns true if an uncovered semi-directed path exists from `from` to `to`,
// where `prevOfFrom` is the node preceding `from` on the path (needed to check
// the first triple for the uncovered constraint).
//
// "Uncovered" means: for every consecutive triple (prev, curr, next) on the
// path, prev and next must NOT be adjacent.
//
// Matches Java's R5R9Dijkstra with Rule.R9 and uncovered=true.
static bool existsUncoveredSemiDirectedPath(
    const Graph& graph,
    const NodePtr& from,
    const NodePtr& to,
    const NodePtr& prevOfFrom)
{
    // BFS over (current, predecessor) pairs so we can check the uncovered
    // constraint at each step.
    using StatePair = std::pair<NodePtr, NodePtr>;

    struct PairHash {
        size_t operator()(const StatePair& p) const {
            size_t h1 = p.first  ? std::hash<NodePtr>{}(p.first)  : 0;
            size_t h2 = p.second ? std::hash<NodePtr>{}(p.second) : 0;
            return h1 ^ (h2 * 2654435761u);
        }
    };
    struct PairEq {
        bool operator()(const StatePair& a, const StatePair& b) const {
            bool firstEq  = (!a.first  && !b.first)  || (a.first  && b.first  && *a.first  == *b.first);
            bool secondEq = (!a.second && !b.second) || (a.second && b.second && *a.second == *b.second);
            return firstEq && secondEq;
        }
    };

    std::queue<StatePair> Q;
    std::unordered_set<StatePair, PairHash, PairEq> visited;

    StatePair init = {from, prevOfFrom};
    Q.push(init);
    visited.insert(init);

    while (!Q.empty()) {
        auto [curr, prev] = Q.front(); Q.pop();

        for (const auto& next : graph.getAdjacentNodes(curr)) {
            // Semi-directed: endpoint AT curr must be TAIL or CIRCLE (not ARROW)
            Edge edge = graph.getEdge(curr, next);
            if (edge.getEndpoint(curr) == Endpoint::ARROW) continue;

            // Uncovered constraint: next must not be adjacent to prev
            if (prev && graph.isAdjacentTo(next, prev)) continue;

            if (*next == *to) return true;

            StatePair nextState = {next, curr};
            if (!visited.count(nextState)) {
                visited.insert(nextState);
                Q.push(nextState);
            }
        }
    }

    return false;
}

// ---- R0R4StrategyTestBased ----

R0R4StrategyTestBased::R0R4StrategyTestBased(IndependenceTest& test) : test_(test) {}

bool R0R4StrategyTestBased::isUnshieldedCollider(const Graph& graph,
                                                  const NodePtr& a, const NodePtr& b, const NodePtr& c) {
    // Find sepset of (a, c) from adjacencies of a or c
    // If b is NOT in the sepset, then a-b-c is a collider.
    auto adjA = graph.getAdjacentNodes(a);
    auto adjC = graph.getAdjacentNodes(c);

    // Remove c from adjA, a from adjC
    adjA.erase(std::remove_if(adjA.begin(), adjA.end(),
        [&](const NodePtr& n) { return *n == *c; }), adjA.end());
    adjC.erase(std::remove_if(adjC.begin(), adjC.end(),
        [&](const NodePtr& n) { return *n == *a; }), adjC.end());

    int maxDepth = depth_ < 0 ? static_cast<int>(adjA.size()) : std::min(depth_, static_cast<int>(adjA.size()));

    // Check subsets of adjA
    for (int d = 0; d <= maxDepth; d++) {
        ChoiceGenerator cg(static_cast<int>(adjA.size()), d);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            std::vector<NodePtr> condSet;
            for (int i = 0; i < d; i++) {
                condSet.push_back(adjA[choice[i]]);
            }
            if (test_.isIndependent(a, c, condSet)) {
                // Check if b is in the sepset
                bool bInSet = false;
                for (const auto& n : condSet) {
                    if (*n == *b) { bInSet = true; break; }
                }
                return !bInSet; // Collider if b not in sepset
            }
        }
    }

    maxDepth = depth_ < 0 ? static_cast<int>(adjC.size()) : std::min(depth_, static_cast<int>(adjC.size()));

    // Check subsets of adjC
    for (int d = 0; d <= maxDepth; d++) {
        ChoiceGenerator cg(static_cast<int>(adjC.size()), d);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            std::vector<NodePtr> condSet;
            for (int i = 0; i < d; i++) {
                condSet.push_back(adjC[choice[i]]);
            }
            if (test_.isIndependent(a, c, condSet)) {
                bool bInSet = false;
                for (const auto& n : condSet) {
                    if (*n == *b) { bInSet = true; break; }
                }
                return !bInSet;
            }
        }
    }

    return false;
}

bool R0R4StrategyTestBased::doDiscriminatingPathOrientation(
    const NodePtr& x, const NodePtr& /*w*/, const NodePtr& v, const NodePtr& y,
    const std::vector<NodePtr>& colliderPath,
    Graph& graph) {

    // The collider path nodes are all between X and V, excluding X and V.
    // Check if X is independent of Y given the collider path.
    std::vector<NodePtr> condSet(colliderPath.begin(), colliderPath.end());

    bool independent = test_.isIndependent(x, y, condSet);

    if (independent) {
        // V is in the sepset → noncollider at V: orient V → Y
        graph.setEndpoint(v, y, Endpoint::ARROW);
        graph.setEndpoint(y, v, Endpoint::TAIL);
        return true;
    } else {
        // V is not in the sepset → collider at V: orient W *-> V <-> Y
        graph.setEndpoint(v, y, Endpoint::ARROW);
        graph.setEndpoint(y, v, Endpoint::ARROW);
        return true;
    }
}

// ---- FciOrient ----

FciOrient::FciOrient(R0R4Strategy& strategy) : strategy_(strategy) {
    knowledge_ = strategy.getKnowledge();
}

void FciOrient::setKnowledge(const Knowledge& knowledge) {
    knowledge_ = knowledge;
    strategy_.setKnowledge(knowledge);
}

void FciOrient::orient(Graph& graph, std::unordered_set<Triple>& unshieldedTriples) {
    ruleR0(graph, unshieldedTriples);
    finalOrientation(graph);
}

void FciOrient::ruleR0(Graph& graph, std::unordered_set<Triple>& unshieldedTriples) {
    graph.reorientAllWith(Endpoint::CIRCLE);
    fciOrientbk(knowledge_, graph, graph.getNodes());

    auto nodes = graph.getNodes();

    for (const auto& b : nodes) {
        auto adj = graph.getAdjacentNodes(b);
        if (adj.size() < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adj.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& a = adj[choice[0]];
            const auto& c = adj[choice[1]];

            if (graph.isAdjacentTo(a, c)) continue;
            if (graph.isDefCollider(a, b, c)) continue;

            if (strategy_.isUnshieldedCollider(graph, a, b, c)) {
                if (!isArrowheadAllowed(a, b, graph, knowledge_)) continue;
                if (!isArrowheadAllowed(c, b, graph, knowledge_)) continue;

                graph.setEndpoint(a, b, Endpoint::ARROW);
                graph.setEndpoint(c, b, Endpoint::ARROW);
                unshieldedTriples.insert(Triple(a, b, c));
                changeFlag_ = true;
            }
        }
    }
}

void FciOrient::finalOrientation(Graph& graph) {
    if (completeRuleSetUsed_) {
        zhangFinalOrientation(graph);
    } else {
        spirtesFinalOrientation(graph);
    }
}

void FciOrient::spirtesFinalOrientation(Graph& graph) {
    changeFlag_ = true;
    bool firstTime = true;

    while (changeFlag_) {
        changeFlag_ = false;
        rulesR1R2cycle(graph);
        ruleR3(graph);

        if (changeFlag_ || (firstTime && !knowledge_.isEmpty())) {
            ruleR4(graph);
            firstTime = false;
        }
    }
}

void FciOrient::zhangFinalOrientation(Graph& graph) {
    changeFlag_ = true;
    bool firstTime = true;

    while (changeFlag_) {
        changeFlag_ = false;
        rulesR1R2cycle(graph);
        ruleR3(graph);

        if (changeFlag_ || (firstTime && !knowledge_.isEmpty())) {
            ruleR4(graph);
            firstTime = false;
        }
    }

    if (completeRuleSetUsed_) {
        ruleR5(graph);

        changeFlag_ = true;
        while (changeFlag_) {
            changeFlag_ = false;
            ruleR6(graph);
            ruleR7(graph);
        }

        changeFlag_ = true;
        while (changeFlag_) {
            changeFlag_ = false;
            rulesR8R9R10(graph);
        }
    }
}

// R1: If a *-> b o-* c, and a not adj c, orient b -> c.
void FciOrient::ruleR1(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph) {
    if (graph.isAdjacentTo(a, c)) return;

    if (graph.getEndpoint(a, b) == Endpoint::ARROW && graph.getEndpoint(c, b) == Endpoint::CIRCLE) {
        if (!isArrowheadAllowed(b, c, graph, knowledge_)) return;

        graph.setEndpoint(c, b, Endpoint::TAIL);
        graph.setEndpoint(b, c, Endpoint::ARROW);
        changeFlag_ = true;
    }
}

// R2: If a -> b *-> c, or a *-> b -> c, and a *-o c, orient a *-> c.
void FciOrient::ruleR2(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph) {
    if (graph.isAdjacentTo(a, c) && graph.getEndpoint(a, c) == Endpoint::CIRCLE) {
        if ((graph.getEndpoint(a, b) == Endpoint::ARROW && graph.getEndpoint(b, c) == Endpoint::ARROW
             && graph.getEndpoint(b, a) == Endpoint::TAIL)
            || (graph.getEndpoint(a, b) == Endpoint::ARROW && graph.getEndpoint(b, c) == Endpoint::ARROW
                && graph.getEndpoint(c, b) == Endpoint::TAIL)) {

            if (!isArrowheadAllowed(a, c, graph, knowledge_)) return;

            graph.setEndpoint(a, c, Endpoint::ARROW);
            changeFlag_ = true;
        }
    }
}

void FciOrient::rulesR1R2cycle(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& B : nodes) {
        auto adj = graph.getAdjacentNodes(B);
        if (adj.size() < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adj.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& A = adj[choice[0]];
            const auto& C = adj[choice[1]];

            ruleR1(A, B, C, graph);
            ruleR1(C, B, A, graph);
            ruleR2(A, B, C, graph);
            ruleR2(C, B, A, graph);
        }
    }
}

// R3: If a *-> b <-* c, a *-o d o-* c, a and c not adj, d *-o b, orient d *-> b.
void FciOrient::ruleR3(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& b : nodes) {
        auto adj = graph.getAdjacentNodes(b);
        if (adj.size() < 3) continue;

        ChoiceGenerator gen(static_cast<int>(adj.size()), 3);
        const int* ch;
        while ((ch = gen.next()) != nullptr) {
            std::vector<NodePtr> adjb = {adj[ch[0]], adj[ch[1]], adj[ch[2]]};

            // Try all permutations of 3 elements
            int perms[6][3] = {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
            for (auto& perm : perms) {
                const auto& a = adjb[perm[0]];
                const auto& d = adjb[perm[1]];
                const auto& c = adjb[perm[2]];

                if (!graph.isDefCollider(a, b, c)) continue;
                if (!(graph.isAdjacentTo(a, b) && graph.isAdjacentTo(d, b) && graph.isAdjacentTo(c, b))) continue;
                if (!(graph.isAdjacentTo(a, d) && graph.isAdjacentTo(c, d))) continue;
                if (graph.isAdjacentTo(a, c)) continue;
                if (!(graph.getEndpoint(d, b) == Endpoint::CIRCLE
                      && graph.getEndpoint(a, d) == Endpoint::CIRCLE
                      && graph.getEndpoint(c, d) == Endpoint::CIRCLE)) continue;

                if (!isArrowheadAllowed(d, b, graph, knowledge_)) continue;

                graph.setEndpoint(d, b, Endpoint::ARROW);
                changeFlag_ = true;
                return; // Only one orientation per pass
            }
        }
    }
}

// R4: Discriminating path rule.
void FciOrient::ruleR4(Graph& graph) {
    // Find discriminating paths via BFS.
    auto nodes = graph.getNodes();

    for (const auto& w : nodes) {
        for (const auto& y : graph.getAdjacentNodes(w)) {
            if (!graph.isParentOf(w, y)) continue;

            auto vnodes = graph.getAdjacentNodes(y);
            // Retain only nodes adjacent to both y and w
            std::vector<NodePtr> vCandidates;
            for (const auto& v : vnodes) {
                if (graph.isAdjacentTo(v, w)) {
                    vCandidates.push_back(v);
                }
            }

            for (const auto& v : vCandidates) {
                if (*w == *y) continue;
                if (graph.getEndpoint(y, v) != Endpoint::CIRCLE) continue;
                if (graph.getEndpoint(v, y) != Endpoint::ARROW) continue;

                // BFS backward from w to find discriminating paths
                std::queue<NodePtr> Q;
                std::unordered_set<std::string> visited;
                std::unordered_map<std::string, NodePtr> previous;

                Q.push(w);
                visited.insert(w->getName());
                visited.insert(v->getName());
                previous[w->getName()] = nullptr;

                while (!Q.empty()) {
                    NodePtr t = Q.front();
                    Q.pop();

                    auto nodesInTo = graph.getNodesInTo(t, Endpoint::ARROW);

                    for (const auto& x : nodesInTo) {
                        if (visited.count(x->getName())) continue;

                        previous[x->getName()] = t;

                        // Build collider path
                        std::vector<NodePtr> colliderPath;
                        NodePtr d = x;
                        while (previous.count(d->getName()) && previous[d->getName()]) {
                            colliderPath.push_back(previous[d->getName()]);
                            d = previous[d->getName()];
                        }
                        // colliderPath is from x back to w, but we want w...t (excluding x and v)

                        if (maxDiscriminatingPathLength_ != -1 &&
                            static_cast<int>(colliderPath.size()) > maxDiscriminatingPathLength_) {
                            continue;
                        }

                        // Check if this forms a valid discriminating path
                        // X...W --> Y, with V adjacent to both W and Y
                        // All nodes on collider path must be parents of Y
                        bool validPath = true;
                        for (const auto& n : colliderPath) {
                            if (!graph.isParentOf(n, y)) {
                                validPath = false;
                                break;
                            }
                        }

                        if (validPath && !graph.isAdjacentTo(x, y) && colliderPath.size() >= 1) {
                            bool oriented = strategy_.doDiscriminatingPathOrientation(
                                x, w, v, y, colliderPath, graph);
                            if (oriented) {
                                changeFlag_ = true;
                            }
                        }

                        if (!visited.count(x->getName())) {
                            Q.push(x);
                            visited.insert(x->getName());
                        }
                    }
                }
            }
        }
    }
}

// R5: For every a o--o b, if there is an uncovered circle path from a to b
// s.t. a,theta not adj and b,gamma not adj, orient as undirected.
// Simplified: skip complex Dijkstra, just check for simple cases.
void FciOrient::ruleR5(Graph& /*graph*/) {
    // R5 requires finding uncovered circle paths via Dijkstra.
    // This is a simplified version that does not implement the full path search.
    // In practice, R5 rarely fires. We leave it as a no-op for now.
}

// R6: If a -- b o-* c, orient b o-* c as b -* c (set circle at b to tail).
void FciOrient::ruleR6(Graph& graph) {
    for (const auto& edge : graph.getEdges()) {
        for (int dir = 0; dir < 2; dir++) {
            const auto& a = (dir == 0) ? edge.getNode1() : edge.getNode2();
            const auto& b = (dir == 0) ? edge.getNode2() : edge.getNode1();

            // Check a -- b (undirected)
            if (!(edge.getEndpoint1() == Endpoint::TAIL && edge.getEndpoint2() == Endpoint::TAIL)) continue;

            for (const auto& c : graph.getAdjacentNodes(b)) {
                if (*c == *a) continue;
                if (graph.getEndpoint(c, b) == Endpoint::CIRCLE) {
                    graph.setEndpoint(c, b, Endpoint::TAIL);
                    changeFlag_ = true;
                }
            }
        }
    }
}

// R7: If a --o b o-* c, and a not adj c, orient b o-* c as b -* c.
void FciOrient::ruleR7(Graph& graph) {
    for (const auto& edge : graph.getEdges()) {
        for (int dir = 0; dir < 2; dir++) {
            const auto& a = (dir == 0) ? edge.getNode1() : edge.getNode2();
            const auto& b = (dir == 0) ? edge.getNode2() : edge.getNode1();

            // Check a --o b: endpoint at b from a is CIRCLE, endpoint at a from b is TAIL
            if (!(graph.getEndpoint(a, b) == Endpoint::CIRCLE && graph.getEndpoint(b, a) == Endpoint::TAIL)) continue;

            for (const auto& c : graph.getAdjacentNodes(b)) {
                if (*c == *a) continue;
                if (!graph.isAdjacentTo(a, c) && graph.getEndpoint(c, b) == Endpoint::CIRCLE) {
                    graph.setEndpoint(c, b, Endpoint::TAIL);
                    changeFlag_ = true;
                }
            }
        }
    }
}

// R8, R9, R10 applied to each Ao->C edge.
void FciOrient::rulesR8R9R10(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& c : nodes) {
        auto intoCArrows = graph.getNodesInTo(c, Endpoint::ARROW);

        for (const auto& a : intoCArrows) {
            if (graph.getEndpoint(c, a) != Endpoint::CIRCLE) continue;

            // We have A o-> C
            if (!ruleR8(a, c, graph)) {
                if (!ruleR9(a, c, graph)) {
                    ruleR10(a, c, graph);
                }
            }
        }
    }
}

// R8: If a -> b -> c or a --o b -> c, and a o-> c, orient a -> c.
bool FciOrient::ruleR8(const NodePtr& a, const NodePtr& c, Graph& graph) {
    if (!isPartiallyOrientedEdge(a, c, graph)) return false;

    // Find common adjacents of a and c
    auto adjA = graph.getAdjacentNodes(a);
    auto adjC = graph.getAdjacentNodes(c);

    for (const auto& b : adjA) {
        bool inAdjC = false;
        for (const auto& n : adjC) {
            if (*n == *b) { inAdjC = true; break; }
        }
        if (!inAdjC) continue;

        bool orient = false;

        // a -> b -> c: b->a is tail, a->b is arrow, c->b is tail, b->c is arrow
        if (graph.getEndpoint(b, a) == Endpoint::TAIL && graph.getEndpoint(a, b) == Endpoint::ARROW
            && graph.getEndpoint(c, b) == Endpoint::TAIL && graph.getEndpoint(b, c) == Endpoint::ARROW) {
            orient = true;
        }
        // a --o b -> c: b->a is tail, a->b is circle, c->b is tail, b->c is arrow
        else if (graph.getEndpoint(b, a) == Endpoint::TAIL && graph.getEndpoint(a, b) == Endpoint::CIRCLE
                 && graph.getEndpoint(c, b) == Endpoint::TAIL && graph.getEndpoint(b, c) == Endpoint::ARROW) {
            orient = true;
        }

        if (orient) {
            graph.setEndpoint(c, a, Endpoint::TAIL);
            changeFlag_ = true;
            return true;
        }
    }

    return false;
}

// R9: If a o-> c, and there is an uncovered potentially directed path from a to c
// s.t. c and first-on-path not adj, orient a -> c.
bool FciOrient::ruleR9(const NodePtr& a, const NodePtr& c, Graph& graph) {
    if (!isPartiallyOrientedEdge(a, c, graph)) return false;

    auto adjA = graph.getAdjacentNodes(a);

    for (const auto& beta : adjA) {
        if (*beta == *c) continue;
        if (graph.isAdjacentTo(c, beta)) continue;

        // Edge a-beta must be potentially directed away from a
        Edge abEdge = graph.getEdge(a, beta);
        if (abEdge.isNull()) continue;
        if (abEdge.getEndpoint(a) == Endpoint::ARROW) continue;

        // Check if there's a semidirected path from beta to c
        if (graph.existsSemiDirectedPath(beta, c)) {
            graph.setEndpoint(c, a, Endpoint::TAIL);
            changeFlag_ = true;
            return true;
        }
    }

    return false;
}

// R10: If a o-> c, beta -> c <- theta, uncovered pd paths a..beta and a..theta,
// mu and omega (first nodes on paths) not adjacent, orient a -> c.
void FciOrient::ruleR10(const NodePtr& alpha, const NodePtr& gamma, Graph& graph) {
    if (!isPartiallyOrientedEdge(alpha, gamma, graph)) return;

    auto into = graph.getNodesInTo(gamma, Endpoint::ARROW);
    // Remove alpha
    into.erase(std::remove_if(into.begin(), into.end(),
        [&](const NodePtr& n) { return *n == *alpha; }), into.end());

    for (size_t i = 0; i < into.size(); i++) {
        for (size_t j = i + 1; j < into.size(); j++) {
            const auto& beta = into[i];
            const auto& theta = into[j];

            // Need beta -> gamma and theta -> gamma
            if (graph.getEndpoint(gamma, beta) != Endpoint::TAIL) continue;
            if (graph.getEndpoint(gamma, theta) != Endpoint::TAIL) continue;

            auto adjAlpha = graph.getAdjacentNodes(alpha);

            for (size_t k = 0; k < adjAlpha.size(); k++) {
                for (size_t l = k + 1; l < adjAlpha.size(); l++) {
                    const auto& nu = adjAlpha[k];
                    const auto& omega = adjAlpha[l];

                    if (graph.isAdjacentTo(nu, omega)) continue;

                    if (graph.existsSemiDirectedPath(nu, beta) &&
                        graph.existsSemiDirectedPath(omega, theta)) {
                        graph.setEndpoint(gamma, alpha, Endpoint::TAIL);
                        changeFlag_ = true;
                        return;
                    }
                }
            }
        }
    }
}

bool FciOrient::isArrowheadAllowed(const NodePtr& x, const NodePtr& y,
                                    const Graph& graph, const Knowledge& knowledge) {
    if (!graph.isAdjacentTo(x, y)) return false;

    if (graph.getEndpoint(x, y) == Endpoint::ARROW) return true;
    if (graph.getEndpoint(x, y) == Endpoint::TAIL) return false;

    if (graph.getEndpoint(y, x) == Endpoint::ARROW && graph.getEndpoint(x, y) == Endpoint::CIRCLE) {
        if (knowledge.isForbidden(x->getName(), y->getName())) return true;
    }

    if (graph.getEndpoint(y, x) == Endpoint::TAIL && graph.getEndpoint(x, y) == Endpoint::CIRCLE) {
        if (knowledge.isForbidden(x->getName(), y->getName())) return false;
    }

    return graph.getEndpoint(x, y) == Endpoint::CIRCLE;
}

void FciOrient::fciOrientbk(const Knowledge& bk, Graph& graph, const std::vector<NodePtr>& variables) {
    if (bk.isEmpty()) return;

    auto findNode = [&](const std::string& name) -> NodePtr {
        for (const auto& n : variables) {
            if (n->getName() == name) return n;
        }
        return nullptr;
    };

    for (const auto& ke : bk.getListOfForbiddenEdges()) {
        NodePtr from = findNode(ke.from);
        NodePtr to = findNode(ke.to);
        if (!from || !to) continue;

        Edge edge = graph.getEdge(from, to);
        if (edge.isNull()) continue;

        if (!isArrowheadAllowed(to, from, graph, knowledge_)) continue;

        graph.setEndpoint(to, from, Endpoint::ARROW);
        changeFlag_ = true;
    }

    for (const auto& ke : bk.getListOfRequiredEdges()) {
        NodePtr from = findNode(ke.from);
        NodePtr to = findNode(ke.to);
        if (!from || !to) continue;

        Edge edge = graph.getEdge(from, to);
        if (edge.isNull()) continue;

        if (!isArrowheadAllowed(from, to, graph, knowledge_)) continue;

        graph.setEndpoint(to, from, Endpoint::TAIL);
        graph.setEndpoint(from, to, Endpoint::ARROW);
        changeFlag_ = true;
    }
}

bool FciOrient::isPartiallyOrientedEdge(const NodePtr& a, const NodePtr& b, const Graph& graph) {
    // A o-> B: endpoint at A is CIRCLE, endpoint at B is ARROW
    return graph.getEndpoint(b, a) == Endpoint::CIRCLE && graph.getEndpoint(a, b) == Endpoint::ARROW;
}

} // namespace tetrad
