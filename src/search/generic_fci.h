#pragma once

#include "search/star_fci.h"
#include "search/score.h"

namespace tetrad {

// GenericFci: Takes a pre-computed CPDAG and applies the *-FCI pipeline.
//
// This enables composable "any algorithm + FCI rules" usage:
//   1. Run any CPDAG-producing algorithm (PC, FGES, BOSS, GRaSP)
//   2. Pass the CPDAG to GenericFci
//   3. Get back a PAG with latent confounders handled
//
// Uses the StarFci template: extra edge removal via independence testing,
// reorient to circles, orient colliders, then FCI rules R1-R10.
class GenericFci : public StarFci {
public:
    GenericFci(IndependenceTest& test, const Graph& initialCpdag);

    Graph getMarkovCpdag() override;

private:
    Graph cpdag_;
};

} // namespace tetrad
