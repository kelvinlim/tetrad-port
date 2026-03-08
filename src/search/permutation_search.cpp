#include "search/permutation_search.h"
#include "search/meek_rules.h"

namespace tetrad {

PermutationSearch::PermutationSearch(SuborderSearch& suborderSearch)
    : suborderSearch_(suborderSearch),
      variables_(suborderSearch.getVariables()) {

    Score& score = suborderSearch.getScore();

    int i = 0;
    for (const auto& node : variables_) {
        index_[node] = i++;
        auto gst = std::make_unique<GrowShrinkTree>(score, index_, node);
        gsts_[node] = gst.get();
        gstStorage_.push_back(std::move(gst));
        order_.push_back(node);
    }
}

Graph PermutationSearch::search() {
    return search(true);
}

Graph PermutationSearch::search(bool cpdag) {
    // Check if all variables are in tiers (optimization path)
    std::vector<std::string> notInTier;
    for (const auto& node : variables_) {
        notInTier.push_back(node->getName());
    }
    for (int i = 0; i < knowledge_.getNumTiers(); i++) {
        auto tier = knowledge_.getTier(i);
        for (const auto& t : tier) {
            notInTier.erase(
                std::remove(notInTier.begin(), notInTier.end(), t),
                notInTier.end());
        }
    }

    if (!knowledge_.isEmpty() && notInTier.empty()) {
        // Tiered search: search each tier separately
        std::vector<NodePtr> savedOrder(order_);
        order_.clear();
        int start = 0;

        for (int i = 0; i < knowledge_.getNumTiers(); i++) {
            std::vector<NodePtr> prefix(order_);
            auto tier = knowledge_.getTier(i);
            std::unordered_set<std::string> tierSet(tier.begin(), tier.end());

            for (const auto& node : savedOrder) {
                if (!tierSet.count(node->getName())) continue;
                order_.push_back(node);

                if (knowledge_.isTierForbiddenWithin(i)) {
                    std::vector<NodePtr> suborder(order_.begin() + start, order_.end());
                    suborderSearch_.searchSuborder(prefix, suborder, gsts_);
                    // Copy suborder back
                    for (int k = 0; k < static_cast<int>(suborder.size()); k++) {
                        order_[start + k] = suborder[k];
                    }
                    start++;
                }
            }

            if (!knowledge_.isTierForbiddenWithin(i)) {
                std::vector<NodePtr> suborder(order_.begin() + start, order_.end());
                suborderSearch_.searchSuborder(prefix, suborder, gsts_);
                for (int k = 0; k < static_cast<int>(suborder.size()); k++) {
                    order_[start + k] = suborder[k];
                }
                start = static_cast<int>(order_.size());
            }
        }
    } else {
        // Simple path: no tiers, search everything
        std::vector<NodePtr> prefix;
        suborderSearch_.searchSuborder(prefix, order_, gsts_);
    }

    return getGraph(variables_, suborderSearch_.getParents(), &knowledge_, cpdag);
}

Graph PermutationSearch::getGraph(const std::vector<NodePtr>& nodes,
                                   const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& parents,
                                   bool cpdag) {
    return getGraph(nodes, parents, nullptr, cpdag);
}

Graph PermutationSearch::getGraph(const std::vector<NodePtr>& nodes,
                                   const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& parents,
                                   const Knowledge* knowledge,
                                   bool cpdag) {
    Graph graph(nodes);

    for (const auto& a : nodes) {
        auto it = parents.find(a);
        if (it != parents.end()) {
            for (const auto& b : it->second) {
                graph.addDirectedEdge(b, a);
            }
        }
    }

    if (cpdag) {
        MeekRules rules;
        if (knowledge) rules.setKnowledge(*knowledge);
        rules.setVerbose(false);
        rules.orientImplied(graph);
    }

    return graph;
}

void PermutationSearch::setKnowledge(const Knowledge& knowledge) {
    knowledge_ = knowledge;
    suborderSearch_.setKnowledge(knowledge);

    for (const auto& node : variables_) {
        std::vector<NodePtr> required;
        std::vector<NodePtr> forbidden;
        for (const auto& parent : variables_) {
            if (knowledge.isRequired(parent->getName(), node->getName())) required.push_back(parent);
            if (knowledge.isForbidden(parent->getName(), node->getName())) forbidden.push_back(parent);
        }
        if (!required.empty() || !forbidden.empty()) {
            gsts_[node]->setKnowledge(required, forbidden);
        }
    }
}

void PermutationSearch::setOrder(const std::vector<NodePtr>& order) {
    order_ = order;
}

} // namespace tetrad
