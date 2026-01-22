#!/bin/bash
# Comprehensive test script for SimpleDB Vector Database

echo "========================================="
echo "SimpleDB Vector Database Test Suite"
echo "========================================="
echo ""

# Build the project
echo "Building SimpleDB..."
make build > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi
echo "✓ Build successful"
echo ""

# Clean up old test files
rm -f test_vectordb.log

# Start server
echo "Starting SimpleDB server (dimension=4)..."
./build/simpledb --port 7777 --log test_vectordb.log --dim 4 > /tmp/server.log 2>&1 &
SERVER_PID=$!
sleep 2

if ! ps -p $SERVER_PID > /dev/null; then
    echo "✗ Server failed to start"
    exit 1
fi
echo "✓ Server started (PID: $SERVER_PID)"
echo ""

# Test 1: Basic vector insertion
echo "Test 1: Vector Insertion"
(
echo "INSERT embedding1 [0.1,0.2,0.3,0.4]"
echo "QUIT"
) | nc localhost 7777 | grep -q "^OK$"
if [ $? -eq 0 ]; then
    echo "✓ Vector insertion successful"
else
    echo "✗ Vector insertion failed"
fi
echo ""

# Test 2: Vector retrieval
echo "Test 2: Vector Retrieval"
(
echo "GET embedding1"
echo "QUIT"
) | nc localhost 7777 | grep -q "^OK \[.*\]$"
if [ $? -eq 0 ]; then
    echo "✓ Vector retrieval successful"
else
    echo "✗ Vector retrieval failed"
fi
echo ""

# Test 3: Insert multiple vectors for search
echo "Test 3: Inserting Multiple Vectors"
(
echo "INSERT vec1 [1.0,0.0,0.0,0.0]"
echo "INSERT vec2 [0.0,1.0,0.0,0.0]"
echo "INSERT vec3 [0.0,0.0,1.0,0.0]"
echo "INSERT vec4 [0.7,0.7,0.0,0.0]"
echo "QUIT"
) | nc localhost 7777 | grep -c "^OK$" | grep -q "4"
if [ $? -eq 0 ]; then
    echo "✓ Multiple vector insertion successful"
else
    echo "✗ Multiple vector insertion failed"
fi
echo ""

# Test 4: Similarity search
echo "Test 4: Similarity Search"
SEARCH_RESULT=$(
(
echo "SEARCH [1.0,0.1,0.0,0.0] TOP 3"
echo "QUIT"
) | nc localhost 7777
)
echo "$SEARCH_RESULT" | grep -q "OK.*results"
if [ $? -eq 0 ]; then
    echo "✓ Similarity search successful"
    echo "  Results:"
    echo "$SEARCH_RESULT" | grep "distance=" | sed 's/^/    /'
else
    echo "✗ Similarity search failed"
fi
echo ""

# Test 5: Vector deletion
echo "Test 5: Vector Deletion"
(
echo "DELETE vec3"
echo "GET vec3"
echo "QUIT"
) | nc localhost 7777 | tail -1 | grep -q "NOT_FOUND"
if [ $? -eq 0 ]; then
    echo "✓ Vector deletion successful"
else
    echo "✗ Vector deletion failed"
fi
echo ""

# Test 6: Transactions
echo "Test 6: Transaction Support"
(
echo "BEGIN"
echo "INSERT txn_vec1 [1.0,2.0,3.0,4.0]"
echo "INSERT txn_vec2 [5.0,6.0,7.0,8.0]"
echo "COMMIT"
echo "GET txn_vec1"
echo "QUIT"
) | nc localhost 7777 | grep -c "^OK" | grep -q "4"
if [ $? -eq 0 ]; then
    echo "✓ Transaction commit successful"
else
    echo "✗ Transaction commit failed"
fi
echo ""

# Test 7: Transaction rollback
echo "Test 7: Transaction Rollback"
(
echo "BEGIN"
echo "INSERT rollback_vec [1.0,2.0,3.0,4.0]"
echo "ROLLBACK"
echo "GET rollback_vec"
echo "QUIT"
) | nc localhost 7777 | tail -1 | grep -q "NOT_FOUND"
if [ $? -eq 0 ]; then
    echo "✓ Transaction rollback successful"
else
    echo "✗ Transaction rollback failed"
fi
echo ""

# Test 8: Persistence
echo "Test 8: Persistence and Recovery"
echo "  Stopping server..."
kill $SERVER_PID
sleep 2

echo "  Restarting server..."
./build/simpledb --port 7777 --log test_vectordb.log --dim 4 > /tmp/server2.log 2>&1 &
SERVER_PID=$!
sleep 2

if ! ps -p $SERVER_PID > /dev/null; then
    echo "✗ Server failed to restart"
    exit 1
fi

(
echo "GET vec1"
echo "GET vec2"
echo "SEARCH [0.0,1.0,0.1,0.0] TOP 2"
echo "QUIT"
) | nc localhost 7777 | grep -q "OK \[1.000000,0.000000,0.000000,0.000000\]"
if [ $? -eq 0 ]; then
    echo "✓ Persistence and recovery successful"
else
    echo "✗ Persistence and recovery failed"
fi
echo ""

# Cleanup
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null
rm -f test_vectordb.log

echo ""
echo "========================================="
echo "Test Suite Complete"
echo "========================================="
