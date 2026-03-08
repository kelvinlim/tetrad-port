#pragma once

#include "search/star_fci.h"
#include "search/score.h"

namespace tetrad {

// BOSS-FCI: Uses BOSS as the initial CPDAG finder in the *-FCI framework.
// Port of edu.cmu.tetrad.search.BossFci from Java Tetrad 7.6.8.
//
// Combines the permutation-based BOSS algorithm (high precision) with
// FCI orientation rules (handles latent confounders). Returns a PAG.
//
// For BOSS only a score is needed, but *-FCI also requires an independence
// test, so both are needed.
class BossFci : public StarFci {
public:
    BossFci(IndependenceTest& test, Score& score);

    Graph getMarkovCpdag() override;

    void setNumStarts(int numStarts) { numStarts_ = numStarts; }
    void setBossUseBes(bool useBes) { bossUseBes_ = useBes; }

private:
    Score& score_;
    int numStarts_ = 1;
    bool bossUseBes_ = false;
};

} // namespace tetrad
