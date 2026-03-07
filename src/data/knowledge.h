#pragma once

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>

namespace tetrad {

// Simple ordered pair of variable names representing a knowledge edge.
struct KnowledgeEdge {
    std::string from;
    std::string to;

    KnowledgeEdge(const std::string& from, const std::string& to)
        : from(from), to(to) {}

    bool operator==(const KnowledgeEdge& other) const {
        return from == other.from && to == other.to;
    }
};

// Stores background knowledge about required/forbidden edges and temporal tiers.
// Edges can be forbidden or required explicitly, or forbidden implicitly via
// temporal tiers (edges from later tiers to earlier tiers are forbidden).
//
// Port of edu.cmu.tetrad.data.Knowledge from Java Tetrad 7.6.8.
// Simplifications: no wildcard matching, no KnowledgeGroups (legacy), no serialization.
class Knowledge {
public:
    using RulePair = std::pair<std::set<std::string>, std::set<std::string>>;

    Knowledge() = default;

    // Construct with initial variable names.
    explicit Knowledge(const std::vector<std::string>& variables);

    // Copy/move use compiler defaults (all members are value types).
    Knowledge(const Knowledge&) = default;
    Knowledge& operator=(const Knowledge&) = default;
    Knowledge(Knowledge&&) = default;
    Knowledge& operator=(Knowledge&&) = default;

    // --- Forbidden edges ---
    void setForbidden(const std::string& from, const std::string& to);
    void removeForbidden(const std::string& from, const std::string& to);
    bool isForbidden(const std::string& from, const std::string& to) const;
    bool isForbiddenByTiers(const std::string& from, const std::string& to) const;

    // --- Required edges ---
    void setRequired(const std::string& from, const std::string& to);
    void removeRequired(const std::string& from, const std::string& to);
    bool isRequired(const std::string& from, const std::string& to) const;

    // True if neither direction is required.
    bool noEdgeRequired(const std::string& x, const std::string& y) const;

    // --- Temporal tiers ---
    void addToTier(int tier, const std::string& var);
    void setTier(int tier, const std::vector<std::string>& vars);
    void removeFromTiers(const std::string& var);
    std::vector<std::string> getTier(int tier) const;
    int getNumTiers() const;
    std::vector<std::string> getVariablesNotInTiers() const;
    void setTierForbiddenWithin(int tier, bool forbidden);
    bool isTierForbiddenWithin(int tier) const;

    // --- Edge iterators (return all forbidden/required edges as lists) ---
    std::vector<KnowledgeEdge> getListOfForbiddenEdges() const;
    std::vector<KnowledgeEdge> getListOfRequiredEdges() const;

    // --- Variables ---
    void addVariable(const std::string& name);
    std::vector<std::string> getVariables() const;

    // --- General ---
    bool isEmpty() const;
    void clear();

private:
    std::set<std::string> variables_;
    std::vector<RulePair> forbiddenRules_;
    std::vector<RulePair> requiredRules_;
    std::vector<std::set<std::string>> tierSpecs_;

    bool isForbiddenByRules(const std::string& from, const std::string& to) const;
    void ensureTiers(int tier);
    int findTier(const std::string& var) const;
};

} // namespace tetrad
