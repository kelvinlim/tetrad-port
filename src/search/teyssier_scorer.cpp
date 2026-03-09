#include "search/teyssier_scorer.h"
#include "search/meek_rules.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace tetrad {

TeyssierScorer::TeyssierScorer(Score& score)
    : score_(score) {
    variables_ = score.getVariables();
    pi_ = variables_;

    for (int i = 0; i < static_cast<int>(variables_.size()); i++) {
        variablesHash_[variables_[i]] = i;
        orderHash_[pi_[i]] = i;
    }

    // Build GrowShrinkTrees for each variable.
    for (const auto& node : variables_) {
        auto tree = std::make_unique<GrowShrinkTree>(score_, variablesHash_, node);
        trees_[node] = tree.get();
        treeStorage_.push_back(std::move(tree));
    }
}

double TeyssierScorer::score(const std::vector<NodePtr>& order) {
    pi_ = order;
    scores_.clear();
    scores_.resize(order.size());
    prefixes_.clear();
    prefixes_.resize(order.size());
    runningScore_ = 0.0;
    initializeScores();
    return sum();
}

double TeyssierScorer::score() {
    return sum();
}

void TeyssierScorer::moveTo(const NodePtr& v, int toIndex) {
    int vIndex = index(v);
    if (vIndex == toIndex) return;
    if (lastMoveSame(vIndex, toIndex)) return;

    pi_.erase(pi_.begin() + vIndex);
    pi_.insert(pi_.begin() + toIndex, v);

    if (toIndex < vIndex) {
        updateScores(toIndex, vIndex);
    } else {
        updateScores(vIndex, toIndex);
    }
}

bool TeyssierScorer::tuck(const NodePtr& j, const NodePtr& k) {
    int jIndex = index(j);
    int kIndex = index(k);

    if (jIndex < kIndex) return false;

    auto ancestors = getAncestors(j);
    int kIdx = kIndex;
    bool changed = false;

    for (int i = jIndex; i > kIndex; i--) {
        if (ancestors.count(get(i))) {
            moveTo(get(i), kIdx++);
            changed = true;
        }
    }

    return changed;
}

bool TeyssierScorer::swap(const NodePtr& m, const NodePtr& n) {
    int i = orderHash_.at(m);
    int j = orderHash_.at(n);

    pi_[i] = n;
    pi_[j] = m;

    if (violatesKnowledge(pi_)) {
        pi_[i] = m;
        pi_[j] = n;
        return false;
    }

    if (i < j) {
        updateScores(i, j);
    } else {
        updateScores(j, i);
    }

    return true;
}

std::vector<NodePtr> TeyssierScorer::getPi() const {
    return pi_;
}

int TeyssierScorer::index(const NodePtr& v) const {
    auto it = orderHash_.find(v);
    if (it == orderHash_.end()) {
        throw std::invalid_argument("Variable not in permutation");
    }
    return it->second;
}

std::set<NodePtr> TeyssierScorer::getParents(int p) {
    if (!scores_[p]) {
        recalculate(p);
    }
    return scores_[p]->parents;
}

std::set<NodePtr> TeyssierScorer::getParents(const NodePtr& v) {
    return getParents(index(v));
}

std::set<NodePtr> TeyssierScorer::getAncestors(const NodePtr& node) {
    std::set<NodePtr> ancestors;
    collectAncestorsVisit(node, ancestors);
    return ancestors;
}

void TeyssierScorer::collectAncestorsVisit(const NodePtr& node, std::set<NodePtr>& ancestors) {
    if (ancestors.count(node)) return;
    ancestors.insert(node);

    auto parents = getParents(node);
    for (const auto& parent : parents) {
        collectAncestorsVisit(parent, ancestors);
    }
}

bool TeyssierScorer::adjacent(const NodePtr& a, const NodePtr& b) {
    if (a == b) return false;
    return getParents(a).count(b) || getParents(b).count(a);
}

bool TeyssierScorer::coveredEdge(const NodePtr& x, const NodePtr& y) {
    if (!adjacent(x, y)) return false;
    auto px = getParents(x);
    auto py = getParents(y);
    px.erase(y);
    py.erase(x);
    return px == py;
}

Graph TeyssierScorer::getGraph(bool cpDag) {
    Graph graph(variables_);
    for (const auto& a : variables_) {
        for (const auto& b : getParents(a)) {
            graph.addDirectedEdge(b, a);
        }
    }

    if (cpDag) {
        MeekRules rules;
        rules.setKnowledge(knowledge_);
        rules.setVerbose(false);
        rules.orientImplied(graph);
    }

    return graph;
}

int TeyssierScorer::getNumEdges() {
    int numEdges = 0;
    for (int p = 0; p < static_cast<int>(pi_.size()); p++) {
        numEdges += static_cast<int>(getParents(p).size());
    }
    return numEdges;
}

std::set<NodePtr> TeyssierScorer::getPrefix(int i) const {
    std::set<NodePtr> prefix;
    for (int j = 0; j < i; j++) {
        prefix.insert(pi_[j]);
    }
    return prefix;
}

