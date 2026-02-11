# Implementation Plan: Tetrad C++ Port - PC Algorithm Vertical Slice

## Context

The Tetrad library (CMU) is a mature Java causal inference library (~2100 Java files). We are porting it to C++ for easier Python/R interfacing. Rather than a big-bang rewrite, we use a **"Vertical Slice" approach**: get one algorithm (PC with Fisher Z test) working end-to-end before expanding.

The Java PC algorithm has a clear dependency tree of ~5000-6000 lines across ~15 classes. Many can be significantly simplified by dropping GUI concerns, serialization, and advanced options that aren't needed for the first pass.

## Architecture Decisions

- **C++17** standard (broad compiler support, structured bindings, `std::optional`, `if constexpr`)
- **CMake** build system with `FetchContent` for dependencies
- **Eigen** for linear algebra (header-only, standard in C++ scientific computing)
- **Catch2** for unit testing
- **nanobind** for Python bindings (deferred to after core C++ works)
- **Node ownership**: `std::shared_ptr<Node>` throughout (simple, correct, matches Java's reference semantics)
- **Namespace**: `tetrad::`
- **No Boost dependency** - keep the dependency tree minimal

## Project Structure

```
tetrad-port/
├── CMakeLists.txt
├── src/
│   ├── graph/
│   │   ├── endpoint.h           # Endpoint enum
│   │   ├── node_type.h          # NodeType enum
│   │   ├── node.h / node.cpp    # Node class
│   │   ├── edge.h / edge.cpp    # Edge class
│   │   ├── triple.h             # Triple class (header-only)
│   │   └── graph.h / graph.cpp  # EdgeListGraph
│   ├── data/
│   │   ├── knowledge.h / knowledge.cpp  # Background knowledge
│   │   └── data_set.h / data_set.cpp    # Simple data matrix wrapper
│   ├── search/
│   │   ├── independence_test.h   # Abstract base class
│   │   ├── independence_result.h # Result struct
│   │   ├── ind_test_fisher_z.h / ind_test_fisher_z.cpp
│   │   ├── sepset_map.h / sepset_map.cpp
│   │   ├── fas.h / fas.cpp       # Fast Adjacency Search
│   │   ├── meek_rules.h / meek_rules.cpp
│   │   └── pc.h / pc.cpp         # PC algorithm
│   └── util/
│       └── choice_generator.h / choice_generator.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_node_edge.cpp
│   ├── test_graph.cpp
│   ├── test_choice_generator.cpp
│   ├── test_fisher_z.cpp
│   ├── test_fas.cpp
│   ├── test_meek_rules.cpp
│   └── test_pc.cpp
└── examples/
    └── run_pc.cpp               # Simple CLI example
```

## Implementation Steps (Bottom-Up by Dependency Layer)

### Step 1: Project Skeleton & Build System
- Create directory structure
- Write root `CMakeLists.txt` with:
  - FetchContent for Eigen, Catch2
  - Library target `tetrad_cpp` (STATIC)
  - Test target
  - Example target
- Verify it compiles with an empty source file

### Step 2: Core Types - Endpoint, NodeType, Node
**Java sources**: `Endpoint.java` (112 lines), `GraphNode.java`, `Node.java` (305 lines)

- `endpoint.h`: `enum class Endpoint { TAIL, ARROW, CIRCLE, NULL_EP }`
- `node_type.h`: `enum class NodeType { MEASURED, LATENT, ERROR }`
- `node.h/cpp`: Simplified Node class
  - Fields: `name_` (string), `type_` (NodeType)
  - Equality/hash by name
  - `like(name)` factory method
  - Drop: GUI coordinates, PropertyChangeListener, attributes, selection bias, NodeVariableType

### Step 3: Edge
**Java source**: `Edge.java` (657 lines)

- Fields: `node1_`, `node2_` (shared_ptr<Node>), `endpoint1_`, `endpoint2_`
- Key methods: `isDirected()`, `pointsTowards(node)`, `getDistalNode(node)`, `reverse()`
- Equality: symmetric (A-B == B-A)
- Drop: color, bold, highlighted, properties, probabilities (GUI concerns)

### Step 4: Triple
**Java source**: `Triple.java` (217 lines)

- Header-only struct: three Node pointers (x, y, z)
- Equality symmetric in x and z: `(x,y,z) == (z,y,x)`
- Hash consistent with equality

### Step 5: Graph (EdgeListGraph)
**Java source**: `EdgeListGraph.java` (1640 lines)

This is the largest single class. Port the core operations needed by PC:

**Storage**: `unordered_map<Node*, unordered_set<Edge>>` for adjacency + `vector<shared_ptr<Node>>` for node list + `unordered_set<Edge>` for all edges

**Required methods** (only what PC/FAS/MeekRules actually call):
- Node operations: `addNode`, `getNode(name)`, `getNodes`, `getNumNodes`, `containsNode`
- Edge operations: `addEdge`, `removeEdge`, `getEdge(n1,n2)`, `getEdges()`, `getEdges(node)`, `setEndpoint`, `getEndpoint`
- Adjacency: `isAdjacentTo`, `getAdjacentNodes`
- Directed queries: `getChildren`, `getParents`, `isParentOf`, `isChildOf`, `getIndegree`, `getOutdegree`
- Collider detection: `isDefCollider`, `isDefNoncollider`
- Structural: `reorientAllWith(endpoint)`, `clear`, `getNumEdges`
- Triple tracking: `getAmbiguousTriples`, `setAmbiguousTriples`, `isAmbiguousTriple`
- Convenience: `addDirectedEdge`, `addUndirectedEdge`, `addBidirectedEdge`
- Paths: `existsDirectedPath` (for cycle detection in MeekRules) - implement as simple BFS, don't need the full 3153-line Paths class

**Drop**: ancillary graphs, ancestor cache, PropertyChangeSupport, serialization

### Step 6: ChoiceGenerator
**Java source**: `ChoiceGenerator.java`

Port the iterative combination generator exactly. This is critical for FAS performance.
- `ChoiceGenerator(int n, int k)` - generate all k-subsets of {0..n-1}
- `next()` returns pointer to internal array or nullptr when exhausted
- Lexicographic order, non-recursive algorithm

### Step 7: SepsetMap
**Java source**: `SepsetMap.java` (251 lines)

- Storage: `map<pair<Node*,Node*>, set<Node*>>` for separation sets
- Key operations: `set(x, y, z)`, `get(x, y)`, `getPValue(x, y)`
- Pair keys should be order-independent (canonicalize by pointer comparison or name)

### Step 8: Knowledge (Stub)
**Java source**: `Knowledge.java`

Minimal stub for the first pass:
- `isForbidden(from, to)` -> always returns false
- `isRequired(from, to)` -> always returns false
- `isEmpty()` -> always returns true
- This lets all PC code compile and run without knowledge constraints
- Full implementation deferred

### Step 9: DataSet (Simple)
- Thin wrapper around `Eigen::MatrixXd` + variable names
- Constructor from matrix + column names
- `getCorrelationMatrix()` method
- Optional: CSV loading utility

### Step 10: IndependenceResult + IndependenceTest Interface
**Java sources**: `IndependenceResult.java` (226 lines), `IndependenceTest.java` (227 lines)

- `IndependenceResult`: struct with `independent` (bool), `p_value` (double), `score` (double)
- `IndependenceTest`: abstract base class with:
  - `virtual IndependenceResult checkIndependence(Node* x, Node* y, const set<Node*>& z) = 0`
  - `virtual vector<shared_ptr<Node>> getVariables() = 0`
  - `virtual int getSampleSize() = 0`

### Step 11: IndTestFisherZ
**Java source**: `IndTestFisherZ.java` (866 lines)

The core statistical test. Algorithm:
1. Extract correlation submatrix for {x, y} ∪ z
2. Compute precision matrix via Cholesky decomposition (Eigen's `LDLT` or `LLT`)
3. Extract partial correlation: `r = -P[0,1] / sqrt(P[0,0] * P[1,1])`
4. Fisher Z transform: `q = 0.5 * log((1+|r|)/(1-|r|))`
5. Test statistic: `z = sqrt(n - 3 - |z|) * q`
6. P-value: `2 * (1 - Phi(|z|))` using standard normal CDF

**First pass simplifications**:
- No shrinkage modes (NONE only)
- No pseudoinverse fallback (add later if needed)
- No row subsetting
- Use Eigen's built-in Cholesky for matrix inversion
- Use `std::erfc` for normal CDF approximation

### Step 12: FAS (Fast Adjacency Search)
**Java source**: `Fas.java` (554 lines)

The skeleton discovery phase. Algorithm:
1. Start with complete undirected graph over all variables
2. For depth d = 0, 1, 2, ... up to maxDepth:
   - For each adjacent pair (X, Y):
     - For each subset S of adj(X)\{Y} of size d (using ChoiceGenerator):
       - Call independenceTest.checkIndependence(X, Y, S)
       - If independent: remove edge X-Y, store sepset(X,Y) = S, break
   - If stable mode: defer edge removals until end of depth iteration
3. Return skeleton graph + SepsetMap

**Key**: implement the "stable" variant (default in Java) where edge removals within a depth level are deferred.

### Step 13: MeekRules
**Java source**: `MeekRules.java` (549 lines)

Orientation propagation. Apply rules repeatedly until no changes:
- **R1**: X→Y—Z and X,Z not adjacent → Y→Z
- **R2**: X→Y→Z and X—Z → X→Z
- **R3**: X—Y, X—Z, Y→W←Z, W—X not adjacent → ... (uncovered paths)
- **R4**: Discriminating paths

Implement cycle prevention check: before orienting an edge, verify it doesn't create a directed cycle (use BFS/DFS on directed edges).

### Step 14: PC Algorithm
**Java source**: `Pc.java` (1177 lines)

Three-phase orchestrator:
1. Run FAS → skeleton + sepsets
2. Orient colliders: for each unshielded triple (X, Z, Y) where X—Z—Y and X not adj Y:
   - If Z not in sepset(X, Y) → orient X→Z←Y
3. Run MeekRules → propagate to CPDAG

**First pass**: SEPSETS collider strategy only (skip CONSERVATIVE and MAX_P). No timeout. No forbidden directed cycles check beyond what MeekRules does.

### Step 15: Unit Tests
Write Catch2 tests for each component:
- `test_node_edge.cpp`: Node creation, equality, Edge directionality, Endpoint
- `test_graph.cpp`: Add/remove nodes and edges, adjacency queries, collider detection
- `test_choice_generator.cpp`: Verify correct combinations generated (compare against known C(n,k) counts)
- `test_fisher_z.cpp`: Known partial correlations, p-values against hand-computed values
- `test_fas.cpp`: Small known graph, verify skeleton recovery
- `test_meek_rules.cpp`: Known orientation patterns
- `test_pc.cpp`: End-to-end on small synthetic dataset, verify CPDAG structure

### Step 16: Integration Example
- `examples/run_pc.cpp`: Load a CSV, run PC, print resulting edges
- Can be used to generate output for conformance testing against Java

## Future Steps (Not in This Plan)
- Python bindings via nanobind
- R bindings via Rcpp/cpp11
- Golden Master conformance testing against Java
- Knowledge (full implementation)
- CONSERVATIVE and MAX_P collider strategies
- Additional algorithms (FCI, GES, etc.)
- Shrinkage modes for FisherZ
- Performance optimization

## Verification Plan

1. **Build**: `cmake --build build` compiles cleanly with no warnings (`-Wall -Wextra`)
2. **Unit tests**: `ctest --test-dir build` passes all tests
3. **Numerical correctness**: FisherZ tests compare against hand-computed values (and optionally against Java output)
4. **End-to-end**: Run PC on a small dataset (e.g., 5 variables, 1000 samples from a known DAG), verify the output CPDAG matches the expected structure
5. **Valgrind/ASAN**: No memory leaks or undefined behavior

## Key Java Source Files (Reference)

| C++ Component | Java Source | Lines |
|---|---|---|
| Endpoint | `tetrad/tetrad-lib/.../graph/Endpoint.java` | 112 |
| Node | `tetrad/tetrad-lib/.../graph/GraphNode.java` + `Node.java` | ~400 |
| Edge | `tetrad/tetrad-lib/.../graph/Edge.java` | 657 |
| Triple | `tetrad/tetrad-lib/.../graph/Triple.java` | 217 |
| Graph | `tetrad/tetrad-lib/.../graph/EdgeListGraph.java` | 1640 |
| ChoiceGenerator | `tetrad/tetrad-lib/.../util/ChoiceGenerator.java` | ~180 |
| SepsetMap | `tetrad/tetrad-lib/.../search/utils/SepsetMap.java` | 251 |
| Knowledge | `tetrad/tetrad-lib/.../data/Knowledge.java` | ~100 |
| IndTestFisherZ | `tetrad/tetrad-lib/.../search/test/IndTestFisherZ.java` | 866 |
| FAS | `tetrad/tetrad-lib/.../search/Fas.java` | 554 |
| MeekRules | `tetrad/tetrad-lib/.../search/utils/MeekRules.java` | 549 |
| PC | `tetrad/tetrad-lib/.../search/Pc.java` | 1177 |
