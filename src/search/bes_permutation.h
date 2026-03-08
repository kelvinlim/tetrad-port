#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "search/meek_rules.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_map>
#include <vector>

namespace tetrad {

// Backward Equivalence Search adapted for permutation-based algorithms.
// Port of edu.cmu.tetrad.search.utils.BesPermutation from Java Tetrad 7.6.8.
//
// Tries to remove edges from a graph to improve the BIC score, while
// maintaining DAG validity with respect to the permutation ordering.
// Used as an optional refinement step in BOSS.
class BesPermutation {
public:
    explicit BesPermutation(Score& score);

    void bes(Graph& graph, const std::vector<NodePtr>& order,
             const std::vector<NodePtr>& suborder);

    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    const Knowledge& getKnowledge() const { return knowledge_; }
    void setVerbose(bool verbose) { verbose_ = verbose; }

private:
    struct Arrow {
        double bump;
        NodePtr a;
        NodePtr b;
        std::set<NodePtr> hOrT;
        std::set<NodePtr> naYX;
        std::set<NodePtr> parents;
        int index;

        bool operator<(const Arrow& other) const {
            if (bump != other.bump) return bump > other.bump; // high to low
            return index < other.index;
        }
    };

    struct ArrowConfig {
        std::set<NodePtr> nayx;
        std::set<NodePtr> parents;
        bool operator==(const ArrowConfig& o) const {
            return nayx == o.nayx && parents == o.parents;
        }
    };

    void reevaluateBackward(const std::set<NodePtr>& toProcess,
                            Graph& graph,
                            const std::unordered_map<NodePtr, int>& hashIndices);

    void calculateArrowsBackward(const NodePtr& a, const NodePtr& b,
                                  Graph& graph,
                                  const std::unordered_map<NodePtr, int>& hashIndices);

    double deleteEval(const NodePtr& x, const NodePtr& y,
                      const std::set<NodePtr>& complement,
                      const std::set<NodePtr>& parents,
                      const std::unordered_map<NodePtr, int>& hashIndices);

    double scoreGraphChange(const NodePtr& x, const NodePtr& y,
                            const std::set<NodePtr>& parents,
                            const std::unordered_map<NodePtr, int>& hashIndices);

    void doDelete(const NodePtr& x, const NodePtr& y,
                  const std::set<NodePtr>& H, double bump,
                  const std::set<NodePtr>& naYX, Graph& graph);

    bool validDelete(const NodePtr& x, const NodePtr& y,
                     const std::set<NodePtr>& H, const std::set<NodePtr>& naYX,
                     Graph& graph, const std::vector<NodePtr>& suborder);

    bool invalidSink(const NodePtr& x, const Graph& graph);

    std::set<NodePtr> getNaYX(const NodePtr& x, const NodePtr& y,
                               const Graph& graph);

    bool isClique(const std::set<NodePtr>& nodes, const Graph& graph);

    std::set<NodePtr> revertToCPDAG(Graph& graph);

    static bool isUndirectedEdge(const Edge& e);

    bool existsKnowledge() const { return !knowledge_.isEmpty(); }

    Score& score_;
    std::vector<NodePtr> variables_;
    Knowledge knowledge_;
    bool verbose_ = false;

    std::set<Arrow> sortedArrows_;
    std::unordered_map<std::string, ArrowConfig> arrowsMap_;
    int arrowIndex_ = 0;
};

} // namespace tetrad
