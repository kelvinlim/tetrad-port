#include "search/pc.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include <iostream>
#include <random>

using namespace tetrad;

int main(int argc, char* argv[]) {
    if (argc > 1) {
        // Load from CSV
        try {
            DataSet ds = DataSet::loadCsv(argv[1]);
            double alpha = (argc > 2) ? std::stod(argv[2]) : 0.05;

            std::cout << "Loaded " << ds.getNumRows() << " rows x "
                      << ds.getNumColumns() << " columns" << std::endl;
            std::cout << "Variables: ";
            for (const auto& name : ds.getVariableNames()) {
                std::cout << name << " ";
            }
            std::cout << std::endl;
            std::cout << "Alpha: " << alpha << std::endl << std::endl;

            IndTestFisherZ test(ds, alpha);
            Pc pc(&test);
            pc.setVerbose(true);

            Graph g = pc.search();
            std::cout << std::endl << g.toString() << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    } else {
        // Demo: generate synthetic data from a known DAG and run PC
        std::cout << "=== PC Algorithm Demo ===" << std::endl;
        std::cout << "True DAG: X -> Y -> Z, X -> Z (fork + chain)" << std::endl;
        std::cout << std::endl;

        int n = 1000;
        Eigen::MatrixXd data(n, 3);
        std::mt19937 rng(42);
        std::normal_distribution<double> dist(0.0, 1.0);

        for (int i = 0; i < n; i++) {
            double x = dist(rng);
            double y = 0.7 * x + 0.5 * dist(rng);
            double z = 0.5 * x + 0.5 * y + 0.3 * dist(rng);
            data(i, 0) = x;
            data(i, 1) = y;
            data(i, 2) = z;
        }

        DataSet ds(data, {"X", "Y", "Z"});
        IndTestFisherZ test(ds, 0.05);
        Pc pc(&test);
        pc.setVerbose(true);

        Graph g = pc.search();

        std::cout << std::endl << "Result:" << std::endl;
        std::cout << g.toString() << std::endl;
    }

    return 0;
}
