#pragma once

#include "graph/graph.h"
#include "search/independence_test.h"
#include "search/sepsets_greedy.h"
#include "data/knowledge.h"
#include <set>

namespace tetrad {

// Abstract template for *-FCI algorithms (BOSS-FCI, GRaSP-FCI, etc.).
// Port of Java Tetrad 7.6.3 BFci/GraspFci pattern.
//
// Subclasses provide a Markov CPDAG via getMarkovCpdag(). This class
// then fixes the CPDAG for latent variables using the GFCI pipeline:
// 1. Extra edge removal via SepsetsGreedy independence testing
// 2. gfciR0: reorient circles, apply knowledge, orient colliders
// 3. Apply FCI orientation rules R1-R10
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
    // gfciR0: reorient circles, apply bk, orient colliders from CPDAG and removed-edge sepsets.
    void gfciR0(Graph& pag, const Graph& referenceCpdag, SepsetsGreedy& sepsets);

    static bool colliderAllowed(const Graph& pag, const NodePtr& x, const NodePtr& y,
                                 const NodePtr& z, const Knowledge& knowledge);

    IndependenceTest& test_;
    Knowledge knowledge_;
    bool completeRuleSetUsed_ = true;
    int maxDiscriminatingPathLength_ = -1;
    int depth_ = -1;
    bool verbose_ = false;
};

} // namespace tetrad
