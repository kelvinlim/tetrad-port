#pragma once

#include "search/suborder_search.h"
#include "search/grow_shrink_tree.h"
#include <random>

namespace tetrad {

class BesPermutation;

// Best Order Score Search (BOSS).
// Port of edu.cmu.tetrad.search.Boss from Java Tetrad 7.6.8.
//
// Finds an optimal variable ordering by iteratively moving each variable
// to the position that maximizes the total BIC score. Uses GrowShrinkTree
// for efficient score caching.
//
// Reference: Andrews, Ramsey, Sanchez Romero, Camchong & Kummerfeld (2024),
// "Fast Scalable and Accurate Discovery of DAGs Using the Best Order Score
// Search and Grow Shrink Trees." NeurIPS 2024.
class Boss : public SuborderSearch {
public:
    explicit Boss(Score& score);
    ~Boss();

    void searchSuborder(const std::vector<NodePtr>& prefix,
                        std::vector<NodePtr>& suborder,
                        std::unordered_map<NodePtr, GrowShrinkTree*>& gsts) override;

    void setKnowledge(const Knowledge& knowledge) override;
    const std::vector<NodePtr>& getVariables() const override { return variables_; }
    const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& getParents() const override { return parents_; }
    Score& getScore() override { return score_; }

    void setUseBes(bool use);
    void setNumStarts(int numStarts) { numStarts_ = numStarts; }
    void setResetAfterBM(bool reset) { resetAfterBM_ = reset; }
    void setResetAfterRS(bool reset) { resetAfterRS_ = reset; }
    void setUseDataOrder(bool use) { useDataOrder_ = use; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setSeed(unsigned int seed) { rng_.seed(seed); seeded_ = true; }

private:
    bool betterMutation(const std::vector<NodePtr>& prefix,
                        std::vector<NodePtr>& suborder,
                        const NodePtr& x);

    double update(const std::vector<NodePtr>& prefix,
                  std::vector<NodePtr>& suborder);

    void makeValidKnowledgeOrder(std::vector<NodePtr>& order);

    void bes(const std::vector<NodePtr>& prefix,
             std::vector<NodePtr>& suborder);

    Score& score_;
    std::vector<NodePtr> variables_;
    std::unordered_map<NodePtr, std::unordered_set<NodePtr>> parents_;
    std::unordered_map<NodePtr, GrowShrinkTree*>* gsts_ = nullptr;
    std::unordered_set<NodePtr> all_;

    Knowledge knowledge_;
    std::unique_ptr<BesPermutation> bes_;
    int numStarts_ = 1;
    bool useDataOrder_ = true;
    bool resetAfterBM_ = false;
    bool resetAfterRS_ = true;
    bool verbose_ = false;
    std::mt19937 rng_;
    bool seeded_ = false;
};

} // namespace tetrad
