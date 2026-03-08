#pragma once

#include "graph/graph.h"
#include "search/suborder_search.h"
#include "search/grow_shrink_tree.h"
#include "data/knowledge.h"
#include <memory>
#include <unordered_map>

namespace tetrad {

// Wrapper for permutation-based search algorithms (BOSS, etc.).
// Port of edu.cmu.tetrad.search.PermutationSearch from Java Tetrad 7.6.8.
//
// Handles:
// - Tiered knowledge optimization (search each tier separately)
// - GrowShrinkTree construction and caching
// - Conversion from permutation + parents to CPDAG via MeekRules
class PermutationSearch {
public:
    explicit PermutationSearch(SuborderSearch& suborderSearch);

    Graph search();
    Graph search(bool cpdag);

    // Static utility: build a graph (DAG or CPDAG) from nodes and parent mapping.
    static Graph getGraph(const std::vector<NodePtr>& nodes,
                          const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& parents,
                          bool cpdag);

    static Graph getGraph(const std::vector<NodePtr>& nodes,
                          const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& parents,
                          const Knowledge* knowledge,
                          bool cpdag);

    void setKnowledge(const Knowledge& knowledge);
    const Knowledge& getKnowledge() const { return knowledge_; }

    const std::vector<NodePtr>& getOrder() const { return order_; }
    void setOrder(const std::vector<NodePtr>& order);

    std::vector<NodePtr> getVariables() const { return variables_; }

private:
    SuborderSearch& suborderSearch_;
    std::vector<NodePtr> variables_;
    std::vector<NodePtr> order_;
    std::unordered_map<NodePtr, int> index_;
    std::vector<std::unique_ptr<GrowShrinkTree>> gstStorage_;
    std::unordered_map<NodePtr, GrowShrinkTree*> gsts_;
    Knowledge knowledge_;
};

} // namespace tetrad
