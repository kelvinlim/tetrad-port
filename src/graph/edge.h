#pragma once

#include "graph/endpoint.h"
#include "graph/node.h"
#include <memory>
#include <string>

namespace tetrad {

class Edge {
public:
    Edge(NodePtr node1, NodePtr node2, Endpoint ep1, Endpoint ep2);

    const NodePtr& getNode1() const { return node1_; }
    const NodePtr& getNode2() const { return node2_; }

    Endpoint getEndpoint1() const { return endpoint1_; }
    Endpoint getEndpoint2() const { return endpoint2_; }

    void setEndpoint1(Endpoint ep) { endpoint1_ = ep; }
    void setEndpoint2(Endpoint ep) { endpoint2_ = ep; }

    Endpoint getEndpoint(const NodePtr& node) const;
    Endpoint getDistalEndpoint(const NodePtr& node) const;
    NodePtr getDistalNode(const NodePtr& node) const;

    bool isDirected() const;
    bool pointsTowards(const NodePtr& node) const;
    bool isNull() const;

    Edge reverse() const;

    bool operator==(const Edge& other) const;
    bool operator!=(const Edge& other) const { return !(*this == other); }
    bool operator<(const Edge& other) const;

    std::string toString() const;

private:
    static bool pointingLeft(Endpoint ep1, Endpoint ep2);

    NodePtr node1_;
    NodePtr node2_;
    Endpoint endpoint1_;
    Endpoint endpoint2_;
};

// Factory functions matching Java's Edges utility class
Edge directedEdge(const NodePtr& from, const NodePtr& to);
Edge undirectedEdge(const NodePtr& n1, const NodePtr& n2);
Edge bidirectedEdge(const NodePtr& n1, const NodePtr& n2);

} // namespace tetrad

namespace std {
template <>
struct hash<tetrad::Edge> {
    size_t operator()(const tetrad::Edge& e) const {
        // Symmetric hash: same for A-B and B-A
        auto h1 = hash<string>()(e.getNode1()->getName());
        auto h2 = hash<string>()(e.getNode2()->getName());
        return h1 + h2;
    }
};
} // namespace std
