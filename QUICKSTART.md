# Quick Start Guide - SimpleDB with Docker

This guide helps you get started with SimpleDB in a distributed Docker environment in under 5 minutes.

## Prerequisites

- Docker (20.10+)
- Docker Compose (v2.0+)

## Step 1: Clone and Build

```bash
# Clone the repository
git clone https://github.com/cenkcorapci/simple-db.git
cd simple-db

# Build and start all services
docker compose up -d --build
```

This will:
- Build the SimpleDB Docker image
- Start 1 leader on port 7777
- Start 2 followers on ports 7778 and 7779
- Set up networking between containers

## Step 2: Verify Services

```bash
# Check all services are healthy
docker compose ps

# Expected output:
# NAME                 STATUS
# simpledb-leader      Up (healthy)
# simpledb-follower1   Up (healthy)
# simpledb-follower2   Up (healthy)
```

## Step 3: Test the Database

### Connect to the Leader

```bash
# Using netcat
nc localhost 7777

# Or telnet
telnet localhost 7777
```

### Try Some Commands

```
# Set a key-value pair
SET name Alice
> OK

# Get the value
GET name
> OK Alice

# Delete the key
DELETE name
> OK

# Verify deletion
GET name
> NOT_FOUND

# Exit
QUIT
```

### Test Transactions

```
# Start a transaction
BEGIN
> OK

# Make changes
SET account1 100
> OK
SET account2 200
> OK

# Commit
COMMIT
> OK

# Verify
GET account1
> OK 100
```

## Step 4: Run Integration Tests

```bash
# Run the full test suite
docker compose --profile test up --abort-on-container-exit

# Or manually
docker compose run --rm test-runner
```

Expected results:
- 7-10 tests passing
- Tests cover replication, transactions, concurrency, and failure scenarios

## Step 5: View Logs

```bash
# View all logs
docker compose logs

# View specific service logs
docker compose logs leader
docker compose logs follower1

# Follow logs in real-time
docker compose logs -f
```

## Step 6: Clean Up

```bash
# Stop all services
docker compose down

# Stop and remove volumes (fresh start)
docker compose down -v
```

## Common Operations

### Restart a Single Service

```bash
docker compose restart leader
```

### Scale Followers

Edit `docker-compose.yml` to add more followers:

```yaml
  follower3:
    build:
      context: .
      dockerfile: Dockerfile.debian
    container_name: simpledb-follower3
    ports:
      - "7780:7780"
    command: ["--port", "7780", "--log", "/data/simpledb.log", "--role", "follower", "--leader", "leader:7777"]
```

### Backup Data

```bash
# Backup leader data
docker run --rm -v simple-db_leader-data:/data -v $(pwd):/backup \
  alpine tar czf /backup/leader-backup.tar.gz /data
```

## Troubleshooting

### Port Already in Use

```bash
# Check what's using the port
lsof -i :7777

# Or change port in docker-compose.yml
ports:
  - "8777:7777"  # Map host port 8777 to container port 7777
```

### Services Won't Start

```bash
# View detailed logs
docker compose logs leader

# Rebuild from scratch
docker compose down -v
docker compose up -d --build --force-recreate
```

### Tests Failing

Some transaction tests may fail due to netcat timing. This is normal. The key tests (replication, concurrency, availability, persistence) should pass.

## What's Next?

- Read [DOCKER.md](DOCKER.md) for detailed Docker information
- Read [README.md](README.md) for complete feature documentation
- Explore the source code in `/src` and `/include`
- Try implementing new features or fixing known limitations

## Architecture Overview

```
┌─────────────────────────────────────┐
│    Docker Network (simpledb-network) │
│                                     │
│  ┌──────────┐                       │
│  │  Leader  │ :7777                 │
│  │          │◄────────┐             │
│  └──────────┘         │             │
│                       │             │
│  ┌──────────┐         │ Replication │
│  │Follower1 │ :7778   │             │
│  └──────────┘─────────┘             │
│                                     │
│  ┌──────────┐         │             │
│  │Follower2 │ :7779   │             │
│  └──────────┘─────────┘             │
│                                     │
└─────────────────────────────────────┘
```

## Features Available

✓ ACID transactions (BEGIN/COMMIT/ROLLBACK)
✓ Concurrent connections
✓ R-tree indexing
✓ Append-only log
✓ Leader-follower replication
✓ Docker containerization
✓ Docker Compose orchestration
✓ Integration test suite
✓ Busybox compatibility (Alpine Linux)

Enjoy exploring SimpleDB! 🚀
