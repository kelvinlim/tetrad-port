#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "search/independence_test.h"
#include "search/sepset_map.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_set>

namespace tetrad {

// Greedy FCI (GFCI) algorithm.
// Port of edu.cmu.tetrad.search.Gfci from Java Tetrad 7.6.8.
//
// Starts with a Markov CPDAG from FGES, then fixes it for latent variables:
// 1. Run FGES to get CPDAG
// 2. Extra edge removal via independence testing
// 3. Reorient to circles, apply knowledge
// 4. Copy colliders from CPDAG, orient from sepsets
// 5. Possible d-sep removal
// 6. Apply FCI orientation rules R1-R10
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

private:
    Graph getMarkovCpdag();

    // Sepset finding: find a separating set from adj(x) or adj(y).
    std::set<NodePtr>* findSepset(const Graph& graph, const NodePtr& x, const NodePtr& y,
                                   const std::set<NodePtr>& containing);

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

    // Temporary storage for sepset results (lifetime managed per search call)
    std::vector<std::unique_ptr<std::set<NodePtr>>> sepsetStorage_;
};

} // namespace tetrad
