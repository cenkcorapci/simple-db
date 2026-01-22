#ifndef SIMPLE_DB_HNSW_H
#define SIMPLE_DB_HNSW_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>
#include <random>
#include <mutex>
#include <queue>
#include <cmath>

namespace simpledb {
namespace storage {

// Vector type definition
using Vector = std::vector<float>;

// Distance metric types
enum class DistanceMetric {
    EUCLIDEAN,
    COSINE
};

// HNSW node representing a vector in the graph
struct HNSWNode {
    std::string key;
    Vector vector;
    size_t file_offset;  // Offset in append-only log
    int max_level;
    
    // Neighbors at each level (level -> set of neighbor keys)
    std::vector<std::unordered_set<std::string>> neighbors;
    
    HNSWNode(const std::string& k, const Vector& v, size_t offset, int level)
        : key(k), vector(v), file_offset(offset), max_level(level) {
        neighbors.resize(level + 1);
    }
};

// Search result with key and distance
struct SearchResult {
    std::string key;
    float distance;
    
    SearchResult(const std::string& k, float d) : key(k), distance(d) {}
    
    bool operator<(const SearchResult& other) const {
        return distance < other.distance;
    }
};

// HNSW (Hierarchical Navigable Small World) graph for vector similarity search
class HNSW {
public:
    HNSW(size_t dim, size_t M = 16, size_t ef_construction = 200, 
         DistanceMetric metric = DistanceMetric::EUCLIDEAN);
    ~HNSW() = default;
    
    // Insert a vector with given key
    void insert(const std::string& key, const Vector& vector, size_t offset);
    
    // Search for k-nearest neighbors
    std::vector<SearchResult> search(const Vector& query, size_t k, size_t ef_search = 50);
    
    // Get vector by key
    bool get(const std::string& key, Vector& vector, size_t& offset);
    
    // Remove a vector (marks as deleted)
    bool remove(const std::string& key);
    
    // Get number of vectors
    size_t size() const { return nodes_.size() - deleted_keys_.size(); }
    
    // Get dimension
    size_t dimension() const { return dimension_; }
    
private:
    size_t dimension_;           // Vector dimension
    size_t M_;                   // Max number of connections per layer
    size_t max_M_;               // Max connections at layer 0
    size_t ef_construction_;     // Size of dynamic candidate list during construction
    double ml_;                  // Normalization factor for level generation
    DistanceMetric metric_;      // Distance metric to use
    
    std::unordered_map<std::string, std::shared_ptr<HNSWNode>> nodes_;
    std::unordered_set<std::string> deleted_keys_;
    std::string entry_point_;    // Entry point for search
    
    mutable std::mutex mutex_;
    std::mt19937 rng_;
    
    // Helper functions
    int get_random_level();
    float compute_distance(const Vector& a, const Vector& b) const;
    
    // Search at a specific level
    std::vector<std::string> search_layer(const Vector& query, 
                                          const std::string& entry_point,
                                          size_t ef, int level);
    
    // Get nearest neighbors
    std::vector<std::string> get_neighbors(const std::string& key, 
                                          const Vector& query,
                                          size_t M, int level);
    
    // Select neighbors using heuristic
    std::vector<std::string> select_neighbors_heuristic(
        const std::vector<std::string>& candidates,
        const Vector& query, size_t M);
};

} // namespace storage
} // namespace simpledb

#endif // SIMPLE_DB_HNSW_H
