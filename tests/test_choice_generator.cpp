#include <catch2/catch_test_macros.hpp>
#include "util/choice_generator.h"
#include <vector>

using namespace tetrad;

TEST_CASE("ChoiceGenerator 5 choose 3", "[choice_generator]") {
    ChoiceGenerator gen(5, 3);
    int count = 0;
    const int* choice;

    while ((choice = gen.next()) != nullptr) {
        // Each combination should be strictly increasing
        for (int i = 0; i < 2; i++) {
            REQUIRE(choice[i] < choice[i + 1]);
        }
        // Values should be in range [0, 5)
        for (int i = 0; i < 3; i++) {
            REQUIRE(choice[i] >= 0);
            REQUIRE(choice[i] < 5);
        }
        count++;
    }

    REQUIRE(count == 10);  // C(5,3) = 10
}

TEST_CASE("ChoiceGenerator 4 choose 2", "[choice_generator]") {
    ChoiceGenerator gen(4, 2);
    int count = 0;
    const int* choice;

    while ((choice = gen.next()) != nullptr) {
        count++;
    }

    REQUIRE(count == 6);  // C(4,2) = 6
}

TEST_CASE("ChoiceGenerator n choose 0", "[choice_generator]") {
    ChoiceGenerator gen(5, 0);
    int count = 0;
    const int* choice;

    while ((choice = gen.next()) != nullptr) {
        count++;
    }

    REQUIRE(count == 1);  // C(n,0) = 1 (empty set)
}

TEST_CASE("ChoiceGenerator n choose n", "[choice_generator]") {
    ChoiceGenerator gen(4, 4);
    int count = 0;
    const int* choice;

    while ((choice = gen.next()) != nullptr) {
        REQUIRE(choice[0] == 0);
        REQUIRE(choice[1] == 1);
        REQUIRE(choice[2] == 2);
        REQUIRE(choice[3] == 3);
        count++;
    }

    REQUIRE(count == 1);  // C(n,n) = 1
}

TEST_CASE("ChoiceGenerator impossible (n < k)", "[choice_generator]") {
    ChoiceGenerator gen(2, 5);
    REQUIRE(gen.next() == nullptr);
}

TEST_CASE("getNumCombinations", "[choice_generator]") {
    REQUIRE(ChoiceGenerator::getNumCombinations(5, 3) == 10);
    REQUIRE(ChoiceGenerator::getNumCombinations(10, 2) == 45);
    REQUIRE(ChoiceGenerator::getNumCombinations(6, 0) == 1);
    REQUIRE(ChoiceGenerator::getNumCombinations(6, 6) == 1);
}
