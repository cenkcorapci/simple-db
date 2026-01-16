#include "transaction/lock_manager.h"
#include <chrono>
#include <algorithm>

namespace simpledb {
namespace transaction {

bool LockManager::acquire_lock(uint64_t txn_id, const std::string& key, LockMode mode) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    auto& queue = locks_[key];
    
    // Check if we can grant the lock immediately
    if (can_grant_lock(queue, mode)) {
        if (mode == LockMode::SHARED) {
            queue.holders_shared.insert(txn_id);
        } else {
            queue.holder_exclusive = txn_id;
        }
        txn_locks_[txn_id].insert(key);
        return true;
    }
    
    // Need to wait
    LockRequest request(txn_id, mode);
    queue.requests.push_back(request);
    
    // Wait for the lock
    queue.cv.wait(lock, [&]() {
        for (auto& req : queue.requests) {
            if (req.txn_id == txn_id) {
                return req.granted;
            }
        }
        return false;
    });
    
    // Remove from request queue
    queue.requests.erase(
        std::remove_if(queue.requests.begin(), queue.requests.end(),
                      [txn_id](const LockRequest& req) { return req.txn_id == txn_id; }),
        queue.requests.end()
    );
    
    txn_locks_[txn_id].insert(key);
    return true;
}

bool LockManager::release_lock(uint64_t txn_id, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = locks_.find(key);
    if (it == locks_.end()) {
        return false;
    }
    
    auto& queue = it->second;
    
    // Release shared lock
    queue.holders_shared.erase(txn_id);
    
    // Release exclusive lock
    if (queue.holder_exclusive == txn_id) {
        queue.holder_exclusive = 0;
    }
    
    // Remove from txn_locks
    txn_locks_[txn_id].erase(key);
    
    // Try to grant waiting locks
    grant_locks(queue);
    
    return true;
}

void LockManager::release_all_locks(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = txn_locks_.find(txn_id);
    if (it == txn_locks_.end()) {
        return;
    }
    
    // Make a copy since we're modifying during iteration
    auto keys = it->second;
    
    for (const auto& key : keys) {
        auto lock_it = locks_.find(key);
        if (lock_it != locks_.end()) {
            auto& queue = lock_it->second;
            
            queue.holders_shared.erase(txn_id);
            if (queue.holder_exclusive == txn_id) {
                queue.holder_exclusive = 0;
            }
            
            grant_locks(queue);
        }
    }
    
    txn_locks_.erase(txn_id);
}

bool LockManager::can_grant_lock(const LockQueue& queue, LockMode mode) {
    if (mode == LockMode::SHARED) {
        // Can grant shared lock if no exclusive lock holder
        return queue.holder_exclusive == 0;
    } else {
        // Can grant exclusive lock if no holders at all
        return queue.holder_exclusive == 0 && queue.holders_shared.empty();
    }
}

void LockManager::grant_locks(LockQueue& queue) {
    bool granted_any = false;
    
    for (auto& request : queue.requests) {
        if (request.granted) {
            continue;
        }
        
        // For exclusive locks, only grant if no other locks held
        if (request.mode == LockMode::EXCLUSIVE) {
            if (queue.holder_exclusive == 0 && queue.holders_shared.empty()) {
                queue.holder_exclusive = request.txn_id;
                request.granted = true;
                granted_any = true;
                break;  // Can only grant one exclusive lock
            }
        } else {
            // For shared locks, grant if no exclusive lock held
            if (queue.holder_exclusive == 0) {
                queue.holders_shared.insert(request.txn_id);
                request.granted = true;
                granted_any = true;
                // Continue to grant other shared locks
            }
        }
    }
    
    if (granted_any) {
        queue.cv.notify_all();
    }
}

} // namespace transaction
} // namespace simpledb
