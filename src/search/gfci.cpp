#include "search/gfci.h"
#include "search/fges.h"
#include "search/fci_orient.h"
#include "util/choice_generator.h"
#include "util/java_hash.h"
#include "util/sublist_generator.h"
#include <algorithm>
#include <queue>
#include <unordered_map>
namespace tetrad {

// ─────────────────────────────────────────────────────────────────────────────
// File-scope helpers for possible d-sep computation
// ─────────────────────────────────────────────────────────────────────────────

// Traverse edge from `node` along a semi-directed step: returns the far end
// if the endpoint AT `node` is TAIL or CIRCLE (i.e. not ARROW), else nullptr.
static NodePtr traverseSemiDirected(const NodePtr& node, const Edge& edge) {
    Endpoint ep = edge.getEndpoint(node);
    if (ep == Endpoint::TAIL || ep == Endpoint::CIRCLE) {
        return edge.getDistalNode(node);
    }
    return nullptr;
}

// BFS: does a semi-directed path from `from` to `to` exist?
static bool existsSemiDirectedPath(const Graph& graph, const NodePtr& from, const NodePtr& to) {
    std::queue<NodePtr> Q;
    std::unordered_set<NodePtr> V;

    for (const auto& u : graph.getAdjacentNodes(from)) {
        Edge edge = graph.getEdge(from, u);
        NodePtr c = traverseSemiDirected(from, edge);
        if (c && !V.count(c)) {
            V.insert(c);
            Q.push(c);
        }
    }

    while (!Q.empty()) {
        NodePtr t = Q.front(); Q.pop();
        if (*t == *to) return true;
        for (const auto& u : graph.getAdjacentNodes(t)) {
            Edge edge = graph.getEdge(t, u);
            NodePtr c = traverseSemiDirected(t, edge);
            if (c && !V.count(c)) {
                V.insert(c);
                Q.push(c);
            }
        }
    }
    return false;
}

// Returns true if there exists some r in previous[w] (r != b, r != x) s.t.
// existsSemiDirectedPath(r, x) or existsSemiDirectedPath(r, b).
static bool existOnePathWithPossibleParents(
    const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& previous,
    const NodePtr& w, const NodePtr& x, const NodePtr& b,
    const Graph& graph)
{
    if (*w == *x) return true;
    auto it = previous.find(w);
    if (it == previous.end()) return false;
    for (const auto& r : it->second) {
        if (*r == *b || *r == *x) continue;
        if (existsSemiDirectedPath(graph, r, x) || existsSemiDirectedPath(graph, r, b)) {
            return true;
        }
    }
    return false;
}

// Compute the possible d-sep set for node x (BFS through definite colliders
// and shielded triples). Returns all reachable nodes except x itself.
// Port of Paths.possibleDsep(Node x, int maxPossibleDsepPathLength).
static std::vector<NodePtr> computePossibleDsep(const Graph& graph,
                                                  const NodePtr& x,
                                                  int maxLength) {
    struct PairHash {
        size_t operator()(const std::pair<NodePtr, NodePtr>& p) const {
            size_t h1 = std::hash<NodePtr>{}(p.first);
            size_t h2 = std::hash<NodePtr>{}(p.second);
            return h1 ^ (h2 * 2654435761u);
        }
    };
    struct PairEq {
        bool operator()(const std::pair<NodePtr, NodePtr>& a,
                        const std::pair<NodePtr, NodePtr>& b) const {
            return *a.first == *b.first && *a.second == *b.second;
        }
    };
    using NodePair = std::pair<NodePtr, NodePtr>;

    std::unordered_set<NodePtr> msep;
    std::queue<NodePair> Q;
    std::unordered_set<NodePair, PairHash, PairEq> V;
    std::unordered_map<NodePtr, std::unordered_set<NodePtr>> previous;
    previous[x] = {};

    // Level-tracking marker (mirrors Java's reference-equality sentinel).
    bool markerSet = false;
    NodePair levelMarker;
    int distance = 0;

    // Sort initial adjacencies by Java HashSet iteration order to replicate
    // the BFS frontier ordering from Paths.possibleDsep (line 1605:
    // new HashSet<>(graph.getAdjacentNodes(x))).
    auto adjX = graph.getAdjacentNodes(x);
    sortByJavaHashOrder(adjX, x);
    for (const auto& b : adjX) {
        NodePair edge = {x, b};
        if (!markerSet) { levelMarker = edge; markerSet = true; }
        Q.push(edge);
        V.insert(edge);
        previous[x].insert(b);  // Java: addToSet(previous, b, x) → previous[x].add(b)
        msep.insert(b);
    }

    while (!Q.empty()) {
        NodePair t = Q.front(); Q.pop();

        // Advance level counter when we reach the front-of-level marker.
        if (markerSet && PairEq{}(t, levelMarker)) {
            markerSet = false;
            distance++;
            int limit = (maxLength < 0) ? 1000 : maxLength;
            if (distance > limit) break;
        }

        const NodePtr& a = t.first;
        const NodePtr& b = t.second;

        if (existOnePathWithPossibleParents(previous, b, x, b, graph)) {
            msep.insert(b);
        }

        // Sort inner-loop adjacencies by Java HashSet order to match
        // EdgeListGraph.getAdjacentNodes() iteration in Paths.java:1636.
        auto adjB = graph.getAdjacentNodes(b);
        sortByJavaHashOrder(adjB, b);
        for (const auto& c : adjB) {
            if (*c == *a || *c == *x) continue;
            previous[c].insert(b);  // Java: addToSet(previous, b, c) → previous[c].add(b)

            if (graph.isDefCollider(a, b, c) || graph.isAdjacentTo(a, c)) {
                NodePair u = {b, c};
                if (!V.count(u)) {
                    V.insert(u);
                    Q.push(u);
                    if (!markerSet) { levelMarker = u; markerSet = true; }
                }
            }
        }
    }

    msep.erase(x);

    // Java sorts the result reverse-alphabetically (Collections.sort + reverse).
    // The ordering matters because SublistGenerator enumerates subsets in index
    // order and the first separating set found wins.
    std::vector<NodePtr> result(msep.begin(), msep.end());
    std::sort(result.begin(), result.end(),
              [](const NodePtr& a, const NodePtr& b) {
                  return a->getName() > b->getName();  // descending (reverse alpha)
              });
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────

Gfci::Gfci(IndependenceTest& test, Score& score) : test_(test), score_(score) {}

Graph Gfci::search() {
    sepsetStorage_.clear();

    auto nodes = test_.getVariables();

    // Step 1: Get Markov CPDAG from FGES
    Graph cpdag = getMarkovCpdag();

    // Step 2: Create PAG copy
    Graph pag(cpdag);

    std::unordered_set<Triple> unshieldedColliders;
    SepsetMap sepsetMap;

    // Step 3: Extra edge removal
    auto edges = pag.getEdges();
    sortEdgesByJavaHashOrder(edges, static_cast<int>(edges.size()));

    for (const auto& edge : edges) {
        const auto& a = edge.getNode1();
        const auto& c = edge.getNode2();

        auto* sepset = findSepset(pag, a, c, {});

        if (sepset) {
            pag.removeEdge(a, c);
            sepsetMap.set(a, c, *sepset);
        }
    }

    // Step 4: Reorient all to circles, apply knowledge
    pag.reorientAllWith(Endpoint::CIRCLE);

    if (!knowledge_.isEmpty()) {
        auto findNode = [&](const std::string& name) -> NodePtr {
            for (const auto& n : nodes) {
                if (n->getName() == name) return n;
            }
            return nullptr;
        };

        for (const auto& ke : knowledge_.getListOfForbiddenEdges()) {
            NodePtr from = findNode(ke.from);
            NodePtr to = findNode(ke.to);
            if (!from || !to) continue;
            Edge e = pag.getEdge(from, to);
            if (e.isNull()) continue;
            pag.setEndpoint(to, from, Endpoint::ARROW);
        }

        for (const auto& ke : knowledge_.getListOfRequiredEdges()) {
            NodePtr from = findNode(ke.from);
            NodePtr to = findNode(ke.to);
            if (!from || !to) continue;
            Edge e = pag.getEdge(from, to);
            if (e.isNull()) continue;
            pag.setEndpoint(to, from, Endpoint::TAIL);
            pag.setEndpoint(from, to, Endpoint::ARROW);
        }
    }

    // Step 5: Orient colliders from CPDAG and sepsets (first round)
    orientCollidersFromCpdag(pag, cpdag, nodes, sepsetMap, unshieldedColliders);

    // Step 6: Possible d-sep removal
    // Matches Java's Gfci.java: try possibleDsep from a first, then from c.
    auto pagEdges = pag.getEdges();
    sortEdgesByJavaHashOrder(pagEdges, static_cast<int>(pagEdges.size()));

    for (const auto& edge : pagEdges) {
        const auto& a = edge.getNode1();
        const auto& c = edge.getNode2();

        if (!pag.isAdjacentTo(a, c)) continue;

        // Try conditioning on possible d-sep set of a
        {
            auto candidates = computePossibleDsep(pag, a, maxDiscriminatingPathLength_);
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [&](const NodePtr& n) { return *n == *a || *n == *c; }),
                candidates.end());

            auto* sepset = findSepsetFromList(pag, a, c, candidates);
            if (sepset) {
                pag.removeEdge(a, c);
                sepsetMap.set(a, c, *sepset);
                continue;
            }
        }

        if (!pag.isAdjacentTo(a, c)) continue;

        // Try conditioning on possible d-sep set of c
        {
            auto candidates = computePossibleDsep(pag, c, maxDiscriminatingPathLength_);
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [&](const NodePtr& n) { return *n == *a || *n == *c; }),
                candidates.end());

