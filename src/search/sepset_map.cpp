#include "search/sepset_map.h"

namespace tetrad {

void SepsetMap::set(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z) {
    sepsets_[makeKey(x, y)] = z;
}

std::optional<std::set<NodePtr>> SepsetMap::get(const NodePtr& x, const NodePtr& y) const {
    auto key = makeKey(x, y);
    auto it = sepsets_.find(key);
    if (it != sepsets_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void SepsetMap::setPValue(const NodePtr& x, const NodePtr& y, double pValue) {
    pValues_[makeKey(x, y)] = pValue;
}

double SepsetMap::getPValue(const NodePtr& x, const NodePtr& y) const {
    auto key = makeKey(x, y);
    auto it = pValues_.find(key);
    if (it != pValues_.end()) {
        return it->second;
    }
    return 0.0;
}

void SepsetMap::clear() {
    sepsets_.clear();
    pValues_.clear();
}

std::pair<std::string, std::string> SepsetMap::makeKey(const NodePtr& x, const NodePtr& y) {
    if (x->getName() <= y->getName()) {
        return {x->getName(), y->getName()};
    }
    return {y->getName(), x->getName()};
}

} // namespace tetrad
