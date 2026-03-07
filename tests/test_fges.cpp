#include <catch2/catch_test_macros.hpp>
#include "search/fges.h"
#include "search/sem_bic_score.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>

using namespace tetrad;

// Generate data from a known DAG: X -> Y -> Z (linear Gaussian)
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

// Generate data from a collider: X -> Z <- Y (linear Gaussian)
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

TEST_CASE("Fges: basic chain structure X->Y->Z", "[fges]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    Fges fges(score);
    fges.setFaithfulnessAssumed(true);

    Graph result = fges.search();

    // Should find X-Y and Y-Z edges (as CPDAG, so X--Y--Z or similar)
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
    // X and Z should NOT be adjacent (conditional independence)
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeZ));

    // Should have exactly 2 edges
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("Fges: collider structure X->Z<-Y", "[fges]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    Fges fges(score);
    fges.setFaithfulnessAssumed(true);

    Graph result = fges.search();

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

    // In the CPDAG, both X->Z and Y->Z should be directed (collider)
    REQUIRE(result.isDirectedFromTo(nodeX, nodeZ));
    REQUIRE(result.isDirectedFromTo(nodeY, nodeZ));
}

TEST_CASE("Fges: model score is finite", "[fges]") {
    auto ds = generateChainData(500);
    SemBicScore score(ds);
    Fges fges(score);
    fges.setFaithfulnessAssumed(true);

    fges.search();

    REQUIRE(std::isfinite(fges.getModelScore()));
}

TEST_CASE("Fges: empty graph for independent variables", "[fges]") {
    // Generate 3 independent variables
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
    Fges fges(score);
    fges.setFaithfulnessAssumed(true);

    Graph result = fges.search();

    // With independent data, should find no edges (or very few)
    REQUIRE(result.getNumEdges() == 0);
}

TEST_CASE("Fges: faithfulness assumed flag", "[fges]") {
    auto ds = generateChainData(500);
    SemBicScore score(ds);
    Fges fges(score);

    fges.setFaithfulnessAssumed(true);
    REQUIRE(fges.isFaithfulnessAssumed());

    fges.setFaithfulnessAssumed(false);
    REQUIRE_FALSE(fges.isFaithfulnessAssumed());
}