            auto* sepset = findSepsetFromList(pag, a, c, candidates);
            if (sepset) {
                pag.removeEdge(a, c);
                sepsetMap.set(a, c, *sepset);
            }
        }
    }

    // Step 7: Re-orient colliders after possible d-sep removal
    orientCollidersFromCpdag(pag, cpdag, nodes, sepsetMap, unshieldedColliders);

    // Step 8: Apply FCI final orientation rules (R1-R10)
    R0R4StrategyTestBased strategy(test_);
    strategy.setKnowledge(knowledge_);
    strategy.setDepth(depth_);

    FciOrient fciOrient(strategy);
    fciOrient.setCompleteRuleSetUsed(completeRuleSetUsed_);
    fciOrient.setMaxDiscriminatingPathLength(maxDiscriminatingPathLength_);
    fciOrient.setVerbose(verbose_);

    fciOrient.finalOrientation(pag);

    return pag;
}

Graph Gfci::getMarkovCpdag() {
    Fges fges(score_);
    fges.setKnowledge(knowledge_);
    fges.setVerbose(verbose_);
    fges.setFaithfulnessAssumed(faithfulnessAssumed_);
    if (maxDegree_ > 0) fges.setMaxDegree(maxDegree_);
    return fges.search();
}

// Helper: find first separating set from a single adjacency list.
// Returns (sepset, p-value) or (nullptr, -1) if none found.
// Matches Java's getSepset() with useMaxP=false (greedy, first-found).
std::pair<std::set<NodePtr>*, double> Gfci::getSepsetFromAdj(
    const NodePtr& x, const NodePtr& y,
    const std::vector<NodePtr>& adj,
    const std::set<NodePtr>& containing) {
    int maxDepth = depth_;
    int d = (maxDepth < 0) ? static_cast<int>(adj.size())
                           : std::min(maxDepth, static_cast<int>(adj.size()));
    SublistGenerator gen(static_cast<int>(adj.size()), d);
    bool valid;
    while (true) {
        const auto& choice = gen.next(valid);
        if (!valid) break;

        std::set<NodePtr> subset;
        for (int idx : choice) subset.insert(adj[idx]);

        bool containsAll = true;
        for (const auto& c : containing) {
            if (!subset.count(c)) { containsAll = false; break; }
        }
        if (!containsAll) continue;

        auto result = test_.checkIndependence(x, y, subset);
        if (result.isIndependent()) {
            sepsetStorage_.push_back(std::make_unique<std::set<NodePtr>>(subset));
            return std::make_pair(sepsetStorage_.back().get(), result.pValue);
        }
    }
    return std::make_pair(nullptr, -1.0);
}

