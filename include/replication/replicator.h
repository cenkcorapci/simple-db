#ifndef SIMPLE_DB_REPLICATOR_H
#define SIMPLE_DB_REPLICATOR_H

#include "storage/append_log.h"
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

namespace simpledb {
namespace replication {

enum class ReplicatorRole {
    LEADER,
    FOLLOWER
};

// Replicator for leader-follower replication
class Replicator {
public:
    Replicator(const std::string& log_file, ReplicatorRole role);
    ~Replicator();
    
    // Leader methods
    void add_follower(const std::string& host, int port);
    void replicate_log_entry(const storage::LogRecord& record);
    
    // Follower methods
    void connect_to_leader(const std::string& host, int port);
    void sync_from_leader();
    
    void start();
    void stop();
    
    ReplicatorRole role() const { return role_; }
    
private:
    std::string log_file_;
    ReplicatorRole role_;
    std::atomic<bool> running_;
    std::thread replication_thread_;
    std::mutex mutex_;
    
    // Leader state
    struct Follower {
        std::string host;
        int port;
        size_t last_synced_offset;
        int socket_fd;
    };
    std::vector<Follower> followers_;
    
    // Follower state
    std::string leader_host_;
    int leader_port_;
    int leader_socket_;
    size_t last_applied_offset_;
    
    void replication_loop();
    void send_to_followers();
    void receive_from_leader();
    int connect_to_host(const std::string& host, int port);
};

} // namespace replication
} // namespace simpledb

#endif // SIMPLE_DB_REPLICATOR_H
