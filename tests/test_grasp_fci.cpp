#include <catch2/catch_test_macros.hpp>
#include "search/grasp_fci.h"
#include "search/sem_bic_score.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>

using namespace tetrad;

// Generate data from chain: X -> Y -> Z
static DataSet generateChainData(int n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);

    Eigen::MatrixXd data(n, 3);
    for (int i = 0; i < n; i++) {
        double x = noise(rng);
        double y = 0.8 * x + noise(rng);
        double z = 0.6 * y + noise(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    return DataSet(data, {"X", "Y", "Z"});
}

// Generate data from collider: X -> Z <- Y
static DataSet generateColliderData(int n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);

    Eigen::MatrixXd data(n, 3);
    for (int i = 0; i < n; i++) {
        double x = noise(rng);
        double y = noise(rng);
        double z = 0.7 * x + 0.5 * y + noise(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    return DataSet(data, {"X", "Y", "Z"});
}

// Generate data with latent common cause: L -> X, L -> Y, X -> Z
static DataSet generateLatentCommonCauseData(int n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);

    Eigen::MatrixXd data(n, 3);
    for (int i = 0; i < n; i++) {
        double l = noise(rng);  // latent
        double x = 0.8 * l + noise(rng);
        double y = 0.7 * l + noise(rng);
        double z = 0.6 * x + noise(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    return DataSet(data, {"X", "Y", "Z"});
}

TEST_CASE("GraspFci: basic chain structure", "[grasp_fci]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    GraspFci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(nodeX != nullptr);
    REQUIRE(nodeY != nullptr);
    REQUIRE(nodeZ != nullptr);

    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("GraspFci: collider structure X->Z<-Y", "[grasp_fci]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    GraspFci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.getNumEdges() == 2);

    // In PAG, collider should have arrows into Z
    Edge xz = result.getEdge(nodeX, nodeZ);
    Edge yz = result.getEdge(nodeY, nodeZ);
    REQUIRE(xz.getEndpoint(nodeZ) == Endpoint::ARROW);
    REQUIRE(yz.getEndpoint(nodeZ) == Endpoint::ARROW);
}

TEST_CASE("GraspFci: independent variables produce empty graph", "[grasp_fci]") {
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, 1.0);

    int n = 500;
    Eigen::MatrixXd data(n, 3);
    for (int i = 0; i < n; i++) {
        data(i, 0) = noise(rng);
        data(i, 1) = noise(rng);
        data(i, 2) = noise(rng);
    }

    DataSet ds(data, {"X", "Y", "Z"});
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    GraspFci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    REQUIRE(result.getNumEdges() == 0);
}

TEST_CASE("GraspFci: latent common cause produces bidirected edge", "[grasp_fci]") {
    auto ds = generateLatentCommonCauseData(2000, 123);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    GraspFci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    // X and Y should be adjacent (due to latent common cause)
    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    // X and Z should be adjacent
    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));

    // The X-Y edge should not have TAIL endpoints
    Edge xy = result.getEdge(nodeX, nodeY);
    REQUIRE(xy.getEndpoint(nodeX) != Endpoint::TAIL);
    REQUIRE(xy.getEndpoint(nodeY) != Endpoint::TAIL);
}

TEST_CASE("GraspFci: setter methods work", "[grasp_fci]") {
    auto ds = generateChainData(100);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    GraspFci gfci(test, score);

    gfci.setCompleteRuleSetUsed(true);
    gfci.setMaxDiscriminatingPathLength(5);
    gfci.setDepth(3);
    gfci.setVerbose(false);
    gfci.setNumStarts(2);
    gfci.setGraspDepth(5);
    gfci.setUncoveredDepth(2);
    gfci.setNonSingularDepth(2);
    gfci.setOrdered(false);
    gfci.setUseDataOrder(true);
    gfci.setSeed(123);

    Knowledge k;
    gfci.setKnowledge(k);
    REQUIRE(gfci.getKnowledge().isEmpty());
}
