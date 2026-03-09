#pragma once

#include "search/star_fci.h"
#include "search/score.h"

namespace tetrad {

// GRASP-FCI: Uses GRaSP as the initial CPDAG finder in the *-FCI framework.
// Port of edu.cmu.tetrad.search.GraspFci from Java Tetrad 7.6.8.
//
// Combines the GRaSP algorithm (permutation-based, tuck search) with
// FCI orientation rules (handles latent confounders). Returns a PAG.
class GraspFci : public StarFci {
public:
    GraspFci(IndependenceTest& test, Score& score);

    Graph getMarkovCpdag() override;

    void setNumStarts(int numStarts) { numStarts_ = numStarts; }
    void setGraspDepth(int depth) { graspDepth_ = depth; }
    void setUncoveredDepth(int d) { uncoveredDepth_ = d; }
    void setNonSingularDepth(int d) { nonSingularDepth_ = d; }
    void setOrdered(bool ordered) { ordered_ = ordered; }
    void setUseDataOrder(bool use) { useDataOrder_ = use; }
    void setSeed(long seed) { seed_ = seed; }

private:
    Score& score_;
    int numStarts_ = 1;
    int graspDepth_ = 3;
    int uncoveredDepth_ = 1;
    int nonSingularDepth_ = 1;
    bool ordered_ = false;
    bool useDataOrder_ = true;
    long seed_ = -1;
};

} // namespace tetrad
