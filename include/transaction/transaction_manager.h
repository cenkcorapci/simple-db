#ifndef SIMPLE_DB_TRANSACTION_MANAGER_H
#define SIMPLE_DB_TRANSACTION_MANAGER_H

#include "transaction/lock_manager.h"
#include "storage/kv_store.h"
#include <memory>
#include <atomic>
#include <unordered_map>
#include <mutex>

namespace simpledb {
namespace transaction {

enum class TxnState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

// Transaction context
struct Transaction {
    uint64_t txn_id;
    TxnState state;
    std::vector<std::pair<std::string, storage::Vector>> write_set;
    
    Transaction(uint64_t id) : txn_id(id), state(TxnState::ACTIVE) {}
};

// Transaction manager for ACID compliance
class TransactionManager {
public:
    TransactionManager(std::shared_ptr<storage::KVStore> store);
    ~TransactionManager() = default;
    
    uint64_t begin_transaction();
    bool commit_transaction(uint64_t txn_id);
    bool rollback_transaction(uint64_t txn_id);
    
    bool read(uint64_t txn_id, const std::string& key, storage::Vector& vector);
    bool write(uint64_t txn_id, const std::string& key, const storage::Vector& vector);
    bool remove(uint64_t txn_id, const std::string& key);
    
    // Search operation (read-only, doesn't need transaction)
    std::vector<storage::SearchResult> search(const storage::Vector& query, size_t k);
    
private:
    std::shared_ptr<storage::KVStore> store_;
    LockManager lock_manager_;
    std::unordered_map<uint64_t, std::unique_ptr<Transaction>> transactions_;
    std::atomic<uint64_t> next_txn_id_;
    std::mutex mutex_;
    
    Transaction* get_transaction(uint64_t txn_id);
};

} // namespace transaction
} // namespace simpledb

#endif // SIMPLE_DB_TRANSACTION_MANAGER_H
