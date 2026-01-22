# simple-db

A vector database implemented in C++ with HNSW (Hierarchical Navigable Small World) for efficient similarity search:

## Features

- **HNSW Vector Search**: Hierarchical Navigable Small World algorithm for fast approximate nearest neighbor search
- **Concurrent Connections**: Multi-threaded TCP server supporting multiple simultaneous client connections
- **ACID Compliance**: Full transaction support with isolation and atomicity guarantees
  - BEGIN/COMMIT/ROLLBACK transaction commands
  - Two-phase locking for concurrency control
  - Write-ahead logging for durability
- **Flexible Distance Metrics**: Support for both Euclidean distance and cosine similarity
- **Append-Only Log**: Persistent storage with crash recovery
- **Leader-Follower Replication**: Distributed replication for high availability
- **CasPaxos Consensus**: Fault-tolerant consensus protocol for distributed coordination
  - Compare-and-swap (CAS) atomic operations
  - Majority quorum-based consensus
  - Ballot-based versioning
- **No External Dependencies**: Uses only C++ standard library

## Architecture

### Storage Layer
- **Append-Only Log**: Durable write-ahead log for all operations
- **HNSW Index**: Hierarchical graph structure for efficient similarity search
- **In-Memory Index**: Fast access to vector data

### Transaction Layer
- **Transaction Manager**: Handles transaction lifecycle (begin, commit, rollback)
- **Lock Manager**: Two-phase locking protocol for concurrency control
- Supports both auto-commit and explicit transactions

### Network Layer
- **TCP Server**: Multi-threaded server accepting concurrent connections
- **Connection Handler**: Per-connection handler for client requests
- Simple text-based protocol

### Replication Layer
- **Leader-Follower**: Asynchronous replication from leader to followers
- **Log Shipping**: Replicates append-only log entries
- **CasPaxos Consensus**: Optional consensus protocol for fault-tolerant coordination
  - Two-phase protocol (PREPARE/COMMIT)
  - Ballot-based versioning for conflict resolution
  - Majority quorum for operation acceptance

## Building

### Native Build

Requires:
- CMake 3.10+
- C++17 compiler (GCC, Clang, or MSVC)

```bash
make build
```

Or manually:
```bash
mkdir build
cd build
cmake ..
make
```

### Docker Build

Build and run using Docker:

```bash
# Build the Docker image (using Debian-based image)
docker build -f Dockerfile.debian -t simpledb:latest .

# Or use Alpine Linux (busybox-based) for smaller image
docker build -f Dockerfile -t simpledb:alpine .

# Run a single instance
docker run -p 7777:7777 simpledb:latest
```

### Docker Compose - Distributed Setup

Run a complete distributed setup with leader and followers:

```bash
# Start all services (1 leader + 2 followers)
docker compose up -d

# Check service status
docker compose ps

# View logs
docker compose logs -f

# Stop all services
docker compose down
```

The docker compose setup includes:
- **Leader** on port 7777
- **Follower 1** on port 7778
- **Follower 2** on port 7779
cd build
cmake ..
make
```

## Running

### Start a leader server:
```bash
./build/simpledb --port 7777 --log simpledb.log --dim 128 --role leader
```

### Start a follower server:
```bash
./build/simpledb --port 7778 --log replica.log --dim 128 --role follower --leader localhost:7777
```

### Command-line options:
- `--port <port>`: Server port (default: 7777)
- `--log <file>`: Log file path (default: simpledb.log)
- `--dim <dimension>`: Vector dimension (default: 128)
- `--role <leader|follower>`: Replication role (default: leader)
- `--leader <host:port>`: Leader address (for follower role)
- `--consensus`: Enable CasPaxos consensus protocol
- `--node-id <id>`: Node ID for CasPaxos (default: 1)
- `--help`: Show help message

## Usage

Connect to the database using any TCP client (e.g., telnet, nc):

```bash
telnet localhost 7777
```

### Commands

#### Vector Operations:
```
INSERT key [v1,v2,v3,...]    # Insert a vector
GET key                      # Retrieve a vector by key
SEARCH [v1,v2,v3,...] TOP k  # Find k nearest neighbors
DELETE key                   # Delete a vector
```

#### Transaction Operations:
```
BEGIN             # Start a transaction
COMMIT            # Commit the current transaction
ROLLBACK          # Rollback the current transaction
```

#### Other:
```
QUIT              # Close connection
```

### Examples

#### Insert and retrieve vectors:
```
INSERT vec1 [1.0,0.0,0.0]
OK
GET vec1
OK [1.000000,0.000000,0.000000]
```

#### Similarity search:
```
INSERT embedding1 [0.1,0.2,0.3,0.4]
OK
INSERT embedding2 [0.2,0.3,0.4,0.5]
OK
INSERT embedding3 [0.9,0.8,0.7,0.6]
OK
SEARCH [0.15,0.25,0.35,0.45] TOP 2
OK 2 results
embedding2 distance=0.141421
embedding1 distance=0.244949
```

#### CasPaxos consensus operations:
```
# Start server with consensus enabled
./build/simpledb --consensus --node-id 1

