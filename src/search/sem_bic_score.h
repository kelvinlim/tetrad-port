#pragma once

#include "search/score.h"
#include "data/data_set.h"
#include <Eigen/Dense>

namespace tetrad {

// Linear, Gaussian BIC score with penalty discount.
// BIC = 2L - c * k * ln(n), where L is log-likelihood, c is penalty discount,
// k is number of parents, n is sample size.
//
// Port of edu.cmu.tetrad.search.score.SemBicScore from Java Tetrad 7.6.8.
// Simplified: only works from precomputed covariance matrix (no missing data handling).
class SemBicScore : public Score {
public:
    explicit SemBicScore(const DataSet& dataSet);

    using Score::localScore;  // bring in convenience overloads
    double localScore(int node, const std::vector<int>& parents) const override;
    double localScoreDiff(int x, int y, const std::vector<int>& z) const override;

    const std::vector<NodePtr>& getVariables() const override { return variables_; }
    int getSampleSize() const override { return sampleSize_; }

    void setPenaltyDiscount(double pd) { penaltyDiscount_ = pd; }
    double getPenaltyDiscount() const { return penaltyDiscount_; }

    void setStructurePrior(double sp) { structurePrior_ = sp; }
    double getStructurePrior() const { return structurePrior_; }

private:
    double getResidualVariance(int i, const std::vector<int>& parents) const;
    double getLikelihood(int i, const std::vector<int>& parents) const;
    double getStructurePrior(int numParents) const;

    Eigen::MatrixXd covMatrix_;
    std::vector<NodePtr> variables_;
    int sampleSize_;
    double logN_;
    double penaltyDiscount_ = 1.0;
    double structurePrior_ = 0.0;
};

} // namespace tetrad
