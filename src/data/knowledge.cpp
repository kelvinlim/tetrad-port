#include "data/knowledge.h"
#include <algorithm>

namespace tetrad {

Knowledge::Knowledge(const std::vector<std::string>& variables) {
    for (const auto& v : variables) {
        variables_.insert(v);
    }
}

// --- Forbidden edges ---

void Knowledge::setForbidden(const std::string& from, const std::string& to) {
    if (isForbidden(from, to)) return;

    addVariable(from);
    addVariable(to);

    std::set<std::string> f1{from};
    std::set<std::string> f2{to};
    RulePair rule{f1, f2};

    // Check for duplicate
    for (const auto& r : forbiddenRules_) {
        if (r == rule) return;
    }
    forbiddenRules_.push_back(std::move(rule));
}

void Knowledge::removeForbidden(const std::string& from, const std::string& to) {
    std::set<std::string> f1{from};
    std::set<std::string> f2{to};
    RulePair rule{f1, f2};

    forbiddenRules_.erase(
        std::remove(forbiddenRules_.begin(), forbiddenRules_.end(), rule),
        forbiddenRules_.end());
}

bool Knowledge::isForbidden(const std::string& from, const std::string& to) const {
    return isForbiddenByRules(from, to) || isForbiddenByTiers(from, to);
}

bool Knowledge::isForbiddenByRules(const std::string& from, const std::string& to) const {
    for (const auto& rule : forbiddenRules_) {
        if (rule.first.count(from) && rule.second.count(to)) {
            return true;
        }
    }
    return false;
}

bool Knowledge::isForbiddenByTiers(const std::string& from, const std::string& to) const {
    // Edge from->to is forbidden by tiers if 'from' is in a higher tier than 'to'
    for (int i = static_cast<int>(tierSpecs_.size()) - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            if (tierSpecs_[i].count(from) && tierSpecs_[j].count(to)) {
                return true;
            }
        }
    }
    return false;
}

// --- Required edges ---

void Knowledge::setRequired(const std::string& from, const std::string& to) {
    if (isRequired(from, to)) return;

    addVariable(from);
    addVariable(to);

    std::set<std::string> f1{from};
    std::set<std::string> f2{to};
    RulePair rule{f1, f2};

    for (const auto& r : requiredRules_) {
        if (r == rule) return;
    }
    requiredRules_.push_back(std::move(rule));
}

void Knowledge::removeRequired(const std::string& from, const std::string& to) {
    std::set<std::string> f1{from};
    std::set<std::string> f2{to};
    RulePair rule{f1, f2};

    requiredRules_.erase(
        std::remove(requiredRules_.begin(), requiredRules_.end(), rule),
        requiredRules_.end());
}

bool Knowledge::isRequired(const std::string& from, const std::string& to) const {
    for (const auto& rule : requiredRules_) {
        if (rule.first.count(from) && rule.second.count(to)) {
            return true;
        }
    }
    return false;
}

bool Knowledge::noEdgeRequired(const std::string& x, const std::string& y) const {
    return !(isRequired(x, y) || isRequired(y, x));
}

// --- Temporal tiers ---

void Knowledge::ensureTiers(int tier) {
    while (static_cast<int>(tierSpecs_.size()) <= tier) {
        tierSpecs_.emplace_back();
    }
}

int Knowledge::findTier(const std::string& var) const {
    for (int i = 0; i < static_cast<int>(tierSpecs_.size()); i++) {
        if (tierSpecs_[i].count(var)) {
            return i;
        }
    }
    return -1;
}

void Knowledge::addToTier(int tier, const std::string& var) {
    if (tier < 0) return;

    addVariable(var);
    ensureTiers(tier);

    // Remove from any existing tier
    for (auto& ts : tierSpecs_) {
        ts.erase(var);
    }

    tierSpecs_[tier].insert(var);
}

void Knowledge::setTier(int tier, const std::vector<std::string>& vars) {
    ensureTiers(tier);
    tierSpecs_[tier].clear();

    for (const auto& var : vars) {
        addToTier(tier, var);
    }
}

void Knowledge::removeFromTiers(const std::string& var) {
    for (auto& ts : tierSpecs_) {
        ts.erase(var);
    }
}

