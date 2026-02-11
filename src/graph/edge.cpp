#include "graph/edge.h"
#include <stdexcept>

namespace tetrad {

Edge::Edge(NodePtr node1, NodePtr node2, Endpoint ep1, Endpoint ep2) {
    if (!node1 || !node2) {
        throw std::invalid_argument("Nodes must not be null");
    }

    // Normalize: flip if pointing left (arrow on ep1, tail/circle on ep2)
    if (pointingLeft(ep1, ep2)) {
        node1_ = std::move(node2);
        node2_ = std::move(node1);
        endpoint1_ = ep2;
        endpoint2_ = ep1;
    } else {
        node1_ = std::move(node1);
        node2_ = std::move(node2);
        endpoint1_ = ep1;
        endpoint2_ = ep2;
    }
}

Endpoint Edge::getEndpoint(const NodePtr& node) const {
    if (*node1_ == *node) return endpoint1_;
    if (*node2_ == *node) return endpoint2_;
    return Endpoint::NULL_EP;
}

Endpoint Edge::getDistalEndpoint(const NodePtr& node) const {
    if (*node1_ == *node) return endpoint2_;
    if (*node2_ == *node) return endpoint1_;
    return Endpoint::NULL_EP;
}

NodePtr Edge::getDistalNode(const NodePtr& node) const {
    if (*node1_ == *node) return node2_;
    if (*node2_ == *node) return node1_;
    return nullptr;
}

bool Edge::isDirected() const {
    return (endpoint1_ == Endpoint::TAIL && endpoint2_ == Endpoint::ARROW) ||
           (endpoint1_ == Endpoint::ARROW && endpoint2_ == Endpoint::TAIL);
}

bool Edge::pointsTowards(const NodePtr& node) const {
    Endpoint proximal = getEndpoint(node);
    Endpoint distal = getDistalEndpoint(node);
    return proximal == Endpoint::ARROW &&
           (distal == Endpoint::TAIL || distal == Endpoint::CIRCLE);
}

bool Edge::isNull() const {
    return endpoint1_ == Endpoint::NULL_EP && endpoint2_ == Endpoint::NULL_EP;
}

Edge Edge::reverse() const {
    return Edge(node2_, node1_, endpoint1_, endpoint2_);
}

bool Edge::operator==(const Edge& other) const {
    bool eq1 = (*node1_ == *other.node1_) && (*node2_ == *other.node2_) &&
               endpoint1_ == other.endpoint1_ && endpoint2_ == other.endpoint2_;
    bool eq2 = (*node1_ == *other.node2_) && (*node2_ == *other.node1_) &&
               endpoint1_ == other.endpoint2_ && endpoint2_ == other.endpoint1_;
    return eq1 || eq2;
}

bool Edge::operator<(const Edge& other) const {
    int cmp1 = node1_->getName().compare(other.node1_->getName());
    if (cmp1 != 0) return cmp1 < 0;
    return node2_->getName().compare(other.node2_->getName()) < 0;
}

std::string Edge::toString() const {
    std::string result = node1_->getName() + " ";

    if (isNull()) {
        result += "...";
    } else {
        if (endpoint1_ == Endpoint::TAIL) result += "-";
        else if (endpoint1_ == Endpoint::ARROW) result += "<";
        else if (endpoint1_ == Endpoint::CIRCLE) result += "o";

        result += "-";

        if (endpoint2_ == Endpoint::TAIL) result += "-";
        else if (endpoint2_ == Endpoint::ARROW) result += ">";
        else if (endpoint2_ == Endpoint::CIRCLE) result += "o";
    }

    result += " " + node2_->getName();
    return result;
}

bool Edge::pointingLeft(Endpoint ep1, Endpoint ep2) {
    return ep1 == Endpoint::ARROW &&
           (ep2 == Endpoint::TAIL || ep2 == Endpoint::CIRCLE);
}

Edge directedEdge(const NodePtr& from, const NodePtr& to) {
    return Edge(from, to, Endpoint::TAIL, Endpoint::ARROW);
}

Edge undirectedEdge(const NodePtr& n1, const NodePtr& n2) {
    return Edge(n1, n2, Endpoint::TAIL, Endpoint::TAIL);
}

Edge bidirectedEdge(const NodePtr& n1, const NodePtr& n2) {
    return Edge(n1, n2, Endpoint::ARROW, Endpoint::ARROW);
}

} // namespace tetrad
