#pragma once

#include "search/independence_test.h"
#include "data/data_set.h"
#include <Eigen/Dense>
#include <string>
#include <unordered_map>

namespace tetrad {

class IndTestFisherZ : public IndependenceTest {
public:
    IndTestFisherZ(const DataSet& dataSet, double alpha);

    IndependenceResult checkIndependence(
        const NodePtr& x, const NodePtr& y,
        const std::set<NodePtr>& z) override;

    const std::vector<NodePtr>& getVariables() const override { return variables_; }
    int getSampleSize() const override { return sampleSize_; }

    double getAlpha() const override { return alpha_; }
    void setAlpha(double alpha) override { alpha_ = alpha; }

    bool isVerbose() const override { return verbose_; }
    void setVerbose(bool verbose) override { verbose_ = verbose; }

private:
    double getPValue(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z);
    double partialCorrelation(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& z);
    static double partialFromPrecision(const Eigen::MatrixXd& P);
    static double normalCdf(double x);

    Eigen::MatrixXd correlationMatrix_;
    std::vector<NodePtr> variables_;
    std::unordered_map<std::string, int> indexMap_;
    int sampleSize_;
    double alpha_;
    bool verbose_ = false;
};

} // namespace tetrad
