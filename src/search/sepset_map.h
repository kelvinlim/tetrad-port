#pragma once

#include "graph/node.h"
#include <map>
#include <set>
#include <string>
#include <optional>

namespace tetrad {

class SepsetMap {
public:
    SepsetMap() = default;

    void set(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z);
    std::optional<std::set<NodePtr>> get(const NodePtr& x, const NodePtr& y) const;

    void setPValue(const NodePtr& x, const NodePtr& y, double pValue);
    double getPValue(const NodePtr& x, const NodePtr& y) const;

    int size() const { return static_cast<int>(sepsets_.size()); }
    void clear();

private:
    // Canonicalize key: always use alphabetically smaller name first
    static std::pair<std::string, std::string> makeKey(const NodePtr& x, const NodePtr& y);

    std::map<std::pair<std::string, std::string>, std::set<NodePtr>> sepsets_;
    std::map<std::pair<std::string, std::string>, double> pValues_;
};

} // namespace tetrad
