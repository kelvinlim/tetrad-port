#include <catch2/catch_test_macros.hpp>
#include "search/fas.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>

using namespace tetrad;

TEST_CASE("FAS recovers simple chain skeleton", "[fas]") {
    // True structure: X → Y → Z (chain)
    // Expected skeleton: X — Y — Z (no X—Z edge)
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

    Fas fas(&test);
    fas.setDepth(-1);
    fas.setStable(true);

    Graph skeleton = fas.search();

    REQUIRE(skeleton.isAdjacentTo(skeleton.getNode("X"), skeleton.getNode("Y")));
    REQUIRE(skeleton.isAdjacentTo(skeleton.getNode("Y"), skeleton.getNode("Z")));
    // X and Z should NOT be adjacent (conditional independence given Y)
    REQUIRE(!skeleton.isAdjacentTo(skeleton.getNode("X"), skeleton.getNode("Z")));
}

TEST_CASE("FAS on independent variables", "[fas]") {
    // All three variables are independent
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

    Fas fas(&test);
    Graph skeleton = fas.search();

    // No edges expected
    REQUIRE(skeleton.getNumEdges() == 0);
}

TEST_CASE("FAS stores separation sets", "[fas]") {
    int n = 500;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        double y = 0.8 * x + 0.3 * dist(rng);
        double z = 0.8 * y + 0.3 * dist(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    DataSet ds(data, {"X", "Y", "Z"});
    IndTestFisherZ test(ds, 0.05);

    Fas fas(&test);
    Graph skeleton = fas.search();

    auto& sepsets = fas.getSepsets();
    auto x = skeleton.getNode("X");
    auto z = skeleton.getNode("Z");

    // If X and Z were separated, there should be a sepset
    if (!skeleton.isAdjacentTo(x, z)) {
        auto S = sepsets.get(x, z);
        REQUIRE(S.has_value());
    }
}
