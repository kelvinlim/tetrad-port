#include <catch2/catch_test_macros.hpp>
#include "search/gfci.h"
#include "search/sem_bic_score.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>

using namespace tetrad;

// Generate data from chain: X -> Y -> Z (linear Gaussian)
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

// Generate data from collider: X -> Z <- Y (linear Gaussian)
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
// (L is unobserved, so X and Y appear correlated)
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

TEST_CASE("Gfci: basic chain structure", "[gfci]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    Gfci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(nodeX != nullptr);
    REQUIRE(nodeY != nullptr);
    REQUIRE(nodeZ != nullptr);

    // X and Y should be adjacent
    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    // Y and Z should be adjacent
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    // X and Z should NOT be adjacent
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeZ));

    // Should have exactly 2 edges
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("Gfci: collider structure X->Z<-Y", "[gfci]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    Gfci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    // X and Z should be adjacent
    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    // Y and Z should be adjacent
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    // X and Y should NOT be adjacent
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeY));

    REQUIRE(result.getNumEdges() == 2);

    // In PAG, collider should have arrows into Z
    Edge xz = result.getEdge(nodeX, nodeZ);
    Edge yz = result.getEdge(nodeY, nodeZ);
    REQUIRE(xz.getEndpoint(nodeZ) == Endpoint::ARROW);
    REQUIRE(yz.getEndpoint(nodeZ) == Endpoint::ARROW);
}

TEST_CASE("Gfci: independent variables produce sparse graph", "[gfci]") {
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
    Gfci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    // Independent data → no edges
    REQUIRE(result.getNumEdges() == 0);
}

TEST_CASE("Gfci: latent common cause produces bidirected edge", "[gfci]") {
    auto ds = generateLatentCommonCauseData(2000, 123);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    Gfci gfci(test, score);
    gfci.setVerbose(false);

    Graph result = gfci.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    // X and Y should be adjacent (due to latent common cause)
    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    // X and Z should be adjacent (X -> Z)
    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));

    // The X-Y edge should not be directed from one to the other
    // (no causal path between them). In a PAG it could be X <-> Y or X o-o Y
    // or X o-> Y depending on orientation rules applied.
    Edge xy = result.getEdge(nodeX, nodeY);
    // Neither endpoint should be TAIL (which would indicate a definite cause)
    REQUIRE(xy.getEndpoint(nodeX) != Endpoint::TAIL);
    REQUIRE(xy.getEndpoint(nodeY) != Endpoint::TAIL);
}

TEST_CASE("Gfci: setter methods work", "[gfci]") {
    auto ds = generateChainData(100);
    SemBicScore score(ds);
    IndTestFisherZ test(ds, 0.05);
    Gfci gfci(test, score);

    // Just verify setters don't crash
    gfci.setCompleteRuleSetUsed(true);
    gfci.setMaxDiscriminatingPathLength(5);
    gfci.setDepth(3);
    gfci.setVerbose(false);
    gfci.setFaithfulnessAssumed(true);
    gfci.setMaxDegree(10);

    Knowledge k;
    gfci.setKnowledge(k);
    REQUIRE(gfci.getKnowledge().isEmpty());
}
