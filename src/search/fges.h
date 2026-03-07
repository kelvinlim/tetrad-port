#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "data/knowledge.h"
#include <set>
#include <unordered_map>
#include <vector>

namespace tetrad {

// Fast Greedy Equivalence Search (FGES).
// Port of edu.cmu.tetrad.search.Fges from Java Tetrad 7.6.8.
// Single-threaded implementation (no parallelism).
//
// The algorithm consists of a forward phase (FES) that adds edges to improve score,
// and a backward phase (BES) that removes edges to improve score. Both phases
// maintain a CPDAG via Meek rules.
//
// Reference: Ramsey et al. (2017), "A million variables and more."
class Fges {
public:
    explicit Fges(Score& score);

    Graph search();

    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    const Knowledge& getKnowledge() const { return knowledge_; }

    void setFaithfulnessAssumed(bool assumed) { faithfulnessAssumed_ = assumed; }
    bool isFaithfulnessAssumed() const { return faithfulnessAssumed_; }

    void setMaxDegree(int maxDegree) { maxDegree_ = maxDegree; }
    int getMaxDegree() const { return maxDegree_; }

    void setVerbose(bool verbose) { verbose_ = verbose; }

    void setBoundGraph(const Graph* boundGraph) { boundGraph_ = boundGraph; }

    double getModelScore() const { return modelScore_; }

private:
    enum class Mode { heuristicSpeedup, coverNoncolliders, allowUnfaithfulness };

    struct Arrow {
        double bump;
        NodePtr a;
        NodePtr b;
        std::set<NodePtr> hOrT;
        std::set<NodePtr> tNeighbors;
        std::set<NodePtr> naYX;
        std::set<NodePtr> parents;
        int index;

        bool operator<(const Arrow& other) const {
            if (bump != other.bump) return bump > other.bump; // high to low
            return index < other.index;
        }
    };

    struct ArrowConfig {
        std::set<NodePtr> T;
        std::set<NodePtr> nayx;
        std::set<NodePtr> parents;
        bool operator==(const ArrowConfig& o) const {
            return T == o.T && nayx == o.nayx && parents == o.parents;
        }
    };

    // Forward phase
    void fes();
    void initializeEffectEdges();
    void reevaluateForward(const std::set<NodePtr>& nodes);
    void calculateArrowsForward(const NodePtr& a, const NodePtr& b);
    double insertEval(const NodePtr& x, const NodePtr& y,
                      const std::set<NodePtr>& T, const std::set<NodePtr>& naYX,
                      const std::set<NodePtr>& parents);
    void insert(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& T, double bump);
    bool validInsert(const NodePtr& x, const NodePtr& y,
                     const std::set<NodePtr>& T, const std::set<NodePtr>& naYX);

    // Backward phase
    void bes();
    void reevaluateBackward(const std::set<NodePtr>& nodes);
    void calculateArrowsBackward(const NodePtr& a, const NodePtr& b);
    double deleteEval(const NodePtr& x, const NodePtr& y,
                      const std::set<NodePtr>& complement, const std::set<NodePtr>& parents);
    void doDelete(const NodePtr& x, const NodePtr& y, const std::set<NodePtr>& H,
                  double bump, const std::set<NodePtr>& naYX);
    bool validDelete(const NodePtr& x, const NodePtr& y,
                     const std::set<NodePtr>& H, const std::set<NodePtr>& naYX);

    // Utility
    std::set<NodePtr> getNaYX(const NodePtr& x, const NodePtr& y) const;
    std::vector<NodePtr> getTNeighbors(const NodePtr& x, const NodePtr& y) const;
    std::set<NodePtr> getCommonAdjacents(const NodePtr& x, const NodePtr& y) const;
    bool isClique(const std::set<NodePtr>& nodes) const;
    bool semidirectedPathCondition(const NodePtr& from, const NodePtr& to,
                                    const std::set<NodePtr>& cond) const;
    static NodePtr traverseSemiDirected(const NodePtr& node, const Edge& edge);
    double scoreGraphChange(const NodePtr& x, const NodePtr& y,
                            const std::set<NodePtr>& parents) const;
    std::set<NodePtr> revertToCpdag();
    void addRequiredEdges();
    double scoreDag(const Graph& dag) const;
    static bool isUndirectedEdge(const Edge& e);

    int indexOf(const NodePtr& node) const;

    Score& score_;
    Knowledge knowledge_;
    Graph graph_;
    Graph effectEdgesGraph_;
    const Graph* boundGraph_ = nullptr;
    std::vector<NodePtr> variables_;
    std::unordered_map<std::string, int> hashIndices_;

    std::set<Arrow> sortedArrows_;
    std::unordered_map<std::string, ArrowConfig> arrowsMap_; // key: "a->b"

    // BES uses separate arrow structures
    std::set<Arrow> sortedArrowsBack_;
    std::unordered_map<std::string, ArrowConfig> arrowsMapBack_;

    int arrowIndex_ = 0;
    int arrowIndexBack_ = 0;
    double modelScore_ = 0.0;
    int maxDegree_ = -1;
    bool faithfulnessAssumed_ = false;
    bool verbose_ = false;
    Mode mode_ = Mode::heuristicSpeedup;
};

} // namespace tetrad
