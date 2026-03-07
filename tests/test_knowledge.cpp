#include <catch2/catch_test_macros.hpp>
#include "data/knowledge.h"
#include <algorithm>

using namespace tetrad;

TEST_CASE("Knowledge: empty by default", "[knowledge]") {
    Knowledge k;
    REQUIRE(k.isEmpty());
    REQUIRE_FALSE(k.isForbidden("X", "Y"));
    REQUIRE_FALSE(k.isRequired("X", "Y"));
    REQUIRE(k.noEdgeRequired("X", "Y"));
    REQUIRE(k.getNumTiers() == 0);
    REQUIRE(k.getVariables().empty());
}

TEST_CASE("Knowledge: construct with variables", "[knowledge]") {
    Knowledge k({"X", "Y", "Z"});
    REQUIRE(k.isEmpty());  // no rules yet
    auto vars = k.getVariables();
    REQUIRE(vars.size() == 3);
    // std::set iteration is sorted
    REQUIRE(vars[0] == "X");
    REQUIRE(vars[1] == "Y");
    REQUIRE(vars[2] == "Z");
}

TEST_CASE("Knowledge: explicit forbidden edges", "[knowledge]") {
    Knowledge k;

    k.setForbidden("X", "Y");
    REQUIRE_FALSE(k.isEmpty());
    REQUIRE(k.isForbidden("X", "Y"));
    REQUIRE_FALSE(k.isForbidden("Y", "X"));  // directional
    REQUIRE_FALSE(k.isForbidden("X", "Z"));

    // Variables auto-added
    auto vars = k.getVariables();
    REQUIRE(vars.size() == 2);

    // Remove forbidden
    k.removeForbidden("X", "Y");
    REQUIRE_FALSE(k.isForbidden("X", "Y"));
}

TEST_CASE("Knowledge: explicit required edges", "[knowledge]") {
    Knowledge k;

    k.setRequired("A", "B");
    REQUIRE(k.isRequired("A", "B"));
    REQUIRE_FALSE(k.isRequired("B", "A"));
    REQUIRE_FALSE(k.isEmpty());

    // noEdgeRequired
    REQUIRE_FALSE(k.noEdgeRequired("A", "B"));
    REQUIRE_FALSE(k.noEdgeRequired("B", "A"));  // checks both directions
    REQUIRE(k.noEdgeRequired("A", "C"));

    // Remove required
    k.removeRequired("A", "B");
    REQUIRE_FALSE(k.isRequired("A", "B"));
    REQUIRE(k.noEdgeRequired("A", "B"));
}

TEST_CASE("Knowledge: duplicate forbidden/required ignored", "[knowledge]") {
    Knowledge k;

    k.setForbidden("X", "Y");
    k.setForbidden("X", "Y");  // duplicate
    auto forbidden = k.getListOfForbiddenEdges();
    int count = 0;
    for (const auto& e : forbidden) {
        if (e.from == "X" && e.to == "Y") count++;
    }
    REQUIRE(count == 1);

    k.setRequired("A", "B");
    k.setRequired("A", "B");  // duplicate
    auto required = k.getListOfRequiredEdges();
    count = 0;
    for (const auto& e : required) {
        if (e.from == "A" && e.to == "B") count++;
    }
    REQUIRE(count == 1);
}

TEST_CASE("Knowledge: temporal tiers basic", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "A");
    k.addToTier(0, "B");
    k.addToTier(1, "C");
    k.addToTier(1, "D");

    REQUIRE_FALSE(k.isEmpty());
    REQUIRE(k.getNumTiers() == 2);

    auto tier0 = k.getTier(0);
    REQUIRE(tier0.size() == 2);
    REQUIRE(tier0[0] == "A");
    REQUIRE(tier0[1] == "B");

    auto tier1 = k.getTier(1);
    REQUIRE(tier1.size() == 2);
    REQUIRE(tier1[0] == "C");
    REQUIRE(tier1[1] == "D");
}

