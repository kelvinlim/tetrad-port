#include "graph/node.h"

namespace tetrad {

Node::Node(std::string name, NodeType type)
    : name_(std::move(name)), type_(type) {}

std::shared_ptr<Node> Node::like(const std::string& name) const {
    return std::make_shared<Node>(name, type_);
}

std::string Node::toString() const {
    return name_;
}

} // namespace tetrad