# Connect and use CAS
CAS counter null 1
OK
GET counter
OK 1
CAS counter 1 2
OK
GET counter
OK 2
CAS counter 1 3
ERROR: CAS failed - condition not met or no quorum
```

#### Explicit transactions:
#### Transactions:
```
BEGIN
OK
INSERT vec1 [1.0,2.0,3.0]
OK
INSERT vec2 [4.0,5.0,6.0]
OK
COMMIT
OK
```

#### Deletion:
```
DELETE vec1
OK
GET vec1
NOT_FOUND
```

## Protocol

The database uses a simple text-based protocol:
- Commands are newline-terminated
- Vectors are formatted as comma-separated floats in square brackets: `[v1,v2,v3,...]`
- Responses start with OK, ERROR, or NOT_FOUND
- Search results include similarity distances

## ACID Guarantees

- **Atomicity**: Transactions are all-or-nothing via write-ahead logging
- **Consistency**: Lock manager ensures consistent state
- **Isolation**: Two-phase locking provides serializable isolation
- **Durability**: Append-only log persists all committed transactions

## Implementation Details

### HNSW Algorithm
- Hierarchical graph structure with multiple layers
- Probabilistic layer assignment for balanced search
- Configurable parameters: M (max connections), ef_construction (build time quality)
- Supports both Euclidean distance and cosine similarity metrics

### Locking
- Shared locks for reads
- Exclusive locks for writes
- Deadlock prevention through lock ordering

### Replication
- Asynchronous log shipping
- Followers replay leader's append-only log
- Basic implementation (full production would need heartbeats, catch-up, etc.)

### CasPaxos Consensus
- Two-phase protocol: PREPARE and COMMIT
- Compare-and-swap (CAS) atomic operations
- Ballot-based versioning for conflict resolution
- Majority quorum (N/2 + 1) for operation acceptance
- Single-node operation supported (quorum of 1)
- Network communication stubs (for multi-node extension)

## Testing

### Manual Testing

The database can be tested with multiple concurrent connections:

```bash
# Terminal 1
./build/simpledb --dim 128

# Terminal 2
telnet localhost 7777

# Terminal 3
telnet localhost 7777
```

Both clients can perform operations concurrently with full ACID guarantees.

### Integration Tests

Run comprehensive integration tests for the distributed setup:

```bash
# Start the distributed environment
docker compose up -d

# Run integration tests
docker compose run --rm test-runner

# Or use the test profile
docker compose --profile test up --abort-on-container-exit
```

The integration test suite includes:
- **Basic Replication**: Tests data replication from leader to followers
- **ACID Transactions**: Tests transaction BEGIN/COMMIT/ROLLBACK
- **Concurrent Operations**: Tests multiple simultaneous operations
- **Leader Availability**: Tests leader service reliability
- **DELETE Operations**: Tests data deletion functionality
- **Network Connectivity**: Verifies network communication between nodes
- **Stress Testing**: Tests multiple sequential transactions
- **Data Persistence**: Verifies data persists across operations
- **Follower Health**: Checks follower services are running correctly

### Failure Scenarios Tested

The integration tests specifically check:
1. Leader node handling multiple concurrent clients
2. Transaction isolation and atomicity
3. Data consistency across operations
4. Network connectivity in distributed setup
5. Service health and availability

## Limitations

This is a learning project with simplified implementations:
- Fixed vector dimension per database instance
- No query optimizer
- Simple replication (CasPaxos provides consensus but network layer needs extension for multi-node)
- Limited error handling in some edge cases
- No authentication/authorization
- No SSL/TLS support
- Fixed text protocol (no binary protocol)
- CasPaxos network communication stubs (single-node only in current implementation)
- HNSW parameters are not dynamically tunable

## License

See LICENSE file.


