#include <catch2/catch_test_macros.hpp>
#include "search/grasp.h"
#include "search/sem_bic_score.h"
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

// Generate data from diamond: X -> Y, X -> Z, Y -> W, Z -> W
static DataSet generateDiamondData(int n, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);

    Eigen::MatrixXd data(n, 4);
    for (int i = 0; i < n; i++) {
        double x = noise(rng);
        double y = 0.8 * x + noise(rng);
        double z = 0.7 * x + noise(rng);
        double w = 0.5 * y + 0.5 * z + noise(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
        data(i, 3) = w;
    }

    return DataSet(data, {"X", "Y", "Z", "W"});
}

TEST_CASE("Grasp: basic chain structure X->Y->Z", "[grasp]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    Grasp grasp(score);
    grasp.setSeed(42);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph result = grasp.getGraph(true);

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

TEST_CASE("Grasp: collider structure X->Z<-Y", "[grasp]") {
    auto ds = generateColliderData(2000);
    SemBicScore score(ds);
    Grasp grasp(score);
    grasp.setSeed(42);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph result = grasp.getGraph(true);

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.getNumEdges() == 2);

    // Both should be directed into Z (collider)
    REQUIRE(result.isDirectedFromTo(nodeX, nodeZ));
    REQUIRE(result.isDirectedFromTo(nodeY, nodeZ));
}

TEST_CASE("Grasp: empty graph for independent variables", "[grasp]") {
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
    Grasp grasp(score);
    grasp.setSeed(42);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph result = grasp.getGraph(true);

    REQUIRE(result.getNumEdges() == 0);
}

TEST_CASE("Grasp: diamond structure", "[grasp]") {
    auto ds = generateDiamondData(5000);
    SemBicScore score(ds);
    Grasp grasp(score);
    grasp.setSeed(42);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph result = grasp.getGraph(true);

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");
    auto nodeW = result.getNode("W");

    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeW));
    REQUIRE(result.isAdjacentTo(nodeZ, nodeW));
    REQUIRE_FALSE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE(result.getNumEdges() == 4);
}

TEST_CASE("Grasp: multiple restarts", "[grasp]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    Grasp grasp(score);
    grasp.setSeed(42);
    grasp.setNumStarts(3);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph result = grasp.getGraph(true);

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("Grasp: setter methods work", "[grasp]") {
    auto ds = generateChainData(100);
    SemBicScore score(ds);
    Grasp grasp(score);

    grasp.setDepth(5);
    grasp.setUncoveredDepth(2);
    grasp.setNonSingularDepth(2);
    grasp.setOrdered(true);
    grasp.setUseDataOrder(true);
    grasp.setAllowInternalRandomness(false);
    grasp.setVerbose(false);
    grasp.setNumStarts(1);
    grasp.setSeed(123);

    Knowledge k;
    grasp.setKnowledge(k);

    // Just verify it doesn't crash
    auto variables = score.getVariables();
    auto result = grasp.bestOrder(variables);
    REQUIRE(!result.empty());
}
