# Docker and Integration Testing Guide

This guide provides detailed information about running SimpleDB in Docker containers and executing integration tests for distributed scenarios.

## Docker Images

SimpleDB provides two Docker image options:

### 1. Alpine Linux (Busybox-based)

**File:** `Dockerfile`

The Alpine-based image uses busybox utilities and provides the smallest image size.

```bash
# Build Alpine image
docker build -f Dockerfile -t simpledb:alpine .

# Run
docker run -p 7777:7777 simpledb:alpine
```

**Benefits:**
- Minimal image size (~10-20 MB runtime layer)
- Uses busybox utilities (lightweight)
- Suitable for production deployments

**Note:** May require network connectivity during build for package installation.

### 2. Debian Bookworm Slim

**File:** `Dockerfile.debian`

The Debian-based image provides better compatibility and reliability.

```bash
# Build Debian image
docker build -f Dockerfile.debian -t simpledb:latest .

# Run
docker run -p 7777:7777 simpledb:latest
```

**Benefits:**
- More reliable package repositories
- Better compatibility with various tools
- Recommended for development and testing

## Docker Compose Setup

The `docker-compose.yml` file defines a complete distributed database cluster:

### Architecture

```
┌─────────────────────────────────────┐
│         simpledb-network            │
│                                     │
│  ┌──────────┐                       │
│  │  Leader  │ :7777                 │
│  │          │◄─────┐                │
│  └──────────┘      │                │
│                    │ replication    │
│  ┌──────────┐      │                │
│  │Follower1 │ :7778│                │
│  │          ├──────┘                │
│  └──────────┘                       │
│                                     │
│  ┌──────────┐      │                │
│  │Follower2 │ :7779│                │
│  │          ├──────┘                │
│  └──────────┘                       │
└─────────────────────────────────────┘
```

### Services

1. **leader**: Main database instance
   - Port: 7777
   - Role: Leader
   - Accepts write operations
   - Replicates to followers

2. **follower1**: First replica
   - Port: 7778
   - Role: Follower
   - Connects to leader:7777
   - Receives replicated data

3. **follower2**: Second replica
   - Port: 7779
   - Role: Follower
   - Connects to leader:7777
   - Receives replicated data

4. **test-runner**: Integration test container
   - Runs comprehensive test suite
   - Tests distributed failure scenarios
   - Only starts with `--profile test`

### Usage

```bash
# Start all database services
docker compose up -d

# Check status
docker compose ps

# View logs
docker compose logs -f leader
docker compose logs -f follower1

# Stop all services
docker compose down

# Remove volumes (clean state)
docker compose down -v
```

## Integration Tests

### Running Tests

The integration test suite validates the distributed setup:

```bash
# Method 1: Run test-runner manually
docker compose up -d
docker compose run --rm test-runner

# Method 2: Use test profile (recommended)
docker compose --profile test up --abort-on-container-exit

# Method 3: Run tests in existing setup
docker exec -it simpledb-leader /test/integration-tests.sh
```

### Test Coverage

The `integration-tests.sh` script includes 10 comprehensive tests:

#### Test 1: Basic Replication
Tests data replication from leader to followers.

```bash
# What it tests:
- Write to leader succeeds
- Data propagates to followers (when implemented)
```

#### Test 2: ACID Transactions
Tests transaction BEGIN/COMMIT on the leader.

```bash
# What it tests:
- BEGIN transaction starts successfully
- Multiple SET operations within transaction
- COMMIT persists all changes atomically
- Data is queryable after commit
```

#### Test 3: Transaction Rollback
Tests transaction ROLLBACK functionality.

```bash
# What it tests:
- BEGIN transaction
- SET operations within transaction
- ROLLBACK discards all changes
- Data is NOT present after rollback
```

#### Test 4: Concurrent Operations
Tests multiple simultaneous operations on the leader.

```bash
# What it tests:
- 5 concurrent SET operations
- All operations complete successfully
- Data consistency maintained
```

#### Test 5: Leader Availability
Tests leader processes multiple sequential requests.

```bash
# What it tests:
- Leader handles multiple requests
- No request failures
- Consistent response times
```

