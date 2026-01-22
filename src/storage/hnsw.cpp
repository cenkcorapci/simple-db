#include "storage/hnsw.h"
#include <algorithm>
#include <limits>
#include <random>

namespace simpledb {
namespace storage {

HNSW::HNSW(size_t dim, size_t M, size_t ef_construction, DistanceMetric metric)
    : dimension_(dim), M_(M), max_M_(M * 2), ef_construction_(ef_construction),
      ml_(1.0 / std::log(2.0)), metric_(metric), rng_(std::random_device{}()) {
}

int HNSW::get_random_level() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double r = dist(rng_);
    return static_cast<int>(-std::log(r) * ml_);
}

float HNSW::compute_distance(const Vector& a, const Vector& b) const {
    if (a.size() != b.size() || a.size() != dimension_) {
        return std::numeric_limits<float>::max();
    }
    
    if (metric_ == DistanceMetric::EUCLIDEAN) {
        float sum = 0.0f;
        for (size_t i = 0; i < dimension_; ++i) {
            float diff = a[i] - b[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    } else { // COSINE
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (size_t i = 0; i < dimension_; ++i) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        
        if (norm_a == 0.0f || norm_b == 0.0f) {
            return 1.0f;
        }
        
        float cosine_sim = dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
        return 1.0f - cosine_sim;  // Convert similarity to distance
    }
}

void HNSW::insert(const std::string& key, const Vector& vector, size_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (vector.size() != dimension_) {
        return;  // Invalid dimension
    }
    
    // Check if key already exists
    if (nodes_.find(key) != nodes_.end()) {
        return;  // Key already exists
    }
    
    int level = get_random_level();
    auto node = std::make_shared<HNSWNode>(key, vector, offset, level);
    nodes_[key] = node;
    
    // If this is the first node, make it the entry point
    if (entry_point_.empty()) {
        entry_point_ = key;
        return;
    }
    
    // Find nearest neighbors at each level
    std::string curr_nearest = entry_point_;
    int entry_level = nodes_[entry_point_]->max_level;
    
    // Search from top level to level+1
    for (int lc = entry_level; lc > level; --lc) {
        auto candidates = search_layer(vector, curr_nearest, 1, lc);
        if (!candidates.empty()) {
            curr_nearest = candidates[0];
        }
    }
    
    // Insert at each level from level to 0
    for (int lc = level; lc >= 0; --lc) {
        auto candidates = search_layer(vector, curr_nearest, ef_construction_, lc);
        
        // Select M neighbors
        size_t M = (lc == 0) ? max_M_ : M_;
        auto neighbors = select_neighbors_heuristic(candidates, vector, M);
        
        // Add bidirectional links
        for (const auto& neighbor_key : neighbors) {
            node->neighbors[lc].insert(neighbor_key);
            
            // Make sure neighbor exists and has enough levels
            auto neighbor_it = nodes_.find(neighbor_key);
            if (neighbor_it != nodes_.end()) {
                auto& neighbor_node = neighbor_it->second;
                if (lc < static_cast<int>(neighbor_node->neighbors.size())) {
                    neighbor_node->neighbors[lc].insert(key);
                    
                    // Prune neighbors if needed
                    auto& neighbor_connections = neighbor_node->neighbors[lc];
                    if (neighbor_connections.size() > M) {
                        std::vector<std::string> neighbor_list(neighbor_connections.begin(), 
                                                               neighbor_connections.end());
                        auto pruned = select_neighbors_heuristic(neighbor_list, 
                                                                neighbor_node->vector, M);
                        neighbor_connections.clear();
                        neighbor_connections.insert(pruned.begin(), pruned.end());
                    }
                }
            }
        }
        
        if (!candidates.empty()) {
            curr_nearest = candidates[0];
        }
    }
    
    // Update entry point if new node has higher level
    if (level > nodes_[entry_point_]->max_level) {
        entry_point_ = key;
    }
}

std::vector<std::string> HNSW::search_layer(const Vector& query, 
                                            const std::string& entry_point,
                                            size_t ef, int level) {
    std::unordered_set<std::string> visited;
    
    // Priority queue: max heap of (distance, key)
    auto cmp = [](const std::pair<float, std::string>& a, 
                  const std::pair<float, std::string>& b) {
        return a.first < b.first;  // Max heap
    };
    std::priority_queue<std::pair<float, std::string>,
                       std::vector<std::pair<float, std::string>>,
                       decltype(cmp)> candidates(cmp);
    
    // Min heap for results
    auto result_cmp = [](const std::pair<float, std::string>& a,
                        const std::pair<float, std::string>& b) {
        return a.first > b.first;  // Min heap
    };
    std::priority_queue<std::pair<float, std::string>,
                       std::vector<std::pair<float, std::string>>,
                       decltype(result_cmp)> results(result_cmp);
    
    float dist = compute_distance(query, nodes_[entry_point]->vector);
    candidates.push({dist, entry_point});
    results.push({dist, entry_point});
    visited.insert(entry_point);
    
    while (!candidates.empty()) {
        auto current = candidates.top();
        candidates.pop();
        
        // If current is farther than furthest result, stop
        size_t results_size = results.size();
        if (results_size >= ef && current.first > results.top().first) {
            break;
        }
        
        // Check neighbors at this level
        auto node = nodes_[current.second];
        if (level < static_cast<int>(node->neighbors.size())) {
            for (const auto& neighbor_key : node->neighbors[level]) {
                if (visited.find(neighbor_key) == visited.end()) {
                    visited.insert(neighbor_key);
                    
                    if (deleted_keys_.find(neighbor_key) != deleted_keys_.end()) {
                        continue;  // Skip deleted nodes
                    }
                    
                    float d = compute_distance(query, nodes_[neighbor_key]->vector);
                    
                    if (results_size < ef || d < results.top().first) {
                        candidates.push({d, neighbor_key});
                        results.push({d, neighbor_key});
                        results_size = results.size();
                        
                        if (results_size > ef) {
                            results.pop();
                            results_size = results.size();
                        }
                    }
                }
            }
        }
    }
    
    // Extract results
    std::vector<std::string> result_keys;
    while (!results.empty()) {
        result_keys.push_back(results.top().second);
        results.pop();
    }
    std::reverse(result_keys.begin(), result_keys.end());
    
    return result_keys;
}

std::vector<std::string> HNSW::select_neighbors_heuristic(
    const std::vector<std::string>& candidates,
    const Vector& query, size_t M) {
    
    if (candidates.size() <= M) {
        return candidates;
    }
    
    // Simple heuristic: select M nearest neighbors
    std::vector<std::pair<float, std::string>> distances;
    for (const auto& candidate : candidates) {
        if (deleted_keys_.find(candidate) == deleted_keys_.end()) {
            float d = compute_distance(query, nodes_[candidate]->vector);
            distances.push_back({d, candidate});
        }
    }
    
    std::sort(distances.begin(), distances.end());
    
    std::vector<std::string> selected;
    for (size_t i = 0; i < std::min(M, distances.size()); ++i) {
        selected.push_back(distances[i].second);
    }
    
    return selected;
}

std::vector<SearchResult> HNSW::search(const Vector& query, size_t k, size_t ef_search) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SearchResult> results;
    
    if (entry_point_.empty() || query.size() != dimension_) {
        return results;
    }
    
    std::string curr_nearest = entry_point_;
    int entry_level = nodes_[entry_point_]->max_level;
    
    // Search from top level down to level 0
    for (int lc = entry_level; lc > 0; --lc) {
        auto candidates = search_layer(query, curr_nearest, 1, lc);
        if (!candidates.empty()) {
            curr_nearest = candidates[0];
        }
    }
    
    // Search at layer 0 with ef_search
    auto candidates = search_layer(query, curr_nearest, std::max(ef_search, k), 0);
    
    // Return top k results
    for (size_t i = 0; i < std::min(k, candidates.size()); ++i) {
        const auto& key = candidates[i];
        if (deleted_keys_.find(key) == deleted_keys_.end()) {
            float dist = compute_distance(query, nodes_[key]->vector);
            results.emplace_back(key, dist);
        }
    }
    
    return results;
}

bool HNSW::get(const std::string& key, Vector& vector, size_t& offset) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(key);
    if (it == nodes_.end() || deleted_keys_.find(key) != deleted_keys_.end()) {
        return false;
    }
    
    vector = it->second->vector;
    offset = it->second->file_offset;
    return true;
}

bool HNSW::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return false;
    }
    
    deleted_keys_.insert(key);
    return true;
}

} // namespace storage
} // namespace simpledb
