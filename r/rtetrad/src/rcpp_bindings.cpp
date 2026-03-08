// [[Rcpp::depends(RcppEigen)]]
#include <RcppEigen.h>
#include "search/pc.h"
#include "search/fges.h"
#include "search/gfci.h"
#include "search/sem_bic_score.h"
#include "search/ind_test_fisher_z.h"
#include "data/data_set.h"
#include "data/knowledge.h"
#include "util/log_stream.h"

using namespace tetrad;

static std::string endpointToString(Endpoint ep) {
    switch (ep) {
        case Endpoint::TAIL:    return "TAIL";
        case Endpoint::ARROW:   return "ARROW";
        case Endpoint::CIRCLE:  return "CIRCLE";
        case Endpoint::NULL_EP: return "NULL";
    }
    return "UNKNOWN";
}

static std::string edgeTypeString(Endpoint ep1, Endpoint ep2) {
    std::string s;
    if (ep1 == Endpoint::TAIL)        s += "-";
    else if (ep1 == Endpoint::ARROW)  s += "<";
    else if (ep1 == Endpoint::CIRCLE) s += "o";
    s += "-";
    if (ep2 == Endpoint::TAIL)        s += "-";
    else if (ep2 == Endpoint::ARROW)  s += ">";
    else if (ep2 == Endpoint::CIRCLE) s += "o";
    return s;
}

static Rcpp::List graphToList(Graph& g, const std::string& algorithm,
                               double model_score = NA_REAL) {
    auto edges = g.getEdges();
    std::sort(edges.begin(), edges.end());

    Rcpp::CharacterVector from_vec, to_vec, ep1_vec, ep2_vec, type_vec;

    for (const auto& e : edges) {
        from_vec.push_back(e.getNode1()->getName());
        to_vec.push_back(e.getNode2()->getName());
        ep1_vec.push_back(endpointToString(e.getEndpoint1()));
        ep2_vec.push_back(endpointToString(e.getEndpoint2()));
        type_vec.push_back(edgeTypeString(e.getEndpoint1(), e.getEndpoint2()));
    }

    Rcpp::DataFrame edge_df = Rcpp::DataFrame::create(
        Rcpp::Named("from") = from_vec,
        Rcpp::Named("to") = to_vec,
        Rcpp::Named("endpoint1") = ep1_vec,
        Rcpp::Named("endpoint2") = ep2_vec,
        Rcpp::Named("edge_type") = type_vec,
        Rcpp::Named("stringsAsFactors") = false
    );

    Rcpp::List result = Rcpp::List::create(
        Rcpp::Named("edges") = edge_df,
        Rcpp::Named("nodes") = Rcpp::wrap(g.getNodeNames()),
        Rcpp::Named("num_edges") = g.getNumEdges(),
        Rcpp::Named("num_nodes") = g.getNumNodes(),
        Rcpp::Named("algorithm") = algorithm,
        Rcpp::Named("model_score") = model_score
    );
    result.attr("class") = "tetrad_result";
    return result;
}

// --- Algorithm wrappers ---

// [[Rcpp::export]]
Rcpp::List run_pc_cpp(Eigen::Map<Eigen::MatrixXd> data,
                      Rcpp::CharacterVector col_names,
                      double alpha,
                      int depth,
                      bool verbose,
                      SEXP knowledge_ptr) {
    std::vector<std::string> names = Rcpp::as<std::vector<std::string>>(col_names);
    Eigen::MatrixXd data_copy = data;

    DataSet ds(data_copy, names);
    IndTestFisherZ test(ds, alpha);

    Pc pc(&test);
    pc.setDepth(depth);
    pc.setVerbose(verbose);

    if (!Rf_isNull(knowledge_ptr)) {
        Rcpp::XPtr<Knowledge> kptr(knowledge_ptr);
        pc.setKnowledge(*kptr);
    }

    Graph g = pc.search();
    return graphToList(g, "PC");
}

// [[Rcpp::export]]
Rcpp::List run_fges_cpp(Eigen::Map<Eigen::MatrixXd> data,
                        Rcpp::CharacterVector col_names,
                        double penalty_discount,
                        bool faithfulness_assumed,
                        int max_degree,
                        bool verbose,
                        SEXP knowledge_ptr) {
    std::vector<std::string> names = Rcpp::as<std::vector<std::string>>(col_names);
    Eigen::MatrixXd data_copy = data;

    DataSet ds(data_copy, names);
    SemBicScore score(ds);
    score.setPenaltyDiscount(penalty_discount);

    Fges fges(score);
    fges.setFaithfulnessAssumed(faithfulness_assumed);
    fges.setVerbose(verbose);
    if (max_degree > 0) fges.setMaxDegree(max_degree);

    if (!Rf_isNull(knowledge_ptr)) {
        Rcpp::XPtr<Knowledge> kptr(knowledge_ptr);
        fges.setKnowledge(*kptr);
    }

    Graph g = fges.search();
    return graphToList(g, "FGES", fges.getModelScore());
}

// [[Rcpp::export]]
Rcpp::List run_gfci_cpp(Eigen::Map<Eigen::MatrixXd> data,
                        Rcpp::CharacterVector col_names,
                        double alpha,
                        double penalty_discount,
                        int depth,
                        int max_degree,
                        bool complete_rule_set,
                        int max_disc_path_length,
                        bool faithfulness_assumed,
                        bool verbose,
                        SEXP knowledge_ptr) {
    std::vector<std::string> names = Rcpp::as<std::vector<std::string>>(col_names);
    Eigen::MatrixXd data_copy = data;

    DataSet ds(data_copy, names);
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

    if (!Rf_isNull(knowledge_ptr)) {
        Rcpp::XPtr<Knowledge> kptr(knowledge_ptr);
        gfci.setKnowledge(*kptr);
    }

    Graph g = gfci.search();
    return graphToList(g, "GFCI");
}

// --- Knowledge wrappers ---

// [[Rcpp::export]]
SEXP create_knowledge_cpp() {
    Rcpp::XPtr<Knowledge> ptr(new Knowledge(), true);
    return ptr;
}

// [[Rcpp::export]]
void knowledge_set_forbidden_cpp(SEXP ptr, std::string from, std::string to) {
    Rcpp::XPtr<Knowledge> kptr(ptr);
    kptr->setForbidden(from, to);
}

// [[Rcpp::export]]
void knowledge_set_required_cpp(SEXP ptr, std::string from, std::string to) {
    Rcpp::XPtr<Knowledge> kptr(ptr);
    kptr->setRequired(from, to);
}

// [[Rcpp::export]]
void knowledge_set_tier_cpp(SEXP ptr, int tier, std::vector<std::string> vars) {
    Rcpp::XPtr<Knowledge> kptr(ptr);
    kptr->setTier(tier, vars);
}

// [[Rcpp::export]]
void knowledge_set_tier_forbidden_within_cpp(SEXP ptr, int tier, bool forbidden) {
    Rcpp::XPtr<Knowledge> kptr(ptr);
    kptr->setTierForbiddenWithin(tier, forbidden);
}

// --- Log stream redirect ---

// [[Rcpp::export]]
void set_log_stream_rcout_cpp() {
    setLogStream(Rcpp::Rcout);
}