// Matches Java's sepsetSubsetOfAdjxOrAdjy: try both adj(x) and adj(y),
// if both find a sepset pick the one with higher p-value.
std::set<NodePtr>* Gfci::findSepset(const Graph& graph, const NodePtr& x, const NodePtr& y,
                                     const std::set<NodePtr>& containing) {
    auto adjX = graph.getAdjacentNodes(x);
    auto adjY = graph.getAdjacentNodes(y);

    // Sort BEFORE filtering: Java's getAdjacentNodes() builds a HashSet from ALL
    // neighbors, then adjx.remove(y) just removes from the ArrayList. The HashSet
    // capacity (and thus bucket assignments) depends on the full neighbor count.
    sortByJavaHashOrder(adjX, x);
    sortByJavaHashOrder(adjY, y);

    adjX.erase(std::remove_if(adjX.begin(), adjX.end(),
        [&](const NodePtr& n) { return *n == *y; }), adjX.end());
    adjY.erase(std::remove_if(adjY.begin(), adjY.end(),
        [&](const NodePtr& n) { return *n == *x; }), adjY.end());

    adjX.erase(std::remove_if(adjX.begin(), adjX.end(),
        [](const NodePtr& n) { return n->getNodeType() == NodeType::LATENT; }), adjX.end());
    adjY.erase(std::remove_if(adjY.begin(), adjY.end(),
        [](const NodePtr& n) { return n->getNodeType() == NodeType::LATENT; }), adjY.end());

    auto [sepset1, p1] = getSepsetFromAdj(x, y, adjX, containing);
    auto [sepset2, p2] = getSepsetFromAdj(y, x, adjY, containing);

    if (!sepset1 && !sepset2) return nullptr;
    if (sepset1 && !sepset2) return sepset1;
    if (!sepset1) return sepset2;

    // Both found: pick higher p-value (matches Java's sepsetSubsetOfAdjxOrAdjy).
    return (p1 > p2) ? sepset1 : sepset2;
}

