#pragma once

#include "graph/graph.h"
#include "search/score.h"
#include "search/grow_shrink_tree.h"
#include "data/knowledge.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <memory>

namespace tetrad {

// Implements a permutation scorer based on Teyssier & Koller (2012).
// Port of edu.cmu.tetrad.search.utils.TeyssierScorer from Java Tetrad 7.6.8.
//
// Given a score function and variable ordering, computes the total score.
// Supports efficient incremental updates when variables are moved, and
// bookmark/restore for backtracking during search.
class TeyssierScorer {
public:
    TeyssierScorer(Score& score);

    // Score a new permutation. Must be called before other operations.
    double score(const std::vector<NodePtr>& order);

    // Return the current score.
    double score();

    // Move variable v to position toIndex in the permutation.
    void moveTo(const NodePtr& v, int toIndex);

    // Tuck: move j and its ancestors to before k.
    bool tuck(const NodePtr& j, const NodePtr& k);

    // Swap two nodes in the permutation.
    bool swap(const NodePtr& m, const NodePtr& n);

    // Get a copy of the current permutation.
    std::vector<NodePtr> getPi() const;

    // Get the current permutation without copy (use carefully).
    const std::vector<NodePtr>& getOrderShallow() const { return pi_; }

    // Get index of a variable in current permutation.
    int index(const NodePtr& v) const;

    // Get the node at index j.
    const NodePtr& get(int j) const { return pi_[j]; }

    // Get parents of node at index p.
    std::set<NodePtr> getParents(int p);

    // Get parents of a node.
    std::set<NodePtr> getParents(const NodePtr& v);

    // Get ancestors of a node (recursive).
    std::set<NodePtr> getAncestors(const NodePtr& node);

    // Check adjacency.
    bool adjacent(const NodePtr& a, const NodePtr& b);

    // Check if x->y is a covered edge.
    bool coveredEdge(const NodePtr& x, const NodePtr& y);

    // Build a Graph (DAG or CPDAG) from the current permutation.
    Graph getGraph(bool cpDag);

    // Number of edges in current DAG.
    int getNumEdges();

    // Get prefix of size i (first i nodes in permutation).
    std::set<NodePtr> getPrefix(int i) const;

    // Size of permutation.
    int size() const { return static_cast<int>(pi_.size()); }

    // Bookmark/restore state for backtracking.
    void bookmark(int key);
    void bookmark();
    void goToBookmark(int key);
    void goToBookmark();
    void clearBookmarks();

    // Set knowledge constraints.
    void setKnowledge(const Knowledge& knowledge);

private:
    struct Pair {
        std::set<NodePtr> parents;
        double score;
    };

    void initializeScores();
    void updateScores(int i1, int i2);
    void recalculate(int p);
    double sum();
    bool lastMoveSame(int i1, int i2) const;
    Pair getGrowShrinkScore(int p);
    bool violatesKnowledge(const std::vector<NodePtr>& order) const;
    void collectAncestorsVisit(const NodePtr& node, std::set<NodePtr>& ancestors);

    std::vector<NodePtr> variables_;
    std::vector<NodePtr> pi_;
    Score& score_;

    std::unordered_map<NodePtr, int> orderHash_;
    std::vector<std::unique_ptr<Pair>> scores_;
    std::vector<std::unique_ptr<std::set<NodePtr>>> prefixes_;
    double runningScore_ = 0.0;

    Knowledge knowledge_;

    // GrowShrinkTree instances for efficient parent set computation.
    std::unordered_map<NodePtr, int> variablesHash_;
    std::vector<std::unique_ptr<GrowShrinkTree>> treeStorage_;
    std::unordered_map<NodePtr, GrowShrinkTree*> trees_;

    // Bookmarked states.
    static constexpr int DEFAULT_BOOKMARK_KEY = -999999;
    struct BookmarkState {
        std::vector<NodePtr> order;
        std::vector<std::unique_ptr<Pair>> scores;
        std::unordered_map<NodePtr, int> orderHash;
        double runningScore;
    };
    std::unordered_map<int, BookmarkState> bookmarks_;
};

} // namespace tetrad
