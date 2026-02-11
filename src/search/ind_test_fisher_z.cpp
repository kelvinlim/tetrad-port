#include "search/ind_test_fisher_z.h"
#include <cmath>
#include <stdexcept>
#include <Eigen/Cholesky>

namespace tetrad {

IndTestFisherZ::IndTestFisherZ(const DataSet& dataSet, double alpha)
    : sampleSize_(dataSet.getNumRows()), alpha_(alpha) {

    correlationMatrix_ = dataSet.getCorrelationMatrix();
    variables_ = dataSet.getVariables();

    for (size_t i = 0; i < variables_.size(); ++i) {
        indexMap_[variables_[i]->getName()] = static_cast<int>(i);
    }
}

IndependenceResult IndTestFisherZ::checkIndependence(
    const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z) {

    double p = getPValue(x, y, z);

    if (std::isnan(p)) {
        throw std::runtime_error("Undefined p-value for independence test");
    }

    bool independent = p > alpha_;
    return IndependenceResult{independent, p, alpha_ - p};
}

double IndTestFisherZ::getPValue(
    const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z) {

    double r = partialCorrelation(x, y, z);
    int n = sampleSize_;

    double absR = std::abs(r);
    // Clamp to avoid log(0)
    if (absR >= 1.0) absR = 1.0 - 1e-15;

    double q = 0.5 * (std::log(1.0 + absR) - std::log(1.0 - absR));
    double df = static_cast<double>(n) - 3.0 - static_cast<double>(z.size());

    if (df < 1.0) {
        throw std::invalid_argument("Nonpositive degrees of freedom: df=" + std::to_string(df));
    }

    double fisherZ = std::sqrt(df) * q;
    return 2.0 * (1.0 - normalCdf(fisherZ));
}

double IndTestFisherZ::partialCorrelation(
    const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z) {

    int dim = static_cast<int>(z.size()) + 2;

    // Build index array: [x, y, z0, z1, ...]
    std::vector<int> indices;
    indices.reserve(dim);
    indices.push_back(indexMap_.at(x->getName()));
    indices.push_back(indexMap_.at(y->getName()));
    for (const auto& node : z) {
        indices.push_back(indexMap_.at(node->getName()));
    }

    // Extract correlation submatrix
    Eigen::MatrixXd corSub(dim, dim);
    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            corSub(i, j) = correlationMatrix_(indices[i], indices[j]);
        }
    }

    // Compute precision matrix (inverse of correlation submatrix) via Cholesky
    Eigen::LLT<Eigen::MatrixXd> llt(corSub);
    if (llt.info() != Eigen::Success) {
        // Fallback: use full pivot LU decomposition
        Eigen::FullPivLU<Eigen::MatrixXd> lu(corSub);
        if (!lu.isInvertible()) {
            throw std::runtime_error("Singular correlation submatrix");
        }
        Eigen::MatrixXd P = lu.inverse();
        return partialFromPrecision(P);
    }

    Eigen::MatrixXd P = llt.solve(Eigen::MatrixXd::Identity(dim, dim));
    return partialFromPrecision(P);
}

double IndTestFisherZ::partialFromPrecision(const Eigen::MatrixXd& P) {
    double w11 = P(0, 0);
    double w22 = P(1, 1);
    double w12 = P(0, 1);
    if (w11 <= 0 || w22 <= 0) {
        throw std::runtime_error("Nonpositive diagonal in precision matrix");
    }
    return -w12 / std::sqrt(w11 * w22);
}

double IndTestFisherZ::normalCdf(double x) {
    // Standard normal CDF using erfc
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

} // namespace tetrad