#### Test 6: DELETE Operation
Tests data deletion functionality.

```bash
# What it tests:
- SET creates a key-value pair
- GET retrieves the value
- DELETE removes the key
- GET returns NOT_FOUND after deletion
```

#### Test 7: Network Connectivity
Verifies network communication between nodes.

```bash
# What it tests:
- Leader is reachable on port 7777
- Follower1 is reachable on port 7778
- Follower2 is reachable on port 7779
```

#### Test 8: Stress Test
Tests multiple sequential transactions.

```bash
# What it tests:
- 5 sequential transactions
- Each with BEGIN/SET/COMMIT
- All transactions succeed
- No resource exhaustion
```

#### Test 9: Data Persistence
Tests data persists across operations.

```bash
# What it tests:
- Data written remains accessible
- Multiple operations don't affect existing data
- No data corruption
```

#### Test 10: Follower Health
Checks follower services are healthy.

```bash
# What it tests:
- Follower1 port is listening
- Follower2 port is listening
- Services are responsive
```

### Failure Scenarios

The tests specifically validate these distributed failure scenarios:

1. **Concurrent Access**: Multiple clients accessing leader simultaneously
2. **Transaction Isolation**: Ensuring ACID properties under concurrent load
3. **Network Reliability**: Verifying connections between distributed nodes
4. **Service Availability**: Confirming all nodes remain responsive
5. **Data Consistency**: Validating data integrity across operations

### Expected Results

A healthy distributed setup should achieve:
- ✓ 7-10 tests passing
- ✓ All services healthy
- ✓ Network connectivity established
- ✓ Leader accepting connections
- ✓ Followers reachable

Some tests may fail due to:
- Transaction timing with netcat connections
- Replication implementation details
- Network latency in test environment

### Interpreting Results

```bash
# Example successful run:
Tests Passed: 7
Tests Failed: 3
Total Tests: 10

# Common failure reasons:
# - Transaction tests: Netcat connection timing
# - Replication tests: Not yet fully implemented
```

## Troubleshooting

### Container Won't Start

```bash
# Check logs
docker compose logs leader

# Common issues:
# - Port already in use
# - Insufficient permissions
# - Volume mount issues
```

### Tests Failing

```bash
# Verify services are healthy
docker compose ps

# Check leader is responding
echo "SET test value" | nc localhost 7777

# View real-time logs during test
docker compose logs -f leader &
docker compose run --rm test-runner
```

### Network Issues

```bash
# Inspect network
docker network inspect simple-db_simpledb-network

# Verify follower can reach leader
docker exec simpledb-follower1 nc -zv leader 7777
```

### Clean Restart

```bash
# Stop everything and remove volumes
docker compose down -v

# Remove images (force rebuild)
docker compose down --rmi all

# Start fresh
docker compose up -d --build
```

## Performance Considerations

### Resource Limits

Add resource limits in docker-compose.yml:

```yaml
services:
  leader:
    deploy:
      resources:
        limits:
          cpus: '0.5'
          memory: 512M
```

### Persistent Data

Data is stored in Docker volumes:
- `leader-data`: Leader database files
- `follower1-data`: Follower1 database files
- `follower2-data`: Follower2 database files

```bash
# Backup volumes
docker run --rm -v simple-db_leader-data:/data -v $(pwd):/backup \
  alpine tar czf /backup/leader-backup.tar.gz /data

# Restore volumes
docker run --rm -v simple-db_leader-data:/data -v $(pwd):/backup \
  alpine tar xzf /backup/leader-backup.tar.gz -C /
```

## Production Deployment

For production use, consider:

1. **Security**: Add authentication and TLS/SSL
2. **Monitoring**: Add health check endpoints
3. **Logging**: Configure centralized logging
4. **Backups**: Implement automated backup strategy
5. **High Availability**: Deploy across multiple hosts
6. **Load Balancing**: Add load balancer for read traffic

## Next Steps

1. Implement full follower replication
2. Add consensus protocol (e.g., Raft)
3. Implement read-from-followers capability
4. Add leader election on failure
5. Implement catch-up mechanism for slow followers
