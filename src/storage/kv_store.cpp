#include "storage/kv_store.h"
#include <chrono>
#include <functional>

namespace simpledb {
namespace storage {

KVStore::KVStore(const std::string& log_filename, size_t dimension) {
    log_ = std::make_unique<AppendLog>(log_filename);
    hnsw_ = std::make_unique<HNSW>(dimension);
    recover();
}

bool KVStore::put_vector(uint64_t txn_id, const std::string& key, const Vector& vector) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::INSERT, txn_id, key, vector, timestamp);
    
    size_t offset = log_->append(record);
    
    // Update index
    index_[key] = offset;
    
    // Update HNSW
    hnsw_->insert(key, vector, offset);
    
    return true;
}

bool KVStore::get_vector(const std::string& key, Vector& vector) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t offset;
    return hnsw_->get(key, vector, offset);
}

std::vector<SearchResult> KVStore::search_vectors(const Vector& query, size_t k) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return hnsw_->search(query, k);
}

bool KVStore::remove(uint64_t txn_id, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) {
        return false;
    }
    
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::DELETE, txn_id, key, Vector(), timestamp);
    
    log_->append(record);
    
    // Remove from index and HNSW
    index_.erase(key);
    hnsw_->remove(key);
    
    return true;
}

bool KVStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_.find(key) != index_.end();
}

void KVStore::commit(uint64_t txn_id) {
    auto timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    LogRecord record(RecordType::COMMIT, txn_id, "", Vector(), timestamp);
    log_->append(record);
    log_->sync();
}

void KVStore::recover() {
    replay_log();
}

void KVStore::replay_log() {
    auto records = log_->read_all();
    
    size_t offset = 0;
    for (const auto& record : records) {
        if (record.type == RecordType::INSERT && record.is_vector) {
            // Reconstruct index with approximate offsets
            index_[record.key] = offset;
            hnsw_->insert(record.key, record.vector_data, offset);
        } else if (record.type == RecordType::DELETE) {
            index_.erase(record.key);
            hnsw_->remove(record.key);
        }
        // Approximate offset increment based on record size
        offset += 100;  // Rough estimate
    }
}

} // namespace storage
} // namespace simpledb
