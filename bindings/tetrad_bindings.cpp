#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/eigen/dense.h>

#include "data/data_set.h"
#include "data/knowledge.h"
#include "search/ind_test_fisher_z.h"
#include "search/pc.h"
#include "search/fges.h"
#include "search/sem_bic_score.h"
#include "search/gfci.h"
#include "search/boss.h"
#include "search/permutation_search.h"
#include "search/boss_fci.h"
#include "search/grasp.h"
#include "search/grasp_fci.h"
#include "graph/graph.h"
#include "graph/edge.h"
#include "graph/node.h"
#include "graph/endpoint.h"

namespace nb = nanobind;
using namespace tetrad;

// Result struct for algorithms returning graph structures.
// Uses only strings/ints so there are no shared_ptr lifetime issues
// across the C++/Python boundary.
struct SearchResult {
    std::vector<std::string> edges;
    std::vector<std::string> nodes;
    int num_edges;
    int num_nodes;
    double model_score;  // For score-based algorithms; NaN otherwise
};

static SearchResult graph_to_result(Graph& g, double model_score = std::numeric_limits<double>::quiet_NaN()) {
    SearchResult result;
    result.num_edges = g.getNumEdges();
    result.num_nodes = g.getNumNodes();
    result.model_score = model_score;

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

// Legacy result type for backwards compatibility
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
    bool verbose,
    const Knowledge& knowledge = Knowledge()
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
    if (!knowledge.isEmpty()) pc.setKnowledge(knowledge);

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

static SearchResult run_fges_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double penalty_discount,
    bool faithfulness_assumed,
    int max_degree,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);

    Fges fges(score);
    fges.setFaithfulnessAssumed(faithfulness_assumed);
    fges.setVerbose(verbose);
    if (max_degree > 0) fges.setMaxDegree(max_degree);
    if (!knowledge.isEmpty()) fges.setKnowledge(knowledge);

    Graph g = fges.search();

    return graph_to_result(g, fges.getModelScore());
}

static SearchResult run_gfci_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double alpha,
    double penalty_discount,
    int depth,
    int max_degree,
    bool complete_rule_set,
    int max_disc_path_length,
    bool faithfulness_assumed,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);
    IndTestFisherZ test(ds, alpha);

    Gfci gfci(test, score);
    gfci.setDepth(depth);
    gfci.setVerbose(verbose);
    gfci.setCompleteRuleSetUsed(complete_rule_set);
    gfci.setMaxDiscriminatingPathLength(max_disc_path_length);
    gfci.setFaithfulnessAssumed(faithfulness_assumed);
    if (max_degree > 0) gfci.setMaxDegree(max_degree);
    if (!knowledge.isEmpty()) gfci.setKnowledge(knowledge);

    Graph g = gfci.search();

    return graph_to_result(g);
}

static SearchResult run_boss_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double penalty_discount,
    bool use_bes,
    int num_starts,
    bool use_data_order,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);

    Boss boss(score);
    boss.setUseBes(use_bes);
    boss.setNumStarts(num_starts);
    boss.setUseDataOrder(use_data_order);
    boss.setVerbose(verbose);
    if (!knowledge.isEmpty()) boss.setKnowledge(knowledge);

    PermutationSearch search(boss);
    if (!knowledge.isEmpty()) search.setKnowledge(knowledge);

    Graph g = search.search();

    return graph_to_result(g);
}

static SearchResult run_boss_fci_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double alpha,
    double penalty_discount,
    int depth,
    bool complete_rule_set,
    int max_disc_path_length,
    bool use_bes,
    int num_starts,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);
    IndTestFisherZ test(ds, alpha);

    BossFci bfci(test, score);
    bfci.setDepth(depth);
    bfci.setVerbose(verbose);
    bfci.setCompleteRuleSetUsed(complete_rule_set);
    bfci.setMaxDiscriminatingPathLength(max_disc_path_length);
    bfci.setBossUseBes(use_bes);
    bfci.setNumStarts(num_starts);
    if (!knowledge.isEmpty()) bfci.setKnowledge(knowledge);

    Graph g = bfci.search();

    return graph_to_result(g);
}

