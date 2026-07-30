#include "search/gfci.h"
#include "search/fges.h"
#include "search/fci_orient.h"
#include "search/sepsets_greedy.h"
#include "util/choice_generator.h"
#include "util/java_hash.h"
#include <algorithm>

namespace tetrad {

Gfci::Gfci(IndependenceTest& test, Score& score) : test_(test), score_(score) {}

Graph Gfci::search() {
    auto nodes = test_.getVariables();

    // Step 1: Get Markov CPDAG from FGES
    Graph cpdag = getMarkovCpdag();
    const Graph referenceCpdag(cpdag);

    // Step 2: Create PAG copy; SepsetsGreedy holds a reference to this evolving graph.
    Graph pag(cpdag);
    SepsetsGreedy sepsets(pag, test_, depth_, knowledge_);

    // Step 3: Extra edge removal (Java 7.6.3 gfciExtraEdgeRemovalStep).
    // For each node b, generate pairs (a,c) from referenceCpdag adjacents.
    // Remove edge a-c from pag if a sepset is found.
    for (const auto& b : nodes) {
        auto adjacentNodes = referenceCpdag.getAdjacentNodes(b);
        sortByJavaHashOrder(adjacentNodes, b);
        if (static_cast<int>(adjacentNodes.size()) < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adjacentNodes.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& a = adjacentNodes[choice[0]];
            const auto& c = adjacentNodes[choice[1]];
            if (pag.isAdjacentTo(a, c) && referenceCpdag.isAdjacentTo(a, c)) {
                const std::set<NodePtr>* sep = sepsets.getSepset(a, c);
                if (sep) {
                    pag.removeEdge(a, c);
                }
            }
        }
    }

    // Step 4: gfciR0 — reorient circles, apply knowledge, orient colliders.
    gfciR0(pag, referenceCpdag, sepsets);

    // Step 5: FCI final orientation rules R1-R10.
    FciOrient fciOrient(sepsets);
    fciOrient.setCompleteRuleSetUsed(completeRuleSetUsed_);
    fciOrient.setMaxDiscriminatingPathLength(maxDiscriminatingPathLength_);
    fciOrient.setVerbose(verbose_);
    fciOrient.setKnowledge(knowledge_);
    fciOrient.setDoDiscriminatingPathColliderRule(doDiscriminatingPathColliderRule_);
    fciOrient.setDoDiscriminatingPathTailRule(doDiscriminatingPathTailRule_);
    fciOrient.doFinalOrientation(pag);

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

// Port of GraphUtils.gfciR0 from Java Tetrad 7.6.3.
// Reorients all edges to circles, applies background knowledge, then orients
// colliders from the reference CPDAG and removed-edge sepsets.
void Gfci::gfciR0(Graph& pag, const Graph& referenceCpdag, SepsetsGreedy& sepsets) {
    pag.reorientAllWith(Endpoint::CIRCLE);

    // Apply background knowledge. Java calls GraphUtils.fciOrientbk here
    // (GraphUtils.java:1793) — the unguarded variant, unconditionally — not
    // FciOrient.fciOrientbk, which guards with isArrowheadAllowed.
    {
        auto nodes = pag.getNodes();
        FciOrient::graphUtilsFciOrientbk(knowledge_, pag, nodes);
    }

    auto nodes = pag.getNodes();

    for (const auto& b : nodes) {
        auto adjacentNodes = pag.getAdjacentNodes(b);
        sortByJavaHashOrder(adjacentNodes, b);
        if (static_cast<int>(adjacentNodes.size()) < 2) continue;

        ChoiceGenerator cg(static_cast<int>(adjacentNodes.size()), 2);
        const int* choice;
        while ((choice = cg.next()) != nullptr) {
            const auto& a = adjacentNodes[choice[0]];
            const auto& c = adjacentNodes[choice[1]];

            if (referenceCpdag.isDefCollider(a, b, c)) {
                if (colliderAllowed(pag, a, b, c, knowledge_)) {
                    pag.setEndpoint(a, b, Endpoint::ARROW);
                    pag.setEndpoint(c, b, Endpoint::ARROW);
                }
            } else if (referenceCpdag.isAdjacentTo(a, c) && !pag.isAdjacentTo(a, c)) {
                const std::set<NodePtr>* sep = sepsets.getSepset(a, c);
                if (sep && sep->find(b) == sep->end()) {
                    if (colliderAllowed(pag, a, b, c, knowledge_)) {
                        pag.setEndpoint(a, b, Endpoint::ARROW);
                        pag.setEndpoint(c, b, Endpoint::ARROW);
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
