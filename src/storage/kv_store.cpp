#include "storage/kv_store.h"
#include <chrono>
#include <functional>

namespace simpledb {
namespace storage {

KVStore::KVStore(const std::string& log_filename) : rtree_(4) {
    log_ = std::make_unique<AppendLog>(log_filename);
    recover();
}

bool KVStore::put(uint64_t txn_id, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::INSERT, txn_id, key, value, timestamp);
    
    size_t offset = log_->append(record);
    
    // Update index
    index_[key] = offset;
    cache_[key] = value;
    
    // Update R-tree
    BoundingBox bbox = hash_to_bbox(key);
    rtree_.insert(key, bbox, offset);
    
    return true;
}

bool KVStore::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check cache first
    auto cache_it = cache_.find(key);
    if (cache_it != cache_.end()) {
        value = cache_it->second;
        return true;
    }
    
    // Check index
    auto it = index_.find(key);
    if (it == index_.end()) {
        return false;
    }
    
    // Read from log
    LogRecord record;
    if (log_->read(it->second, record)) {
        if (record.type == RecordType::INSERT) {
            value = record.value;
            cache_[key] = value;  // Update cache
            return true;
        }
    }
    
    return false;
}

bool KVStore::remove(uint64_t txn_id, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) {
        return false;
    }
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::DELETE, txn_id, key, "", timestamp);
    
    log_->append(record);
    
    // Remove from index and cache
    index_.erase(key);
    cache_.erase(key);
    
    return true;
}

bool KVStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_.find(key) != index_.end();
}

void KVStore::commit(uint64_t txn_id) {
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::COMMIT, txn_id, "", "", timestamp);
    log_->append(record);
    log_->sync();
}

void KVStore::recover() {
    replay_log();
}

void KVStore::replay_log() {
    auto records = log_->read_all();
    
    for (const auto& record : records) {
        if (record.type == RecordType::INSERT) {
            // Reconstruct index
            size_t offset = 0;  // We'd need to track offsets during read
            index_[record.key] = offset;
            cache_[record.key] = record.value;
            
            BoundingBox bbox = hash_to_bbox(record.key);
            rtree_.insert(record.key, bbox, offset);
        } else if (record.type == RecordType::DELETE) {
            index_.erase(record.key);
            cache_.erase(record.key);
        }
    }
}

BoundingBox KVStore::hash_to_bbox(const std::string& key) {
    // Simple hash function to convert key to 2D bounding box
    std::hash<std::string> hasher;
    size_t hash = hasher(key);
    
    double x = static_cast<double>(hash & 0xFFFFFFFF) / 0xFFFFFFFF;
    double y = static_cast<double>((hash >> 32) & 0xFFFFFFFF) / 0xFFFFFFFF;
    
    // Small bounding box centered at (x, y)
    double delta = 0.001;
    return BoundingBox(x - delta, y - delta, x + delta, y + delta);
}

} // namespace storage
} // namespace simpledb
