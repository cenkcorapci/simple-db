#!/bin/bash
# Demonstration script for CasPaxos consensus in SimpleDB

echo "=========================================="
echo "  CasPaxos Consensus Demonstration"
echo "=========================================="
echo ""

# Check if server binary exists
if [ ! -f "./build/simpledb" ]; then
    echo "Error: simpledb binary not found. Please run 'make build' first."
    exit 1
fi

# Start server with CasPaxos enabled
echo "Starting SimpleDB with CasPaxos consensus..."
./build/simpledb --port 7780 --consensus --node-id 1 > /tmp/caspaxos_demo.log 2>&1 &
SERVER_PID=$!
echo "Server PID: $SERVER_PID"
sleep 2

echo ""
echo "Server started successfully!"
echo ""

# Test 1: Basic CAS and GET operations
echo "=========================================="
echo "Test 1: CAS Operations"
echo "=========================================="
echo ""
(
echo "CAS counter null 1"
echo "GET counter"
echo "CAS counter 1 2"
echo "GET counter"
echo "CAS counter 1 3"
echo "GET counter"
echo "QUIT"
) | nc localhost 7780
echo ""

# Test 2: Multiple keys
echo "=========================================="
echo "Test 2: Multiple Keys"
echo "=========================================="
echo ""
(
echo "CAS user:alice null active"
echo "CAS user:bob null active"
echo "GET user:alice"
echo "GET user:bob"
echo "QUIT"
) | nc localhost 7780
echo ""

# Cleanup
echo "=========================================="
echo "Stopping server (PID: $SERVER_PID)..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null
echo "Demonstration completed!"
echo "=========================================="

