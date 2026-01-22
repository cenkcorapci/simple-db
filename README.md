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

## Building

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

## Testing

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

## Limitations

This is a learning project with simplified implementations:
- Fixed vector dimension per database instance
- No query optimizer
- Simple replication (no consensus protocol like Raft)
- Limited error handling in some edge cases
- No authentication/authorization
- No SSL/TLS support
- Fixed text protocol (no binary protocol)
- HNSW parameters are not dynamically tunable

## License

See LICENSE file.


