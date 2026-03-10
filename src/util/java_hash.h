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
// Models new HashSet<>(Collection c) constructor: capacity = tableSizeFor(max(c.size()/0.75+1, 16)).
inline int javaHashSetCapacity(int n) {
    if (n == 0) return 16;
    int target = static_cast<int>((static_cast<double>(n) / 0.75) + 1);
    int cap = 1;
    while (cap < target) cap <<= 1;
    return cap < 16 ? 16 : cap;
}

// Capacity of Java HashSet created with default constructor (new HashSet<>()) after n insertions.
// Default initial capacity 16, load factor 0.75, threshold 12.
// Resizes to 2x when size exceeds threshold (size > cap * 0.75).
inline int javaHashSetDefaultCapacity(int n) {
    int cap = 16;
    int threshold = 12;
    while (n > threshold) {
        cap <<= 1;
        threshold = static_cast<int>(cap * 0.75);
    }
    return cap;
}

// Sort a vector of NodePtr into Java's HashSet<Node> iteration order.
// Java's getAdjacentNodes(queryNode) does:
//   1. Iterate per-node edge HashSet (capacity based on #edges via collection constructor)
//   2. Extract distal nodes, insert into new HashSet<>() (default constructor, cap 16 for ≤12)
//   3. Return ArrayList from that HashSet's iteration (bucket order)
//
// Within-bucket ordering in the adj HashSet is determined by insertion order,
// which comes from the per-node edge HashSet iteration order (bucket order of edges).
//
// queryNode: the node whose adjacency list we're sorting (needed for edge hash tiebreaker)
inline void sortByJavaHashOrder(std::vector<NodePtr>& nodes, const NodePtr& queryNode = nullptr) {
    if (nodes.size() <= 1) return;

    // Adj HashSet uses new HashSet<>() default constructor
    int adjCapacity = javaHashSetDefaultCapacity(static_cast<int>(nodes.size()));

    // Per-node edge HashSet capacity: addEdge() calls new HashSet<>(existingEdgeSet)
    // For n edges, last call copies set of n-1 then adds 1. Use collection constructor cap.
    int edgeCapacity = javaHashSetCapacity(static_cast<int>(nodes.size()));

    int32_t queryHash = queryNode ? javaStringHashCode(queryNode->getName()) : 0;

    std::stable_sort(nodes.begin(), nodes.end(),
        [adjCapacity, edgeCapacity, queryHash](const NodePtr& a, const NodePtr& b) {
            int32_t hashA = javaStringHashCode(a->getName());
            int32_t hashB = javaStringHashCode(b->getName());
            int bucketA = javaHashBucket(hashA, adjCapacity);
            int bucketB = javaHashBucket(hashB, adjCapacity);
            if (bucketA != bucketB) return bucketA < bucketB;

            // Within-bucket tiebreaker: order by edge bucket in the per-node edge HashSet.
            // Edge.hashCode() = node1.hashCode() + node2.hashCode()
            // Edges in earlier buckets are iterated first → their distal nodes are inserted
            // first into the adj HashSet → they come first within the same adj bucket.
            int32_t edgeHashA = queryHash + hashA;
            int32_t edgeHashB = queryHash + hashB;
            int edgeBucketA = javaHashBucket(edgeHashA, edgeCapacity);
            int edgeBucketB = javaHashBucket(edgeHashB, edgeCapacity);
            return edgeBucketA < edgeBucketB;
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
            if (bucketA != bucketB) return bucketA < bucketB;

            // Within-bucket tiebreaker: use perturbed hash value (unsigned comparison).
            // Java's HashMap linked lists maintain insertion order; this approximation
            // works when edges in the same bucket have different perturbed hash values.
            uint32_t phA = static_cast<uint32_t>(javaHashMapPerturb(hashA));
            uint32_t phB = static_cast<uint32_t>(javaHashMapPerturb(hashB));
            return phA < phB;
        });
}

} // namespace tetrad
