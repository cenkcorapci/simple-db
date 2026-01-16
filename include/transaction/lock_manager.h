#ifndef SIMPLE_DB_LOCK_MANAGER_H
#define SIMPLE_DB_LOCK_MANAGER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <set>
#include <vector>

namespace simpledb {
namespace transaction {

enum class LockMode {
    SHARED,
    EXCLUSIVE
};

// Lock manager for concurrency control
class LockManager {
public:
    LockManager() = default;
    ~LockManager() = default;
    
    bool acquire_lock(uint64_t txn_id, const std::string& key, LockMode mode);
    bool release_lock(uint64_t txn_id, const std::string& key);
    void release_all_locks(uint64_t txn_id);
    
private:
    struct LockRequest {
        uint64_t txn_id;
        LockMode mode;
        bool granted;
        
        LockRequest(uint64_t tid, LockMode m, bool g = false)
            : txn_id(tid), mode(m), granted(g) {}
    };
    
    struct LockQueue {
        std::vector<LockRequest> requests;
        std::set<uint64_t> holders_shared;
        uint64_t holder_exclusive;
        std::condition_variable cv;
        
        LockQueue() : holder_exclusive(0) {}
    };
    
    std::unordered_map<std::string, LockQueue> locks_;
    std::unordered_map<uint64_t, std::set<std::string>> txn_locks_;
    std::mutex mutex_;
    
    bool can_grant_lock(const LockQueue& queue, LockMode mode);
    void grant_locks(LockQueue& queue);
};

} // namespace transaction
} // namespace simpledb

#endif // SIMPLE_DB_LOCK_MANAGER_H
