#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "search/independence_test.h"
#include "search/sepsets_greedy.h"
#include "data/knowledge.h"
#include <set>

namespace tetrad {

// Greedy FCI (GFCI) algorithm.
// Port of edu.cmu.tetrad.search.GFci from Java Tetrad 7.6.3.
//
// Starts with a Markov CPDAG from FGES, then fixes it for latent variables:
// 1. Run FGES to get CPDAG
// 2. Extra edge removal via SepsetsGreedy independence testing
// 3. gfciR0: reorient circles, apply knowledge, orient colliders from CPDAG/sepsets
// 4. Apply FCI orientation rules R1-R10
//
// Reference: Ogarrio, Spirtes & Ramsey (2016), "A hybrid causal search
// algorithm for latent variable models."
class Gfci {
public:
    Gfci(IndependenceTest& test, Score& score);

    Graph search();

    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    const Knowledge& getKnowledge() const { return knowledge_; }

    void setCompleteRuleSetUsed(bool used) { completeRuleSetUsed_ = used; }
    void setMaxDiscriminatingPathLength(int len) { maxDiscriminatingPathLength_ = len; }
    void setDepth(int depth) { depth_ = depth; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setFaithfulnessAssumed(bool assumed) { faithfulnessAssumed_ = assumed; }
    void setMaxDegree(int maxDegree) { maxDegree_ = maxDegree; }
    void setDoDiscriminatingPathRule(bool b) {
        doDiscriminatingPathColliderRule_ = b;
        doDiscriminatingPathTailRule_ = b;
    }

private:
    Graph getMarkovCpdag();

    // Port of GraphUtils.gfciR0: reorient all to circles, apply knowledge,
    // then orient colliders from CPDAG definite colliders and removed-edge sepsets.
    void gfciR0(Graph& pag, const Graph& cpdag, SepsetsGreedy& sepsets);

    // Check if collider orientation is allowed by knowledge.
    static bool colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                                 const NodePtr& z, const Knowledge& knowledge);

    IndependenceTest& test_;
    Score& score_;
    Knowledge knowledge_;
    bool completeRuleSetUsed_ = true;
    int maxDiscriminatingPathLength_ = -1;
    int depth_ = -1;
    int maxDegree_ = -1;
    bool verbose_ = false;
    bool faithfulnessAssumed_ = true;
    bool doDiscriminatingPathColliderRule_ = true;
    bool doDiscriminatingPathTailRule_ = true;
};

} // namespace tetrad
