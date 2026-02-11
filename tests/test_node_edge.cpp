#include <catch2/catch_test_macros.hpp>
#include "graph/node.h"
#include "graph/edge.h"
#include "graph/endpoint.h"

using namespace tetrad;

TEST_CASE("Node creation and equality", "[node]") {
    auto n1 = std::make_shared<Node>("X");
    auto n2 = std::make_shared<Node>("Y");
    auto n3 = std::make_shared<Node>("X");

    REQUIRE(n1->getName() == "X");
    REQUIRE(*n1 == *n3);
    REQUIRE(*n1 != *n2);
}

TEST_CASE("Node type", "[node]") {
    auto n = std::make_shared<Node>("X", NodeType::LATENT);
    REQUIRE(n->getNodeType() == NodeType::LATENT);
    n->setNodeType(NodeType::MEASURED);
    REQUIRE(n->getNodeType() == NodeType::MEASURED);
}

TEST_CASE("Node like factory", "[node]") {
    auto n1 = std::make_shared<Node>("X", NodeType::LATENT);
    auto n2 = n1->like("Y");
    REQUIRE(n2->getName() == "Y");
    REQUIRE(n2->getNodeType() == NodeType::LATENT);
}

TEST_CASE("Edge directed", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    Edge e = directedEdge(x, y);

    REQUIRE(e.isDirected());
    REQUIRE(e.pointsTowards(y));
    REQUIRE(!e.pointsTowards(x));
    REQUIRE(e.getEndpoint1() == Endpoint::TAIL);
    REQUIRE(e.getEndpoint2() == Endpoint::ARROW);
}

TEST_CASE("Edge undirected", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    Edge e = undirectedEdge(x, y);

    REQUIRE(!e.isDirected());
    REQUIRE(!e.pointsTowards(x));
    REQUIRE(!e.pointsTowards(y));
}

TEST_CASE("Edge equality is symmetric", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");

    Edge e1(x, y, Endpoint::TAIL, Endpoint::ARROW);
    Edge e2(y, x, Endpoint::ARROW, Endpoint::TAIL);

    REQUIRE(e1 == e2);
}

TEST_CASE("Edge getDistalNode", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");
    Edge e = directedEdge(x, y);

    REQUIRE(*e.getDistalNode(x) == *y);
    REQUIRE(*e.getDistalNode(y) == *x);
}

TEST_CASE("Edge toString", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");

    REQUIRE(directedEdge(x, y).toString() == "X --> Y");
    REQUIRE(undirectedEdge(x, y).toString() == "X --- Y");
    REQUIRE(bidirectedEdge(x, y).toString() == "X <-> Y");
}

TEST_CASE("Edge normalization flips left-pointing", "[edge]") {
    auto x = std::make_shared<Node>("X");
    auto y = std::make_shared<Node>("Y");

    // Arrow on first endpoint, tail on second = pointing left, should flip
    Edge e(x, y, Endpoint::ARROW, Endpoint::TAIL);

    // After normalization: should be stored as Y --> X  (i.e., node1=Y, node2=X)
    REQUIRE(*e.getNode1() == *y);
    REQUIRE(*e.getNode2() == *x);
    REQUIRE(e.getEndpoint1() == Endpoint::TAIL);
    REQUIRE(e.getEndpoint2() == Endpoint::ARROW);
    REQUIRE(e.pointsTowards(x));
}
