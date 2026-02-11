#include "search/pc.h"
#include <algorithm>
#include <iostream>

namespace tetrad {

Pc::Pc(IndependenceTest* test) : test_(test) {}

Graph Pc::search() {
    return search(test_->getVariables());
}

Graph Pc::search(const std::vector<NodePtr>& nodes) {
    // Phase 1: Skeleton via FAS
    fas_ = std::make_unique<Fas>(test_);
    fas_->setKnowledge(knowledge_);
    fas_->setDepth(depth_);
    fas_->setStable(fasStable_);
    fas_->setVerbose(verbose_);

    Graph g = fas_->search(nodes);
    SepsetMap sepsets = fas_->getSepsets();

    if (verbose_) {
        std::cout << "FAS complete. Skeleton has " << g.getNumEdges() << " edges." << std::endl;
    }

    // Phase 2: Orient colliders
    orientUnshieldedTriples(g, sepsets);

    if (verbose_) {
        std::cout << "Collider orientation complete." << std::endl;
    }

    // Phase 3: Meek rules to closure
    applyMeekRules(g);

    if (verbose_) {
        std::cout << "Meek rules complete." << std::endl;
    }

    return g;
}

std::vector<Pc::TripleInfo> Pc::collectUnshieldedTriples(const Graph& g) const {
    auto nodes = g.getNodes();
    std::sort(nodes.begin(), nodes.end(),
        [](const NodePtr& a, const NodePtr& b) { return a->getName() < b->getName(); });

    std::vector<TripleInfo> triples;

    for (const auto& z : nodes) {
        auto adj = g.getAdjacentNodes(z);
        std::sort(adj.begin(), adj.end(),
            [](const NodePtr& a, const NodePtr& b) { return a->getName() < b->getName(); });

        int m = static_cast<int>(adj.size());
        for (int i = 0; i < m; i++) {
            for (int j = i + 1; j < m; j++) {
                auto xi = adj[i];
                auto yj = adj[j];
                if (!g.isAdjacentTo(xi, yj)) {
                    // Canonicalize: x < y by name
                    auto x = xi;
                    auto y = yj;
                    if (x->getName() > y->getName()) std::swap(x, y);
                    triples.push_back({x, z, y});
                }
            }
        }
    }

    // Sort for deterministic order
    std::sort(triples.begin(), triples.end(),
        [](const TripleInfo& a, const TripleInfo& b) {
            if (a.x->getName() != b.x->getName()) return a.x->getName() < b.x->getName();
            if (a.z->getName() != b.z->getName()) return a.z->getName() < b.z->getName();
            return a.y->getName() < b.y->getName();
        });

    return triples;
}

void Pc::orientUnshieldedTriples(Graph& g, const SepsetMap& sepsets) {
    auto triples = collectUnshieldedTriples(g);

    for (const auto& t : triples) {
        // Already a collider? Skip
        if (g.isParentOf(t.x, t.z) && g.isParentOf(t.y, t.z)) continue;

        // SEPSETS strategy: z not in sepset(x, y) → collider
        auto S = sepsets.get(t.x, t.y);
        if (!S.has_value()) continue;

        // Check if z is in the separation set
        bool zInSepset = false;
        for (const auto& node : S.value()) {
            if (*node == *t.z) {
                zInSepset = true;
                break;
            }
        }

        if (!zInSepset) {
            // z not in sepset → orient as collider x → z ← y
            if (canOrientCollider(g, t.x, t.z, t.y)) {
                orientCollider(g, t.x, t.z, t.y);
                if (verbose_) {
                    std::cout << "Collider: " << t.x->getName() << " -> "
                              << t.z->getName() << " <- " << t.y->getName() << std::endl;
                }
            }
        }
    }
}

bool Pc::canOrientCollider(const Graph& g, const NodePtr& x,
                            const NodePtr& z, const NodePtr& y) const {
    if (!g.isAdjacentTo(x, z) || !g.isAdjacentTo(z, y)) return false;

    // Prevent directed cycles
    if (g.existsDirectedPath(z, x)) return false;
    if (g.existsDirectedPath(z, y)) return false;

    // Prevent bidirected edges
    if (g.isParentOf(z, x) || g.isParentOf(z, y)) return false;

    return true;
}

void Pc::orientCollider(Graph& g, const NodePtr& x, const NodePtr& z, const NodePtr& y) {
    // Orient x → z
    Edge e1 = g.getEdge(x, z);
    if (!e1.isNull()) {
        g.removeEdge(e1);
        g.addDirectedEdge(x, z);
    }

    // Orient y → z
    Edge e2 = g.getEdge(y, z);
    if (!e2.isNull()) {
        g.removeEdge(e2);
        g.addDirectedEdge(y, z);
    }
}

void Pc::applyMeekRules(Graph& g) {
    MeekRules meekRules;
    meekRules.setKnowledge(knowledge_);
    meekRules.setMeekPreventCycles(true);
    meekRules.setVerbose(verbose_);
    meekRules.setRevertToUnshieldedColliders(false);
    meekRules.orientImplied(g);
}

} // namespace tetrad
