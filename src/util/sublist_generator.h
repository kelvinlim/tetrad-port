#pragma once

#include "util/choice_generator.h"
#include <vector>

namespace tetrad {

// Generates all subsets of {0, 1, ..., n-1} with size from 0 to depth.
// Port of edu.cmu.tetrad.util.SublistGenerator from Java Tetrad 7.6.8.
class SublistGenerator {
public:
    SublistGenerator(int n, int depth)
        : n_(n), depth_(depth < 0 ? n : (depth > n ? n : depth)),
          currentSize_(0), choiceGen_(nullptr) {}

    // Returns pointer to next subset (as sorted indices), or nullptr if exhausted.
    // Returned pointer is to internal buffer.
    const std::vector<int>& next(bool& valid) {
        valid = false;
        while (currentSize_ <= depth_) {
            if (!choiceGen_) {
                if (currentSize_ == 0) {
                    // Empty set
                    result_.clear();
                    currentSize_++;
                    valid = true;
                    return result_;
                }
                choiceGen_ = std::make_unique<ChoiceGenerator>(n_, currentSize_);
            }

            const int* choice = choiceGen_->next();
            if (choice) {
                result_.resize(currentSize_);
                for (int i = 0; i < currentSize_; i++) {
                    result_[i] = choice[i];
                }
                valid = true;
                return result_;
            }

            // Exhausted this size, move to next
            choiceGen_.reset();
            currentSize_++;
        }

        return result_;
    }

private:
    int n_;
    int depth_;
    int currentSize_;
    std::unique_ptr<ChoiceGenerator> choiceGen_;
    std::vector<int> result_;
};

} // namespace tetrad
