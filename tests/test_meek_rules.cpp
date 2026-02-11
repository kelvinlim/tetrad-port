#include <catch2/catch_test_macros.hpp>
#include "search/meek_rules.h"

using namespace tetrad;

TEST_CASE("Meek R1: a→b—c, a not adj c → b→c", "[meek]") {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");

    Graph g;
    g.addNode(a); g.addNode(b); g.addNode(c);
    g.addDirectedEdge(a, b);
    g.addUndirectedEdge(b, c);
    // a not adjacent to c

    MeekRules rules;
    rules.orientImplied(g);

    REQUIRE(g.isDirectedFromTo(b, c));
}

TEST_CASE("Meek R2: a→b→c, a—c → a→c", "[meek]") {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");

    Graph g;
    g.addNode(a); g.addNode(b); g.addNode(c);
    g.addDirectedEdge(a, b);
    g.addDirectedEdge(b, c);
    g.addUndirectedEdge(a, c);

    MeekRules rules;
    rules.orientImplied(g);

    REQUIRE(g.isDirectedFromTo(a, c));
}

TEST_CASE("Meek R3: d—a, d—b, d—c, b→a, c→a, b not adj c → d→a", "[meek]") {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");
    auto d = std::make_shared<Node>("D");

    Graph g;
    g.addNode(a); g.addNode(b); g.addNode(c); g.addNode(d);
    g.addUndirectedEdge(d, a);
    g.addUndirectedEdge(d, b);
    g.addUndirectedEdge(d, c);
    g.addDirectedEdge(b, a);
    g.addDirectedEdge(c, a);
    // b not adjacent to c

    MeekRules rules;
    rules.orientImplied(g);

    REQUIRE(g.isDirectedFromTo(d, a));
}

TEST_CASE("Meek prevents cycles", "[meek]") {
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");

    Graph g;
    g.addNode(a); g.addNode(b); g.addNode(c);
    g.addDirectedEdge(a, b);
    g.addDirectedEdge(b, c);
    g.addUndirectedEdge(c, a);

    // R1 would want to orient c→a (because b→c and b not adj a... wait, b IS adj to a)
    // Actually, this tests that Meek doesn't create cycle c→a when a→b→c exists.
    MeekRules rules;
    rules.setMeekPreventCycles(true);
    rules.orientImplied(g);

    // If meek tried to orient a→c, that's fine (no cycle).
    // If it tried c→a, that would create a→b→c→a cycle and should be prevented.
    // Neither R1 nor R2 applies to orient c→a here, so it might stay undirected.
    // But R2 could orient a→c since a→b→c and a—c.
    REQUIRE(g.isDirectedFromTo(a, c));
    REQUIRE(!g.isDirectedFromTo(c, a));
}
