#pragma once

#include "graph/graph.h"
#include "graph/triple.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_set>
#include <functional>

namespace tetrad {

class IndependenceTest;

// Strategy interface for R0 and R4 — the only FCI orientation rules that
// require looking at the distribution rather than only at the graph.
// Port of edu.cmu.tetrad.search.utils.R0R4Strategy.
class R0R4Strategy {
public:
    virtual ~R0R4Strategy() = default;

    // Is the triple a-b-c an unshielded collider based on sepset data?
    virtual bool isUnshieldedCollider(const Graph& graph,
                                      const NodePtr& a, const NodePtr& b, const NodePtr& c) = 0;

    // Discriminating path orientation for R4.
    // Returns true if an orientation was made.
    virtual bool doDiscriminatingPathOrientation(
        const NodePtr& x, const NodePtr& w, const NodePtr& v, const NodePtr& y,
        const std::vector<NodePtr>& colliderPath,
        Graph& graph) = 0;

    virtual void setKnowledge(const Knowledge& knowledge) = 0;
    virtual const Knowledge& getKnowledge() const = 0;
};

// Test-based R0R4 strategy: uses an IndependenceTest for R0 and R4.
// Port of edu.cmu.tetrad.search.utils.R0R4StrategyTestBased.
class R0R4StrategyTestBased : public R0R4Strategy {
public:
    explicit R0R4StrategyTestBased(IndependenceTest& test);

    bool isUnshieldedCollider(const Graph& graph,
                              const NodePtr& a, const NodePtr& b, const NodePtr& c) override;

    bool doDiscriminatingPathOrientation(
        const NodePtr& x, const NodePtr& w, const NodePtr& v, const NodePtr& y,
        const std::vector<NodePtr>& colliderPath,
        Graph& graph) override;

    void setKnowledge(const Knowledge& knowledge) override { knowledge_ = knowledge; }
    const Knowledge& getKnowledge() const override { return knowledge_; }

    void setDepth(int depth) { depth_ = depth; }

private:
    IndependenceTest& test_;
    Knowledge knowledge_;
    int depth_ = -1;
};

// FCI orientation rules R0-R10.
// Port of edu.cmu.tetrad.search.utils.FciOrient from Java Tetrad 7.6.8.
class FciOrient {
public:
    explicit FciOrient(R0R4Strategy& strategy);

    // Full orientation: R0 then finalOrientation.
    void orient(Graph& graph, std::unordered_set<Triple>& unshieldedTriples);

    // R0: Orient unshielded colliders.
    void ruleR0(Graph& graph, std::unordered_set<Triple>& unshieldedTriples);

    // Final orientation: R1-R10 (Zhang complete) or R1-R4 (Spirtes).
    void finalOrientation(Graph& graph);

    // Individual rules (public for use by other algorithms).
    void ruleR1(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph);
    void ruleR2(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph);
    void ruleR3(Graph& graph);
    void ruleR4(Graph& graph);
    void ruleR5(Graph& graph);
    void ruleR6(Graph& graph);
    void ruleR7(Graph& graph);
    void rulesR8R9R10(Graph& graph);
    bool ruleR8(const NodePtr& a, const NodePtr& c, Graph& graph);
    bool ruleR9(const NodePtr& a, const NodePtr& c, Graph& graph);
    void ruleR10(const NodePtr& alpha, const NodePtr& gamma, Graph& graph);

    void rulesR1R2cycle(Graph& graph);

    // Utility
    static bool isArrowheadAllowed(const NodePtr& x, const NodePtr& y,
                                    const Graph& graph, const Knowledge& knowledge);
    void fciOrientbk(const Knowledge& bk, Graph& graph, const std::vector<NodePtr>& variables);

    void setCompleteRuleSetUsed(bool used) { completeRuleSetUsed_ = used; }
    void setMaxDiscriminatingPathLength(int len) { maxDiscriminatingPathLength_ = len; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setKnowledge(const Knowledge& knowledge);

private:
    void zhangFinalOrientation(Graph& graph);
    void spirtesFinalOrientation(Graph& graph);

    static bool isPartiallyOrientedEdge(const NodePtr& a, const NodePtr& b, const Graph& graph);

    R0R4Strategy& strategy_;
    Knowledge knowledge_;
    bool completeRuleSetUsed_ = true;
    int maxDiscriminatingPathLength_ = -1;
    bool verbose_ = false;
    bool changeFlag_ = true;
};

} // namespace tetrad
