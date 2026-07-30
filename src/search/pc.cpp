// Port of edu.cmu.tetrad.search.Pc together with the pieces of
// edu.cmu.tetrad.search.utils.PcCommon and
// edu.cmu.tetrad.search.utils.GraphSearchUtils that Pc actually reaches
// (Tetrad 7.6.3).
//
// Pc's defaults pin down which PcCommon branches are live:
//   Pc.java:85  conflictRule      = ConflictRule.PRIORITIZE_EXISTING
//   Pc.java:87  stable            = true          -> FasType.STABLE
//   Pc.java:89  meekPreventCycles = true
//   Pc.java:91  useMaxPHeuristic  = false         -> ColliderDiscovery.FAS_SEPSETS
//   Pc.java:93  pcHeuristicType   = PcHeuristicType.NONE
// so only orientCollidersUsingSepsets + PRIORITIZE_EXISTING are transcribed.
// The MAX_P / CONSERVATIVE collider-discovery branches and the other two
// conflict rules are unreachable from Pc and are not ported (the C++ header
// exposes no setters for them).
//
// This is a deliberately literal, bug-for-bug transcription.  See
// SUSPECTED_BUGS.md before "fixing" anything.

#include "search/pc.h"

#include "util/choice_generator.h"
#include "util/java_hash.h"
#include "util/log_stream.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace tetrad {

