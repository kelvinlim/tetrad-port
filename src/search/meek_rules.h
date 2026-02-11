#pragma once

#include "graph/graph.h"
#include "data/knowledge.h"
#include <set>

namespace tetrad {

class MeekRules {
public:
    MeekRules() = default;

    std::set<NodePtr> orientImplied(Graph& graph);

    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    void setMeekPreventCycles(bool prevent) { meekPreventCycles_ = prevent; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setRevertToUnshieldedColliders(bool revert) { revertToUnshieldedColliders_ = revert; }

private:
    bool meekR1(const NodePtr& b, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited);
    bool meekR2(const NodePtr& a, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited);
    bool meekR3(const NodePtr& d, const NodePtr& a, Graph& graph, std::set<NodePtr>& visited);

    bool direct(const NodePtr& a, const NodePtr& c, Graph& graph, std::set<NodePtr>& visited);

    static bool isUndirectedEdge(const Edge& e);
    static bool isUndirected(const Graph& g, const NodePtr& x, const NodePtr& y);
    static bool isDirected(const Graph& g, const NodePtr& x, const NodePtr& y);
    static std::vector<NodePtr> getCommonAdjacents(const NodePtr& x, const NodePtr& y, const Graph& graph);
    static bool isArrowheadAllowed(const NodePtr& from, const NodePtr& to, const Knowledge& knowledge);

    Knowledge knowledge_;
    bool meekPreventCycles_ = true;
    bool verbose_ = false;
    bool revertToUnshieldedColliders_ = true;
};

} // namespace tetrad
