#include "search/grasp.h"
#include "util/java_hash.h"
#include "util/log_stream.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace tetrad {

Grasp::Grasp(Score& score)
    : score_(score), variables_(score.getVariables()) {}

std::vector<NodePtr> Grasp::bestOrder(const std::vector<NodePtr>& order) {
    std::mt19937 rng;
    if (seed_ != -1) {
        rng.seed(static_cast<unsigned>(seed_));
    } else {
        rng.seed(42);
    }

    auto currentOrder = order;

    scorer_ = std::make_unique<TeyssierScorer>(score_);
    scorer_->setKnowledge(knowledge_);
    scorer_->clearBookmarks();

    std::vector<NodePtr> bestPerm;
    double best = -std::numeric_limits<double>::infinity();

    scorer_->score(currentOrder);

    for (int r = 0; r < numStarts_; r++) {
        if ((r == 0 && !useDataOrder_) || r > 0) {
            std::shuffle(currentOrder.begin(), currentOrder.end(), rng);
        }

        makeValidKnowledgeOrder(currentOrder);
        scorer_->score(currentOrder);

        auto perm = grasp(*scorer_);
        scorer_->score(perm);

        if (scorer_->score() > best) {
            best = scorer_->score();
            bestPerm = perm;
        }
    }

    if (bestPerm.empty()) return {};

    scorer_->score(bestPerm);
    return bestPerm;
}

Graph Grasp::getGraph(bool cpDag) {
    if (!scorer_) {
        throw std::runtime_error("Please run algorithm first.");
    }
    return scorer_->getGraph(cpDag);
}

int Grasp::getNumEdges() {
    return scorer_->getNumEdges();
}

std::vector<NodePtr> Grasp::grasp(TeyssierScorer& scorer) {
    scorer.clearBookmarks();
    std::vector<std::vector<int>> depths;

    // GRaSP-TSP
    if (ordered_ && uncoveredDepth_ != 0 && nonSingularDepth_ != 0) {
        depths.push_back({depth_ < 1 ? std::numeric_limits<int>::max() : depth_, 0, 0});
    }

    // GRaSP-ESP
    if (ordered_ && nonSingularDepth_ != 0) {
        depths.push_back({depth_ < 1 ? std::numeric_limits<int>::max() : depth_,
                          uncoveredDepth_ < 0 ? std::numeric_limits<int>::max() : uncoveredDepth_, 0});
    }

    // GRaSP (full)
    depths.push_back({depth_ < 1 ? std::numeric_limits<int>::max() : depth_,
                      uncoveredDepth_ < 0 ? std::numeric_limits<int>::max() : uncoveredDepth_,
                      nonSingularDepth_ < 0 ? std::numeric_limits<int>::max() : nonSingularDepth_});

    double sNew = scorer.score();
    double sOld;

    for (const auto& depth : depths) {
        do {
            sOld = sNew;
            std::set<std::set<NodePtr>> tucks;
            std::set<std::set<std::set<NodePtr>>> dfsHistory;
            graspDfs(scorer, sOld, depth, 1, tucks, dfsHistory);
            sNew = scorer.score();
        } while (sNew > sOld);
    }

    return scorer.getPi();
}

