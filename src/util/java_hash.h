#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include "graph/edge.h"
#include "graph/node.h"

namespace tetrad {

// Replicate Java's String.hashCode(): h = 31 * h + c for each character.
// Uses int32_t to match Java's 32-bit signed int overflow behavior.
inline int32_t javaStringHashCode(const std::string& s) {
    int32_t h = 0;
    for (char c : s) {
        h = 31 * h + static_cast<int32_t>(c);
    }
    return h;
}

// Java 8+ HashMap.hash() perturbation: h ^ (h >>> 16).
// The unsigned right shift is emulated by casting to uint32_t.
inline int32_t javaHashMapPerturb(int32_t h) {
    return h ^ static_cast<int32_t>(static_cast<uint32_t>(h) >> 16);
}

// Bucket index in Java's HashMap: (capacity - 1) & hash(hashCode).
inline int javaHashBucket(int32_t hashCode, int capacity) {
    return static_cast<int>(static_cast<uint32_t>(javaHashMapPerturb(hashCode)) & static_cast<uint32_t>(capacity - 1));
}

// Smallest power-of-2 capacity for a Java HashSet holding n elements.
// Java's default load factor is 0.75, initial capacity 16.
inline int javaHashSetCapacity(int n) {
    if (n == 0) return 16;
    int target = static_cast<int>((static_cast<double>(n) / 0.75) + 1);
    int cap = 1;
    while (cap < target) cap <<= 1;
    return cap < 16 ? 16 : cap;
}

// Sort a vector of NodePtr into Java's HashSet<Node> iteration order.
// Java's HashSet iterates through buckets 0..capacity-1; within each bucket
// in linked-list (insertion) order. For getAdjacentNodes(), the insertion order
// comes from edge HashSet iteration. We use edge hash as tiebreaker for
// within-bucket collisions.
//
// queryNode: the node whose adjacency list we're sorting (needed for edge hash tiebreaker)
inline void sortByJavaHashOrder(std::vector<NodePtr>& nodes, const NodePtr& queryNode = nullptr) {
    if (nodes.size() <= 1) return;

    int capacity = javaHashSetCapacity(static_cast<int>(nodes.size()));

    // Precompute bucket index for each node
    // Edge hash tiebreaker: Edge.hashCode() = node1.hashCode() + node2.hashCode()
    int32_t queryHash = queryNode ? javaStringHashCode(queryNode->getName()) : 0;

    std::stable_sort(nodes.begin(), nodes.end(),
        [capacity, queryHash](const NodePtr& a, const NodePtr& b) {
            int32_t hashA = javaStringHashCode(a->getName());
            int32_t hashB = javaStringHashCode(b->getName());
            int bucketA = javaHashBucket(hashA, capacity);
            int bucketB = javaHashBucket(hashB, capacity);
            if (bucketA != bucketB) return bucketA < bucketB;

            // Within-bucket tiebreaker: order by edge hash bucket in the edge HashSet.
            // Edge.hashCode() = node1.hashCode() + node2.hashCode()
            // The edge HashSet also has its own capacity based on the number of edges.
            // For simplicity, use the raw edge hash as tiebreaker.
            int32_t edgeHashA = queryHash + hashA;
            int32_t edgeHashB = queryHash + hashB;
            return javaHashMapPerturb(edgeHashA) < javaHashMapPerturb(edgeHashB);
        });
}

// Sort a vector of Edges into Java's HashSet<Edge> iteration order.
// Edge.hashCode() = node1.hashCode() + node2.hashCode() (symmetric/unordered).
// edgesSetSize: total number of edges in the graph's edgesSet (determines HashSet capacity).
inline void sortEdgesByJavaHashOrder(std::vector<Edge>& edges, int edgesSetSize) {
    if (edges.size() <= 1) return;

    int capacity = javaHashSetCapacity(edgesSetSize);

    std::stable_sort(edges.begin(), edges.end(),
        [capacity](const Edge& a, const Edge& b) {
            int32_t hashA = javaStringHashCode(a.getNode1()->getName())
                          + javaStringHashCode(a.getNode2()->getName());
            int32_t hashB = javaStringHashCode(b.getNode1()->getName())
                          + javaStringHashCode(b.getNode2()->getName());
            int bucketA = javaHashBucket(hashA, capacity);
            int bucketB = javaHashBucket(hashB, capacity);
            return bucketA < bucketB;
        });
}

} // namespace tetrad
