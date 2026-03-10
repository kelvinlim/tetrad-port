#include "search/generic_fci.h"

namespace tetrad {

GenericFci::GenericFci(IndependenceTest& test, const Graph& initialCpdag)
    : StarFci(test), cpdag_(initialCpdag) {}

Graph GenericFci::getMarkovCpdag() {
    return cpdag_;
}

} // namespace tetrad
