#ifndef SIMPLE_DB_CASPAXOS_H
#define SIMPLE_DB_CASPAXOS_H

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <optional>

namespace simpledb {
namespace replication {

// Ballot number structure (epoch + node_id for uniqueness)
struct Ballot {
    uint64_t epoch;
    uint32_t node_id;
    
    Ballot() : epoch(0), node_id(0) {}
    Ballot(uint64_t e, uint32_t n) : epoch(e), node_id(n) {}
    
    bool operator<(const Ballot& other) const {
        if (epoch != other.epoch) return epoch < other.epoch;
        return node_id < other.node_id;
    }
    
    bool operator>(const Ballot& other) const {
        return other < *this;
    }
    
    bool operator==(const Ballot& other) const {
        return epoch == other.epoch && node_id == other.node_id;
    }
    
    bool operator!=(const Ballot& other) const {
        return !(*this == other);
    }
    
    bool operator<=(const Ballot& other) const {
        return *this < other || *this == other;
    }
    
    bool operator>=(const Ballot& other) const {
        return *this > other || *this == other;
    }
};

// Versioned value entry
struct VersionedValue {
    Ballot ballot;
    std::string value;
    bool committed;
    
    VersionedValue() : committed(false) {}
    VersionedValue(const Ballot& b, const std::string& v, bool c = false)
        : ballot(b), value(v), committed(c) {}
};

// Message types for CasPaxos protocol
enum class MessageType {
    PREPARE,
    PROMISE,
    COMMIT,
    ACK,
    REJECT
};

// Base message
struct Message {
    MessageType type;
    Ballot ballot;
    std::string key;
    
    Message(MessageType t, const Ballot& b, const std::string& k)
        : type(t), ballot(b), key(k) {}
    virtual ~Message() = default;
};

// PREPARE message
struct PrepareMessage : public Message {
    std::optional<std::string> old_value;
    std::string new_value;
    
    PrepareMessage(const Ballot& b, const std::string& k,
                   const std::optional<std::string>& old_val,
                   const std::string& new_val)
        : Message(MessageType::PREPARE, b, k),
          old_value(old_val), new_value(new_val) {}
};

// PROMISE response
struct PromiseMessage : public Message {
    std::optional<VersionedValue> current_value;
    Ballot highest_ballot;
    
    PromiseMessage(const Ballot& b, const std::string& k,
                   const std::optional<VersionedValue>& cur_val,
                   const Ballot& highest)
        : Message(MessageType::PROMISE, b, k),
          current_value(cur_val), highest_ballot(highest) {}
};

// COMMIT message
struct CommitMessage : public Message {
    std::string value;
    
    CommitMessage(const Ballot& b, const std::string& k, const std::string& v)
        : Message(MessageType::COMMIT, b, k), value(v) {}
};

// ACK response
struct AckMessage : public Message {
    bool success;
    
    AckMessage(const Ballot& b, const std::string& k, bool s)
        : Message(MessageType::ACK, b, k), success(s) {}
};

// REJECT response
struct RejectMessage : public Message {
    Ballot highest_ballot;
    std::string reason;
    
    RejectMessage(const Ballot& b, const std::string& k,
                  const Ballot& highest, const std::string& r)
        : Message(MessageType::REJECT, b, k),
          highest_ballot(highest), reason(r) {}
};

// Acceptor state (replica)
class AcceptorState {
public:
    AcceptorState(uint32_t node_id);
    
    // Process PREPARE message
    std::optional<PromiseMessage> handle_prepare(const PrepareMessage& msg);
    
    // Process COMMIT message
    AckMessage handle_commit(const CommitMessage& msg);
    
    // Get value for key
    std::optional<VersionedValue> get_value(const std::string& key) const;
    
    // Get current ballot
    Ballot get_highest_ballot() const { return highest_ballot_; }
    
private:
    uint32_t node_id_;
    Ballot highest_ballot_;
    std::map<std::string, VersionedValue> values_;
    mutable std::mutex mutex_;
};

// Proposer state (leader)
class ProposerState {
public:
    ProposerState(uint32_t node_id);
    
    // Get next ballot for proposal
    Ballot get_next_ballot();
    
    // Update ballot based on received higher ballot
    void update_ballot(const Ballot& ballot);
    
private:
    uint32_t node_id_;
    uint64_t current_epoch_;
    mutable std::mutex mutex_;
};

// CasPaxos consensus engine
class CasPaxos {
public:
    CasPaxos(uint32_t node_id, const std::vector<std::string>& replicas);
    ~CasPaxos();
    
    // Perform compare-and-swap operation
    bool cas(const std::string& key,
             const std::optional<std::string>& old_value,
             const std::string& new_value);
    
    // Get value for key
    std::optional<std::string> get(const std::string& key);
    
    // Set value for key (unconditional write)
    bool set(const std::string& key, const std::string& value);
    
    // Delete key (CAS with empty new value)
    bool del(const std::string& key, const std::optional<std::string>& old_value);
    
    // Get quorum size (majority)
    size_t get_quorum_size() const {
        // Total nodes = replicas + self (local node)
        size_t total_nodes = replicas_.size() + 1;
        return (total_nodes / 2) + 1;
    }
    
private:
    uint32_t node_id_;
    std::vector<std::string> replicas_;
    ProposerState proposer_;
    AcceptorState acceptor_;
    
    // Send PREPARE to all replicas and collect PROMISE responses
    std::vector<PromiseMessage> send_prepare(const PrepareMessage& msg);
    
    // Send COMMIT to all replicas and collect ACK responses
    std::vector<AckMessage> send_commit(const CommitMessage& msg);
    
    // Check if CAS condition is satisfied
    bool check_cas_condition(const std::optional<std::string>& expected,
                            const std::optional<std::string>& actual);
};

} // namespace replication
} // namespace simpledb

#endif // SIMPLE_DB_CASPAXOS_H
