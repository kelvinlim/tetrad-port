#pragma once

#include "graph/graph.h"
#include "graph/triple.h"
#include "search/independence_test.h"
#include "search/sepset_map.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_set>

namespace tetrad {

// Abstract template for *-FCI algorithms (BOSS-FCI, GRASP-FCI, etc.).
// Port of edu.cmu.tetrad.search.StarFci from Java Tetrad 7.6.8.
//
// Subclasses provide a Markov CPDAG via getMarkovCpdag(). This class
// then fixes the CPDAG for latent variables:
// 1. Extra edge removal via independence testing
// 2. Reorient to circles, apply knowledge
// 3. Orient colliders from CPDAG and sepsets
// 4. Apply FCI orientation rules R1-R10
//
// This is a template method pattern — the only variation point is
// how the initial CPDAG is obtained.
class StarFci {
public:
    explicit StarFci(IndependenceTest& test);
    virtual ~StarFci() = default;

    Graph search();

    // Subclasses override to provide the initial Markov CPDAG.
    virtual Graph getMarkovCpdag() = 0;

    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    const Knowledge& getKnowledge() const { return knowledge_; }

    void setCompleteRuleSetUsed(bool used) { completeRuleSetUsed_ = used; }
    void setMaxDiscriminatingPathLength(int len) { maxDiscriminatingPathLength_ = len; }
    void setDepth(int depth) { depth_ = depth; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    bool isVerbose() const { return verbose_; }

    IndependenceTest& getIndependenceTest() { return test_; }

private:
    // Find separating set from adj(x) or adj(y).
    std::set<NodePtr>* findSepset(const Graph& graph, const NodePtr& x, const NodePtr& y,
                                   const std::set<NodePtr>& containing);

    static bool colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                                 const NodePtr& z, const Knowledge& knowledge);

    IndependenceTest& test_;
    Knowledge knowledge_;
    bool completeRuleSetUsed_ = true;
    int maxDiscriminatingPathLength_ = -1;
    int depth_ = -1;
    bool verbose_ = false;

    // Lifetime management for sepset pointers returned by findSepset
    std::vector<std::unique_ptr<std::set<NodePtr>>> sepsetStorage_;
};

} // namespace tetrad
