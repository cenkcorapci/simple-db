#ifndef SIMPLE_DB_KV_STORE_H
#define SIMPLE_DB_KV_STORE_H

#include "storage/append_log.h"
#include "storage/hnsw.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

namespace simpledb {
namespace storage {

// Key-value store with HNSW vector indexing
class KVStore {
public:
    KVStore(const std::string& log_filename, size_t dimension = 128);
    ~KVStore() = default;
    
    // Vector operations
    [[nodiscard]] bool put_vector(uint64_t txn_id, const std::string& key, const Vector& vector);
    [[nodiscard]] bool get_vector(const std::string& key, Vector& vector);
    [[nodiscard]] std::vector<SearchResult> search_vectors(const Vector& query, size_t k);
    
    [[nodiscard]] bool remove(uint64_t txn_id, const std::string& key);
    [[nodiscard]] bool exists(const std::string& key);
    
    void commit(uint64_t txn_id);
    void recover();  // Recover from log on startup
    
    [[nodiscard]] size_t size() const { return hnsw_->size(); }
    [[nodiscard]] size_t dimension() const { return hnsw_->dimension(); }
    
private:
    std::unique_ptr<AppendLog> log_;
    std::unique_ptr<HNSW> hnsw_;
    std::unordered_map<std::string, size_t> index_;  // key -> log offset
    mutable std::mutex mutex_;
    
    void replay_log();
};

} // namespace storage
} // namespace simpledb

#endif // SIMPLE_DB_KV_STORE_H