TEST_CASE("Knowledge: tier forbids later->earlier edges", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "A");
    k.addToTier(1, "B");
    k.addToTier(2, "C");

    // Later tier -> earlier tier is forbidden
    REQUIRE(k.isForbidden("B", "A"));     // tier 1 -> tier 0
    REQUIRE(k.isForbidden("C", "A"));     // tier 2 -> tier 0
    REQUIRE(k.isForbidden("C", "B"));     // tier 2 -> tier 1

    // Earlier -> later is allowed
    REQUIRE_FALSE(k.isForbidden("A", "B"));
    REQUIRE_FALSE(k.isForbidden("A", "C"));
    REQUIRE_FALSE(k.isForbidden("B", "C"));

    // Same tier is allowed (unless setTierForbiddenWithin)
    k.addToTier(0, "A2");
    REQUIRE_FALSE(k.isForbidden("A", "A2"));

    // isForbiddenByTiers specifically
    REQUIRE(k.isForbiddenByTiers("C", "A"));
    REQUIRE_FALSE(k.isForbiddenByTiers("A", "C"));
}

TEST_CASE("Knowledge: addToTier moves variable between tiers", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "X");
    REQUIRE(k.getTier(0).size() == 1);

    // Move X to tier 1
    k.addToTier(1, "X");
    REQUIRE(k.getTier(0).empty());
    REQUIRE(k.getTier(1).size() == 1);
    REQUIRE(k.getTier(1)[0] == "X");
}

TEST_CASE("Knowledge: setTier replaces tier contents", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "A");
    k.addToTier(0, "B");
    k.setTier(0, {"C", "D"});

    auto tier0 = k.getTier(0);
    REQUIRE(tier0.size() == 2);
    REQUIRE(tier0[0] == "C");
    REQUIRE(tier0[1] == "D");
}

TEST_CASE("Knowledge: removeFromTiers", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "A");
    k.addToTier(1, "B");
    k.removeFromTiers("A");

    REQUIRE(k.getTier(0).empty());
    REQUIRE(k.getTier(1).size() == 1);
}

TEST_CASE("Knowledge: getVariablesNotInTiers", "[knowledge]") {
    Knowledge k({"A", "B", "C", "D"});

    k.addToTier(0, "A");
    k.addToTier(1, "B");

    auto notInTiers = k.getVariablesNotInTiers();
    REQUIRE(notInTiers.size() == 2);
    // std::set iteration is sorted
    REQUIRE(std::find(notInTiers.begin(), notInTiers.end(), "C") != notInTiers.end());
    REQUIRE(std::find(notInTiers.begin(), notInTiers.end(), "D") != notInTiers.end());
}

TEST_CASE("Knowledge: tier forbidden within", "[knowledge]") {
    Knowledge k;

    k.addToTier(0, "A");
    k.addToTier(0, "B");

    REQUIRE_FALSE(k.isTierForbiddenWithin(0));
    REQUIRE_FALSE(k.isForbidden("A", "B"));

    k.setTierForbiddenWithin(0, true);
    REQUIRE(k.isTierForbiddenWithin(0));
    REQUIRE(k.isForbidden("A", "B"));
    REQUIRE(k.isForbidden("B", "A"));

    // Undo
    k.setTierForbiddenWithin(0, false);
    REQUIRE_FALSE(k.isTierForbiddenWithin(0));
    REQUIRE_FALSE(k.isForbidden("A", "B"));
}

TEST_CASE("Knowledge: getListOfForbiddenEdges", "[knowledge]") {
    Knowledge k;

    k.setForbidden("X", "Y");
    k.addToTier(0, "A");
    k.addToTier(1, "B");

    auto edges = k.getListOfForbiddenEdges();

    // Should contain: X->Y (explicit) and B->A (tier)
    bool hasXY = false, hasBA = false;
    for (const auto& e : edges) {
        if (e.from == "X" && e.to == "Y") hasXY = true;
        if (e.from == "B" && e.to == "A") hasBA = true;
    }
    REQUIRE(hasXY);
    REQUIRE(hasBA);
}

