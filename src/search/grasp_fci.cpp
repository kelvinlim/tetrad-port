#include "search/grasp_fci.h"
#include "search/grasp.h"

namespace tetrad {

GraspFci::GraspFci(IndependenceTest& test, Score& score)
    : StarFci(test), score_(score) {}

Graph GraspFci::getMarkovCpdag() {
    Grasp alg(score_);
    alg.setSeed(seed_);
    alg.setOrdered(ordered_);
    alg.setUseDataOrder(useDataOrder_);
    alg.setDepth(graspDepth_);
    alg.setUncoveredDepth(uncoveredDepth_);
    alg.setNonSingularDepth(nonSingularDepth_);
    alg.setNumStarts(numStarts_);
    alg.setVerbose(isVerbose());
    alg.setKnowledge(getKnowledge());

    auto variables = score_.getVariables();
    alg.bestOrder(variables);
    return alg.getGraph(true);
}

} // namespace tetrad