static SearchResult run_grasp_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double penalty_discount,
    int depth,
    int uncovered_depth,
    int non_singular_depth,
    bool ordered,
    int num_starts,
    bool use_data_order,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);

    Grasp grasp(score);
    grasp.setDepth(depth);
    grasp.setUncoveredDepth(uncovered_depth);
    grasp.setNonSingularDepth(non_singular_depth);
    grasp.setOrdered(ordered);
    grasp.setNumStarts(num_starts);
    grasp.setUseDataOrder(use_data_order);
    grasp.setVerbose(verbose);
    if (!knowledge.isEmpty()) grasp.setKnowledge(knowledge);

    auto variables = score.getVariables();
    grasp.bestOrder(variables);
    Graph g = grasp.getGraph(true);

    return graph_to_result(g);
}

static SearchResult run_grasp_fci_raw(
    const Eigen::MatrixXd& data,
    const std::vector<std::string>& col_names,
    double alpha,
    double penalty_discount,
    int depth,
    int grasp_depth,
    int uncovered_depth,
    int non_singular_depth,
    bool ordered,
    bool complete_rule_set,
    int max_disc_path_length,
    int num_starts,
    bool use_data_order,
    bool verbose,
    const Knowledge& knowledge = Knowledge()
) {
    if (static_cast<int>(col_names.size()) != data.cols()) {
        throw std::invalid_argument(
            "Number of column names (" + std::to_string(col_names.size()) +
            ") must match number of columns (" + std::to_string(data.cols()) + ")"
        );
    }

    DataSet ds(data, col_names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);
    IndTestFisherZ test(ds, alpha);

    GraspFci gfci(test, score);
    gfci.setDepth(depth);
    gfci.setGraspDepth(grasp_depth);
    gfci.setUncoveredDepth(uncovered_depth);
    gfci.setNonSingularDepth(non_singular_depth);
    gfci.setOrdered(ordered);
    gfci.setVerbose(verbose);
    gfci.setCompleteRuleSetUsed(complete_rule_set);
    gfci.setMaxDiscriminatingPathLength(max_disc_path_length);
    gfci.setNumStarts(num_starts);
    gfci.setUseDataOrder(use_data_order);
    if (!knowledge.isEmpty()) gfci.setKnowledge(knowledge);

    Graph g = gfci.search();

    return graph_to_result(g);
}

