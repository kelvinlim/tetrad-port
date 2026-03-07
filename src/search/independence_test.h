#pragma once

#include "graph/node.h"
#include "search/independence_result.h"
#include <memory>
#include <set>
#include <vector>

namespace tetrad {

class IndependenceTest {
public:
    virtual ~IndependenceTest() = default;

    virtual IndependenceResult checkIndependence(
        const NodePtr& x, const NodePtr& y,
        const std::set<NodePtr>& z) = 0;

    // Convenience: check independence with a vector conditioning set.
    bool isIndependent(const NodePtr& x, const NodePtr& y,
                       const std::vector<NodePtr>& z) {
        std::set<NodePtr> zSet(z.begin(), z.end());
        return checkIndependence(x, y, zSet).isIndependent();
    }

    virtual const std::vector<NodePtr>& getVariables() const = 0;
    virtual int getSampleSize() const = 0;

    virtual double getAlpha() const = 0;
    virtual void setAlpha(double alpha) = 0;

    virtual bool isVerbose() const { return false; }
    virtual void setVerbose(bool /*verbose*/) {}
};

} // namespace tetrad
