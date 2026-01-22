#include "replication/caspaxos.h"
#include <algorithm>
#include <iostream>

namespace simpledb {
namespace replication {

// AcceptorState implementation
AcceptorState::AcceptorState(uint32_t node_id)
    : node_id_(node_id), highest_ballot_(0, node_id) {
}

std::optional<PromiseMessage> AcceptorState::handle_prepare(const PrepareMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if ballot is higher than any we've seen
    if (msg.ballot < highest_ballot_) {
        // Reject: we've promised a higher ballot
        return std::nullopt;
    }
    
    // Update highest ballot (promise not to accept lower ballots)
    highest_ballot_ = msg.ballot;
    
    // Get current value for the key
    std::optional<VersionedValue> current_value;
    auto it = values_.find(msg.key);
    if (it != values_.end()) {
        current_value = it->second;
    }
    
    // Check CAS condition if provided
    if (msg.old_value.has_value()) {
        // CAS operation: check if current value matches expected
        if (current_value.has_value()) {
            if (current_value->value != msg.old_value.value()) {
                // CAS condition not satisfied
                return std::nullopt;
            }
        } else {
            // Key doesn't exist but old_value was expected
            return std::nullopt;
        }
    }
    
    // Send PROMISE with current value
    return PromiseMessage(msg.ballot, msg.key, current_value, highest_ballot_);
}

AckMessage AcceptorState::handle_commit(const CommitMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if ballot is still valid
    if (msg.ballot < highest_ballot_) {
        return AckMessage(msg.ballot, msg.key, false);
    }
    
    // Store the value with ballot number
    VersionedValue versioned_value(msg.ballot, msg.value, true);
    values_[msg.key] = versioned_value;
    
    return AckMessage(msg.ballot, msg.key, true);
}

std::optional<VersionedValue> AcceptorState::get_value(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = values_.find(key);
    if (it != values_.end() && it->second.committed) {
        return it->second;
    }
    
    return std::nullopt;
}

// ProposerState implementation
ProposerState::ProposerState(uint32_t node_id)
    : node_id_(node_id), current_epoch_(1) {
}

Ballot ProposerState::get_next_ballot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return Ballot(current_epoch_++, node_id_);
}

void ProposerState::update_ballot(const Ballot& ballot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (ballot.epoch >= current_epoch_) {
        current_epoch_ = ballot.epoch + 1;
    }
}

// CasPaxos implementation
CasPaxos::CasPaxos(uint32_t node_id, const std::vector<std::string>& replicas)
    : node_id_(node_id), replicas_(replicas),
      proposer_(node_id), acceptor_(node_id) {
}

CasPaxos::~CasPaxos() {
}

bool CasPaxos::cas(const std::string& key,
                   const std::optional<std::string>& old_value,
                   const std::string& new_value) {
    // Phase 1: PREPARE
    Ballot ballot = proposer_.get_next_ballot();
    PrepareMessage prepare(ballot, key, old_value, new_value);
    
    // Handle prepare locally first
    auto local_promise = acceptor_.handle_prepare(prepare);
    if (!local_promise.has_value()) {
        return false;
    }
    
    // Send PREPARE to other replicas
    std::vector<PromiseMessage> promises = send_prepare(prepare);
    promises.push_back(local_promise.value());
    
    // Check if we have quorum
    if (promises.size() < get_quorum_size()) {
        std::cerr << "Failed to get quorum for PREPARE: " 
                  << promises.size() << "/" << get_quorum_size() << std::endl;
        return false;
    }
    
    // Check for higher ballots in promises
    for (const auto& promise : promises) {
        if (promise.highest_ballot > ballot) {
            proposer_.update_ballot(promise.highest_ballot);
            return false;
        }
    }
    
    // Phase 2: COMMIT
    CommitMessage commit(ballot, key, new_value);
    
    // Handle commit locally first
    auto local_ack = acceptor_.handle_commit(commit);
    if (!local_ack.success) {
        return false;
    }
    
    // Send COMMIT to other replicas
    std::vector<AckMessage> acks = send_commit(commit);
    acks.push_back(local_ack);
    
    // Count successful acks
    size_t success_count = 0;
    for (const auto& ack : acks) {
        if (ack.success) {
            success_count++;
        }
    }
    
    // Check if we have quorum
    if (success_count < get_quorum_size()) {
        std::cerr << "Failed to get quorum for COMMIT: " 
                  << success_count << "/" << get_quorum_size() << std::endl;
        return false;
    }
    
    return true;
}

std::optional<std::string> CasPaxos::get(const std::string& key) {
    // Read from local acceptor
    auto value = acceptor_.get_value(key);
    if (value.has_value()) {
        return value->value;
    }
    
    return std::nullopt;
}

bool CasPaxos::set(const std::string& key, const std::string& value) {
    // Unconditional write: CAS without checking old value
    return cas(key, std::nullopt, value);
}

bool CasPaxos::del(const std::string& key, const std::optional<std::string>& old_value) {
    // Delete is CAS with empty new value
    return cas(key, old_value, "");
}

std::vector<PromiseMessage> CasPaxos::send_prepare(const PrepareMessage& msg) {
    std::vector<PromiseMessage> promises;
    
    // In a real implementation, this would send messages over the network
    // to other replicas and collect responses.
    // For now, this is a placeholder that returns empty (only local acceptor).
    
    // TODO: Implement network communication with replicas
    
    return promises;
}

std::vector<AckMessage> CasPaxos::send_commit(const CommitMessage& msg) {
    std::vector<AckMessage> acks;
    
    // In a real implementation, this would send messages over the network
    // to other replicas and collect responses.
    // For now, this is a placeholder that returns empty (only local acceptor).
    
    // TODO: Implement network communication with replicas
    
    return acks;
}

} // namespace replication
} // namespace simpledb
