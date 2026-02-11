#include "graph/graph.h"
#include <algorithm>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace tetrad {

Graph::Graph() = default;

Graph::Graph(const std::vector<NodePtr>& nodes) {
    for (const auto& node : nodes) {
        addNode(node);
    }
}

Graph::Graph(const Graph& other)
    : nodes_(other.nodes_),
      edgesSet_(other.edgesSet_),
      namesHash_(other.namesHash_),
      edgeLists_(other.edgeLists_),
      ambiguousTriples_(other.ambiguousTriples_) {}

Graph& Graph::operator=(const Graph& other) {
    if (this != &other) {
        nodes_ = other.nodes_;
        edgesSet_ = other.edgesSet_;
        namesHash_ = other.namesHash_;
        edgeLists_ = other.edgeLists_;
        ambiguousTriples_ = other.ambiguousTriples_;
    }
    return *this;
}

// Node operations

bool Graph::addNode(const NodePtr& node) {
    if (!node) return false;
    if (namesHash_.count(node->getName())) return false;
    nodes_.push_back(node);
    namesHash_[node->getName()] = node;
    edgeLists_[node->getName()] = {};
    return true;
}

bool Graph::removeNode(const NodePtr& node) {
    if (!node) return false;
    auto it = namesHash_.find(node->getName());
    if (it == namesHash_.end()) return false;

    // Remove all edges incident to this node
    auto edges = getEdges(node);
    for (const auto& edge : edges) {
        removeEdge(edge);
    }

    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(),
        [&](const NodePtr& n) { return *n == *node; }), nodes_.end());
    namesHash_.erase(it);
    edgeLists_.erase(node->getName());
    return true;
}

bool Graph::containsNode(const NodePtr& node) const {
    if (!node) return false;
    return namesHash_.count(node->getName()) > 0;
}

NodePtr Graph::getNode(const std::string& name) const {
    auto it = namesHash_.find(name);
    if (it != namesHash_.end()) return it->second;
    return nullptr;
}

std::vector<std::string> Graph::getNodeNames() const {
    std::vector<std::string> names;
    names.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        names.push_back(node->getName());
    }
    return names;
}

// Edge operations

bool Graph::addEdge(const Edge& edge) {
    if (edgesSet_.count(edge)) return false;

    const auto& n1 = edge.getNode1();
    const auto& n2 = edge.getNode2();

    if (!containsNode(n1) || !containsNode(n2)) return false;

    edgesSet_.insert(edge);
    edgeLists_[n1->getName()].push_back(edge);
    edgeLists_[n2->getName()].push_back(edge);
    return true;
}

bool Graph::removeEdge(const Edge& edge) {
    auto it = edgesSet_.find(edge);
    if (it == edgesSet_.end()) return false;

    edgesSet_.erase(it);

    auto removeFrom = [&](const std::string& name) {
        auto& list = edgeLists_[name];
        list.erase(std::remove(list.begin(), list.end(), edge), list.end());
    };

    removeFrom(edge.getNode1()->getName());
    removeFrom(edge.getNode2()->getName());
    return true;
}

bool Graph::removeEdge(const NodePtr& n1, const NodePtr& n2) {
    Edge edge = getEdge(n1, n2);
    if (edge.isNull()) return false;
    return removeEdge(edge);
}

bool Graph::removeEdges(const NodePtr& n1, const NodePtr& n2) {
    auto edgesCopy = getEdges();
    bool removed = false;
    for (const auto& edge : edgesCopy) {
        if ((*edge.getNode1() == *n1 && *edge.getNode2() == *n2) ||
            (*edge.getNode1() == *n2 && *edge.getNode2() == *n1)) {
            removeEdge(edge);
            removed = true;
        }
    }
    return removed;
}

Edge Graph::getEdge(const NodePtr& n1, const NodePtr& n2) const {
    if (!n1 || !n2) return Edge(std::make_shared<Node>("_null1"), std::make_shared<Node>("_null2"), Endpoint::NULL_EP, Endpoint::NULL_EP);

    auto it = edgeLists_.find(n1->getName());
    if (it == edgeLists_.end()) return Edge(std::make_shared<Node>("_null1"), std::make_shared<Node>("_null2"), Endpoint::NULL_EP, Endpoint::NULL_EP);

    for (const auto& edge : it->second) {
        auto distal = edge.getDistalNode(n1);
        if (distal && *distal == *n2) {
            return edge;
        }
    }

    return Edge(std::make_shared<Node>("_null1"), std::make_shared<Node>("_null2"), Endpoint::NULL_EP, Endpoint::NULL_EP);
}

