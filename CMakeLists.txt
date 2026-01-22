cmake_minimum_required(VERSION 3.10)
project(SimpleDB VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(SOURCES
    src/main.cpp
    src/storage/append_log.cpp
    src/storage/hnsw.cpp
    src/storage/kv_store.cpp
    src/transaction/transaction_manager.cpp
    src/transaction/lock_manager.cpp
    src/network/server.cpp
    src/network/connection.cpp
    src/replication/replicator.cpp
    src/replication/caspaxos.cpp
)

# Create executable
add_executable(simpledb ${SOURCES})

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(simpledb Threads::Threads)

# Test executable for CasPaxos
add_executable(test_caspaxos 
    test_caspaxos.cpp
    src/replication/caspaxos.cpp
)
target_link_libraries(test_caspaxos Threads::Threads)
