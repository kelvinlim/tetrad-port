#include "search/gfci.h"
#include "search/fges.h"
#include "search/fci_orient.h"
#include "util/choice_generator.h"
#include "util/sublist_generator.h"
#include <algorithm>

namespace tetrad {

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

    // Apply knowledge-based orientation
    if (!knowledge_.isEmpty()) {
        // fciOrientbk
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

    // Step 5: Orient colliders from CPDAG and sepsets
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
                    if (!pag.isAdjacentTo(x, z)) {
                        unshieldedColliders.insert(Triple(x, y, z));
                    }
                }
            } else if (cpdag.isAdjacentTo(x, z)) {
                auto sepset = sepsetMap.get(x, z);
                if (sepset) {
                    // Check if y is NOT in the sepset → collider
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

    // Step 6: Apply FCI final orientation rules (R1-R10)
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

std::set<NodePtr>* Gfci::findSepset(const Graph& graph, const NodePtr& x, const NodePtr& y,
                                     const std::set<NodePtr>& containing) {
    auto adjX = graph.getAdjacentNodes(x);
    auto adjY = graph.getAdjacentNodes(y);

    // Remove y from adjX, x from adjY
    adjX.erase(std::remove_if(adjX.begin(), adjX.end(),
        [&](const NodePtr& n) { return *n == *y; }), adjX.end());
    adjY.erase(std::remove_if(adjY.begin(), adjY.end(),
        [&](const NodePtr& n) { return *n == *x; }), adjY.end());

    // Remove latent variables
    adjX.erase(std::remove_if(adjX.begin(), adjX.end(),
        [](const NodePtr& n) { return n->getNodeType() == NodeType::LATENT; }), adjX.end());
    adjY.erase(std::remove_if(adjY.begin(), adjY.end(),
        [](const NodePtr& n) { return n->getNodeType() == NodeType::LATENT; }), adjY.end());

    int maxDepth = depth_;

    // Try subsets of adjX
    int depthX = (maxDepth < 0) ? static_cast<int>(adjX.size()) : std::min(maxDepth, static_cast<int>(adjX.size()));

    SublistGenerator genX(static_cast<int>(adjX.size()), depthX);
    bool valid;
    while (true) {
        const auto& choice = genX.next(valid);
        if (!valid) break;

        std::set<NodePtr> subset;
        for (int idx : choice) {
            subset.insert(adjX[idx]);
        }

        // Check if subset contains required nodes
        bool containsAll = true;
        for (const auto& c : containing) {
            if (!subset.count(c)) { containsAll = false; break; }
        }
        if (!containsAll) continue;

        std::vector<NodePtr> condVec(subset.begin(), subset.end());
        if (test_.isIndependent(x, y, condVec)) {
            sepsetStorage_.push_back(std::make_unique<std::set<NodePtr>>(subset));
            return sepsetStorage_.back().get();
        }
    }

    // Try subsets of adjY
    int depthY = (maxDepth < 0) ? static_cast<int>(adjY.size()) : std::min(maxDepth, static_cast<int>(adjY.size()));

    SublistGenerator genY(static_cast<int>(adjY.size()), depthY);
    while (true) {
        const auto& choice = genY.next(valid);
        if (!valid) break;

        std::set<NodePtr> subset;
        for (int idx : choice) {
            subset.insert(adjY[idx]);
        }

        bool containsAll = true;
        for (const auto& c : containing) {
            if (!subset.count(c)) { containsAll = false; break; }
        }
        if (!containsAll) continue;

        std::vector<NodePtr> condVec(subset.begin(), subset.end());
        if (test_.isIndependent(x, y, condVec)) {
            sepsetStorage_.push_back(std::make_unique<std::set<NodePtr>>(subset));
            return sepsetStorage_.back().get();
        }
    }

    return nullptr;
}

bool Gfci::colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                            const NodePtr& z, const Knowledge& knowledge) {
    if (knowledge.isEmpty()) return true;
    return FciOrient::isArrowheadAllowed(x, y, pag, knowledge) &&
           FciOrient::isArrowheadAllowed(z, y, pag, knowledge);
}

} // namespace tetrad
