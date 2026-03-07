#pragma once

#include <string>

namespace tetrad {

// Stub implementation: always permits everything.
// Full implementation deferred until background knowledge is needed.
class Knowledge {
public:
    Knowledge() = default;

    bool isForbidden(const std::string& /*from*/, const std::string& /*to*/) const { return false; }
    bool isRequired(const std::string& /*from*/, const std::string& /*to*/) const { return false; }
    bool noEdgeRequired(const std::string& x, const std::string& y) const {
        return !(isRequired(x, y) || isRequired(y, x));
    }
    bool isEmpty() const { return true; }
};

} // namespace tetrad
