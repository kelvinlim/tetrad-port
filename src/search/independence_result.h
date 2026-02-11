#pragma once

namespace tetrad {

struct IndependenceResult {
    bool independent;
    double pValue;
    double score;  // alpha - pValue

    bool isIndependent() const { return independent; }
    bool isDependent() const { return !independent; }
};

} // namespace tetrad