namespace {

// GraphSearchUtils.java:766-774  translate(String a, List<Node> nodes)
NodePtr translate(const std::string& a, const std::vector<NodePtr>& nodes) {
    for (const NodePtr& node : nodes) {
        if (node->getName() == a) {
            return node;
        }
    }
    return nullptr;
}

bool setContainsNode(const std::set<NodePtr>& s, const NodePtr& n) {
    // std::set<NodePtr> orders by pointer, so a name-based scan is used rather
    // than count(), to stay correct even if the sepset holds equal-by-name
    // Node objects from a different container.  Java's HashSet<Node> compares
    // by name (GraphNode.java:210-214).
    for (const NodePtr& m : s) {
        if (m && n && *m == *n) return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
// Pc.java:101-107  public Pc(IndependenceTest independenceTest)
// ---------------------------------------------------------------------------
Pc::Pc(IndependenceTest* test) : test_(test) {
    if (test_ == nullptr) {
        throw std::invalid_argument("Independence test is null.");
    }

    // The header's getSepsets() dereferences fas_, so it must never be null.
    // Java's Pc leaves `sepsets` null until search() runs.
    fas_ = std::make_unique<Fas>(test_);
}

// ---------------------------------------------------------------------------
// Pc.java:122-125  public Graph search()
//     return search(new HashSet<>(this.independenceTest.getVariables()));
// ---------------------------------------------------------------------------
Graph Pc::search() {
    return search(test_->getVariables());
}

// ---------------------------------------------------------------------------
// Pc.java:140-146  public Graph search(Set<Node> nodes)
//   -> Pc.java:158-192  public Graph search(IFas fas, Set<Node> nodes)
//   -> PcCommon.java:150-247  search() / search(List<Node> nodes)
//
// IMPORTANT (reproduced deliberately): the `nodes` argument is used *only* for
// the domain check at Pc.java:170-174.  Pc.java:176-178 then calls
// `search.search()` -- PcCommon's *no-argument* overload -- which at
// PcCommon.java:151 runs over `getIndependenceTest().getVariables()`, i.e. all
// variables of the test, regardless of what was passed in.  Restricting the
// search to a subset of nodes therefore silently does nothing in 7.6.3.
// See SUSPECTED_BUGS.md #4.
// ---------------------------------------------------------------------------
Graph Pc::search(const std::vector<NodePtr>& nodes) {
    const std::vector<NodePtr>& allNodes = test_->getVariables();

    // Pc.java:171-174 / PcCommon.java:175-178:
    //     if (!new HashSet<>(allNodes).containsAll(nodes)) throw ...
    for (const NodePtr& n : nodes) {
        bool found = false;
        for (const NodePtr& m : allNodes) {
            if (m && n && *m == *n) { found = true; break; }
        }
        if (!found) {
            throw std::invalid_argument(
                "All of the given nodes must be in the domain of the independence "
                "test provided.");
        }
    }

    // PcCommon.java:151/160-161: the node list the rest of the search uses.
    std::vector<NodePtr> searchNodes = allNodes;

    if (verbose_) {
        logStream() << "Starting PC algorithm\n";
    }

    // PcCommon.java:180-193.  Pc always takes the STABLE branch (stable = true
    // by default), which differs from REGULAR only by fas.setStable(true).
    // setPcHeuristicType(NONE) is a no-op and has no C++ counterpart.
    fas_ = std::make_unique<Fas>(test_);
    fas_->setStable(fasStable_);
    fas_->setKnowledge(knowledge_);
    fas_->setDepth(depth_);
    fas_->setVerbose(verbose_);

    // PcCommon.java:197-198
    Graph graph = fas_->search();
    const SepsetMap& sepsets = fas_->getSepsets();

    // PcCommon.java:200-201: `if (graph.paths().existsDirectedCycle()) throw`.
    // FAS returns a purely undirected skeleton, so this can never fire; not
    // ported (it would cost a cycle check per search for no effect).

    // PcCommon.java:203
    pcOrientbk(graph, searchNodes);

    // PcCommon.java:205-206 (ColliderDiscovery.FAS_SEPSETS)
    orientUnshieldedTriples(graph, sepsets);

    // PcCommon.java:229: `this.graph = GraphUtils.replaceNodes(this.graph, nodes);`
    // replaceNodes swaps each graph node for the same-named node from `nodes`.
    // Here the graph is already built out of the test's own NodePtr objects,
    // which is exactly `nodes`, so this is a no-op and is not ported.

    // PcCommon.java:231-235
    applyMeekRules(graph);

    if (verbose_) {
        logStream() << "Finishing PC Algorithm.\n";
    }

    return graph;
}

// ---------------------------------------------------------------------------
// GraphSearchUtils.java:52-97  pcOrientbk(Knowledge bk, Graph graph, List<Node> nodes)
// ---------------------------------------------------------------------------
void Pc::pcOrientbk(Graph& g, const std::vector<NodePtr>& nodes) {
    // ITERATION ORDER: Java's forbiddenEdgesIterator() (Knowledge.java:302-305)
    // walks getListOfForbiddenEdges(), which is `new ArrayList<>(HashSet)`
    // (Knowledge.java:726+), i.e. hash order over KnowledgeEdge.  The C++
    // Knowledge is a reimplementation with its own ordering; we cannot
    // reproduce Java's.  See SUSPECTED_BUGS.md "iteration order" #C.
    for (const KnowledgeEdge& edge : knowledge_.getListOfForbiddenEdges()) {
        // match strings to variables in the graph
        NodePtr from = translate(edge.from, nodes);
        NodePtr to = translate(edge.to, nodes);

        if (!from || !to) {
            continue;
        }

        // `if (graph.getEdge(from, to) == null) continue;`
        if (!g.isAdjacentTo(from, to)) {
            continue;
        }

        // Orient to-->from  (the forbidden direction is from-->to)
        g.removeEdge(from, to);
        g.addDirectedEdge(to, from);
    }

    // Same iteration-order caveat; Knowledge.java:521-531 builds a HashSet.
    for (const KnowledgeEdge& edge : knowledge_.getListOfRequiredEdges()) {
        NodePtr from = translate(edge.from, nodes);
        NodePtr to = translate(edge.to, nodes);

        if (!from || !to) {
            continue;
        }

        if (!g.isAdjacentTo(from, to)) {
            continue;
        }

        // Orient from-->to.  Note the asymmetry with the forbidden loop above:
        // that one calls removeEdge (singular), this one removeEdges (plural).
        g.removeEdges(from, to);
        g.addDirectedEdge(from, to);
    }
}

// ---------------------------------------------------------------------------
// Enumeration half of PcCommon.java:514-576 orientCollidersUsingSepsets.
//
// Splitting enumeration from orientation is safe: the only graph mutation the
// orientation step performs is removeEdge + addDirectedEdge on the same node
// pairs (PcCommon.java:86-89), which never changes adjacency, and the
// enumeration depends on adjacency alone.
// ---------------------------------------------------------------------------
std::vector<Pc::TripleInfo> Pc::collectUnshieldedTriples(const Graph& g) const {
    std::vector<TripleInfo> triples;

    // PcCommon.java:522: `List<Node> nodes = graph.getNodes();`
    const std::vector<NodePtr>& nodes = g.getNodes();

    for (const NodePtr& b : nodes) {
        // PcCommon.java:525:
        //     List<Node> adjacentNodes = new ArrayList<>(graph.getAdjacentNodes(b));
        // EdgeListGraph.getAdjacentNodes (EdgeListGraph.java:561-573) funnels the
        // neighbours through a HashSet<Node> before returning an ArrayList, so
        // the *positions* the ChoiceGenerator indexes into are in Java hash
        // order, not insertion order.  That order decides which of a and c is
        // the first element of each pair, and hence the argument order handed to
        // orientCollider -- which matters under PRIORITIZE_EXISTING.  Modelled
        // with sortByJavaHashOrder; see SUSPECTED_BUGS.md "iteration order" #B.
        std::vector<NodePtr> adjacentNodes = g.getAdjacentNodes(b);
        sortByJavaHashOrder(adjacentNodes, b);

        if (adjacentNodes.size() < 2) {
            continue;
        }

        ChoiceGenerator cg(static_cast<int>(adjacentNodes.size()), 2);
        const int* combination;

        while ((combination = cg.next()) != nullptr) {
            const NodePtr& a = adjacentNodes[combination[0]];
            const NodePtr& c = adjacentNodes[combination[1]];

            // Skip triples that are shielded.
            if (g.isAdjacentTo(a, c)) {
                continue;
            }

            triples.push_back(TripleInfo{a, b, c});
        }
    }

    return triples;
}

// ---------------------------------------------------------------------------
// Orientation half of PcCommon.java:514-576 orientCollidersUsingSepsets.
// Triple naming: TripleInfo{x, z, y} is x--z--y with z the candidate collider,
// i.e. Java's (a, b, c) with b the collider.
// ---------------------------------------------------------------------------
void Pc::orientUnshieldedTriples(Graph& g, const SepsetMap& sepsets) {
    if (verbose_) {
        logStream() << "FAS Sepset orientation...\n";
    }

    std::vector<TripleInfo> triples = collectUnshieldedTriples(g);

    for (const TripleInfo& t : triples) {
        // PcCommon.java:547-549: `Set<Node> sepset = set.get(a, c);
        //                         if (sepset == null) continue;`
        // A null sepset (pair never separated) is distinct from an empty one.
        std::optional<std::set<NodePtr>> sepset = sepsets.get(t.x, t.y);

        if (!sepset.has_value()) continue;

        if (setContainsNode(*sepset, t.z)) continue;   // `if (!sepset.contains(b))`

        // PcCommon.java:552-562 inlines exactly the colliderAllowed() test
        // (PcCommon.java:496-508) with (a, b, c) = (x, z, y).
        if (!colliderAllowed(t.x, t.z, t.y)) continue;

        // PcCommon.java:564 -> orientCollider(a, b, c, PRIORITIZE_EXISTING, graph)
        if (canOrientCollider(g, t.x, t.z, t.y)) {
            orientCollider(g, t.x, t.z, t.y);
        }

        if (verbose_) {
            logStream() << "Collider orientation <" << t.x->getName() << ", "
                        << t.z->getName() << ", " << t.y->getName() << ">\n";
        }
    }
}

// ---------------------------------------------------------------------------
// PcCommon.java:496-508  colliderAllowed(Node x, Node y, Node z, Knowledge k)
// with y the collider node -- here z.
//
// Java's short-circuit structure (`if (!result) return false;` then the second
// pair) is equivalent to the conjunction below because `knowledge` is never
// null in this code path (PcCommon.java:47 initialises it).
// ---------------------------------------------------------------------------
bool Pc::colliderAllowed(const NodePtr& x, const NodePtr& z, const NodePtr& y) const {
    // arrowhead into z from x is allowed ...
    if (knowledge_.isRequired(z->getName(), x->getName())) return false;
    if (knowledge_.isForbidden(x->getName(), z->getName())) return false;
    // ... and arrowhead into z from y is allowed
    if (knowledge_.isRequired(z->getName(), y->getName())) return false;
    if (knowledge_.isForbidden(y->getName(), z->getName())) return false;
    return true;
}

// ---------------------------------------------------------------------------
// PcCommon.java:84-90, the PRIORITIZE_EXISTING guard:
//     if (!(graph.getEndpoint(x, y) == Endpoint.ARROW
//        && graph.getEndpoint(z, y) == Endpoint.ARROW)) { ...orient... }
//
// EdgeListGraph.getEndpoint(node1, node2) (EdgeListGraph.java:598-612) returns
// "the endpoint along the edge from node1 to node2 *at the node2 end*", so both
// tests below read the endpoint AT the collider node z.  The guard means: leave
// the triple alone only if it is already a collider at z; otherwise (re)orient,
// overwriting any single conflicting arrowhead that is already there.
// ---------------------------------------------------------------------------
bool Pc::canOrientCollider(const Graph& g, const NodePtr& x, const NodePtr& z,
                           const NodePtr& y) const {
    return !(g.getEndpoint(x, z) == Endpoint::ARROW &&
             g.getEndpoint(y, z) == Endpoint::ARROW);
}

// PcCommon.java:85-89, the body of the PRIORITIZE_EXISTING branch.
void Pc::orientCollider(Graph& g, const NodePtr& x, const NodePtr& z,
                        const NodePtr& y) {
    g.removeEdge(x, z);
    g.removeEdge(y, z);
    g.addDirectedEdge(x, z);
    g.addDirectedEdge(y, z);
}

// ---------------------------------------------------------------------------
// PcCommon.java:231-235
//     MeekRules meekRules = new MeekRules();
//     meekRules.setKnowledge(this.knowledge);
//     meekRules.setVerbose(verbose);
//     meekRules.setMeekPreventCycles(this.meekPreventCycles);
//     meekRules.orientImplied(this.graph);
//
// revertToUnshieldedColliders is left at its default of true (MeekRules.java:58),
// so orientImplied first strips every orientation that is not part of an
// unshielded collider -- including the knowledge orientations made by
// pcOrientbk, except those protected by the knowledge check at
// MeekRules.java:340-341.
// ---------------------------------------------------------------------------
void Pc::applyMeekRules(Graph& g) {
    MeekRules rules;
    rules.setKnowledge(knowledge_);
    rules.setVerbose(verbose_);
    // PcCommon's own default for meekPreventCycles is false, but Pc.java:89
    // defaults it to true and always forwards it (Pc.java:198).  The C++ header
    // exposes no setter, so the Pc default is inlined here.
    rules.setMeekPreventCycles(true);
    rules.orientImplied(g);
}

}  // namespace tetrad
