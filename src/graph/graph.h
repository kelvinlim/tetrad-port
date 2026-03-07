#pragma once

#include "graph/node.h"
#include "graph/edge.h"
#include "graph/endpoint.h"
#include "graph/triple.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <set>

namespace tetrad {

class Graph {
public:
    Graph();
    explicit Graph(const std::vector<NodePtr>& nodes);
    Graph(const Graph& other);
    Graph& operator=(const Graph& other);

    // Node operations
    bool addNode(const NodePtr& node);
    bool removeNode(const NodePtr& node);
    bool containsNode(const NodePtr& node) const;
    NodePtr getNode(const std::string& name) const;
    const std::vector<NodePtr>& getNodes() const { return nodes_; }
    int getNumNodes() const { return static_cast<int>(nodes_.size()); }
    std::vector<std::string> getNodeNames() const;

    // Edge operations
    bool addEdge(const Edge& edge);
    bool removeEdge(const Edge& edge);
    bool removeEdge(const NodePtr& n1, const NodePtr& n2);
    bool removeEdges(const NodePtr& n1, const NodePtr& n2);
    Edge getEdge(const NodePtr& n1, const NodePtr& n2) const;
    std::vector<Edge> getEdges() const;
    std::vector<Edge> getEdges(const NodePtr& node) const;
    int getNumEdges() const { return static_cast<int>(edgesSet_.size()); }
    bool containsEdge(const Edge& edge) const;

    // Convenience edge addition
    bool addDirectedEdge(const NodePtr& from, const NodePtr& to);
    bool addUndirectedEdge(const NodePtr& n1, const NodePtr& n2);
    bool addBidirectedEdge(const NodePtr& n1, const NodePtr& n2);

    // Adjacency
    bool isAdjacentTo(const NodePtr& n1, const NodePtr& n2) const;
    std::vector<NodePtr> getAdjacentNodes(const NodePtr& node) const;

    // Endpoint manipulation
    Endpoint getEndpoint(const NodePtr& from, const NodePtr& to) const;
    bool setEndpoint(const NodePtr& from, const NodePtr& to, Endpoint ep);

    // Directed graph queries
    std::vector<NodePtr> getChildren(const NodePtr& node) const;
    std::vector<NodePtr> getParents(const NodePtr& node) const;
    bool isParentOf(const NodePtr& n1, const NodePtr& n2) const;
    bool isChildOf(const NodePtr& n1, const NodePtr& n2) const;
    bool isDirectedFromTo(const NodePtr& from, const NodePtr& to) const;
    int getIndegree(const NodePtr& node) const;
    int getOutdegree(const NodePtr& node) const;
    int getDegree(const NodePtr& node) const;

    // Returns nodes adjacent to 'node' where the edge endpoint at 'node' equals 'endpoint'.
    // E.g., getNodesInTo(X, ARROW) returns all Y such that Y *-> X.
    std::vector<NodePtr> getNodesInTo(const NodePtr& node, Endpoint endpoint) const;

    // Collider detection
    bool isDefCollider(const NodePtr& n1, const NodePtr& n2, const NodePtr& n3) const;
    bool isDefNoncollider(const NodePtr& n1, const NodePtr& n2, const NodePtr& n3) const;

    // Path queries (simple BFS implementations for cycle detection)
    bool existsDirectedPath(const NodePtr& from, const NodePtr& to) const;
    bool existsSemiDirectedPath(const NodePtr& from, const NodePtr& to) const;

    // Structural operations
    void reorientAllWith(Endpoint ep);
    void clear();

    // Ambiguous triple tracking
    const std::unordered_set<Triple>& getAmbiguousTriples() const { return ambiguousTriples_; }
    void setAmbiguousTriples(const std::unordered_set<Triple>& triples) { ambiguousTriples_ = triples; }
    bool isAmbiguousTriple(const NodePtr& x, const NodePtr& y, const NodePtr& z) const;

    std::string toString() const;

private:
    NodePtr findNode(const NodePtr& node) const;

    std::vector<NodePtr> nodes_;
    std::unordered_set<Edge> edgesSet_;
    std::unordered_map<std::string, NodePtr> namesHash_;
    std::unordered_map<std::string, std::vector<Edge>> edgeLists_;
    std::unordered_set<Triple> ambiguousTriples_;
};

} // namespace tetrad
