#pragma once

#include <vector>

namespace tetrad {

class ChoiceGenerator {
public:
    ChoiceGenerator(int a, int b);

    // Returns pointer to next combination, or nullptr if exhausted.
    // The returned pointer is to an internal buffer - do not free.
    const int* next();

    int getA() const { return a_; }
    int getB() const { return b_; }

    static int getNumCombinations(int a, int b);

private:
    void fill(int index);

    int a_;
    int b_;
    int diff_;
    std::vector<int> choiceLocal_;
    std::vector<int> choiceReturned_;
    bool begun_;
    static int emptyChoice_;
};

} // namespace tetrad
