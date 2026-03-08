#include "search/boss_fci.h"
#include "search/boss.h"
#include "search/permutation_search.h"

namespace tetrad {

BossFci::BossFci(IndependenceTest& test, Score& score)
    : StarFci(test), score_(score) {}

Graph BossFci::getMarkovCpdag() {
    Boss boss(score_);
    boss.setUseBes(bossUseBes_);
    boss.setNumStarts(numStarts_);
    boss.setVerbose(isVerbose());

    PermutationSearch alg(boss);
    alg.setKnowledge(getKnowledge());

    return alg.search();
}

} // namespace tetrad
