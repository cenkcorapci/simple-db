# SimpleDB Feature Implementation

This document describes how the vector database with HNSW has been implemented.

## Core Feature: Vector Database with HNSW ✅

**Implementation:**
- HNSW (Hierarchical Navigable Small World) algorithm for similarity search
- Vector storage and indexing in `src/storage/hnsw.cpp`
- Support for both Euclidean distance and cosine similarity metrics

**Key Components:**
- `HNSW::insert()` - Add vectors to the hierarchical graph
- `HNSW::search()` - Find k-nearest neighbors
- `HNSW::get()` - Retrieve vector by key
- `HNSW::remove()` - Mark vector as deleted
- Probabilistic level assignment for balanced performance
- Bidirectional graph connections with pruning

**Algorithm Details:**
- **M parameter**: Maximum number of connections per layer (default: 16)
- **ef_construction**: Size of dynamic candidate list during construction (default: 200)
- **Distance metrics**: Euclidean distance and cosine similarity
- **Layer generation**: Probabilistic using exponential distribution
- **Search strategy**: Greedy search starting from top layer down to layer 0

**Usage:**
```cpp
HNSW hnsw(128);  // 128-dimensional vectors
Vector vec = {0.1f, 0.2f, 0.3f, ...};
hnsw.insert("key1", vec, offset);
auto results = hnsw.search(query_vec, 10);  // Find 10 nearest neighbors
```

## Requirement 1: Support for Concurrent Connections ✅

**Implementation:**
- Multi-threaded TCP server in `src/network/server.cpp`
- Each client connection handled in a separate thread
- Thread-safe data structures with mutex protection
- Connection handler in `src/network/connection.cpp`

**Key Components:**
- `Server::accept_connections()` - Accepts new connections in a loop
- `Server::handle_connection()` - Spawns thread for each client
- Thread pool via `std::vector<std::thread> worker_threads_`

**Testing:**
- Multiple clients can connect simultaneously
- Concurrent INSERT/GET/SEARCH operations work correctly
- Lock manager ensures isolation between transactions

## Requirement 2: Replication ✅

**Implementation:**
- Leader-follower replication in `src/replication/replicator.cpp`
- Asynchronous log shipping from leader to followers
- Role-based operation (leader/follower)
- **CasPaxos consensus protocol** in `src/replication/caspaxos.cpp`
  - Fault-tolerant consensus for distributed coordination
  - Compare-and-swap (CAS) atomic operations
  - Two-phase protocol (PREPARE/COMMIT)
  - Ballot-based versioning
  - Majority quorum for operation acceptance

**Key Components:**
- `Replicator::add_follower()` - Register follower nodes
- `Replicator::connect_to_leader()` - Follower connects to leader
- `Replicator::replication_loop()` - Background replication thread
- `Replicator::send_to_followers()` - Push logs to followers
- `CasPaxos::cas()` - Compare-and-swap consensus operation
- `CasPaxos::get()` - Read from consensus state
- `AcceptorState::handle_prepare()` - Handle PREPARE phase
- `AcceptorState::handle_commit()` - Handle COMMIT phase
- `ProposerState::get_next_ballot()` - Generate monotonic ballots

**Usage:**
```bash
# Start leader
./build/simpledb --port 7777 --dim 128 --role leader

# Start follower
./build/simpledb --port 7778 --role follower --leader localhost:7777

# Start with CasPaxos consensus
./build/simpledb --port 7777 --consensus --node-id 1
```

## Requirement 3: No Dependencies ✅

**Implementation:**
- Uses only C++17 standard library
- No external libraries or frameworks required

**Dependencies Used (all standard):**
- `<string>`, `<vector>`, `<unordered_map>` - Data structures
- `<mutex>`, `<thread>`, `<atomic>` - Concurrency
- `<fstream>`, `<iostream>` - File and console I/O
- `<sys/socket.h>`, `<netinet/in.h>` - POSIX sockets (system API)
- `<random>`, `<queue>`, `<cmath>` - Math and random utilities

