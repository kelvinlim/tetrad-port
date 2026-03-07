#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "search/sem_bic_score.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <random>
#include <cmath>

using namespace tetrad;
using Catch::Matchers::WithinAbs;

// Generate data from a known DAG: X -> Y -> Z (linear Gaussian)
static DataSet generateLinearData(int n, unsigned seed = 42) {
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

TEST_CASE("SemBicScore: basic construction", "[sem_bic_score]") {
    auto ds = generateLinearData(500);
    SemBicScore score(ds);

    REQUIRE(score.getSampleSize() == 500);
    REQUIRE(score.getVariables().size() == 3);
    REQUIRE(score.getPenaltyDiscount() == 1.0);
}

TEST_CASE("SemBicScore: localScore returns finite values", "[sem_bic_score]") {
    auto ds = generateLinearData(500);
    SemBicScore score(ds);

    // Score with no parents
    double s0 = score.localScore(0);
    REQUIRE(std::isfinite(s0));

    // Score with one parent
    double s1 = score.localScore(1, {0});
    REQUIRE(std::isfinite(s1));

    // Score with two parents
    double s2 = score.localScore(2, {0, 1});
    REQUIRE(std::isfinite(s2));
}

TEST_CASE("SemBicScore: adding true parent improves score", "[sem_bic_score]") {
    auto ds = generateLinearData(1000);
    SemBicScore score(ds);

    // Y's score should improve when we add X as parent (X -> Y in true DAG)
    double yNoParents = score.localScore(1, {});
    double yWithX = score.localScore(1, {0});
    REQUIRE(yWithX > yNoParents);

    // Z's score should improve when we add Y as parent (Y -> Z in true DAG)
    double zNoParents = score.localScore(2, {});
    double zWithY = score.localScore(2, {1});
    REQUIRE(zWithY > zNoParents);
}

TEST_CASE("SemBicScore: localScoreDiff positive for true edges", "[sem_bic_score]") {
    auto ds = generateLinearData(1000);
    SemBicScore score(ds);

    // X -> Y: diff should be positive
    double diff_xy = score.localScoreDiff(0, 1, {});
    REQUIRE(diff_xy > 0);

    // Y -> Z: diff should be positive
    double diff_yz = score.localScoreDiff(1, 2, {});
    REQUIRE(diff_yz > 0);
}

TEST_CASE("SemBicScore: adding spurious parent does not improve score", "[sem_bic_score]") {
    auto ds = generateLinearData(1000);
    SemBicScore score(ds);

    // X is independent of Z given Y. Adding X as parent of Z when Y is
    // already parent should not improve the score (negative diff).
    double diff = score.localScoreDiff(0, 2, {1});
    // With enough data, this should be negative (BIC penalizes the extra parameter)
    REQUIRE(diff < 0);
}

TEST_CASE("SemBicScore: penalty discount affects score", "[sem_bic_score]") {
    auto ds = generateLinearData(500);
    SemBicScore score1(ds);
    score1.setPenaltyDiscount(1.0);

    SemBicScore score2(ds);
    score2.setPenaltyDiscount(2.0);

    // Higher penalty = lower score for models with parents
    double s1 = score1.localScore(1, {0});
    double s2 = score2.localScore(1, {0});
    REQUIRE(s1 > s2);

    // No parents => no penalty difference
    double s1_nopar = score1.localScore(0);
    double s2_nopar = score2.localScore(0);
    REQUIRE_THAT(s1_nopar, WithinAbs(s2_nopar, 1e-10));
}

TEST_CASE("SemBicScore: isEffectEdge", "[sem_bic_score]") {
    auto ds = generateLinearData(100);
    SemBicScore score(ds);

    REQUIRE(score.isEffectEdge(0.5));
    REQUIRE_FALSE(score.isEffectEdge(-0.5));
    REQUIRE_FALSE(score.isEffectEdge(0.0));
}
