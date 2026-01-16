#ifndef SIMPLE_DB_KV_STORE_H
#define SIMPLE_DB_KV_STORE_H

#include "storage/append_log.h"
#include "storage/rtree.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

namespace simpledb {
namespace storage {

// Key-value store with R-tree indexing
class KVStore {
public:
    KVStore(const std::string& log_filename);
    ~KVStore() = default;
    
    bool put(uint64_t txn_id, const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    bool remove(uint64_t txn_id, const std::string& key);
    bool exists(const std::string& key);
    
    void commit(uint64_t txn_id);
    void recover();  // Recover from log on startup
    
    size_t size() const { return index_.size(); }
    
private:
    std::unique_ptr<AppendLog> log_;
    RTree rtree_;
    std::unordered_map<std::string, size_t> index_;  // key -> log offset
    std::unordered_map<std::string, std::string> cache_;  // In-memory cache
    mutable std::mutex mutex_;
    
    void replay_log();
    BoundingBox hash_to_bbox(const std::string& key);
};

} // namespace storage
} // namespace simpledb

#endif // SIMPLE_DB_KV_STORE_H
