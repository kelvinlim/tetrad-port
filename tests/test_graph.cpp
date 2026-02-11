#include <catch2/catch_test_macros.hpp>
#include "graph/graph.h"

using namespace tetrad;

TEST_CASE("Graph add and query nodes", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");

    REQUIRE(g.addNode(x));
    REQUIRE(g.addNode(y));
    REQUIRE(!g.addNode(x));  // duplicate

    REQUIRE(g.getNumNodes() == 2);
    REQUIRE(g.containsNode(x));
    REQUIRE(g.getNode("X") != nullptr);
    REQUIRE(g.getNode("Z") == nullptr);
}

TEST_CASE("Graph add and query edges", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    auto z = std::make_shared<Node>("Z");
    g.addNode(x);
    g.addNode(y);
    g.addNode(z);

    g.addDirectedEdge(x, y);
    g.addUndirectedEdge(y, z);

    REQUIRE(g.getNumEdges() == 2);
    REQUIRE(g.isAdjacentTo(x, y));
    REQUIRE(g.isAdjacentTo(y, z));
    REQUIRE(!g.isAdjacentTo(x, z));
}

TEST_CASE("Graph directed queries", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    g.addNode(x);
    g.addNode(y);

    g.addDirectedEdge(x, y);

    REQUIRE(g.isParentOf(x, y));
    REQUIRE(g.isChildOf(y, x));
    REQUIRE(!g.isParentOf(y, x));
    REQUIRE(g.isDirectedFromTo(x, y));
    REQUIRE(!g.isDirectedFromTo(y, x));

    auto parents = g.getParents(y);
    REQUIRE(parents.size() == 1);
    REQUIRE(*parents[0] == *x);

    auto children = g.getChildren(x);
    REQUIRE(children.size() == 1);
    REQUIRE(*children[0] == *y);
}

TEST_CASE("Graph adjacency", "[graph]") {
    Graph g;
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");
    g.addNode(a);
    g.addNode(b);
    g.addNode(c);
    g.addUndirectedEdge(a, b);
    g.addUndirectedEdge(b, c);

    auto adj = g.getAdjacentNodes(b);
    REQUIRE(adj.size() == 2);
}

TEST_CASE("Graph remove edge", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    g.addNode(x);
    g.addNode(y);
    g.addUndirectedEdge(x, y);

    REQUIRE(g.getNumEdges() == 1);
    g.removeEdge(x, y);
    REQUIRE(g.getNumEdges() == 0);
    REQUIRE(!g.isAdjacentTo(x, y));
}

TEST_CASE("Graph collider detection", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    auto z = std::make_shared<Node>("Z");
    g.addNode(x);
    g.addNode(y);
    g.addNode(z);

    g.addDirectedEdge(x, y);
    g.addDirectedEdge(z, y);

    REQUIRE(g.isDefCollider(x, y, z));
    REQUIRE(!g.isDefNoncollider(x, y, z));
}

TEST_CASE("Graph directed path", "[graph]") {
    Graph g;
    auto a = std::make_shared<Node>("A");
    auto b = std::make_shared<Node>("B");
    auto c = std::make_shared<Node>("C");
    g.addNode(a);
    g.addNode(b);
    g.addNode(c);

    g.addDirectedEdge(a, b);
    g.addDirectedEdge(b, c);

    REQUIRE(g.existsDirectedPath(a, c));
    REQUIRE(!g.existsDirectedPath(c, a));
}

TEST_CASE("Graph setEndpoint", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    g.addNode(x);
    g.addNode(y);
    g.addUndirectedEdge(x, y);

    // Orient: change endpoint at y to ARROW
    g.setEndpoint(x, y, Endpoint::ARROW);

    REQUIRE(g.isDirectedFromTo(x, y));
}

TEST_CASE("Graph reorientAllWith", "[graph]") {
    Graph g;
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    g.addNode(x);
    g.addNode(y);
    g.addDirectedEdge(x, y);

    g.reorientAllWith(Endpoint::TAIL);

    Edge e = g.getEdge(x, y);
    REQUIRE(!e.isNull());
    REQUIRE(e.getEndpoint1() == Endpoint::TAIL);
    REQUIRE(e.getEndpoint2() == Endpoint::TAIL);
}
