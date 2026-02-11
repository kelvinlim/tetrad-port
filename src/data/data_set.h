#pragma once

#include "graph/node.h"
#include <Eigen/Dense>
#include <string>
#include <vector>
#include <memory>

namespace tetrad {

class DataSet {
public:
    DataSet(const Eigen::MatrixXd& data, const std::vector<std::string>& variableNames);

    const Eigen::MatrixXd& getData() const { return data_; }
    int getNumRows() const { return static_cast<int>(data_.rows()); }
    int getNumColumns() const { return static_cast<int>(data_.cols()); }

    const std::vector<NodePtr>& getVariables() const { return variables_; }
    const std::vector<std::string>& getVariableNames() const { return variableNames_; }

    NodePtr getVariable(const std::string& name) const;
    int getColumn(const NodePtr& node) const;

    Eigen::MatrixXd getCorrelationMatrix() const;
    Eigen::MatrixXd getCovarianceMatrix() const;

    // Load from CSV file
    static DataSet loadCsv(const std::string& filename);

private:
    Eigen::MatrixXd data_;
    std::vector<std::string> variableNames_;
    std::vector<NodePtr> variables_;
};

} // namespace tetrad
