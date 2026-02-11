#pragma once

#include "graph/graph.h"
#include "search/independence_test.h"
#include "search/sepset_map.h"
#include "data/knowledge.h"
#include <memory>

namespace tetrad {

class Fas {
public:
    explicit Fas(IndependenceTest* test);

    Graph search();
    Graph search(const std::vector<NodePtr>& nodes);

    SepsetMap& getSepsets() { return sepset_; }
    const SepsetMap& getSepsets() const { return sepset_; }

    void setDepth(int depth) { depth_ = depth; }
    void setStable(bool stable) { stable_ = stable; }
    void setVerbose(bool verbose) { verbose_ = verbose; }
    void setKnowledge(const Knowledge& knowledge) { knowledge_ = knowledge; }

private:
    struct EdgeRemoval {
        NodePtr x, y;
        std::set<NodePtr> S;
        double pValue;
    };

    bool searchAtDepth(const Graph& checkAdj, Graph& modify, int d);
    void decideOnePair(const Graph& checkAdj, int d, const NodePtr& x,
                       const NodePtr& y, std::vector<EdgeRemoval>& removals);
    int freeDegree(const Graph& graph) const;

    static std::vector<NodePtr> possibleParents(const NodePtr& x,
        const std::vector<NodePtr>& adjx, const Knowledge& knowledge, const NodePtr& y);

    IndependenceTest* test_;
    SepsetMap sepset_;
    Knowledge knowledge_;
    int depth_ = -1;
    bool stable_ = true;
    bool verbose_ = false;
};

} // namespace tetrad
