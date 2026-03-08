#pragma once

#include "graph/node.h"
#include "search/score.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <algorithm>
#include <cmath>
#include <memory>

namespace tetrad {

// Caching tree that efficiently computes local scores for a target node
// as variables are added (grow) or removed (shrink).
//
// Port of edu.cmu.tetrad.search.utils.GrowShrinkTree from Java Tetrad 7.6.8.
// Author: bryanandrews (original Java), ported to C++.
//
// The tree lazily builds branches: for each candidate parent variable,
// it computes the score with that variable included (grow phase), then
// greedily removes variables that don't help (shrink phase).
class GrowShrinkTree {
public:
    GrowShrinkTree(Score& score, const std::unordered_map<NodePtr, int>& index, const NodePtr& node);

    // Trace the tree given a prefix (candidate parents) and all available nodes.
    // Returns the best local score found.
    double trace(const std::unordered_set<NodePtr>& prefix,
                 const std::unordered_set<NodePtr>& all);

    // Trace with output parents set.
    double trace(const std::unordered_set<NodePtr>& prefix,
                 const std::unordered_set<NodePtr>& all,
                 std::unordered_set<NodePtr>& parents);

    const NodePtr& getNode() const { return node_; }

    // Get nodes in the first layer of branches.
    std::vector<NodePtr> getFirstLayer() const;

    int getIndex(const NodePtr& node) const;

    // Local score with no parents.
    double localScore() const;

    // Local score with given parent indices.
    double localScore(const std::vector<int>& parentIndices) const;

    bool isRequired(const NodePtr& node) const;
    bool isForbidden(const NodePtr& node) const;

    const std::vector<NodePtr>& getVariables() const { return score_.getVariables(); }
    const std::vector<NodePtr>& getRequired() const { return required_; }
    const std::vector<NodePtr>& getForbidden() const { return forbidden_; }

    void setKnowledge(const std::vector<NodePtr>& required, const std::vector<NodePtr>& forbidden);
    void reset();

private:
    // Inner node of the grow-shrink tree.
    struct GSTNode {
        GrowShrinkTree* tree;
        NodePtr add;           // variable added at this level (nullptr for root)
        double growScore;
        double shrinkScore;
        bool growDone;
        bool shrinkDone;
        std::vector<std::unique_ptr<GSTNode>> branches;
        std::unordered_set<NodePtr> remove;  // variables removed during shrink

        // Root constructor
        explicit GSTNode(GrowShrinkTree* tree);

        // Branch constructor
        GSTNode(GrowShrinkTree* tree, const NodePtr& add,
                const std::unordered_set<NodePtr>& parents);

        void grow(const std::unordered_set<NodePtr>& available,
                  const std::unordered_set<NodePtr>& parents);

        void shrink(std::unordered_set<NodePtr>& parents);

        double trace(const std::unordered_set<NodePtr>& prefix,
                     std::unordered_set<NodePtr>& available,
                     std::unordered_set<NodePtr>& parents);
    };

    Score& score_;
    const std::unordered_map<NodePtr, int>& index_;
    NodePtr node_;
    int nodeIndex_;
    std::vector<NodePtr> required_;
    std::vector<NodePtr> forbidden_;
    std::unique_ptr<GSTNode> root_;
};

} // namespace tetrad
