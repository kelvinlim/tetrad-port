#include <stdexcept>
#include "search/fci_orient.h"
#include "util/choice_generator.h"
#include "util/java_hash.h"
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
    using StatePair = std::pair<NodePtr, NodePtr>;  // (current, predecessor)

    std::queue<StatePair> Q;
    std::unordered_set<std::string> visited;  // settled nodes (by name)

    Q.push({from, prevOfFrom});
    visited.insert(from->getName());
    // Prevent the path from cycling back through the origin node (prevOfFrom).
    // Java's Dijkstra achieves this implicitly: the origin is at distance 0 and
    // can never be relaxed to a longer distance.
    if (prevOfFrom) visited.insert(prevOfFrom->getName());

    while (!Q.empty()) {
        auto [curr, prev] = Q.front(); Q.pop();

        for (const auto& next : graph.getAdjacentNodes(curr)) {
            // Semi-directed: endpoint AT curr must be TAIL or CIRCLE (not ARROW)
            Edge edge = graph.getEdge(curr, next);
            if (edge.getEndpoint(curr) == Endpoint::ARROW) continue;

            // Uncovered constraint: next must not be adjacent to prev
            if (prev && graph.isAdjacentTo(next, prev)) continue;

            if (*next == *to) return true;

            if (!visited.count(next->getName())) {
                visited.insert(next->getName());
                Q.push({next, curr});
            }
        }
    }

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// File-scope helpers for R5
// ─────────────────────────────────────────────────────────────────────────────

// Returns the uncovered circle path from x to y (including both endpoints) if
// one exists, or an empty vector otherwise.
//
// R5 constraints (Zhang 2008):
//   - Only o-o (nondirected) edges may be traversed.
//   - Path must have >= 3 edges (>= 2 intermediate nodes, i.e. >= 4 nodes total).
//   - gamma (first hop from x) must NOT be adjacent to y.
//   - theta (node just before y) must NOT be adjacent to x.
//   - Uncovered: for every consecutive triple (prev, curr, next), prev and next
//     must NOT be adjacent.
//
// Matches Java's R5R9Dijkstra with Rule.R5 and uncovered=true.
static std::vector<NodePtr> findUncoveredCirclePath(
    const Graph& graph, const NodePtr& x, const NodePtr& y)
{
    using StatePair = std::pair<NodePtr, NodePtr>; // (current, predecessor)

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
    std::unordered_map<StatePair, StatePair, PairHash, PairEq> parent;

    StatePair init = {x, nullptr};
    Q.push(init);
    visited.insert(init);

    while (!Q.empty()) {
        auto [curr, prev] = Q.front(); Q.pop();

        for (const auto& next : graph.getAdjacentNodes(curr)) {
            // R5: only traverse nondirected (o-o) edges
            Edge edge = graph.getEdge(curr, next);
            if (edge.getEndpoint(curr) != Endpoint::CIRCLE) continue;
            if (edge.getEndpoint(next) != Endpoint::CIRCLE) continue;

            // Skip length-1 paths (direct x→y)
            if (*curr == *x && *next == *y) continue;

            // Gamma constraint: first hop from x must not be adjacent to y
            if (*curr == *x && graph.isAdjacentTo(next, y)) continue;

            // Skip length-2 paths (prev is x and we're about to reach y)
            if (*next == *y && prev && *prev == *x) continue;

            // Theta constraint (R5 only): node just before y must not be adj to x
            if (*next == *y && graph.isAdjacentTo(curr, x)) continue;

            // Uncovered constraint: next must not be adjacent to predecessor
            if (prev && graph.isAdjacentTo(next, prev)) continue;

            StatePair nextState = {next, curr};
            if (visited.count(nextState)) continue;

            visited.insert(nextState);
            parent[nextState] = {curr, prev};

            if (*next == *y) {
                // Reconstruct path from x to y
                std::vector<NodePtr> path;
                StatePair s = nextState;
                path.push_back(s.first);          // y
                while (parent.count(s)) {
                    s = parent[s];
                    path.push_back(s.first);      // intermediate nodes, then x
                }
                std::reverse(path.begin(), path.end());
                return path;
            }

            Q.push(nextState);
        }
    }

    return {};
}

