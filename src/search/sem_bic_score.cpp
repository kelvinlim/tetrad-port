#include "search/sem_bic_score.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace tetrad {

SemBicScore::SemBicScore(const DataSet& dataSet)
    : covMatrix_(dataSet.getCovarianceMatrix()),
      variables_(dataSet.getVariables()),
      sampleSize_(dataSet.getNumRows()),
      logN_(std::log(static_cast<double>(sampleSize_))) {}

double SemBicScore::localScore(int node, const std::vector<int>& parents) const {
    int k = static_cast<int>(parents.size());

    double lik = getLikelihood(node, parents);
    if (std::isnan(lik)) return std::numeric_limits<double>::quiet_NaN();

    double c = penaltyDiscount_;
    double score = 2.0 * lik - c * k * logN_ - getStructurePrior(k);

    if (std::isnan(score) || std::isinf(score)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return score;
}

double SemBicScore::localScoreDiff(int x, int y, const std::vector<int>& z) const {
    std::vector<int> zx = z;
    zx.push_back(x);
    return localScore(y, zx) - localScore(y, z);
}

double SemBicScore::getLikelihood(int i, const std::vector<int>& parents) const {
    double sigmaSquared = getResidualVariance(i, parents);
    if (sigmaSquared <= 0 || std::isnan(sigmaSquared)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return -0.5 * sampleSize_ * (std::log(2.0 * M_PI * sigmaSquared) + 1.0);
}

double SemBicScore::getResidualVariance(int i, const std::vector<int>& parents) const {
    if (parents.empty()) {
        // No parents: residual variance is just the variance of the variable
        return covMatrix_(i, i);
    }

    int k = static_cast<int>(parents.size());

    // Build sorted parent indices
    std::vector<int> sortedParents = parents;
    std::sort(sortedParents.begin(), sortedParents.end());

    // Extract submatrices from the covariance matrix:
    // covxx = cov(parents, parents)
    // covxy = cov(parents, node)
    Eigen::MatrixXd covxx(k, k);
    Eigen::VectorXd covxy(k);

    for (int r = 0; r < k; r++) {
        covxy(r) = covMatrix_(sortedParents[r], i);
        for (int c = 0; c < k; c++) {
            covxx(r, c) = covMatrix_(sortedParents[r], sortedParents[c]);
        }
    }

    // Regression coefficients: b = covxx^{-1} * covxy
    // Use LDLT for numerical stability (covariance matrices are positive semi-definite)
    Eigen::LDLT<Eigen::MatrixXd> ldlt(covxx);
    if (ldlt.info() != Eigen::Success) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    Eigen::VectorXd b = ldlt.solve(covxy);

    // Residual variance: var(i) - b' * covxy
    // Equivalent to: bStar' * fullCov * bStar where bStar = [1, -b1, -b2, ...]
    double residualVar = covMatrix_(i, i) - b.dot(covxy);

    return std::max(residualVar, 1e-20);  // floor to avoid log(0)
}

double SemBicScore::getStructurePrior(int numParents) const {
    if (std::abs(structurePrior_) <= 0) {
        return 0.0;
    }
    int numVars = static_cast<int>(variables_.size());
    double p = structurePrior_ / numVars;
    return -(numParents * std::log(p) + (numVars - numParents) * std::log(1.0 - p));
}

} // namespace tetrad
