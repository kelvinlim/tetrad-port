#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "search/teyssier_scorer.h"
#include "data/knowledge.h"
#include <vector>
#include <set>
#include <random>

namespace tetrad {

// Implements the GRaSP (Greedy Relaxations of the Sparsest Permutation) algorithm.
// Port of edu.cmu.tetrad.search.Grasp from Java Tetrad 7.6.8.
//
// GRaSP searches in the space of variable permutations for ones that imply
// CPDAGs close to the true model. Uses depth-first "tuck" moves with
// backtracking via TeyssierScorer.
//
// Reference: Lam, Andrews, Ramsey (2022). Greedy relaxations of the sparsest
// permutation algorithm. UAI.
class Grasp {
public:
    explicit Grasp(Score& score);

    // Run the algorithm starting from the given variable order.
    // Returns the best permutation found.
    std::vector<NodePtr> bestOrder(const std::vector<NodePtr>& order);

    // Get the graph implied by the discovered permutation.
    Graph getGraph(bool cpDag);

    // Number of edges in the discovered graph.
    int getNumEdges();

    // Configuration setters.
    void setNumStarts(int numStarts) { numStarts_ = numStarts; }
    void setDepth(int depth) { depth_ = depth; }
    void setUncoveredDepth(int d) { uncoveredDepth_ = d; }
    void setNonSingularDepth(int d) { nonSingularDepth_ = d; }
    void setOrdered(bool ordered) { ordered_ = ordered; }
    void setUseDataOrder(bool use) { useDataOrder_ = use; }
    void setAllowInternalRandomness(bool allow) { allowInternalRandomness_ = allow; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }
    void setSeed(long seed) { seed_ = seed; }

    const std::vector<NodePtr>& getVariables() const { return variables_; }

private:
    // Main GRaSP search loop.
    std::vector<NodePtr> grasp(TeyssierScorer& scorer);

    // Recursive depth-first search for improving tuck sequences.
    void graspDfs(TeyssierScorer& scorer, double sOld, const std::vector<int>& depth,
                  int currentDepth, std::set<std::set<NodePtr>>& tucks,
                  std::set<std::set<std::set<NodePtr>>>& dfsHistory);

    // Reorder variables to satisfy knowledge constraints.
    void makeValidKnowledgeOrder(std::vector<NodePtr>& order);

    // Check if order violates required edge constraints.
    bool violatesKnowledge(const std::vector<NodePtr>& order) const;

    Score& score_;
    std::vector<NodePtr> variables_;
    std::unique_ptr<TeyssierScorer> scorer_;
    Knowledge knowledge_;

    int numStarts_ = 1;
    int depth_ = 3;
    int uncoveredDepth_ = 1;
    int nonSingularDepth_ = 1;
    bool ordered_ = false;
    bool useDataOrder_ = true;
    bool allowInternalRandomness_ = false;
    bool verbose_ = false;
    long seed_ = -1;
};

} // namespace tetrad
