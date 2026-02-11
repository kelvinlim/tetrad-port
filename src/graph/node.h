#pragma once

#include "node_type.h"
#include <memory>
#include <string>
#include <functional>

namespace tetrad {

class Node {
public:
    explicit Node(std::string name, NodeType type = NodeType::MEASURED);

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    NodeType getNodeType() const { return type_; }
    void setNodeType(NodeType type) { type_ = type; }

    std::shared_ptr<Node> like(const std::string& name) const;

    bool operator==(const Node& other) const { return name_ == other.name_; }
    bool operator!=(const Node& other) const { return !(*this == other); }
    bool operator<(const Node& other) const { return name_ < other.name_; }

    std::string toString() const;

private:
    std::string name_;
    NodeType type_;
};

using NodePtr = std::shared_ptr<Node>;

} // namespace tetrad

namespace std {
template <>
struct hash<tetrad::Node> {
    size_t operator()(const tetrad::Node& n) const {
        return hash<string>()(n.getName());
    }
};

template <>
struct hash<shared_ptr<tetrad::Node>> {
    size_t operator()(const shared_ptr<tetrad::Node>& n) const {
        if (!n) return 0;
        return hash<string>()(n->getName());
    }
};

template <>
struct equal_to<shared_ptr<tetrad::Node>> {
    bool operator()(const shared_ptr<tetrad::Node>& a,
                    const shared_ptr<tetrad::Node>& b) const {
        if (!a && !b) return true;
        if (!a || !b) return false;
        return *a == *b;
    }
};
} // namespace std
