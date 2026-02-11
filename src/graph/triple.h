#pragma once

#include "graph/node.h"
#include <memory>
#include <functional>

namespace tetrad {

class Triple {
public:
    Triple(NodePtr x, NodePtr y, NodePtr z)
        : x_(std::move(x)), y_(std::move(y)), z_(std::move(z)) {}

    const NodePtr& getX() const { return x_; }
    const NodePtr& getY() const { return y_; }
    const NodePtr& getZ() const { return z_; }

    // Symmetric in x and z: (x,y,z) == (z,y,x)
    bool operator==(const Triple& other) const {
        if (!x_ || !y_ || !z_ || !other.x_ || !other.y_ || !other.z_)
            return false;
        bool fwd = (*x_ == *other.x_) && (*y_ == *other.y_) && (*z_ == *other.z_);
        bool rev = (*x_ == *other.z_) && (*y_ == *other.y_) && (*z_ == *other.x_);
        return fwd || rev;
    }

    bool operator!=(const Triple& other) const { return !(*this == other); }

    std::string toString() const {
        return "<" + x_->getName() + ", " + y_->getName() + ", " + z_->getName() + ">";
    }

private:
    NodePtr x_, y_, z_;
};

} // namespace tetrad

namespace std {
template <>
struct hash<tetrad::Triple> {
    size_t operator()(const tetrad::Triple& t) const {
        // Symmetric in x and z
        auto hx = hash<string>()(t.getX()->getName());
        auto hz = hash<string>()(t.getZ()->getName());
        auto hy = hash<string>()(t.getY()->getName());
        return (hx + hz) * 19 + hy * 23;
    }
};
} // namespace std