// ---- FciOrient ----

FciOrient::FciOrient(SepsetsGreedy& sepsets) : sepsets_(sepsets) {}

void FciOrient::orient(Graph& graph) {
    ruleR0(graph);
    finalOrientation(graph);
}

void FciOrient::ruleR0(Graph& graph) {
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

            if (sepsets_.isUnshieldedCollider(a, b, c)) {
                if (!isArrowheadAllowed(a, b, graph, knowledge_)) continue;
                if (!isArrowheadAllowed(c, b, graph, knowledge_)) continue;

                graph.setEndpoint(a, b, Endpoint::ARROW);
                graph.setEndpoint(c, b, Endpoint::ARROW);
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
            ruleR4B(graph);
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
            ruleR4B(graph);
            firstTime = false;
        }
    }

    if (completeRuleSetUsed_) {
        ruleR5(graph);

        changeFlag_ = true;
        while (changeFlag_) {
            changeFlag_ = false;
            ruleR6R7(graph);
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
        if ((graph.getEndpoint(a, b) == Endpoint::ARROW && graph.getEndpoint(b, c) == Endpoint::ARROW)
            && (graph.getEndpoint(b, a) == Endpoint::TAIL || graph.getEndpoint(c, b) == Endpoint::TAIL)) {

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

// R3 (7.6.3): D*-oB, A*->B<-*C and A*-oD o-*C, !adj(A,C), D*-oB -> orient D*->B.
// Uses getNodesInTo(b, ARROW) to pick pairs (a, c), then finds D as common adjacent.
// NOTE: preserves 7.6.3 behavior where !isArrowheadAllowed causes `return` (not `continue`).
void FciOrient::ruleR3(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& b : nodes) {
        auto intoBArrows = graph.getNodesInTo(b, Endpoint::ARROW);
        if (static_cast<int>(intoBArrows.size()) < 2) continue;

        ChoiceGenerator gen(static_cast<int>(intoBArrows.size()), 2);
        const int* choice;
        while ((choice = gen.next()) != nullptr) {
            const auto& a = intoBArrows[choice[0]];
            const auto& c = intoBArrows[choice[1]];

            // Common adjacents of a and c
            auto adjA = graph.getAdjacentNodes(a);
            auto adjC = graph.getAdjacentNodes(c);

            for (const auto& d : adjA) {
                // d must be in adj(c)
                bool inAdjC = false;
                for (const auto& n : adjC) {
                    if (*n == *d) { inAdjC = true; break; }
                }
                if (!inAdjC) continue;

                // a *-o d: endpoint at d from a = CIRCLE
                if (graph.getEndpoint(a, d) != Endpoint::CIRCLE) continue;
                // d o-* c: endpoint at d from c = CIRCLE
                if (graph.getEndpoint(c, d) != Endpoint::CIRCLE) continue;
                // a and c not adjacent
                if (graph.isAdjacentTo(a, c)) continue;
                // d *-o b: endpoint at b from d = CIRCLE
                if (graph.getEndpoint(d, b) != Endpoint::CIRCLE) continue;
                // d must be adjacent to b (checked via getEndpoint above if edge exists)
                if (!graph.isAdjacentTo(d, b)) continue;

                // 7.6.3 bug: return instead of continue
                if (!isArrowheadAllowed(d, b, graph, knowledge_)) return;

                graph.setEndpoint(d, b, Endpoint::ARROW);
                changeFlag_ = true;
            }
        }
    }
}

// R4 (7.6.3): Discriminating path rule.
// possA = getNodesOutTo(b, ARROW): nodes a where b *-> a (endpoint at a from b = ARROW)
// possC = getNodesInTo(b, CIRCLE): nodes c where c *-o b (endpoint at b from c = CIRCLE)
// Conditions: a != c, isParentOf(a, c), getEndpoint(b, c) == ARROW
void FciOrient::ruleR4B(Graph& graph) {
    if (!doDiscriminatingPathColliderRule_ && !doDiscriminatingPathTailRule_) return;

    auto nodes = graph.getNodes();
    for (const auto& b : nodes) {
        // possA: nodes a where b *-> a (endpoint at a from b = ARROW)
        auto adjB = graph.getAdjacentNodes(b);

        for (const auto& a : adjB) {
            // b *-> a: endpoint at a from b = ARROW
            if (graph.getEndpoint(b, a) != Endpoint::ARROW) continue;

            for (const auto& c : graph.getNodesInTo(b, Endpoint::CIRCLE)) {
                if (*c == *a) continue;
                if (!graph.isParentOf(a, c)) continue;
                // b *-> c: endpoint at c from b = ARROW
                if (graph.getEndpoint(b, c) != Endpoint::ARROW) continue;

                ddpOrient(a, b, c, graph);
            }
        }
    }
}

// BFS backward from a to find discriminating path endpoint d (not adj to c).
// All intermediate nodes must be definite colliders on the path and parents of c.
void FciOrient::ddpOrient(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph) {
    std::queue<NodePtr> Q;
    std::unordered_set<std::string> V;
    std::unordered_map<std::string, NodePtr> previous;

    auto cParents = graph.getParents(c);

    Q.push(a);
    V.insert(a->getName());
    V.insert(b->getName());
    previous[a->getName()] = b;  // b is sentinel predecessor for a

    while (!Q.empty()) {
        NodePtr t = Q.front(); Q.pop();

        for (const auto& d : graph.getNodesInTo(t, Endpoint::ARROW)) {
            if (V.count(d->getName())) continue;

            previous[d->getName()] = t;
            NodePtr p = previous.count(t->getName()) ? previous.at(t->getName()) : nullptr;

            // d must be a definite collider at t: d *-> t <-* p
            if (!p || !graph.isDefCollider(d, t, p)) continue;

            // Check max path length
            if (maxDiscriminatingPathLength_ != -1) {
                int len = 0;
                NodePtr curr = d;
                while (previous.count(curr->getName()) && previous.at(curr->getName())) {
                    len++;
                    curr = previous.at(curr->getName());
                }
                if (len > maxDiscriminatingPathLength_) {
                    V.insert(d->getName());
                    continue;
                }
            }

            if (!graph.isAdjacentTo(d, c)) {
                // Found discriminating path endpoint d (not adjacent to c)
                if (doDdpOrientation(d, a, b, c, graph)) {
                    return;  // noncollider case: done
                }
                // collider case: don't return, continue BFS
            }

            // Add to queue only if d is a parent of c
            bool isCParent = false;
            for (const auto& cp : cParents) {
                if (*cp == *d) { isCParent = true; break; }
            }
            if (isCParent) {
                Q.push(d);
                V.insert(d->getName());
            } else {
                V.insert(d->getName());
            }
        }
    }
}

// Orientation decision for discriminating path rule (7.6.3 version).
// Gets sep(d, c) from sepsets and checks if b is in it.
// Returns true for noncollider (tail rule applied), false for collider.
bool FciOrient::doDdpOrientation(const NodePtr& d, const NodePtr& a, const NodePtr& b,
                                   const NodePtr& c, Graph& graph) {
    // Java precondition (FciOrient.java:850-852): d and c are non-adjacent by
    // construction in ddpOrient; Java throws IllegalArgumentException otherwise.
    if (graph.isAdjacentTo(d, c)) {
        throw std::invalid_argument("doDdpOrientation: d and c must be non-adjacent");
    }

    const std::set<NodePtr>* sepset = sepsets_.getSepset(d, c);

    if (sepset == nullptr) {
        return false;
    }

    bool bInSepset = (sepset->find(b) != sepset->end());

    if (!bInSepset && doDiscriminatingPathColliderRule_) {
        // b not in sep(d,c): collider. Java sets BOTH arrowheads at b — from a
        // and from c — each guarded by its own isArrowheadAllowed, and returns
        // false on either failure (FciOrient.java:867-877).
        if (!isArrowheadAllowed(a, b, graph, knowledge_)) return false;
        if (!isArrowheadAllowed(c, b, graph, knowledge_)) return false;

        graph.setEndpoint(a, b, Endpoint::ARROW);
        graph.setEndpoint(c, b, Endpoint::ARROW);
        changeFlag_ = true;
        return false;  // 7.6.3: falls through to `return false` for the collider case
    } else if (doDiscriminatingPathTailRule_) {
        // Java's else-if is on doDiscriminatingPathTailRule alone
        // (FciOrient.java:885) — there is no sepset.contains(b) guard, so this
        // also fires when b is not in sep(d,c) but the collider rule is off.
        graph.setEndpoint(c, b, Endpoint::TAIL);
        changeFlag_ = true;
        return true;
    }

    return false;
}

// R5 (7.6.3): Node-centric iteration.
// If a o-o b, and there is an uncovered circle path u from a to b s.t.
// gamma (first hop from a) not adj to b, theta (last before b) not adj to a,
// orient a -- b and all path edges as undirected.
void FciOrient::ruleR5(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& a : nodes) {
        // adjacents = getNodesInTo(a, CIRCLE): nodes b where c *-o a (circle at a from b)
        auto adjacents = graph.getNodesInTo(a, Endpoint::CIRCLE);

        for (const auto& b : adjacents) {
            // Also check a o-o b: endpoint at b from a = CIRCLE
            if (graph.getEndpoint(a, b) != Endpoint::CIRCLE) continue;
            // We know a o-o b.

            std::vector<NodePtr> path = findUncoveredCirclePath(graph, a, b);
            if (path.empty()) continue;

            // Orient a-b as undirected (TAIL-TAIL)
            graph.setEndpoint(a, b, Endpoint::TAIL);
            graph.setEndpoint(b, a, Endpoint::TAIL);

            // Orient all edges on the found path as undirected
            for (size_t i = 0; i + 1 < path.size(); i++) {
                graph.setEndpoint(path[i], path[i + 1], Endpoint::TAIL);
                graph.setEndpoint(path[i + 1], path[i], Endpoint::TAIL);
            }

            changeFlag_ = true;
        }
    }
}

// R6 and R7 combined (7.6.3 style): ChoiceGenerator over adj(b).
// R6: If A---Bo-*C (A not adj C), orient B-*C as B--*C (set circle at b from c to tail).
// R7: If A--oBo-*C (A not adj C), orient B-*C as B--*C.
void FciOrient::ruleR6R7(Graph& graph) {
    auto nodes = graph.getNodes();

    for (const auto& b : nodes) {
        auto adjacents = graph.getAdjacentNodes(b);
        if (static_cast<int>(adjacents.size()) < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adjacents.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& a = adjacents[choice[0]];
            const auto& c = adjacents[choice[1]];

            // Both R6 and R7 require a and c not adjacent (7.6.3 behavior)
            if (graph.isAdjacentTo(a, c)) continue;

            // A --* B: endpoint at a from b = TAIL (a is tail-ended)
            if (graph.getEndpoint(b, a) != Endpoint::TAIL) continue;
            // B o-* C: endpoint at b from c = CIRCLE
            if (graph.getEndpoint(c, b) != Endpoint::CIRCLE) continue;

            // R6: A --- B (also tail at b from a)
            if (graph.getEndpoint(a, b) == Endpoint::TAIL) {
                graph.setEndpoint(c, b, Endpoint::TAIL);
                changeFlag_ = true;
            }
            // R7: A --o B (circle at b from a)
            else if (graph.getEndpoint(a, b) == Endpoint::CIRCLE) {
                graph.setEndpoint(c, b, Endpoint::TAIL);
                changeFlag_ = true;
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

    auto intoCArrows = graph.getNodesInTo(c, Endpoint::ARROW);

    for (const auto& b : intoCArrows) {
        if (!graph.isAdjacentTo(a, b)) continue;

        // endpoint at a from b = TAIL: a is tail-ended (a --* b)
        if (graph.getEndpoint(b, a) != Endpoint::TAIL) continue;
        // endpoint at b from c = TAIL: b -> c
        if (graph.getEndpoint(c, b) != Endpoint::TAIL) continue;
        // endpoint at b from a != TAIL: a --> b or a --o b
        if (graph.getEndpoint(a, b) == Endpoint::TAIL) continue;

        // A-->B-->C or A--oB-->C: R8 applies
        graph.setEndpoint(c, a, Endpoint::TAIL);
        changeFlag_ = true;
        return true;
    }

    return false;
}

// R9: If a o-> c, and there is an uncovered pd path from a to c
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

        // Check if there's an uncovered semi-directed path from beta to c
        // (beta is first step after a, so prevOfFrom = a).
        if (existsUncoveredSemiDirectedPath(graph, beta, c, a)) {
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

    auto intoCArrows = graph.getNodesInTo(gamma, Endpoint::ARROW);

    for (const auto& beta : intoCArrows) {
        if (*beta == *alpha) continue;
        // beta -> gamma: endpoint at beta from gamma = TAIL
        if (graph.getEndpoint(gamma, beta) != Endpoint::TAIL) continue;

        for (const auto& theta : intoCArrows) {
            if (*theta == *alpha || *theta == *beta) continue;
            // 7.6.3: uses getEndpoint(theta, gamma) instead of getEndpoint(gamma, theta)
            // This is a bug in 7.6.3 (always fails since theta is in intoCArrows meaning
            // endpoint at gamma from theta = ARROW, so getEndpoint(theta, gamma) = ARROW != TAIL).
            // Replicating the bug for 7.6.3 compatibility:
            if (graph.getEndpoint(theta, gamma) != Endpoint::TAIL) continue;

            auto adjAlpha = graph.getAdjacentNodes(alpha);
            adjAlpha.erase(std::remove_if(adjAlpha.begin(), adjAlpha.end(),
                [&](const NodePtr& n) { return *n == *beta || *n == *theta; }), adjAlpha.end());

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

// Port of GraphUtils.fciOrientbk (GraphUtils.java:1833-1871).
//
// Deliberately NOT the same as FciOrient::fciOrientbk above. 7.6.3 has two
// separate implementations, and gfciR0 calls this one (GraphUtils.java:1793):
// it has no isArrowheadAllowed guard and forces the endpoint unconditionally.
// It also has no `if (knowledge.isEmpty()) return` early exit — with empty
// knowledge both iterators are simply empty, so the effect is the same, but the
// call site must not gate on it either.
void FciOrient::graphUtilsFciOrientbk(const Knowledge& bk, Graph& graph,
                                      const std::vector<NodePtr>& variables) {
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

        if (graph.getEdge(from, to).isNull()) continue;

        // Orient to *-> from. No arrowhead-allowed check: Java does not have one.
        graph.setEndpoint(to, from, Endpoint::ARROW);
    }

    for (const auto& ke : bk.getListOfRequiredEdges()) {
        NodePtr from = findNode(ke.from);
        NodePtr to = findNode(ke.to);
        if (!from || !to) continue;

        if (graph.getEdge(from, to).isNull()) continue;

        graph.setEndpoint(to, from, Endpoint::TAIL);
        graph.setEndpoint(from, to, Endpoint::ARROW);
    }
}

bool FciOrient::isPartiallyOrientedEdge(const NodePtr& a, const NodePtr& b, const Graph& graph) {
    // A o-> B: endpoint at A is CIRCLE, endpoint at B is ARROW
    return graph.getEndpoint(b, a) == Endpoint::CIRCLE && graph.getEndpoint(a, b) == Endpoint::ARROW;
}

} // namespace tetrad
