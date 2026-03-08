#pragma once

#include "graph/node.h"
#include "search/score.h"
#include "data/knowledge.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace tetrad {

class GrowShrinkTree;

// Interface for suborder searches used by PermutationSearch.
// A "suborder search" optimizes a permutation of a subset of variables
// with a fixed prefix. PermutationSearch handles tiered knowledge by
// searching each tier separately.
//
// Port of edu.cmu.tetrad.search.SuborderSearch from Java Tetrad 7.6.8.
class SuborderSearch {
public:
    virtual ~SuborderSearch() = default;

    virtual void searchSuborder(const std::vector<NodePtr>& prefix,
                                std::vector<NodePtr>& suborder,
                                std::unordered_map<NodePtr, GrowShrinkTree*>& gsts) = 0;

    virtual void setKnowledge(const Knowledge& knowledge) = 0;
    virtual const std::vector<NodePtr>& getVariables() const = 0;
    virtual const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& getParents() const = 0;
    virtual Score& getScore() = 0;
};

} // namespace tetrad
