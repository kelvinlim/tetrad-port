#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "data/data_set.h"
#include "search/ind_test_fisher_z.h"
#include "search/sem_bic_score.h"
#include "search/fges.h"
#include "search/pc.h"
#include "search/boss.h"
#include "search/permutation_search.h"
#include "search/generic_fci.h"

using namespace tetrad;

// Helper: create a simple chain dataset X -> Y -> Z
static DataSet makeChainData() {
    Eigen::MatrixXd data(200, 3);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);
    for (int i = 0; i < 200; ++i) {
        data(i, 0) = dist(rng);
        data(i, 1) = 0.8 * data(i, 0) + 0.5 * dist(rng);
        data(i, 2) = 0.6 * data(i, 1) + 0.5 * dist(rng);
    }
    return DataSet(data, {"X", "Y", "Z"});
}

TEST_CASE("GenericFci with FGES initial CPDAG", "[generic_fci]") {
    auto ds = makeChainData();
    SemBicScore score(ds);
    score.setPenaltyDiscount(1.0);

    // Get CPDAG from FGES
    Fges fges(score);
    fges.setFaithfulnessAssumed(true);
    fges.setVerbose(false);
    Graph cpdag = fges.search();

    // Pipe through GenericFci
    IndTestFisherZ test(ds, 0.05);
    GenericFci fci(test, cpdag);
    fci.setCompleteRuleSetUsed(true);
    fci.setDepth(-1);
    fci.setMaxDiscriminatingPathLength(-1);
    fci.setVerbose(false);

    Graph pag = fci.search();

    REQUIRE(pag.getNumNodes() == 3);
    REQUIRE(pag.getNumEdges() >= 1);
    REQUIRE(pag.getNumEdges() <= 3);
}

TEST_CASE("GenericFci with PC initial CPDAG", "[generic_fci]") {
    auto ds = makeChainData();

    // Get CPDAG from PC
    IndTestFisherZ pcTest(ds, 0.05);
    Pc pc(&pcTest);
    pc.setDepth(-1);
    pc.setVerbose(false);
    Graph cpdag = pc.search();

    // Pipe through GenericFci
    IndTestFisherZ test(ds, 0.05);
    GenericFci fci(test, cpdag);
    fci.setCompleteRuleSetUsed(true);
    fci.setVerbose(false);

    Graph pag = fci.search();

    REQUIRE(pag.getNumNodes() == 3);
    REQUIRE(pag.getNumEdges() >= 1);
}

TEST_CASE("GenericFci with BOSS initial CPDAG", "[generic_fci]") {
    auto ds = makeChainData();
    SemBicScore score(ds);
    score.setPenaltyDiscount(1.0);

    // Get CPDAG from BOSS
    Boss boss(score);
    boss.setUseBes(false);
    boss.setNumStarts(1);
    boss.setVerbose(false);
    PermutationSearch search(boss);
    Graph cpdag = search.search();

    // Pipe through GenericFci
    IndTestFisherZ test(ds, 0.05);
    GenericFci fci(test, cpdag);
    fci.setCompleteRuleSetUsed(true);
    fci.setVerbose(false);

    Graph pag = fci.search();

    REQUIRE(pag.getNumNodes() == 3);
    REQUIRE(pag.getNumEdges() >= 1);
}

TEST_CASE("GenericFci with knowledge", "[generic_fci]") {
    auto ds = makeChainData();
    SemBicScore score(ds);
    score.setPenaltyDiscount(1.0);

    Fges fges(score);
    fges.setFaithfulnessAssumed(true);
    Graph cpdag = fges.search();

    Knowledge kn;
    kn.setForbidden("X", "Y");

    IndTestFisherZ test(ds, 0.05);
    GenericFci fci(test, cpdag);
    fci.setKnowledge(kn);
    fci.setCompleteRuleSetUsed(true);
    fci.setVerbose(false);

    Graph pag = fci.search();

    REQUIRE(pag.getNumNodes() == 3);
    // X --> Y should not be present
    auto xNode = pag.getNode("X");
    auto yNode = pag.getNode("Y");
    REQUIRE_FALSE(pag.isDirectedFromTo(xNode, yNode));
}
