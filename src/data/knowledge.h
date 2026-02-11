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
    bool isEmpty() const { return true; }
};

} // namespace tetrad
