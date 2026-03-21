#pragma once

#include "graph/graph.h"
#include "graph/triple.h"
#include "search/sepsets_greedy.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_set>

namespace tetrad {

// FCI orientation rules R0-R10.
// Port of edu.cmu.tetrad.search.utils.FciOrient from Java Tetrad 7.6.3.
//
// R0: Orient unshielded colliders using SepsetsGreedy.isUnshieldedCollider().
// R1-R4: Spirtes' orientation rules (R4 uses SepsetsGreedy.getSepset()).
// R5-R10: Zhang's complete rule set.
//
// Reference: Zhang (2008), "On the completeness of orientation rules for
// causal discovery in the presence of latent confounders and selection bias."
class FciOrient {
public:
    explicit FciOrient(SepsetsGreedy& sepsets);

    // R0 + finalOrientation. Normally called from algorithm search() methods.
    void orient(Graph& graph);

    // R0: reorient all edges to circles, apply bk, orient unshielded colliders.
    void ruleR0(Graph& graph);

    // R1-R10 (Zhang complete) or R1-R4 (Spirtes), depending on settings.
    void finalOrientation(Graph& graph);
    // Alias used by algorithms that match Java's doFinalOrientation().
    void doFinalOrientation(Graph& graph) { finalOrientation(graph); }

    // Individual rules (public so algorithms can call them directly).
    void ruleR1(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph);
    void ruleR2(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph);
    void ruleR3(Graph& graph);
    void ruleR4B(Graph& graph);
    void ruleR5(Graph& graph);
    void ruleR6R7(Graph& graph);
    void rulesR8R9R10(Graph& graph);
    bool ruleR8(const NodePtr& a, const NodePtr& c, Graph& graph);
    bool ruleR9(const NodePtr& a, const NodePtr& c, Graph& graph);
    void ruleR10(const NodePtr& alpha, const NodePtr& gamma, Graph& graph);

    void rulesR1R2cycle(Graph& graph);

    // Utility
    static bool isArrowheadAllowed(const NodePtr& x, const NodePtr& y,
                                    const Graph& graph, const Knowledge& knowledge);
    void fciOrientbk(const Knowledge& bk, Graph& graph,
                     const std::vector<NodePtr>& variables);

    void setCompleteRuleSetUsed(bool used) { completeRuleSetUsed_ = used; }
    void setMaxDiscriminatingPathLength(int len) { maxDiscriminatingPathLength_ = len; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    void setDoDiscriminatingPathColliderRule(bool b) { doDiscriminatingPathColliderRule_ = b; }
    void setDoDiscriminatingPathTailRule(bool b) { doDiscriminatingPathTailRule_ = b; }

private:
    void zhangFinalOrientation(Graph& graph);
    void spirtesFinalOrientation(Graph& graph);

    // R4 helpers (ddpOrient pattern from 7.6.3).
    void ddpOrient(const NodePtr& a, const NodePtr& b, const NodePtr& c, Graph& graph);
    bool doDdpOrientation(const NodePtr& d, const NodePtr& a, const NodePtr& b,
                          const NodePtr& c, Graph& graph);

    static bool isPartiallyOrientedEdge(const NodePtr& a, const NodePtr& b,
                                         const Graph& graph);

    SepsetsGreedy& sepsets_;
    Knowledge knowledge_;
    bool completeRuleSetUsed_ = true;
    int maxDiscriminatingPathLength_ = -1;
    bool verbose_ = false;
    bool changeFlag_ = true;
    bool doDiscriminatingPathColliderRule_ = true;
    bool doDiscriminatingPathTailRule_ = true;
};

} // namespace tetrad