void TeyssierScorer::bookmark(int key) {
    BookmarkState state;
    state.order = pi_;
    state.orderHash = orderHash_;
    state.runningScore = runningScore_;

    // Deep copy scores
    state.scores.resize(scores_.size());
    for (size_t i = 0; i < scores_.size(); i++) {
        if (scores_[i]) {
            state.scores[i] = std::make_unique<Pair>(*scores_[i]);
        }
    }

    bookmarks_[key] = std::move(state);
}

void TeyssierScorer::bookmark() {
    bookmark(DEFAULT_BOOKMARK_KEY);
}

void TeyssierScorer::goToBookmark(int key) {
    auto it = bookmarks_.find(key);
    if (it == bookmarks_.end()) {
        throw std::invalid_argument("Bookmark key not found");
    }

    pi_ = it->second.order;
    orderHash_ = it->second.orderHash;
    runningScore_ = it->second.runningScore;

    // Deep copy scores back
    scores_.resize(it->second.scores.size());
    for (size_t i = 0; i < it->second.scores.size(); i++) {
        if (it->second.scores[i]) {
            scores_[i] = std::make_unique<Pair>(*(it->second.scores[i]));
        } else {
            scores_[i].reset();
        }
    }
}

void TeyssierScorer::goToBookmark() {
    goToBookmark(DEFAULT_BOOKMARK_KEY);
}

void TeyssierScorer::clearBookmarks() {
    bookmarks_.clear();
}

void TeyssierScorer::setKnowledge(const Knowledge& knowledge) {
    knowledge_ = knowledge;

    for (const auto& node : variables_) {
        std::vector<NodePtr> required;
        std::vector<NodePtr> forbidden;
        for (const auto& parent : variables_) {
            if (knowledge.isRequired(parent->getName(), node->getName())) {
                required.push_back(parent);
            }
            if (knowledge.isForbidden(parent->getName(), node->getName())) {
                forbidden.push_back(parent);
            }
        }
        if (required.empty() && forbidden.empty()) continue;
        trees_[node]->setKnowledge(required, forbidden);
    }
}

void TeyssierScorer::initializeScores() {
    for (size_t i = 0; i < pi_.size(); i++) {
        prefixes_[i].reset();
    }
    updateScores(0, static_cast<int>(pi_.size()) - 1);
}

void TeyssierScorer::updateScores(int i1, int i2) {
    for (int i = i1; i <= i2; i++) {
        orderHash_[pi_[i]] = i;
        scores_[i].reset();
    }
}

void TeyssierScorer::recalculate(int p) {
    auto currentPrefix = getPrefix(p);
    bool needRecalc = !prefixes_[p] || !std::includes(
        prefixes_[p]->begin(), prefixes_[p]->end(),
        currentPrefix.begin(), currentPrefix.end());

    if (needRecalc) {
        auto pair = getGrowShrinkScore(p);
        if (!scores_[p]) {
            runningScore_ += pair.score;
        } else {
            runningScore_ += pair.score - scores_[p]->score;
        }
        scores_[p] = std::make_unique<Pair>(std::move(pair));
    }
}

double TeyssierScorer::sum() {
    double s = 0.0;
    for (int i = 0; i < static_cast<int>(pi_.size()); i++) {
        if (!scores_[i]) {
            recalculate(i);
        }
        s += scores_[i]->score;
    }
    return s;
}

bool TeyssierScorer::lastMoveSame(int i1, int i2) const {
    auto prefix0 = getPrefix(std::min(i1, i2));

    int lo = std::min(i1, i2);
    int hi = std::max(i1, i2);

    for (int i = lo; i <= hi; i++) {
        prefix0.insert(get(i));
        if (!prefixes_[i] || *prefixes_[i] != prefix0) return false;
    }

    return true;
}

TeyssierScorer::Pair TeyssierScorer::getGrowShrinkScore(int p) {
    const NodePtr& n = pi_[p];

    std::unordered_set<NodePtr> prefix;
    for (int j = 0; j < p; j++) {
        prefix.insert(pi_[j]);
    }

    std::unordered_set<NodePtr> all(variables_.begin(), variables_.end());
    std::unordered_set<NodePtr> parentsUnordered;
    double sMax = trees_[n]->trace(prefix, all, parentsUnordered);

    Pair result;
    result.parents = std::set<NodePtr>(parentsUnordered.begin(), parentsUnordered.end());
    result.score = std::isnan(sMax) ? -std::numeric_limits<double>::infinity() : sMax;
    return result;
}

bool TeyssierScorer::violatesKnowledge(const std::vector<NodePtr>& order) const {
    if (knowledge_.isEmpty()) return false;

    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        for (int j = 0; j < i; j++) {
            if (knowledge_.isRequired(order[i]->getName(), order[j]->getName())) {
                return true;
            }
        }
    }

    return false;
}

} // namespace tetrad