std::vector<std::string> Knowledge::getTier(int tier) const {
    if (tier < 0 || tier >= static_cast<int>(tierSpecs_.size())) {
        return {};
    }
    std::vector<std::string> result(tierSpecs_[tier].begin(), tierSpecs_[tier].end());
    std::sort(result.begin(), result.end());
    return result;
}

int Knowledge::getNumTiers() const {
    return static_cast<int>(tierSpecs_.size());
}

std::vector<std::string> Knowledge::getVariablesNotInTiers() const {
    std::vector<std::string> result;
    for (const auto& var : variables_) {
        bool inTier = false;
        for (const auto& ts : tierSpecs_) {
            if (ts.count(var)) {
                inTier = true;
                break;
            }
        }
        if (!inTier) {
            result.push_back(var);
        }
    }
    return result;
}

void Knowledge::setTierForbiddenWithin(int tier, bool forbidden) {
    ensureTiers(tier);
    const auto& varsInTier = tierSpecs_[tier];
    RulePair rule{varsInTier, varsInTier};

    if (forbidden) {
        for (const auto& r : forbiddenRules_) {
            if (r == rule) return;
        }
        forbiddenRules_.push_back(rule);
    } else {
        forbiddenRules_.erase(
            std::remove(forbiddenRules_.begin(), forbiddenRules_.end(), rule),
            forbiddenRules_.end());
    }
}

bool Knowledge::isTierForbiddenWithin(int tier) const {
    if (tier < 0 || tier >= static_cast<int>(tierSpecs_.size())) return false;

    const auto& varsInTier = tierSpecs_[tier];
    if (varsInTier.empty()) return false;

    RulePair rule{varsInTier, varsInTier};
    for (const auto& r : forbiddenRules_) {
        if (r == rule) return true;
    }
    return false;
}

// --- Edge iterators ---

std::vector<KnowledgeEdge> Knowledge::getListOfForbiddenEdges() const {
    std::set<std::pair<std::string, std::string>> edgeSet;

    // Tier-forbidden-within edges
    for (int i = 0; i < static_cast<int>(tierSpecs_.size()); i++) {
        if (isTierForbiddenWithin(i)) {
            for (const auto& x : tierSpecs_[i]) {
                for (const auto& y : tierSpecs_[i]) {
                    if (x != y) {
                        edgeSet.emplace(x, y);
                    }
                }
            }
        }
    }

    // Tier ordering: later->earlier forbidden
    for (int i = static_cast<int>(tierSpecs_.size()) - 1; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (const auto& x : tierSpecs_[i]) {
                for (const auto& y : tierSpecs_[j]) {
                    edgeSet.emplace(x, y);
                }
            }
        }
    }

    // Explicit forbidden rules
    for (const auto& rule : forbiddenRules_) {
        for (const auto& s1 : rule.first) {
            for (const auto& s2 : rule.second) {
                if (s1 != s2) {
                    edgeSet.emplace(s1, s2);
                }
            }
        }
    }

    std::vector<KnowledgeEdge> result;
    result.reserve(edgeSet.size());
    for (const auto& p : edgeSet) {
        result.emplace_back(p.first, p.second);
    }
    return result;
}

std::vector<KnowledgeEdge> Knowledge::getListOfRequiredEdges() const {
    std::set<std::pair<std::string, std::string>> edgeSet;

    for (const auto& rule : requiredRules_) {
        for (const auto& s1 : rule.first) {
            for (const auto& s2 : rule.second) {
                if (s1 != s2) {
                    edgeSet.emplace(s1, s2);
                }
            }
        }
    }

    std::vector<KnowledgeEdge> result;
    result.reserve(edgeSet.size());
    for (const auto& p : edgeSet) {
        result.emplace_back(p.first, p.second);
    }
    return result;
}

// --- Variables ---

void Knowledge::addVariable(const std::string& name) {
    variables_.insert(name);
}

std::vector<std::string> Knowledge::getVariables() const {
    return std::vector<std::string>(variables_.begin(), variables_.end());
}

// --- General ---

bool Knowledge::isEmpty() const {
    return forbiddenRules_.empty() && requiredRules_.empty() && tierSpecs_.empty();
}

void Knowledge::clear() {
    variables_.clear();
    forbiddenRules_.clear();
    requiredRules_.clear();
    tierSpecs_.clear();
}

} // namespace tetrad
