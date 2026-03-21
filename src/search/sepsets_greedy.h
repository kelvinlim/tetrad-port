#pragma once

#include "graph/graph.h"
#include "search/independence_test.h"
#include "data/knowledge.h"
#include <set>
#include <memory>
#include <vector>

namespace tetrad {

// Greedy sepset finder.
// Port of edu.cmu.tetrad.search.utils.SepsetsGreedy from Java Tetrad 7.6.3.
//
// For any pair (i, k), searches subsets of adj(i)\{k} then adj(k)\{i} at
// each depth level d = 0, 1, ..., returning the first separating set found.
// Uses knowledge to filter possible-parent conditioning sets.
//
// Holds a reference to the live graph — edge removals during search are
// automatically reflected in subsequent getSepset() calls.
class SepsetsGreedy {
public:
    SepsetsGreedy(const Graph& graph, IndependenceTest& test,
                  int depth, const Knowledge& knowledge);

    // Returns pointer to the first separating set of (i, k), or nullptr.
    // Ownership remains with this object (lifetime = this object's lifetime).
    const std::set<NodePtr>* getSepset(const NodePtr& i, const NodePtr& k);

    // Returns true iff j is NOT in the separating set of (i, k)
    // (i.e. the triple i-j-k is an unshielded collider).
    bool isUnshieldedCollider(const NodePtr& i, const NodePtr& j, const NodePtr& k);

    bool isIndependent(const NodePtr& a, const NodePtr& b, const std::set<NodePtr>& c);

    // Score of the last independence test: -(p - alpha).
    double getScore() const { return lastScore_; }

private:
    const std::set<NodePtr>* getSepsetGreedy(const NodePtr& i, const NodePtr& k);

    // Filter subset to nodes that are "possible parents" of x w.r.t. knowledge.
    std::set<NodePtr> possibleParents(const NodePtr& x,
                                       const std::set<NodePtr>& adjx,
                                       const NodePtr& y) const;

    const Graph& graph_;
    IndependenceTest& test_;
    int depth_;
    Knowledge knowledge_;
    double lastScore_ = 0.0;

    // Owns all sets returned by getSepset (pointers remain valid until cleared).
    std::vector<std::unique_ptr<std::set<NodePtr>>> storage_;
};

} // namespace tetrad
