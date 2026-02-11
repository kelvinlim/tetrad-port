#include "util/choice_generator.h"
#include <cmath>
#include <stdexcept>

namespace tetrad {

int ChoiceGenerator::emptyChoice_ = 0;

ChoiceGenerator::ChoiceGenerator(int a, int b)
    : a_(a), b_(b), diff_(a - b),
      choiceLocal_(b), choiceReturned_(b), begun_(false) {

    if (a < 0 || b < 0) {
        throw std::invalid_argument("a and b must be non-negative");
    }

    // Initialize: [0, 1, 2, ..., b-2]  (last element is b-2, not b-1)
    // This ensures first call to next() returns [0, 1, ..., b-1]
    for (int i = 0; i < b - 1; i++) {
        choiceLocal_[i] = i;
    }
    if (b > 0) {
        choiceLocal_[b - 1] = b - 2;
    }
}

const int* ChoiceGenerator::next() {
    if (a_ < b_) return nullptr;

    int i = b_;

    // Scan from right for first index whose value < its maximum (i + diff_)
    while (--i > -1) {
        if (choiceLocal_[i] < i + diff_) {
            fill(i);
            begun_ = true;
            std::copy(choiceLocal_.begin(), choiceLocal_.end(), choiceReturned_.begin());
            return choiceReturned_.data();
        }
    }

    if (begun_) {
        return nullptr;
    } else {
        begun_ = true;
        if (b_ == 0) {
            // Return non-null sentinel for the empty set
            return &emptyChoice_;
        }
        std::copy(choiceLocal_.begin(), choiceLocal_.end(), choiceReturned_.begin());
        return choiceReturned_.data();
    }
}

void ChoiceGenerator::fill(int index) {
    choiceLocal_[index]++;
    for (int i = index + 1; i < b_; i++) {
        choiceLocal_[i] = choiceLocal_[i - 1] + 1;
    }
}

int ChoiceGenerator::getNumCombinations(int a, int b) {
    if (b < 0 || b > a) return 0;
    if (b == 0 || b == a) return 1;
    // Use the smaller of b and a-b for efficiency
    if (b > a - b) b = a - b;
    double result = 1.0;
    for (int i = 0; i < b; i++) {
        result *= (a - i);
        result /= (i + 1);
    }
    return static_cast<int>(std::round(result));
}

} // namespace tetrad
