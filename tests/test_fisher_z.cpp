#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>
#include <cmath>

using namespace tetrad;
using Catch::Matchers::WithinAbs;

TEST_CASE("FisherZ on perfectly correlated data", "[fisher_z]") {
    // X and Y are perfectly correlated, Z is independent
    int n = 200;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        data(i, 0) = x;
        data(i, 1) = x + 0.01 * dist(rng);  // Y strongly correlated with X
        data(i, 2) = dist(rng);               // Z independent
    }

    DataSet ds(data, {"X", "Y", "Z"});
    IndTestFisherZ test(ds, 0.05);

    auto x = test.getVariables()[0];
    auto y = test.getVariables()[1];
    auto z = test.getVariables()[2];

    // X and Y should be dependent (not independent)
    auto result1 = test.checkIndependence(x, y, {});
    REQUIRE(!result1.isIndependent());

    // X and Z should be independent
    auto result2 = test.checkIndependence(x, z, {});
    REQUIRE(result2.isIndependent());
}

TEST_CASE("FisherZ conditional independence", "[fisher_z]") {
    // X → Z ← Y where X and Y are independent
    // Z = X + Y + noise
    int n = 2000;
    Eigen::MatrixXd data(n, 3);
    std::mt19937 rng(123);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        double x = dist(rng);
        double y = dist(rng);
        double z = x + y + 0.3 * dist(rng);
        data(i, 0) = x;
        data(i, 1) = y;
        data(i, 2) = z;
    }

    DataSet ds(data, {"X", "Y", "Z"});
    IndTestFisherZ test(ds, 0.05);

    auto x = test.getVariables()[0];
    auto y = test.getVariables()[1];
    auto z = test.getVariables()[2];

    // X and Y should be marginally independent
    auto result = test.checkIndependence(x, y, {});
    REQUIRE(result.isIndependent());

    // X and Y should be conditionally dependent given Z
    auto result2 = test.checkIndependence(x, y, {z});
    REQUIRE(!result2.isIndependent());
}

TEST_CASE("FisherZ p-value is valid", "[fisher_z]") {
    int n = 100;
    Eigen::MatrixXd data(n, 2);
    std::mt19937 rng(99);
    std::normal_distribution<double> dist(0.0, 1.0);

    for (int i = 0; i < n; i++) {
        data(i, 0) = dist(rng);
        data(i, 1) = dist(rng);
    }

    DataSet ds(data, {"X", "Y"});
    IndTestFisherZ test(ds, 0.05);

    auto x = test.getVariables()[0];
    auto y = test.getVariables()[1];

    auto result = test.checkIndependence(x, y, {});
    REQUIRE(result.pValue >= 0.0);
    REQUIRE(result.pValue <= 1.0);
}