TEST_CASE("Knowledge: getListOfRequiredEdges", "[knowledge]") {
    Knowledge k;

    k.setRequired("X", "Y");
    k.setRequired("A", "B");

    auto edges = k.getListOfRequiredEdges();
    REQUIRE(edges.size() == 2);

    bool hasXY = false, hasAB = false;
    for (const auto& e : edges) {
        if (e.from == "X" && e.to == "Y") hasXY = true;
        if (e.from == "A" && e.to == "B") hasAB = true;
    }
    REQUIRE(hasXY);
    REQUIRE(hasAB);
}

TEST_CASE("Knowledge: clear resets everything", "[knowledge]") {
    Knowledge k;

    k.setForbidden("X", "Y");
    k.setRequired("A", "B");
    k.addToTier(0, "C");

    REQUIRE_FALSE(k.isEmpty());
    k.clear();
    REQUIRE(k.isEmpty());
    REQUIRE_FALSE(k.isForbidden("X", "Y"));
    REQUIRE_FALSE(k.isRequired("A", "B"));
    REQUIRE(k.getNumTiers() == 0);
    REQUIRE(k.getVariables().empty());
}

TEST_CASE("Knowledge: copy semantics", "[knowledge]") {
    Knowledge k1;
    k1.setForbidden("X", "Y");
    k1.addToTier(0, "A");

    Knowledge k2 = k1;
    REQUIRE(k2.isForbidden("X", "Y"));
    REQUIRE(k2.getTier(0).size() == 1);

    // Modifications to k2 don't affect k1
    k2.setForbidden("A", "B");
    REQUIRE_FALSE(k1.isForbidden("A", "B"));
}

TEST_CASE("Knowledge: getTier out of range returns empty", "[knowledge]") {
    Knowledge k;
    REQUIRE(k.getTier(0).empty());
    REQUIRE(k.getTier(5).empty());
    REQUIRE(k.getTier(-1).empty());
}

TEST_CASE("Knowledge: ensureTiers creates intermediate tiers", "[knowledge]") {
    Knowledge k;
    k.addToTier(3, "X");
    REQUIRE(k.getNumTiers() == 4);  // tiers 0, 1, 2, 3
    REQUIRE(k.getTier(0).empty());
    REQUIRE(k.getTier(1).empty());
    REQUIRE(k.getTier(2).empty());
    REQUIRE(k.getTier(3).size() == 1);
}

TEST_CASE("Knowledge: three-tier temporal ordering", "[knowledge]") {
    // Common use case: lag variables
    // tier 0 = {X_lag2, Y_lag2} (oldest)
    // tier 1 = {X_lag1, Y_lag1}
    // tier 2 = {X, Y}          (most recent)
    Knowledge k;
    k.addToTier(0, "X_lag2");
    k.addToTier(0, "Y_lag2");
    k.addToTier(1, "X_lag1");
    k.addToTier(1, "Y_lag1");
    k.addToTier(2, "X");
    k.addToTier(2, "Y");

    // Current cannot cause past
    REQUIRE(k.isForbidden("X", "X_lag1"));
    REQUIRE(k.isForbidden("X", "Y_lag2"));
    REQUIRE(k.isForbidden("Y", "X_lag2"));

    // Past can cause present
    REQUIRE_FALSE(k.isForbidden("X_lag2", "X"));
    REQUIRE_FALSE(k.isForbidden("Y_lag1", "X"));

    // Same tier allowed
    REQUIRE_FALSE(k.isForbidden("X", "Y"));
    REQUIRE_FALSE(k.isForbidden("X_lag1", "Y_lag1"));
}