void Grasp::graspDfs(TeyssierScorer& scorer, double sOld, const std::vector<int>& depth,
                     int currentDepth, std::set<std::set<NodePtr>>& tucks,
                     std::set<std::set<std::set<NodePtr>>>& dfsHistory) {
    auto vars = scorer.getPi();

    if (allowInternalRandomness_) {
        std::mt19937 rng(42);
        std::shuffle(vars.begin(), vars.end(), rng);
    }

    for (const auto& y : vars) {
        auto ancestors = scorer.getAncestors(y);
        auto parentSet = scorer.getParents(y);

        // Java: List<Node> parents = new ArrayList<>(scorer.getParents(y));
        // (Grasp.java:449). getParents returns a HashSet<Node>, so Java's
        // iteration order is name-hash bucket order — arbitrary but deterministic.
        // std::set<NodePtr> orders by raw pointer address instead, which varies
        // with heap layout between runs in the same process. Since the loop below
        // returns on the first improving tuck, that order fully determines the
        // result: without this, GRaSP is non-deterministic.
        std::vector<NodePtr> parents(parentSet.begin(), parentSet.end());
        sortByJavaHashOrder(parents);

        for (const auto& x : parents) {
            bool covered = scorer.coveredEdge(x, y);
            bool singular = true;

            std::set<NodePtr> tuck;
            tuck.insert(x);
            tuck.insert(y);

            if (covered && tucks.count(tuck)) continue;
            if (currentDepth > depth[1] && !covered) continue;

            int xIdx = scorer.index(x);
            int yIdx = scorer.index(y);

            scorer.bookmark(currentDepth);

            // Perform the tuck: move y and ancestors of x between x and y
            bool first = true;
            int i = xIdx;

            // Collect nodes between x and y
            std::vector<NodePtr> Z;
            for (int k = xIdx + 1; k < yIdx; k++) {
                Z.push_back(scorer.get(k));
            }

            auto zItr = Z.begin();
            if (first) {
                scorer.moveTo(y, i);
                first = false;
            }
            while (zItr != Z.end()) {
                const auto& z = *zItr;
                if (ancestors.count(z)) {
                    if (scorer.getParents(z).count(x)) {
                        singular = false;
                    }
                    scorer.moveTo(z, i++);
                }
                ++zItr;
            }

            if (currentDepth > depth[2] && !singular) {
                scorer.goToBookmark(currentDepth);
                continue;
            }

            if (violatesKnowledge(scorer.getPi())) {
                scorer.goToBookmark(currentDepth);
                continue;
            }

            double sNew = scorer.score();
            if (sNew > sOld) {
                // Found improvement — return immediately.
                // Java logs here (Grasp.java:505-508); the format is matched so the
                // two traces can be diffed directly to locate the first divergent
                // decision. Note C++ scores are 2x Java's (BIC convention), so
                // compare the tuck sequence and edge counts, not the raw deltas.
                if (verbose_) {
                    logStream() << "Edges: " << scorer.getNumEdges()
                                << " \t|\t Score Improvement: " << (sNew - sOld)
                                << " \t|\t Tuck: [" << x->getName() << ", " << y->getName()
                                << "] depth " << currentDepth << "\n";
                }
                return;
            }

            if (sNew == sOld && currentDepth < depth[0]) {
                tucks.insert(tuck);
                if (currentDepth > depth[1]) {
                    if (!dfsHistory.count(tucks)) {
                        dfsHistory.insert(tucks);
                        graspDfs(scorer, sOld, depth, currentDepth + 1, tucks, dfsHistory);
                    }
                } else {
                    graspDfs(scorer, sOld, depth, currentDepth + 1, tucks, dfsHistory);
                }
                tucks.erase(tuck);
            }

            if (scorer.score() > sOld) return;

            scorer.goToBookmark(currentDepth);
        }
    }
}

void Grasp::makeValidKnowledgeOrder(std::vector<NodePtr>& order) {
    if (knowledge_.isEmpty()) return;

    int idx = 0;

    // First: variables not in any tier
    auto notInTiers = knowledge_.getVariablesNotInTiers();
    std::set<std::string> tier(notInTiers.begin(), notInTiers.end());
    for (int i = 0; i < static_cast<int>(order.size()); i++) {
        if (tier.count(order[i]->getName())) {
            auto x = order[i];
            order.erase(order.begin() + i);
            order.insert(order.begin() + idx++, x);
        }
    }

    // Then: each tier in order
    for (int t = 0; t < knowledge_.getNumTiers(); t++) {
        auto tierVars = knowledge_.getTier(t);
        std::set<std::string> tierSet(tierVars.begin(), tierVars.end());
        for (int j = 0; j < static_cast<int>(order.size()); j++) {
            if (tierSet.count(order[j]->getName())) {
                auto x = order[j];
                order.erase(order.begin() + j);
                order.insert(order.begin() + idx++, x);
            }
        }
    }

    // Fix required edges
    if (knowledge_.isEmpty()) return;
    for (int i = 1; i < static_cast<int>(order.size()); i++) {
        const std::string& a = order[i]->getName();
        for (int j = 0; j < i; j++) {
            const std::string& b = order[j]->getName();
            if (knowledge_.isRequired(a, b)) {
                auto x = order[i];
                order.erase(order.begin() + i);
                order.insert(order.begin() + j, x);
                break;
            }
        }
    }
}

bool Grasp::violatesKnowledge(const std::vector<NodePtr>& order) const {
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
