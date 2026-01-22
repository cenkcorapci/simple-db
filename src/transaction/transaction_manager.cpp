#include "transaction/transaction_manager.h"

namespace simpledb {
namespace transaction {

TransactionManager::TransactionManager(std::shared_ptr<storage::KVStore> store)
    : store_(store), next_txn_id_(1) {
}

uint64_t TransactionManager::begin_transaction() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t txn_id = next_txn_id_++;
    transactions_[txn_id] = std::make_unique<Transaction>(txn_id);
    
    return txn_id;
}

bool TransactionManager::commit_transaction(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto* txn = get_transaction(txn_id);
    if (!txn || txn->state != TxnState::ACTIVE) {
        return false;
    }
    
    // Apply all writes in the write set
    for (const auto& write : txn->write_set) {
        store_->put_vector(txn_id, write.first, write.second);
    }
    
    // Commit to log
    store_->commit(txn_id);
    
    // Update state
    txn->state = TxnState::COMMITTED;
    
    // Release all locks
    lock_manager_.release_all_locks(txn_id);
    
    // Clean up transaction
    transactions_.erase(txn_id);
    
    return true;
}

bool TransactionManager::rollback_transaction(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto* txn = get_transaction(txn_id);
    if (!txn || txn->state != TxnState::ACTIVE) {
        return false;
    }
    
    // Update state
    txn->state = TxnState::ABORTED;
    
    // Release all locks
    lock_manager_.release_all_locks(txn_id);
    
    // Clean up transaction
    transactions_.erase(txn_id);
    
    return true;
}

bool TransactionManager::read(uint64_t txn_id, const std::string& key, storage::Vector& vector) {
    auto* txn = get_transaction(txn_id);
    if (!txn || txn->state != TxnState::ACTIVE) {
        return false;
    }
    
    // Acquire shared lock
    if (!lock_manager_.acquire_lock(txn_id, key, LockMode::SHARED)) {
        return false;
    }
    
    // Check write set first (read your own writes)
    for (const auto& write : txn->write_set) {
        if (write.first == key) {
            vector = write.second;
            return true;
        }
    }
    
    // Read from store
    return store_->get_vector(key, vector);
}

bool TransactionManager::write(uint64_t txn_id, const std::string& key, const storage::Vector& vector) {
    auto* txn = get_transaction(txn_id);
    if (!txn || txn->state != TxnState::ACTIVE) {
        return false;
    }
    
    // Acquire exclusive lock
    if (!lock_manager_.acquire_lock(txn_id, key, LockMode::EXCLUSIVE)) {
        return false;
    }
    
    // Check if key already exists in write set and update it
    bool found = false;
    for (auto& write : txn->write_set) {
        if (write.first == key) {
            write.second = vector;
            found = true;
            break;
        }
    }
    
    // Add to write set if not found
    if (!found) {
        txn->write_set.push_back({key, vector});
    }
    
    return true;
}

bool TransactionManager::remove(uint64_t txn_id, const std::string& key) {
    auto* txn = get_transaction(txn_id);
    if (!txn || txn->state != TxnState::ACTIVE) {
        return false;
    }
    
    // Acquire exclusive lock
    if (!lock_manager_.acquire_lock(txn_id, key, LockMode::EXCLUSIVE)) {
        return false;
    }
    
    // Perform delete
    return store_->remove(txn_id, key);
}

std::vector<storage::SearchResult> TransactionManager::search(const storage::Vector& query, size_t k) {
    // Search doesn't need a transaction or locks - it's a read-only operation
    return store_->search_vectors(query, k);
}

Transaction* TransactionManager::get_transaction(uint64_t txn_id) {
    auto it = transactions_.find(txn_id);
    if (it == transactions_.end()) {
        return nullptr;
    }
    return it->second.get();
}

} // namespace transaction
} // namespace simpledb
