#include "search/boss.h"
#include "search/bes_permutation.h"
#include "search/permutation_search.h"
#include <algorithm>
#include <limits>
#include <chrono>

namespace tetrad {

Boss::Boss(Score& score)
    : score_(score), variables_(score.getVariables()) {
    for (const auto& x : variables_) {
        parents_[x] = {};
    }
    rng_.seed(std::random_device{}());
}

Boss::~Boss() = default;

void Boss::searchSuborder(const std::vector<NodePtr>& prefix,
                           std::vector<NodePtr>& suborder,
                           std::unordered_map<NodePtr, GrowShrinkTree*>& gsts) {
    gsts_ = &gsts;
    all_.clear();
    all_.insert(prefix.begin(), prefix.end());
    all_.insert(suborder.begin(), suborder.end());

    std::vector<NodePtr> bestSuborder;
    double bestScore = -std::numeric_limits<double>::infinity();

    for (int i = 0; i < numStarts_; i++) {
        if ((i == 0 && !useDataOrder_) || i > 0) {
            std::shuffle(suborder.begin(), suborder.end(), rng_);
        }

        if (i > 0 && resetAfterRS_) {
            for (const auto& root : suborder) {
                gsts.at(root)->reset();
            }
        }

        makeValidKnowledgeOrder(suborder);

        bool improved;
        do {
            improved = false;
            for (const auto& x : std::vector<NodePtr>(suborder)) {
                improved |= betterMutation(prefix, suborder, x);
            }
        } while (improved);

        if (bes_) bes(prefix, suborder);

        double score = update(prefix, suborder);

        if (score > bestScore) {
            bestSuborder = suborder;
            bestScore = score;
        }
    }

    suborder = bestSuborder;
    update(prefix, suborder);
}

void Boss::setUseBes(bool use) {
    bes_.reset();
    if (use) {
        bes_ = std::make_unique<BesPermutation>(score_);
        bes_->setVerbose(false);
        bes_->setKnowledge(knowledge_);
    }
}

void Boss::setKnowledge(const Knowledge& knowledge) {
    knowledge_ = knowledge;
    if (bes_) {
        bes_->setKnowledge(knowledge);
    }
}

bool Boss::betterMutation(const std::vector<NodePtr>& prefix,
                           std::vector<NodePtr>& suborder,
                           const NodePtr& x) {
    std::vector<double> scores(suborder.size() + 1, 0.0);
    std::unordered_set<NodePtr> Z(prefix.begin(), prefix.end());

    int i = 0;
    double score = 0;
    int curr = 0;

    // Forward pass
    auto it = suborder.begin();
    while (it != suborder.end()) {
        const NodePtr& z = *it;

        if (knowledge_.isRequired(x->getName(), z->getName())) {
            break;
        }

        scores[i++] = gsts_->at(x)->trace(Z, all_) + score;
        if (z != x) {
            score += gsts_->at(z)->trace(Z, all_);
            Z.insert(z);
        } else {
            curr = i - 1;
        }
        ++it;
    }

    scores[i] = gsts_->at(x)->trace(Z, all_) + score;
    int best = i;

    Z.insert(x);
    score = 0;

    // Backward pass
    while (it != suborder.begin()) {
        --it;
        const NodePtr& z = *it;

        if (knowledge_.isRequired(z->getName(), x->getName())) break;

        if (z != x) {
            Z.erase(z);
            score += gsts_->at(z)->trace(Z, all_);
        }

        scores[--i] += score;
        if (scores[i] + 1e-6 > scores[best]) best = i;
    }

    if (scores[curr] + 1e-6 > scores[best]) return false;
    if (best > curr) best--;

    // Remove x from its current position and insert at best
    suborder.erase(std::find(suborder.begin(), suborder.end(), x));
    suborder.insert(suborder.begin() + best, x);

    return true;
}

double Boss::update(const std::vector<NodePtr>& prefix,
                     std::vector<NodePtr>& suborder) {
    double score = 0;
    std::unordered_set<NodePtr> Z(prefix.begin(), prefix.end());

    for (const auto& x : suborder) {
        auto& parents = parents_[x];
        parents.clear();
        score += gsts_->at(x)->trace(Z, all_, parents);
        Z.insert(x);
    }

    return score;
}

void Boss::makeValidKnowledgeOrder(std::vector<NodePtr>& order) {
    if (knowledge_.isEmpty()) return;

    int index = 0;

    // Move variables not in any tier to front
    std::unordered_set<std::string> tier(knowledge_.getVariablesNotInTiers().begin(),
                                          knowledge_.getVariablesNotInTiers().end());
    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        if (tier.count(order[i]->getName())) {
            NodePtr x = order[i];
            order.erase(order.begin() + i);
            order.insert(order.begin() + index++, x);
        }
    }

    // Move each tier's variables in order
    for (int t = 0; t < knowledge_.getNumTiers(); t++) {
        auto tierVars = knowledge_.getTier(t);
        tier = std::unordered_set<std::string>(tierVars.begin(), tierVars.end());
        for (int j = 0; j < static_cast<int>(order.size()); j++) {
            if (tier.count(order[j]->getName())) {
                NodePtr x = order[j];
                order.erase(order.begin() + j);
                order.insert(order.begin() + index++, x);
            }
        }
    }

    // Fix required edges
    for (int i = 1; i < static_cast<int>(order.size()); i++) {
        const std::string& a = order[i]->getName();
        for (int j = 0; j < i; j++) {
            const std::string& b = order[j]->getName();
            if (knowledge_.isRequired(a, b)) {
                NodePtr x = order[i];
                order.erase(order.begin() + i);
                order.insert(order.begin() + j, x);
                break;
            }
        }
    }
}

void Boss::bes(const std::vector<NodePtr>& prefix,
                std::vector<NodePtr>& suborder) {
    std::vector<NodePtr> allNodes(prefix);
    allNodes.insert(allNodes.end(), suborder.begin(), suborder.end());

    Graph graph = PermutationSearch::getGraph(allNodes, parents_, &knowledge_, true);
    bes_->bes(graph, allNodes, suborder);

    // makeValidOrder: topological sort of suborder from graph
    // Simplified: reorder suborder so parents come before children
    std::vector<NodePtr> sorted;
    std::unordered_set<NodePtr> remaining(suborder.begin(), suborder.end());
    std::unordered_set<NodePtr> placed;
    placed.insert(prefix.begin(), prefix.end());

    while (!remaining.empty()) {
        bool found = false;
        for (auto it = remaining.begin(); it != remaining.end(); ++it) {
            const NodePtr& node = *it;
            auto parents = graph.getParents(node);
            bool allParentsPlaced = true;
            for (const auto& p : parents) {
                if (remaining.count(p) && !placed.count(p)) {
                    allParentsPlaced = false;
                    break;
                }
            }
            if (allParentsPlaced) {
                sorted.push_back(node);
                placed.insert(node);
                remaining.erase(it);
                found = true;
                break;
            }
        }
        if (!found) {
            // Cycle or remaining nodes, just add them
            for (const auto& node : remaining) {
                sorted.push_back(node);
            }
            break;
        }
    }

    suborder = sorted;
}

} // namespace tetrad