std::vector<Edge> Graph::getEdges() const {
    return std::vector<Edge>(edgesSet_.begin(), edgesSet_.end());
}

std::vector<Edge> Graph::getEdges(const NodePtr& node) const {
    if (!node) return {};
    auto it = edgeLists_.find(node->getName());
    if (it == edgeLists_.end()) return {};
    return it->second;
}

bool Graph::containsEdge(const Edge& edge) const {
    return edgesSet_.count(edge) > 0;
}

// Convenience edge addition

bool Graph::addDirectedEdge(const NodePtr& from, const NodePtr& to) {
    return addEdge(directedEdge(from, to));
}

bool Graph::addUndirectedEdge(const NodePtr& n1, const NodePtr& n2) {
    return addEdge(undirectedEdge(n1, n2));
}

bool Graph::addBidirectedEdge(const NodePtr& n1, const NodePtr& n2) {
    return addEdge(bidirectedEdge(n1, n2));
}

// Adjacency

bool Graph::isAdjacentTo(const NodePtr& n1, const NodePtr& n2) const {
    if (!n1 || !n2) return false;
    auto it = edgeLists_.find(n1->getName());
    if (it == edgeLists_.end()) return false;

    for (const auto& edge : it->second) {
        auto distal = edge.getDistalNode(n1);
        if (distal && *distal == *n2) return true;
    }
    return false;
}

std::vector<NodePtr> Graph::getAdjacentNodes(const NodePtr& node) const {
    std::vector<NodePtr> adj;
    if (!node) return adj;

    auto it = edgeLists_.find(node->getName());
    if (it == edgeLists_.end()) return adj;

    std::unordered_set<std::string> seen;
    for (const auto& edge : it->second) {
        auto distal = edge.getDistalNode(node);
        if (distal && seen.insert(distal->getName()).second) {
            adj.push_back(distal);
        }
    }
    return adj;
}

// Endpoint manipulation

Endpoint Graph::getEndpoint(const NodePtr& from, const NodePtr& to) const {
    Edge edge = getEdge(from, to);
    if (edge.isNull()) return Endpoint::NULL_EP;
    return edge.getEndpoint(to);
}

bool Graph::setEndpoint(const NodePtr& from, const NodePtr& to, Endpoint ep) {
    Edge oldEdge = getEdge(from, to);
    if (oldEdge.isNull()) return false;

    removeEdge(oldEdge);

    // Determine new endpoints: keep the endpoint at 'from', change endpoint at 'to'
    Endpoint epAtFrom = oldEdge.getEndpoint(from);
    Edge newEdge(from, to, epAtFrom, ep);
    addEdge(newEdge);
    return true;
}

// Directed graph queries

std::vector<NodePtr> Graph::getChildren(const NodePtr& node) const {
    std::vector<NodePtr> children;
    auto edges = getEdges(node);
    for (const auto& edge : edges) {
        if (edge.pointsTowards(edge.getDistalNode(node))) {
            auto child = edge.getDistalNode(node);
            if (child) children.push_back(child);
        }
    }
    return children;
}

std::vector<NodePtr> Graph::getParents(const NodePtr& node) const {
    std::vector<NodePtr> parents;
    auto edges = getEdges(node);
    for (const auto& edge : edges) {
        if (edge.pointsTowards(node)) {
            auto parent = edge.getDistalNode(node);
            if (parent) parents.push_back(parent);
        }
    }
    return parents;
}

bool Graph::isParentOf(const NodePtr& n1, const NodePtr& n2) const {
    auto parents = getParents(n2);
    for (const auto& p : parents) {
        if (*p == *n1) return true;
    }
    return false;
}

bool Graph::isChildOf(const NodePtr& n1, const NodePtr& n2) const {
    return isParentOf(n2, n1);
}

bool Graph::isDirectedFromTo(const NodePtr& from, const NodePtr& to) const {
    Edge edge = getEdge(from, to);
    if (edge.isNull()) return false;
    return edge.pointsTowards(to) && edge.getEndpoint(from) == Endpoint::TAIL;
}

