#include "data/data_set.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace tetrad {

DataSet::DataSet(const Eigen::MatrixXd& data, const std::vector<std::string>& variableNames)
    : data_(data), variableNames_(variableNames) {

    if (static_cast<int>(variableNames.size()) != data.cols()) {
        throw std::invalid_argument("Number of variable names must match number of columns");
    }

    variables_.reserve(variableNames.size());
    for (const auto& name : variableNames) {
        variables_.push_back(std::make_shared<Node>(name));
    }
}

NodePtr DataSet::getVariable(const std::string& name) const {
    for (const auto& var : variables_) {
        if (var->getName() == name) return var;
    }
    return nullptr;
}

int DataSet::getColumn(const NodePtr& node) const {
    for (size_t i = 0; i < variables_.size(); ++i) {
        if (*variables_[i] == *node) return static_cast<int>(i);
    }
    return -1;
}

Eigen::MatrixXd DataSet::getCorrelationMatrix() const {
    int n = getNumRows();
    int p = getNumColumns();

    // Center the data
    Eigen::MatrixXd centered = data_.rowwise() - data_.colwise().mean();

    // Covariance matrix
    Eigen::MatrixXd cov = (centered.transpose() * centered) / (n - 1);

    // Convert to correlation
    Eigen::MatrixXd corr(p, p);
    for (int i = 0; i < p; i++) {
        for (int j = 0; j < p; j++) {
            double denom = std::sqrt(cov(i, i) * cov(j, j));
            if (denom > 0) {
                corr(i, j) = cov(i, j) / denom;
            } else {
                corr(i, j) = (i == j) ? 1.0 : 0.0;
            }
        }
    }
    return corr;
}

Eigen::MatrixXd DataSet::getCovarianceMatrix() const {
    int n = getNumRows();
    Eigen::MatrixXd centered = data_.rowwise() - data_.colwise().mean();
    return (centered.transpose() * centered) / (n - 1);
}

DataSet DataSet::loadCsv(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::string line;
    std::vector<std::string> variableNames;
    std::vector<std::vector<double>> rows;

    // Read header
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ',')) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            variableNames.push_back(token);
        }
    }

    // Read data rows
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string token;
        std::vector<double> row;
        while (std::getline(iss, token, ',')) {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            row.push_back(std::stod(token));
        }
        rows.push_back(row);
    }

    if (rows.empty()) {
        throw std::runtime_error("No data rows in file");
    }

    int numRows = static_cast<int>(rows.size());
    int numCols = static_cast<int>(variableNames.size());

    Eigen::MatrixXd data(numRows, numCols);
    for (int i = 0; i < numRows; i++) {
        for (int j = 0; j < numCols; j++) {
            data(i, j) = rows[i][j];
        }
    }

    return DataSet(data, variableNames);
}

} // namespace tetrad
