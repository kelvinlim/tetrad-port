#pragma once

#include "graph/node.h"
#include <vector>
#include <cmath>

namespace tetrad {

// Abstract interface for a decomposable score function.
// Port of edu.cmu.tetrad.search.score.Score from Java Tetrad 7.6.8.
//
// All variables are referenced by integer index (column in the data matrix).
// Higher scores indicate more dependence; negative scores indicate independence.
class Score {
public:
    virtual ~Score() = default;

    // Core method: score of a node given its parent set.
    virtual double localScore(int node, const std::vector<int>& parents) const = 0;

    // Score difference: localScore(y, z ∪ {x}) - localScore(y, z).
    // Default implementation calls localScore twice.
    virtual double localScoreDiff(int x, int y, const std::vector<int>& z) const {
        std::vector<int> zx = z;
        zx.push_back(x);
        return localScore(y, zx) - localScore(y, z);
    }

    // Convenience: score with no parents.
    double localScore(int node) const {
        return localScore(node, {});
    }

    // Variables of the score.
    virtual const std::vector<NodePtr>& getVariables() const = 0;

    // Sample size.
    virtual int getSampleSize() const = 0;

    // True if the given bump (score difference) indicates an effect edge.
    virtual bool isEffectEdge(double bump) const { return bump > 0; }

    // Maximum degree for the score.
    virtual int getMaxDegree() const {
        return static_cast<int>(std::ceil(std::log(getSampleSize())));
    }
};

} // namespace tetrad
