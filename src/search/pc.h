#pragma once

#include "graph/graph.h"
#include "search/independence_test.h"
#include "search/fas.h"
#include "search/meek_rules.h"
#include "search/sepset_map.h"
#include "data/knowledge.h"

namespace tetrad {

class Pc {
public:
    explicit Pc(IndependenceTest* test);

    Graph search();
    Graph search(const std::vector<NodePtr>& nodes);

    void setDepth(int depth) { depth_ = depth; }
    void setFasStable(bool stable) { fasStable_ = stable; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }

    Fas* getFas() { return fas_.get(); }
    const SepsetMap& getSepsets() const { return fas_->getSepsets(); }

private:
    struct TripleInfo {
        NodePtr x, z, y;  // x--z--y unshielded triple
    };

    std::vector<TripleInfo> collectUnshieldedTriples(const Graph& g) const;
    void pcOrientbk(Graph& g, const std::vector<NodePtr>& nodes);
    void orientUnshieldedTriples(Graph& g, const SepsetMap& sepsets);
    void applyMeekRules(Graph& g);
    bool colliderAllowed(const NodePtr& x, const NodePtr& z, const NodePtr& y) const;
    bool canOrientCollider(const Graph& g, const NodePtr& x,
                           const NodePtr& z, const NodePtr& y) const;
    static void orientCollider(Graph& g, const NodePtr& x,
                               const NodePtr& z, const NodePtr& y);

    IndependenceTest* test_;
    Knowledge knowledge_;
    int depth_ = -1;
    bool fasStable_ = true;
    bool verbose_ = false;
    std::unique_ptr<Fas> fas_;
};

} // namespace tetrad
