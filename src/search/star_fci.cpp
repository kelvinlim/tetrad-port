#include "search/star_fci.h"
#include "search/fci_orient.h"
#include "util/choice_generator.h"
#include "util/sublist_generator.h"
#include <algorithm>

namespace tetrad {

StarFci::StarFci(IndependenceTest& test) : test_(test) {}

Graph StarFci::search() {
    sepsetStorage_.clear();

    auto nodes = test_.getVariables();

    // Step 1: Get Markov CPDAG from subclass
    Graph cpdag = getMarkovCpdag();

    // Step 2: Create PAG copy
    Graph pag(cpdag);
    std::unordered_set<Triple> unshieldedColliders;
    SepsetMap sepsetMap;

    // Step 3: Extra edge removal via independence testing
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

    // Step 4: Reorient all to circles, apply knowledge-based orientation
    pag.reorientAllWith(Endpoint::CIRCLE);

    // fciOrientbk: apply knowledge constraints
    if (!knowledge_.isEmpty()) {
        R0R4StrategyTestBased strategyTmp(test_);
        strategyTmp.setKnowledge(knowledge_);
        FciOrient fciOrientTmp(strategyTmp);
        fciOrientTmp.fciOrientbk(knowledge_, pag, nodes);
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
                    unshieldedColliders.insert(Triple(x, y, z));
                }
            } else if (cpdag.isAdjacentTo(x, z)) {
                auto sepset = sepsetMap.get(x, z);

                if (sepset.has_value() && sepset->count(y) == 0) {
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

    // Step 6: Apply FCI final orientation rules (R1-R10)
    R0R4StrategyTestBased strategy(test_);
    strategy.setKnowledge(knowledge_);
    strategy.setDepth(-1);

    FciOrient fciOrient(strategy);
    fciOrient.setCompleteRuleSetUsed(completeRuleSetUsed_);
    fciOrient.setMaxDiscriminatingPathLength(maxDiscriminatingPathLength_);
    fciOrient.setVerbose(verbose_);

    fciOrient.finalOrientation(pag);

    return pag;
}

std::set<NodePtr>* StarFci::findSepset(const Graph& graph, const NodePtr& x, const NodePtr& y,
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

bool StarFci::colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                                const NodePtr& z, const Knowledge& knowledge) {
    if (knowledge.isEmpty()) return true;
    return FciOrient::isArrowheadAllowed(x, y, pag, knowledge) &&
           FciOrient::isArrowheadAllowed(z, y, pag, knowledge);
}

} // namespace tetrad
