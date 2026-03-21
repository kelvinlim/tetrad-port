#include "search/sepsets_greedy.h"
#include "util/choice_generator.h"
#include <algorithm>

namespace tetrad {

SepsetsGreedy::SepsetsGreedy(const Graph& graph, IndependenceTest& test,
                               int depth, const Knowledge& knowledge)
    : graph_(graph), test_(test), depth_(depth), knowledge_(knowledge) {}

const std::set<NodePtr>* SepsetsGreedy::getSepset(const NodePtr& i, const NodePtr& k) {
    return getSepsetGreedy(i, k);
}

bool SepsetsGreedy::isUnshieldedCollider(const NodePtr& i, const NodePtr& j, const NodePtr& k) {
    const std::set<NodePtr>* sep = getSepsetGreedy(i, k);
    return sep != nullptr && sep->find(j) == sep->end();
}

bool SepsetsGreedy::isIndependent(const NodePtr& a, const NodePtr& b,
                                   const std::set<NodePtr>& c) {
    auto result = test_.checkIndependence(a, b, c);
    lastScore_ = -(result.pValue - test_.getAlpha());
    return result.isIndependent();
}

const std::set<NodePtr>* SepsetsGreedy::getSepsetGreedy(const NodePtr& i,
                                                          const NodePtr& k) {
    auto adji = graph_.getAdjacentNodes(i);
    auto adjk = graph_.getAdjacentNodes(k);

    adji.erase(std::remove_if(adji.begin(), adji.end(),
        [&](const NodePtr& n) { return *n == *k; }), adji.end());
    adjk.erase(std::remove_if(adjk.begin(), adjk.end(),
        [&](const NodePtr& n) { return *n == *i; }), adjk.end());

    int maxAdj = std::max(static_cast<int>(adji.size()), static_cast<int>(adjk.size()));
    int maxD   = (depth_ < 0) ? maxAdj : std::min(depth_, maxAdj);

    for (int d = 0; d <= maxD; d++) {
        if (d <= static_cast<int>(adji.size())) {
            ChoiceGenerator gen(static_cast<int>(adji.size()), d);
            const int* choice;
            while ((choice = gen.next()) != nullptr) {
                std::set<NodePtr> v;
                for (int idx = 0; idx < d; idx++) v.insert(adji[choice[idx]]);
                v = possibleParents(i, v, k);
                auto result = test_.checkIndependence(i, k, v);
                if (result.isIndependent()) {
                    lastScore_ = -(result.pValue - test_.getAlpha());
                    storage_.push_back(std::make_unique<std::set<NodePtr>>(v));
                    return storage_.back().get();
                }
            }
        }
        if (d <= static_cast<int>(adjk.size())) {
            ChoiceGenerator gen(static_cast<int>(adjk.size()), d);
            const int* choice;
            while ((choice = gen.next()) != nullptr) {
                std::set<NodePtr> v;
                for (int idx = 0; idx < d; idx++) v.insert(adjk[choice[idx]]);
                v = possibleParents(k, v, i);
                auto result = test_.checkIndependence(i, k, v);
                if (result.isIndependent()) {
                    lastScore_ = -(result.pValue - test_.getAlpha());
                    storage_.push_back(std::make_unique<std::set<NodePtr>>(v));
                    return storage_.back().get();
                }
            }
        }
    }

    return nullptr;
}

std::set<NodePtr> SepsetsGreedy::possibleParents(const NodePtr& x,
                                                   const std::set<NodePtr>& adjx,
                                                   const NodePtr& y) const {
    if (knowledge_.isEmpty()) return adjx;

    std::set<NodePtr> result;
    for (const auto& z : adjx) {
        if (*z == *x) continue;
        if (*z == *y) continue;
        if (!knowledge_.isForbidden(z->getName(), x->getName()) &&
            !knowledge_.isRequired(x->getName(), z->getName())) {
            result.insert(z);
        }
    }
    return result;
}

} // namespace tetrad