NB_MODULE(_tetrad_cpp, m) {
    m.doc() = "C++ tetrad-port bindings: PC, FGES, GFCI, BOSS, BOSS-FCI, GRaSP, and GRaSP-FCI algorithms for causal discovery";

    nb::class_<Knowledge>(m, "Knowledge")
        .def(nb::init<>())
        .def("set_forbidden", &Knowledge::setForbidden, nb::arg("from_var"), nb::arg("to_var"),
             "Forbid a directed edge from from_var to to_var.")
        .def("remove_forbidden", &Knowledge::removeForbidden, nb::arg("from_var"), nb::arg("to_var"))
        .def("is_forbidden", &Knowledge::isForbidden, nb::arg("from_var"), nb::arg("to_var"))
        .def("set_required", &Knowledge::setRequired, nb::arg("from_var"), nb::arg("to_var"),
             "Require a directed edge from from_var to to_var.")
        .def("remove_required", &Knowledge::removeRequired, nb::arg("from_var"), nb::arg("to_var"))
        .def("is_required", &Knowledge::isRequired, nb::arg("from_var"), nb::arg("to_var"))
        .def("add_to_tier", &Knowledge::addToTier, nb::arg("tier"), nb::arg("var"),
             "Add a variable to a temporal tier. Edges from higher tiers to lower tiers are forbidden.")
        .def("set_tier", &Knowledge::setTier, nb::arg("tier"), nb::arg("vars"),
             "Set all variables in a tier at once.")
        .def("get_tier", &Knowledge::getTier, nb::arg("tier"))
        .def("get_num_tiers", &Knowledge::getNumTiers)
        .def("set_tier_forbidden_within", &Knowledge::setTierForbiddenWithin,
             nb::arg("tier"), nb::arg("forbidden"),
             "If true, forbid edges between variables within the same tier.")
        .def("is_empty", &Knowledge::isEmpty)
        .def("clear", &Knowledge::clear);

    nb::enum_<Endpoint>(m, "Endpoint")
        .value("TAIL", Endpoint::TAIL)
        .value("ARROW", Endpoint::ARROW)
        .value("CIRCLE", Endpoint::CIRCLE)
        .value("NULL_EP", Endpoint::NULL_EP);

    // Legacy PcResult for backwards compatibility
    nb::class_<PcResult>(m, "PcResult")
        .def_ro("edges", &PcResult::edges)
        .def_ro("nodes", &PcResult::nodes)
        .def_ro("num_edges", &PcResult::num_edges)
        .def_ro("num_nodes", &PcResult::num_nodes);

    // New unified SearchResult
    nb::class_<SearchResult>(m, "SearchResult")
        .def_ro("edges", &SearchResult::edges)
        .def_ro("nodes", &SearchResult::nodes)
        .def_ro("num_edges", &SearchResult::num_edges)
        .def_ro("num_nodes", &SearchResult::num_nodes)
        .def_ro("model_score", &SearchResult::model_score);

    m.def("run_pc_raw", &run_pc_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("alpha") = 0.05,
        nb::arg("depth") = -1,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the PC algorithm on data.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    alpha: significance level for independence tests\n"
        "    depth: maximum conditioning set size (-1 for unlimited)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    PcResult with edges (list of strings) and nodes (list of names)"
    );

    m.def("run_fges_raw", &run_fges_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("penalty_discount") = 1.0,
        nb::arg("faithfulness_assumed") = true,
        nb::arg("max_degree") = -1,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the FGES (Fast Greedy Equivalence Search) algorithm on data.\n\n"
        "FGES is a score-based algorithm that searches over CPDAGs using\n"
        "a greedy forward-backward strategy with BIC scoring.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    faithfulness_assumed: assume faithfulness (faster, default True)\n"
        "    max_degree: maximum node degree (-1 for unlimited)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges, nodes, and model_score"
    );

    m.def("run_gfci_raw", &run_gfci_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("alpha") = 0.05,
        nb::arg("penalty_discount") = 1.0,
        nb::arg("depth") = -1,
        nb::arg("max_degree") = -1,
        nb::arg("complete_rule_set") = true,
        nb::arg("max_disc_path_length") = -1,
        nb::arg("faithfulness_assumed") = true,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the GFCI (Greedy FCI) algorithm on data.\n\n"
        "GFCI is a hybrid algorithm that combines score-based search (FGES)\n"
        "with FCI orientation rules to handle latent (unmeasured) confounders.\n"
        "It returns a PAG (Partial Ancestral Graph) with four edge types:\n"
        "  --> directed, --- undirected, <-> bidirected, o-> partially oriented\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    alpha: significance level for independence tests\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    depth: maximum conditioning set size (-1 for unlimited)\n"
        "    max_degree: maximum node degree (-1 for unlimited)\n"
        "    complete_rule_set: use Zhang's complete rules R1-R10 (default True)\n"
        "    max_disc_path_length: max discriminating path length (-1 unlimited)\n"
        "    faithfulness_assumed: assume faithfulness for FGES (default True)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges (PAG edge strings) and nodes"
    );

    m.def("run_boss_raw", &run_boss_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("penalty_discount") = 1.0,
        nb::arg("use_bes") = false,
        nb::arg("num_starts") = 1,
        nb::arg("use_data_order") = true,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the BOSS (Best Order Score Search) algorithm on data.\n\n"
        "BOSS is a permutation-based algorithm that finds optimal variable\n"
        "orderings by iteratively moving variables to score-maximizing\n"
        "positions using GrowShrink trees for efficient caching.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    use_bes: run Backward Equivalence Search refinement (default False)\n"
        "    num_starts: number of random restarts (default 1)\n"
        "    use_data_order: use data column order for first run (default True)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges, nodes"
    );

    m.def("run_boss_fci_raw", &run_boss_fci_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("alpha") = 0.05,
        nb::arg("penalty_discount") = 1.0,
        nb::arg("depth") = -1,
        nb::arg("complete_rule_set") = true,
        nb::arg("max_disc_path_length") = -1,
        nb::arg("use_bes") = false,
        nb::arg("num_starts") = 1,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the BOSS-FCI algorithm on data.\n\n"
        "BOSS-FCI combines the permutation-based BOSS algorithm with FCI\n"
        "orientation rules to handle latent (unmeasured) confounders.\n"
        "Returns a PAG (Partial Ancestral Graph).\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    alpha: significance level for independence tests\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    depth: maximum conditioning set size (-1 for unlimited)\n"
        "    complete_rule_set: use Zhang's complete rules R1-R10 (default True)\n"
        "    max_disc_path_length: max discriminating path length (-1 unlimited)\n"
        "    use_bes: run BES refinement in BOSS (default False)\n"
        "    num_starts: number of random restarts for BOSS (default 1)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges (PAG edge strings) and nodes"
    );

    m.def("run_grasp_raw", &run_grasp_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("penalty_discount") = 1.0,
        nb::arg("depth") = 3,
        nb::arg("uncovered_depth") = 1,
        nb::arg("non_singular_depth") = 1,
        nb::arg("ordered") = false,
        nb::arg("num_starts") = 1,
        nb::arg("use_data_order") = true,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the GRaSP (Greedy Relaxations of SP) algorithm on data.\n\n"
        "GRaSP searches permutation space using depth-first tuck moves\n"
        "with backtracking to find optimal variable orderings.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    depth: max DFS depth for singular tucks (default 3)\n"
        "    uncovered_depth: max depth for uncovered tucks (default 1)\n"
        "    non_singular_depth: max depth for non-singular tucks (default 1)\n"
        "    ordered: enforce GRaSP0/1/2 ordering (default False)\n"
        "    num_starts: number of random restarts (default 1)\n"
        "    use_data_order: use data column order for first run (default True)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges, nodes"
    );

    m.def("run_grasp_fci_raw", &run_grasp_fci_raw,
        nb::arg("data"),
        nb::arg("col_names"),
        nb::arg("alpha") = 0.05,
        nb::arg("penalty_discount") = 1.0,
        nb::arg("depth") = -1,
        nb::arg("grasp_depth") = 3,
        nb::arg("uncovered_depth") = 1,
        nb::arg("non_singular_depth") = 1,
        nb::arg("ordered") = false,
        nb::arg("complete_rule_set") = true,
        nb::arg("max_disc_path_length") = -1,
        nb::arg("num_starts") = 1,
        nb::arg("use_data_order") = true,
        nb::arg("verbose") = false,
        nb::arg("knowledge") = Knowledge(),
        "Run the GRaSP-FCI algorithm on data.\n\n"
        "GRaSP-FCI combines the GRaSP algorithm with FCI orientation rules\n"
        "to handle latent (unmeasured) confounders. Returns a PAG.\n\n"
        "Args:\n"
        "    data: numpy array (n_samples x n_variables)\n"
        "    col_names: list of variable names\n"
        "    alpha: significance level for independence tests\n"
        "    penalty_discount: BIC penalty multiplier (1.0 = standard BIC)\n"
        "    depth: max conditioning set size for FCI (-1 unlimited)\n"
        "    grasp_depth: max DFS depth for GRaSP tucks (default 3)\n"
        "    uncovered_depth: max depth for uncovered tucks (default 1)\n"
        "    non_singular_depth: max depth for non-singular tucks (default 1)\n"
        "    ordered: enforce GRaSP0/1/2 ordering (default False)\n"
        "    complete_rule_set: use Zhang's complete rules R1-R10 (default True)\n"
        "    max_disc_path_length: max discriminating path length (-1 unlimited)\n"
        "    num_starts: number of random restarts (default 1)\n"
        "    use_data_order: use data column order for first run (default True)\n"
        "    verbose: print progress to stdout\n"
        "    knowledge: background knowledge (Knowledge object)\n\n"
        "Returns:\n"
        "    SearchResult with edges (PAG edge strings) and nodes"
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
