#include <catch2/catch_test_macros.hpp>
#include "search/pc.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>
#include <iostream>

using namespace tetrad;

TEST_CASE("PC on simple chain X→Y→Z", "[pc]") {
    // True DAG: X → Y → Z
    // Expected CPDAG: X — Y — Z (chain is Markov equivalent to X←Y←Z and X←Y→Z)
    int n = 5000;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        double y = 0.6 * x + 0.5 * dist(rng);
        double z = 0.6 * y + 0.5 * dist(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    DataSet ds(data, {"X", "Y", "Z"});
    IndTestFisherZ test(ds, 0.05);

    Pc pc(&test);
    Graph g = pc.search();

    // Should have 2 edges: X—Y, Y—Z
    REQUIRE(g.getNumEdges() == 2);
    REQUIRE(g.isAdjacentTo(g.getNode("X"), g.getNode("Y")));
    REQUIRE(g.isAdjacentTo(g.getNode("Y"), g.getNode("Z")));
    REQUIRE(!g.isAdjacentTo(g.getNode("X"), g.getNode("Z")));
}

TEST_CASE("PC on collider X→Z←Y", "[pc]") {
    // True DAG: X → Z ← Y (collider/v-structure)
    // Expected CPDAG: X → Z ← Y
    int n = 2000;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(123);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        double y = dist(rng);
        double z = 0.8 * x + 0.8 * y + 0.3 * dist(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    DataSet ds(data, {"X", "Y", "Z"});
    IndTestFisherZ test(ds, 0.05);

    Pc pc(&test);
    Graph g = pc.search();

    std::cout << g.toString() << std::endl;

    // Should have 2 edges: X→Z, Y→Z
    REQUIRE(g.getNumEdges() == 2);
    REQUIRE(g.isAdjacentTo(g.getNode("X"), g.getNode("Z")));
    REQUIRE(g.isAdjacentTo(g.getNode("Y"), g.getNode("Z")));
    REQUIRE(!g.isAdjacentTo(g.getNode("X"), g.getNode("Y")));

    // Check collider orientation
    REQUIRE(g.isDirectedFromTo(g.getNode("X"), g.getNode("Z")));
    REQUIRE(g.isDirectedFromTo(g.getNode("Y"), g.getNode("Z")));
}

TEST_CASE("PC on diamond structure", "[pc]") {
    // True DAG: X → Y, X → Z, Y → W, Z → W
    int n = 2000;
    Eigen::MatrixXd data(n, 4);
    std::mt19937 rng(999);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        double y = 0.7 * x + 0.3 * dist(rng);
        double z = 0.7 * x + 0.3 * dist(rng);
        double w = 0.5 * y + 0.5 * z + 0.3 * dist(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
        data(i, 3) = w;
    }

    DataSet ds(data, {"X", "Y", "Z", "W"});
    IndTestFisherZ test(ds, 0.05);

    Pc pc(&test);
    Graph g = pc.search();

    std::cout << g.toString() << std::endl;

    // Should have 4 edges
    REQUIRE(g.getNumEdges() == 4);
    REQUIRE(g.isAdjacentTo(g.getNode("X"), g.getNode("Y")));
    REQUIRE(g.isAdjacentTo(g.getNode("X"), g.getNode("Z")));
    REQUIRE(g.isAdjacentTo(g.getNode("Y"), g.getNode("W")));
    REQUIRE(g.isAdjacentTo(g.getNode("Z"), g.getNode("W")));
}

TEST_CASE("PC on independent variables", "[pc]") {
    int n = 500;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(77);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        data(i, 0) = dist(rng);
        data(i, 1) = dist(rng);
        data(i, 2) = dist(rng);
    }

    DataSet ds(data, {"A", "B", "C"});
    IndTestFisherZ test(ds, 0.05);

    Pc pc(&test);
    Graph g = pc.search();

    REQUIRE(g.getNumEdges() == 0);
}
