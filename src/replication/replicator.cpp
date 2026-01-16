#include "replication/replicator.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace simpledb {
namespace replication {

Replicator::Replicator(const std::string& log_file, ReplicatorRole role)
    : log_file_(log_file), role_(role), running_(false), 
      leader_port_(0), leader_socket_(-1), last_applied_offset_(0) {
}

Replicator::~Replicator() {
    stop();
}

void Replicator::add_follower(const std::string& host, int port) {
    if (role_ != ReplicatorRole::LEADER) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    Follower follower;
    follower.host = host;
    follower.port = port;
    follower.last_synced_offset = 0;
    follower.socket_fd = -1;
    
    followers_.push_back(follower);
}

void Replicator::replicate_log_entry(const storage::LogRecord& record) {
    if (role_ != ReplicatorRole::LEADER) {
        return;
    }
    
    // In a real implementation, we would serialize and send the record
    // to all followers
}

void Replicator::connect_to_leader(const std::string& host, int port) {
    if (role_ != ReplicatorRole::FOLLOWER) {
        return;
    }
    
    leader_host_ = host;
    leader_port_ = port;
    
    leader_socket_ = connect_to_host(host, port);
}

void Replicator::sync_from_leader() {
    if (role_ != ReplicatorRole::FOLLOWER || leader_socket_ < 0) {
        return;
    }
    
    // In a real implementation, we would request and receive log entries
    // from the leader
}

void Replicator::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    replication_thread_ = std::thread(&Replicator::replication_loop, this);
}

void Replicator::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (replication_thread_.joinable()) {
        replication_thread_.join();
    }
    
    // Close connections
    if (leader_socket_ >= 0) {
        close(leader_socket_);
        leader_socket_ = -1;
    }
    
    for (auto& follower : followers_) {
        if (follower.socket_fd >= 0) {
            close(follower.socket_fd);
        }
    }
}

void Replicator::replication_loop() {
    while (running_) {
        if (role_ == ReplicatorRole::LEADER) {
            send_to_followers();
        } else {
            receive_from_leader();
        }
        
        // Sleep for a short period
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Replicator::send_to_followers() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Connect to followers if not connected
    for (auto& follower : followers_) {
        if (follower.socket_fd < 0) {
            follower.socket_fd = connect_to_host(follower.host, follower.port);
        }
    }
    
    // In a real implementation, we would:
    // 1. Read new log entries since last sync
    // 2. Send them to each follower
    // 3. Wait for acknowledgment
    // 4. Update last_synced_offset
}

void Replicator::receive_from_leader() {
    if (leader_socket_ < 0) {
        // Try to reconnect
        leader_socket_ = connect_to_host(leader_host_, leader_port_);
        if (leader_socket_ < 0) {
            return;
        }
    }
    
    // In a real implementation, we would:
    // 1. Receive log entries from leader
    // 2. Apply them to local log
    // 3. Send acknowledgment
    // 4. Update last_applied_offset
}

int Replicator::connect_to_host(const std::string& host, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    struct sockaddr_in serv_addr;
    std::memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

} // namespace replication
} // namespace simpledb
