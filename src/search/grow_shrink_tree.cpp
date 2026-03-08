#include "search/grow_shrink_tree.h"
#include <algorithm>
#include <limits>

namespace tetrad {

// --- GrowShrinkTree ---

GrowShrinkTree::GrowShrinkTree(Score& score,
                               const std::unordered_map<NodePtr, int>& index,
                               const NodePtr& node)
    : score_(score), index_(index), node_(node),
      nodeIndex_(index.at(node)),
      root_(std::make_unique<GSTNode>(this)) {}

double GrowShrinkTree::trace(const std::unordered_set<NodePtr>& prefix,
                             const std::unordered_set<NodePtr>& all) {
    std::unordered_set<NodePtr> parents;
    return trace(prefix, all, parents);
}

double GrowShrinkTree::trace(const std::unordered_set<NodePtr>& prefix,
                             const std::unordered_set<NodePtr>& all,
                             std::unordered_set<NodePtr>& parents) {
    std::unordered_set<NodePtr> available(all);
    available.erase(node_);
    for (const auto& f : forbidden_) {
        available.erase(f);
    }
    return root_->trace(prefix, available, parents);
}

std::vector<NodePtr> GrowShrinkTree::getFirstLayer() const {
    std::vector<NodePtr> firstLayer;
    for (const auto& branch : root_->branches) {
        firstLayer.push_back(branch->add);
    }
    return firstLayer;
}

int GrowShrinkTree::getIndex(const NodePtr& node) const {
    return index_.at(node);
}

double GrowShrinkTree::localScore() const {
    double s = score_.localScore(nodeIndex_);
    return std::isnan(s) ? 0.0 : s;
}

double GrowShrinkTree::localScore(const std::vector<int>& parentIndices) const {
    double s = score_.localScore(nodeIndex_, parentIndices);
    return std::isnan(s) ? -std::numeric_limits<double>::infinity() : s;
}

bool GrowShrinkTree::isRequired(const NodePtr& node) const {
    for (const auto& r : required_) {
        if (r == node) return true;
    }
    return false;
}

bool GrowShrinkTree::isForbidden(const NodePtr& node) const {
    for (const auto& f : forbidden_) {
        if (f == node) return true;
    }
    return false;
}

void GrowShrinkTree::setKnowledge(const std::vector<NodePtr>& required,
                                   const std::vector<NodePtr>& forbidden) {
    required_ = required;
    forbidden_ = forbidden;
    reset();
}

void GrowShrinkTree::reset() {
    root_ = std::make_unique<GSTNode>(this);
}

// --- GSTNode ---

GrowShrinkTree::GSTNode::GSTNode(GrowShrinkTree* tree)
    : tree(tree), add(nullptr), growScore(tree->localScore()),
      shrinkScore(0.0), growDone(false), shrinkDone(false) {}

GrowShrinkTree::GSTNode::GSTNode(GrowShrinkTree* tree, const NodePtr& add,
                                  const std::unordered_set<NodePtr>& parents)
    : tree(tree), add(add), shrinkScore(0.0),
      growDone(false), shrinkDone(false) {
    std::vector<int> X;
    X.reserve(parents.size() + 1);
    for (const auto& parent : parents) {
        X.push_back(tree->getIndex(parent));
    }
    X.push_back(tree->getIndex(add));
    growScore = tree->localScore(X);
}

void GrowShrinkTree::GSTNode::grow(const std::unordered_set<NodePtr>& available,
                                    const std::unordered_set<NodePtr>& parents) {
    if (growDone) return;

    branches.clear();
    std::vector<GSTNode*> requiredBranches;

    for (const auto& addNode : available) {
        auto branch = std::make_unique<GSTNode>(tree, addNode, parents);
        if (tree->isRequired(addNode)) {
            requiredBranches.push_back(branch.get());
            branches.push_back(std::move(branch));
        } else if (branch->growScore >= this->growScore) {
            branches.push_back(std::move(branch));
        }
    }

    // Sort non-required branches by growScore descending.
    // Required branches were added first; separate them, sort the rest, then prepend required.
    // Actually, let's match Java: sort all, then insert required at front.
    // Simpler: partition required vs non-required, sort non-required, concatenate.
    std::vector<std::unique_ptr<GSTNode>> required;
    std::vector<std::unique_ptr<GSTNode>> nonRequired;

    for (auto& b : branches) {
        bool isReq = false;
        for (const auto* rp : requiredBranches) {
            if (b.get() == rp) { isReq = true; break; }
        }
        if (isReq) {
            required.push_back(std::move(b));
        } else {
            nonRequired.push_back(std::move(b));
        }
    }

    // Sort non-required by growScore descending (matching Java's reverseOrder compareTo)
    std::sort(nonRequired.begin(), nonRequired.end(),
              [](const std::unique_ptr<GSTNode>& a, const std::unique_ptr<GSTNode>& b) {
                  return a->growScore > b->growScore;
              });

    branches.clear();
    for (auto& r : required) branches.push_back(std::move(r));
    for (auto& nr : nonRequired) branches.push_back(std::move(nr));

    growDone = true;
}

void GrowShrinkTree::GSTNode::shrink(std::unordered_set<NodePtr>& parents) {
    if (shrinkDone) return;

    remove.clear();
    shrinkScore = growScore;
    if (parents.empty()) {
        shrinkDone = true;
        return;
    }

    NodePtr best;
    do {
        best = nullptr;
        std::vector<int> X;
        X.reserve(parents.size() - 1);

        for (const auto& removeNode : parents) {
            if (tree->isRequired(removeNode)) continue;

            X.clear();
            for (const auto& parent : parents) {
                if (parent != removeNode) {
                    X.push_back(tree->getIndex(parent));
                }
            }

            double s = tree->localScore(X);
            if (s > shrinkScore) {
                shrinkScore = s;
                best = removeNode;
            }
        }

        if (best) {
            parents.erase(best);
            remove.insert(best);
        }
    } while (best != nullptr);

    shrinkDone = true;
}

double GrowShrinkTree::GSTNode::trace(const std::unordered_set<NodePtr>& prefix,
                                       std::unordered_set<NodePtr>& available,
                                       std::unordered_set<NodePtr>& parents) {
    if (!growDone) grow(available, parents);

    for (const auto& branch : branches) {
        const NodePtr& addNode = branch->add;
        available.erase(addNode);
        if (prefix.count(addNode)) {
            parents.insert(addNode);
            return branch->trace(prefix, available, parents);
        }
    }

    if (!shrinkDone) shrink(parents);

    for (const auto& r : remove) {
        parents.erase(r);
    }
    return shrinkScore;
}

} // namespace tetrad