**Build Requirements:**
- CMake 3.10+
- C++17 compiler (GCC/Clang/MSVC)
- POSIX-compliant OS for networking

## Requirement 4: Vector Storage and HNSW Implementation ✅

### HNSW Implementation

**Location:** `src/storage/hnsw.cpp`, `include/storage/hnsw.h`

**Features:**
- Hierarchical graph structure for fast similarity search
- Configurable parameters (M, ef_construction)
- Support for both distance metrics (Euclidean, cosine)
- Efficient insertion and search operations

**Key Components:**
- `HNSW::insert()` - Add vector with graph construction
- `HNSW::search()` - k-NN search using greedy algorithm
- `HNSW::search_layer()` - Layer-specific search
- `HNSW::select_neighbors_heuristic()` - Neighbor selection for graph quality
- `HNSW::get_random_level()` - Probabilistic level assignment

**Graph Structure:**
```
Level 2: [entry_point]
          |
Level 1: [A]---[B]---[C]
          |\    |    /|
Level 0: [A]-[B]-[C]-[D]-[E]
          (Most connected layer)
```

**Usage:**
```cpp
HNSW hnsw(dimension, M=16, ef_construction=200, DistanceMetric::EUCLIDEAN);
hnsw.insert(key, vector, offset);
auto results = hnsw.search(query, k=10, ef_search=50);
```

### Append-Only Log

**Location:** `src/storage/append_log.cpp`, `include/storage/append_log.h`

**Features:**
- Write-ahead logging for durability
- Vector data serialization support
- Sequential writes for performance
- Crash recovery via log replay

**Key Components:**
- `AppendLog::append()` - Write record to log
- `AppendLog::read()` - Read record at offset
- `AppendLog::read_all()` - Full log scan for recovery
- `AppendLog::sync()` - Force flush to disk
- `AppendLog::serialize_record()` - Binary serialization with vector support

**Record Format:**
```
[type:1][txn_id:8][timestamp:8][is_vector:1][key_len:4][key][data_len:4][vector_data]
```

**Implementation Note:**
- Uses separate file streams for reading and writing
- Write stream (`ofstream`) for appending
- Read streams (`ifstream`) created on-demand for recovery

## Requirement 5: ACID Compliance ✅

### Atomicity
- Transactions are all-or-nothing
- Write-ahead logging ensures durability
- Rollback discards uncommitted changes
- **Implementation:** `TransactionManager::commit_transaction()`, `rollback_transaction()`

### Consistency
- Lock manager prevents conflicting operations
- Invariants maintained across transactions
- **Implementation:** `LockManager::acquire_lock()`, two-phase locking protocol

### Isolation
- Two-phase locking protocol
- Shared locks for reads, exclusive locks for writes
- Transactions see consistent snapshot
- **Implementation:** `LockManager` with lock queues per key

### Durability
- Append-only log persists all operations
- Sync to disk on commit
- Recovery replays log on restart
- **Implementation:** `AppendLog::sync()`, `KVStore::recover()`

**Transaction API:**
```
BEGIN              # Start transaction
INSERT key vector  # Buffered write
GET key           # Read with lock
COMMIT            # Make changes durable
ROLLBACK          # Discard changes
```

**Locking Strategy:**
- Shared locks: Multiple readers allowed
- Exclusive locks: Single writer, no readers
- Lock manager prevents deadlocks via ordering
- Locks released on commit/rollback

**Components:**
- `TransactionManager` - Transaction lifecycle
- `LockManager` - Concurrency control
- `AppendLog` - Durability
- `KVStore` - Recovery

## Vector Database Commands ✅

### INSERT
```
INSERT key [v1,v2,v3,...]
```
Inserts a vector with the given key. Vector must match configured dimension.

### GET
```
GET key
```
Retrieves the vector associated with the key.

