#include <catch2/catch_test_macros.hpp>
#include "search/boss.h"
#include "search/permutation_search.h"
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

// Generate data from a diamond: X -> Y, X -> Z, Y -> W, Z -> W
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

TEST_CASE("Boss: basic chain structure X->Y->Z", "[boss]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    Boss boss(score);
    boss.setSeed(42);

    PermutationSearch search(boss);
    Graph result = search.search();

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

TEST_CASE("Boss: collider structure X->Z<-Y", "[boss]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    Boss boss(score);
    boss.setSeed(42);

    PermutationSearch search(boss);
    Graph result = search.search();

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

    // Both X->Z and Y->Z should be directed (collider)
    REQUIRE(result.isDirectedFromTo(nodeX, nodeZ));
    REQUIRE(result.isDirectedFromTo(nodeY, nodeZ));
}

TEST_CASE("Boss: empty graph for independent variables", "[boss]") {
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
    Boss boss(score);
    boss.setSeed(42);

    PermutationSearch search(boss);
    Graph result = search.search();

    REQUIRE(result.getNumEdges() == 0);
}

TEST_CASE("Boss: diamond structure", "[boss]") {
    auto ds = generateDiamondData(2000);
    SemBicScore score(ds);
    Boss boss(score);
    boss.setSeed(42);

    PermutationSearch search(boss);
    Graph result = search.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");
    auto nodeW = result.getNode("W");

    // X-Y, X-Z, Y-W, Z-W should all be adjacent
    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeW));
    REQUIRE(result.isAdjacentTo(nodeZ, nodeW));

    // Y and Z should NOT be adjacent (conditionally independent given X)
    REQUIRE_FALSE(result.isAdjacentTo(nodeY, nodeZ));

    REQUIRE(result.getNumEdges() == 4);
}

TEST_CASE("Boss: with BES refinement", "[boss]") {
    auto ds = generateChainData(1000);
    SemBicScore score(ds);
    Boss boss(score);
    boss.setSeed(42);
    boss.setUseBes(true);

    PermutationSearch search(boss);
    Graph result = search.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("Boss: multiple restarts", "[boss]") {
    auto ds = generateColliderData(1000);
    SemBicScore score(ds);
    Boss boss(score);
    boss.setSeed(42);
    boss.setNumStarts(3);

    PermutationSearch search(boss);
    Graph result = search.search();

    auto nodeX = result.getNode("X");
    auto nodeY = result.getNode("Y");
    auto nodeZ = result.getNode("Z");

    REQUIRE(result.isAdjacentTo(nodeX, nodeZ));
    REQUIRE(result.isAdjacentTo(nodeY, nodeZ));
    REQUIRE_FALSE(result.isAdjacentTo(nodeX, nodeY));
    REQUIRE(result.getNumEdges() == 2);
}

TEST_CASE("Boss: PermutationSearch getGraph static utility", "[boss]") {
    // Build a simple graph manually
    auto nodeX = std::make_shared<Node>("X");
    auto nodeY = std::make_shared<Node>("Y");
    auto nodeZ = std::make_shared<Node>("Z");

    std::vector<NodePtr> nodes = {nodeX, nodeY, nodeZ};
    std::unordered_map<NodePtr, std::unordered_set<NodePtr>> parents;
    parents[nodeX] = {};
    parents[nodeY] = {nodeX};
    parents[nodeZ] = {nodeY};

    // DAG
    Graph dag = PermutationSearch::getGraph(nodes, parents, false);
    REQUIRE(dag.getNumEdges() == 2);
    REQUIRE(dag.isDirectedFromTo(nodeX, nodeY));
    REQUIRE(dag.isDirectedFromTo(nodeY, nodeZ));

    // CPDAG (chain should have undirected edges)
    Graph cpdag = PermutationSearch::getGraph(nodes, parents, true);
    REQUIRE(cpdag.getNumEdges() == 2);
    REQUIRE(cpdag.isAdjacentTo(nodeX, nodeY));
    REQUIRE(cpdag.isAdjacentTo(nodeY, nodeZ));
}