std::set<NodePtr>* Gfci::findSepsetFromList(const Graph& graph, const NodePtr& x, const NodePtr& y,
                                              const std::vector<NodePtr>& candidates) {
    (void)graph;
    int maxDepth = depth_;
    int depthLimit = (maxDepth < 0) ? static_cast<int>(candidates.size())
                                    : std::min(maxDepth, static_cast<int>(candidates.size()));

    // Matches Java's Gfci.getSepset: enumerate all subsets via SublistGenerator,
    // return first that yields independence. No size or adj-subset filters
    // (those are only in Paths.removeByPossibleDsep, which GFCI doesn't use).
    SublistGenerator gen(static_cast<int>(candidates.size()), depthLimit);
    bool valid;
    while (true) {
        const auto& choice = gen.next(valid);
        if (!valid) break;

        std::set<NodePtr> subset;
        for (int idx : choice) subset.insert(candidates[idx]);

        std::vector<NodePtr> condVec(subset.begin(), subset.end());
        if (test_.isIndependent(x, y, condVec)) {
            sepsetStorage_.push_back(std::make_unique<std::set<NodePtr>>(subset));
            return sepsetStorage_.back().get();
        }
    }
    return nullptr;
}

void Gfci::orientCollidersFromCpdag(Graph& pag, const Graph& cpdag,
                                     const std::vector<NodePtr>& nodes,
                                     const SepsetMap& sepsetMap,
                                     std::unordered_set<Triple>& unshieldedColliders) {
    for (const auto& y : nodes) {
        auto adj = pag.getAdjacentNodes(y);
        if (adj.size() < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adj.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& x = adj[choice[0]];
            const auto& z = adj[choice[1]];

            if (cpdag.isDefCollider(x, y, z)) {
                if (colliderAllowed(pag, x, y, z, knowledge_)) {
                    pag.setEndpoint(x, y, Endpoint::ARROW);
                    pag.setEndpoint(z, y, Endpoint::ARROW);
                    // Java adds unconditionally; match that behavior.
                    unshieldedColliders.insert(Triple(x, y, z));
                }
            } else if (cpdag.isAdjacentTo(x, z)) {
                auto sepset = sepsetMap.get(x, z);
                if (sepset) {
                    bool yInSepset = false;
                    for (const auto& n : *sepset) {
                        if (*n == *y) { yInSepset = true; break; }
                    }
                    if (!yInSepset) {
                        if (colliderAllowed(pag, x, y, z, knowledge_)) {
                            pag.setEndpoint(x, y, Endpoint::ARROW);
                            pag.setEndpoint(z, y, Endpoint::ARROW);
                            if (!pag.isAdjacentTo(x, z)) {
                                unshieldedColliders.insert(Triple(x, y, z));
                            }
                        }
                    }
                }
            }
        }
    }
}

bool Gfci::colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                            const NodePtr& z, const Knowledge& knowledge) {
    if (knowledge.isEmpty()) return true;
    return FciOrient::isArrowheadAllowed(x, y, pag, knowledge) &&
           FciOrient::isArrowheadAllowed(z, y, pag, knowledge);
}

} // namespace tetrad