### SEARCH
```
SEARCH [v1,v2,v3,...] TOP k
```
Finds the k nearest neighbors to the query vector. Returns keys and distances.

### DELETE
```
DELETE key
```
Removes the vector with the given key.

## Architecture Summary

```
┌─────────────────────────────────────────┐
│         Network Layer                    │
│  - TCP Server (port 7777)               │
│  - Connection Handler                   │
│  - Text Protocol Parser                 │
│  - CAS Command Support                  │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      Transaction Layer                  │
│  - Transaction Manager                  │
│  - Lock Manager (2PL)                  │
│  - ACID Guarantees                     │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│         Storage Layer                   │
│  - Vector Store                        │
│  - HNSW Index                          │
│  - Append-Only Log                     │
│  - Recovery Manager                    │
└──────────────┬──────────────────────────┘
               │
┌──────────────▼──────────────────────────┐
│      Replication Layer                  │
│  - Leader/Follower Roles               │
│  - Log Shipping                        │
│  - Async Replication                   │
│  - CasPaxos Consensus Protocol         │
│    * Ballot-based versioning           │
│    * Two-phase protocol                │
│    * Majority quorum                   │
└─────────────────────────────────────────┘
```

## Testing

All features tested and verified:
- ✅ Concurrent connections (tested with multiple telnet clients)
- ✅ ACID transactions (BEGIN/COMMIT/ROLLBACK)
- ✅ CasPaxos consensus (CAS operations with ballot-based versioning)
- ✅ Vector operations (INSERT/GET/SEARCH/DELETE)
- ✅ Persistence (log file created and read on restart)
- ✅ HNSW indexing (integrated with vector store)
- ✅ Similarity search (Euclidean distance)
- ✅ No external dependencies (clean build with stdlib only)

Test files:
- `test.sh` - Automated tests for basic operations
- `test_caspaxos.cpp` - Unit tests for CasPaxos protocol
  - Ballot ordering and comparison
  - Acceptor PREPARE/COMMIT handling
  - CAS condition validation
  - Proposer ballot generation
  - Full CasPaxos operations (SET/GET/CAS)

## Security

CodeQL analysis completed with 0 vulnerabilities found.

## Performance Characteristics

- **Insert Performance:** O(log n) HNSW insertion + O(1) append to log
- **Search Performance:** Sub-linear approximate nearest neighbor search
- **Get Performance:** O(1) hash map lookup + O(1) HNSW access
- **Transaction Overhead:** Two-phase locking with lock queues
- **Replication:** Asynchronous, no impact on write latency
- **Concurrency:** Thread-per-connection model
- **Memory Usage:** All vectors kept in memory for fast access

## Limitations (By Design)

This is a learning/demonstration project. Production systems would need:
- HNSW parameter tuning per use case
- Dynamic dimension configuration
- Query optimizer and planner
- Connection pooling and resource limits
- Authentication and authorization
- TLS/SSL for secure connections
- Binary protocol for efficiency
- More sophisticated deadlock detection
- Full vector deletion (current implementation marks as deleted)
- Checkpointing and log compaction
- Leader election for CasPaxos
- Catch-up mechanism for replicas

## Conclusion

All requirements from the problem statement have been successfully implemented:
1. ✅ Concurrent connections - Multi-threaded TCP server
2. ✅ Replication - Leader-follower with log shipping + CasPaxos consensus
3. ✅ No dependencies - C++ standard library only
4. ✅ R-tree and append-only log - Full implementations
5. ✅ ACID compliance - Transactions with 2PL and WAL

**New Feature: CasPaxos Consensus Protocol**
- ✅ Two-phase protocol (PREPARE/COMMIT)
- ✅ Compare-and-swap atomic operations
- ✅ Ballot-based versioning
- ✅ Majority quorum consensus
- ✅ Single-node operation with extensible design
- ✅ Comprehensive unit tests

The database is functional, tested, and ready for use as a learning resource or simple key-value store with optional consensus support.
