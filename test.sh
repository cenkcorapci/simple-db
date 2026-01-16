#!/bin/bash
# Simple test script for SimpleDB

echo "====================================="
echo "SimpleDB Test Script"
echo "====================================="
echo ""

# Check if server is running
if ! nc -z localhost 7777 2>/dev/null; then
    echo "Starting SimpleDB server on port 7777..."
    ./build/simpledb --port 7777 > /tmp/simpledb_test.log 2>&1 &
    SERVER_PID=$!
    echo "Server PID: $SERVER_PID"
    sleep 2
else
    echo "Server already running on port 7777"
fi

echo ""
echo "Running tests..."
echo ""

# Test 1: Basic operations
echo "Test 1: Basic SET/GET operations"
(
echo "SET key1 value1"
echo "GET key1"
echo "SET key2 value2"
echo "GET key2"
echo "QUIT"
) | nc localhost 7777 2>&1 | grep -E "OK|NOT_FOUND"

echo ""

# Test 2: Transactions
echo "Test 2: Transaction COMMIT"
(
echo "BEGIN"
echo "SET account1 100"
echo "SET account2 200"
echo "COMMIT"
echo "GET account1"
echo "GET account2"
echo "QUIT"
) | nc localhost 7777 2>&1 | grep -E "OK|NOT_FOUND"

echo ""

# Test 3: Transaction ROLLBACK
echo "Test 3: Transaction ROLLBACK"
(
echo "BEGIN"
echo "SET temp_key temp_value"
echo "ROLLBACK"
echo "GET temp_key"
echo "QUIT"
) | nc localhost 7777 2>&1 | grep -E "OK|NOT_FOUND"

echo ""

# Test 4: DELETE operation
echo "Test 4: DELETE operation"
(
echo "SET delete_me test"
echo "GET delete_me"
echo "DELETE delete_me"
echo "GET delete_me"
echo "QUIT"
) | nc localhost 7777 2>&1 | grep -E "OK|NOT_FOUND"

echo ""
echo "====================================="
echo "Tests completed!"
echo "====================================="

# Kill the server if we started it
if [ ! -z "$SERVER_PID" ]; then
    echo "Stopping server (PID: $SERVER_PID)..."
    kill $SERVER_PID 2>/dev/null
fi