int Graph::getIndegree(const NodePtr& node) const {
    return static_cast<int>(getParents(node).size());
}

int Graph::getOutdegree(const NodePtr& node) const {
    return static_cast<int>(getChildren(node).size());
}

int Graph::getDegree(const NodePtr& node) const {
    return static_cast<int>(getAdjacentNodes(node).size());
}

// Collider detection

bool Graph::isDefCollider(const NodePtr& n1, const NodePtr& n2, const NodePtr& n3) const {
    Edge edge1 = getEdge(n1, n2);
    Edge edge2 = getEdge(n2, n3);
    if (edge1.isNull() || edge2.isNull()) return false;
    return edge1.getEndpoint(n2) == Endpoint::ARROW && edge2.getEndpoint(n2) == Endpoint::ARROW;
}

bool Graph::isDefNoncollider(const NodePtr& n1, const NodePtr& n2, const NodePtr& n3) const {
    Edge edge1 = getEdge(n1, n2);
    Edge edge2 = getEdge(n2, n3);
    if (edge1.isNull() || edge2.isNull()) return false;
    return edge1.getEndpoint(n2) != Endpoint::ARROW || edge2.getEndpoint(n2) != Endpoint::ARROW;
}

// Path queries

bool Graph::existsDirectedPath(const NodePtr& from, const NodePtr& to) const {
    if (!from || !to) return false;
    if (*from == *to) return true;

    std::queue<NodePtr> queue;
    std::unordered_set<std::string> visited;

    queue.push(from);
    visited.insert(from->getName());

    while (!queue.empty()) {
        NodePtr current = queue.front();
        queue.pop();

        for (const auto& child : getChildren(current)) {
            if (*child == *to) return true;
            if (visited.insert(child->getName()).second) {
                queue.push(child);
            }
        }
    }
    return false;
}

bool Graph::existsSemiDirectedPath(const NodePtr& from, const NodePtr& to) const {
    if (!from || !to) return false;
    if (*from == *to) return true;

    std::queue<NodePtr> queue;
    std::unordered_set<std::string> visited;

    queue.push(from);
    visited.insert(from->getName());

    while (!queue.empty()) {
        NodePtr current = queue.front();
        queue.pop();

        auto edges = getEdges(current);
        for (const auto& edge : edges) {
            auto distal = edge.getDistalNode(current);
            if (!distal) continue;
            // Follow edge if it doesn't point back towards current
            // (i.e., it's not an arrow into current)
            if (edge.getEndpoint(current) != Endpoint::ARROW) {
                if (*distal == *to) return true;
                if (visited.insert(distal->getName()).second) {
                    queue.push(distal);
                }
            }
        }
    }
    return false;
}

// Structural operations

void Graph::reorientAllWith(Endpoint ep) {
    // Collect all edges, remove them, re-add with new endpoints
    auto edges = getEdges();
    for (const auto& edge : edges) {
        removeEdge(edge);
    }
    for (const auto& edge : edges) {
        Edge newEdge(edge.getNode1(), edge.getNode2(), ep, ep);
        addEdge(newEdge);
    }
}

void Graph::clear() {
    nodes_.clear();
    edgesSet_.clear();
    namesHash_.clear();
    edgeLists_.clear();
    ambiguousTriples_.clear();
}

// Ambiguous triples

bool Graph::isAmbiguousTriple(const NodePtr& x, const NodePtr& y, const NodePtr& z) const {
    Triple t(x, y, z);
    return ambiguousTriples_.count(t) > 0;
}

NodePtr Graph::findNode(const NodePtr& node) const {
    if (!node) return nullptr;
    auto it = namesHash_.find(node->getName());
    if (it != namesHash_.end()) return it->second;
    return nullptr;
}

std::string Graph::toString() const {
    std::ostringstream oss;
    oss << "Graph with " << getNumNodes() << " nodes and " << getNumEdges() << " edges:\n";
    oss << "Nodes: ";
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << nodes_[i]->getName();
    }
    oss << "\nEdges:\n";
    auto edges = getEdges();
    // Sort edges for deterministic output
    std::sort(edges.begin(), edges.end());
    for (const auto& edge : edges) {
        oss << "  " << edge.toString() << "\n";
    }
    return oss.str();
}

} // namespace tetrad
