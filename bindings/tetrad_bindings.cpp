#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/eigen/dense.h>

#include "data/data_set.h"
#include "search/ind_test_fisher_z.h"
#include "search/pc.h"
#include "graph/graph.h"
#include "graph/edge.h"
#include "graph/node.h"
#include "graph/endpoint.h"

namespace nb = nanobind;
using namespace tetrad;

// Simple result struct returned by the high-level run_pc_raw function.
// Uses only strings/ints so there are no shared_ptr lifetime issues
// across the C++/Python boundary.
struct PcResult {
    std::vector<std::string> edges;
    std::vector<std::string> nodes;
    int num_edges;
    int num_nodes;
};

static PcResult run_pc_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double alpha,
    int depth,
    bool verbose
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    IndTestFisherZ test(ds, alpha);

    Pc pc(&test);
    pc.setDepth(depth);
    pc.setVerbose(verbose);

    Graph g = pc.search();

    PcResult result;
    result.num_edges = g.getNumEdges();
    result.num_nodes = g.getNumNodes();

    for (const auto& node : g.getNodes()) {
        result.nodes.push_back(node->getName());
    }

    auto edges = g.getEdges();
    std::sort(edges.begin(), edges.end());
    for (const auto& edge : edges) {
        result.edges.push_back(edge.toString());
    }

    return result;
}

NB_MODULE(_tetrad_cpp, m) {
    m.doc() = "C++ tetrad-port bindings: PC algorithm with Fisher Z test";

    nb::enum_<Endpoint>(m, "Endpoint")
        .value("TAIL", Endpoint::TAIL)
        .value("ARROW", Endpoint::ARROW)
        .value("CIRCLE", Endpoint::CIRCLE)
        .value("NULL_EP", Endpoint::NULL_EP);

    nb::class_<PcResult>(m, "PcResult")
        .def_ro("edges", &PcResult::edges)
        .def_ro("nodes", &PcResult::nodes)
        .def_ro("num_edges", &PcResult::num_edges)
        .def_ro("num_nodes", &PcResult::num_nodes);

    m.def("run_pc_raw", &run_pc_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("alpha") = 0.05,
        nb::arg("depth") = -1,
        nb::arg("verbose") = false,
        "Run the PC algorithm on data.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    alpha: significance level for independence tests\n"
        "    depth: maximum conditioning set size (-1 for unlimited)\n"
        "    verbose: print progress to stdout\n\n"
        "Returns:\n"
        "    PcResult with edges (list of strings) and nodes (list of names)"
    );

    nb::class_<Node>(m, "Node")
        .def(nb::init<std::string>(), nb::arg("name"))
        .def("get_name", &Node::getName)
        .def("__repr__", &Node::toString)
        .def("__eq__", &Node::operator==);

    nb::class_<Edge>(m, "Edge")
        .def("get_node1", &Edge::getNode1)
        .def("get_node2", &Edge::getNode2)
        .def("get_endpoint1", &Edge::getEndpoint1)
        .def("get_endpoint2", &Edge::getEndpoint2)
        .def("is_directed", &Edge::isDirected)
        .def("__repr__", &Edge::toString);

    nb::class_<Graph>(m, "Graph")
        .def("get_num_nodes", &Graph::getNumNodes)
        .def("get_num_edges", &Graph::getNumEdges)
        .def("get_node_names", &Graph::getNodeNames)
        .def("get_edges", nb::overload_cast<>(&Graph::getEdges, nb::const_))
        .def("get_nodes", &Graph::getNodes)
        .def("is_adjacent_to", &Graph::isAdjacentTo)
        .def("is_directed_from_to", &Graph::isDirectedFromTo)
        .def("__repr__", &Graph::toString);
}
