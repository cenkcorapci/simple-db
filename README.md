# simple-db

A simple key-value database implemented in C++ with advanced features:

## Features

- **Concurrent Connections**: Multi-threaded TCP server supporting multiple simultaneous client connections
- **ACID Compliance**: Full transaction support with isolation and atomicity guarantees
  - BEGIN/COMMIT/ROLLBACK transaction commands
  - Two-phase locking for concurrency control
  - Write-ahead logging for durability
- **R-tree Indexing**: Spatial indexing structure for efficient key lookup
- **Append-Only Log**: Persistent storage with crash recovery
- **Leader-Follower Replication**: Distributed replication for high availability
- **No External Dependencies**: Uses only C++ standard library

## Architecture

### Storage Layer
- **Append-Only Log**: Durable write-ahead log for all operations
- **R-tree Index**: Spatial data structure for efficient key-value lookups
- **In-Memory Cache**: Fast access to frequently used data

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
./build/simpledb --port 7777 --log simpledb.log --role leader
```

### Start a follower server:
```bash
./build/simpledb --port 7778 --log replica.log --role follower --leader localhost:7777
```

### Command-line options:
- `--port <port>`: Server port (default: 7777)
- `--log <file>`: Log file path (default: simpledb.log)
- `--role <leader|follower>`: Replication role (default: leader)
- `--leader <host:port>`: Leader address (for follower role)
- `--help`: Show help message

## Usage

Connect to the database using any TCP client (e.g., telnet, nc):

```bash
telnet localhost 7777
```

### Commands

#### Basic Operations:
```
SET key value      # Store a key-value pair
GET key           # Retrieve a value by key
DELETE key        # Delete a key-value pair
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

#### Auto-commit mode (single operations):
```
SET name Alice
OK
GET name
OK Alice
DELETE name
OK
GET name
NOT_FOUND
```

#### Explicit transactions:
```
BEGIN
OK
SET account1 100
OK
SET account2 200
OK
COMMIT
OK
```

#### Transaction rollback:
```
BEGIN
OK
SET balance 1000
OK
ROLLBACK
OK
GET balance
NOT_FOUND
```

## Protocol

The database uses a simple text-based protocol:
- Commands are newline-terminated
- Responses start with OK, ERROR, or NOT_FOUND
- Multi-word values are space-separated

## ACID Guarantees

- **Atomicity**: Transactions are all-or-nothing via write-ahead logging
- **Consistency**: Lock manager ensures consistent state
- **Isolation**: Two-phase locking provides serializable isolation
- **Durability**: Append-only log persists all committed transactions

## Implementation Details

### R-tree
- Max entries per node: 4 (configurable)
- Keys are mapped to 2D space using hash function
- Provides efficient spatial queries (though not currently exposed via API)

### Locking
- Shared locks for reads
- Exclusive locks for writes
- Deadlock prevention through lock ordering (simple implementation)

### Replication
- Asynchronous log shipping
- Followers replay leader's append-only log
- Basic implementation (full production would need heartbeats, catch-up, etc.)

## Testing

The database can be tested with multiple concurrent connections:

```bash
# Terminal 1
./build/simpledb

# Terminal 2
telnet localhost 7777

# Terminal 3
telnet localhost 7777
```

Both clients can perform operations concurrently with full ACID guarantees.

## Limitations

This is a learning project with simplified implementations:
- No query optimizer
- Simple replication (no consensus protocol like Raft)
- Limited error handling in some edge cases
- No authentication/authorization
- No SSL/TLS support
- Fixed text protocol (no binary protocol)
- R-tree is functional but not fully optimized

## License

See LICENSE file.

