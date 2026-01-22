#include "replication/caspaxos.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>

using namespace simpledb::replication;

void test_ballot_ordering() {
    std::cout << "Testing Ballot ordering..." << std::endl;
    
    Ballot b1(1, 1);
    Ballot b2(1, 2);
    Ballot b3(2, 1);
    
    assert(b1 < b2);
    assert(b2 < b3);
    assert(b1 < b3);
    assert(b1 != b2);
    assert(b1 == b1);
    
    std::cout << "  ✓ Ballot ordering works correctly" << std::endl;
}

void test_acceptor_prepare() {
    std::cout << "Testing Acceptor PREPARE handling..." << std::endl;
    
    AcceptorState acceptor(1);
    
    // Test 1: First PREPARE should succeed
    Ballot b1(1, 1);
    PrepareMessage prepare1(b1, "key1", std::nullopt, "value1");
    auto promise1 = acceptor.handle_prepare(prepare1);
    assert(promise1.has_value());
    assert(!promise1->current_value.has_value());  // Key doesn't exist yet
    
    std::cout << "  ✓ First PREPARE accepted" << std::endl;
    
    // Test 2: Lower ballot should be rejected
    Ballot b0(0, 1);
    PrepareMessage prepare0(b0, "key1", std::nullopt, "value1");
    auto promise0 = acceptor.handle_prepare(prepare0);
    assert(!promise0.has_value());
    
    std::cout << "  ✓ Lower ballot rejected" << std::endl;
    
    // Test 3: Higher ballot should succeed
    Ballot b2(2, 1);
    PrepareMessage prepare2(b2, "key1", std::nullopt, "value2");
    auto promise2 = acceptor.handle_prepare(prepare2);
    assert(promise2.has_value());
    
    std::cout << "  ✓ Higher ballot accepted" << std::endl;
}

void test_acceptor_commit() {
    std::cout << "Testing Acceptor COMMIT handling..." << std::endl;
    
    AcceptorState acceptor(1);
    
    // Prepare first
    Ballot b1(1, 1);
    PrepareMessage prepare(b1, "key1", std::nullopt, "value1");
    acceptor.handle_prepare(prepare);
    
    // Commit the value
    CommitMessage commit(b1, "key1", "value1");
    auto ack = acceptor.handle_commit(commit);
    assert(ack.success);
    
    // Verify value is stored
    auto value = acceptor.get_value("key1");
    assert(value.has_value());
    assert(value->value == "value1");
    assert(value->committed);
    
    std::cout << "  ✓ COMMIT stored value correctly" << std::endl;
}

void test_cas_condition() {
    std::cout << "Testing CAS condition checking..." << std::endl;
    
    AcceptorState acceptor(1);
    
    // First, commit a value
    Ballot b1(1, 1);
    PrepareMessage prepare1(b1, "key1", std::nullopt, "initial");
    acceptor.handle_prepare(prepare1);
    CommitMessage commit1(b1, "key1", "initial");
    acceptor.handle_commit(commit1);
    
    // Test CAS with correct old value
    Ballot b2(2, 1);
    PrepareMessage prepare2(b2, "key1", std::string("initial"), "updated");
    auto promise2 = acceptor.handle_prepare(prepare2);
    assert(promise2.has_value());
    
    std::cout << "  ✓ CAS with correct old_value accepted" << std::endl;
    
    // Test CAS with incorrect old value
    Ballot b3(3, 1);
    PrepareMessage prepare3(b3, "key1", std::string("wrong"), "updated2");
    auto promise3 = acceptor.handle_prepare(prepare3);
    assert(!promise3.has_value());
    
    std::cout << "  ✓ CAS with incorrect old_value rejected" << std::endl;
}

void test_proposer_ballot_generation() {
    std::cout << "Testing Proposer ballot generation..." << std::endl;
    
    ProposerState proposer(1);
    
    Ballot b1 = proposer.get_next_ballot();
    Ballot b2 = proposer.get_next_ballot();
    
    assert(b2 > b1);
    assert(b1.node_id == 1);
    assert(b2.node_id == 1);
    
    std::cout << "  ✓ Ballot generation is monotonic" << std::endl;
    
    // Test ballot update
    Ballot higher(100, 2);
    proposer.update_ballot(higher);
    
    Ballot b3 = proposer.get_next_ballot();
    assert(b3 > higher);
    
    std::cout << "  ✓ Ballot update works correctly" << std::endl;
}

void test_caspaxos_basic_operations() {
    std::cout << "Testing CasPaxos basic operations..." << std::endl;
    
    std::vector<std::string> replicas;
    CasPaxos paxos(1, replicas);
    
    // Test SET operation
    bool result = paxos.set("test_key", "test_value");
    assert(result);
    
    std::cout << "  ✓ SET operation succeeded" << std::endl;
    
    // Test GET operation
    auto value = paxos.get("test_key");
    assert(value.has_value());
    assert(value.value() == "test_value");
    
    std::cout << "  ✓ GET operation retrieved correct value" << std::endl;
    
    // Test CAS with correct old value
    result = paxos.cas("test_key", std::string("test_value"), "new_value");
    assert(result);
    
    value = paxos.get("test_key");
    assert(value.has_value());
    assert(value.value() == "new_value");
    
    std::cout << "  ✓ CAS with correct old_value succeeded" << std::endl;
    
    // Test CAS with incorrect old value
    result = paxos.cas("test_key", std::string("wrong_value"), "should_fail");
    assert(!result);
    
    value = paxos.get("test_key");
    assert(value.has_value());
    assert(value.value() == "new_value");  // Value should not have changed
    
    std::cout << "  ✓ CAS with incorrect old_value failed as expected" << std::endl;
}

void test_caspaxos_delete() {
    std::cout << "Testing CasPaxos DELETE operation..." << std::endl;
    
    std::vector<std::string> replicas;
    CasPaxos paxos(1, replicas);
    
    // Set a value
    paxos.set("delete_key", "delete_value");
    
    // Delete with correct old value
    bool result = paxos.del("delete_key", std::string("delete_value"));
    assert(result);
    
    auto value = paxos.get("delete_key");
    assert(value.has_value());
    assert(value.value() == "");  // Deleted key has empty value
    
    std::cout << "  ✓ DELETE operation succeeded" << std::endl;
}

void test_versioned_value() {
    std::cout << "Testing VersionedValue..." << std::endl;
    
    Ballot b1(1, 1);
    VersionedValue v1(b1, "value1", true);
    
    assert(v1.ballot == b1);
    assert(v1.value == "value1");
    assert(v1.committed);
    
    std::cout << "  ✓ VersionedValue creation works correctly" << std::endl;
}

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "    CasPaxos Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        test_ballot_ordering();
        std::cout << std::endl;
        
        test_acceptor_prepare();
        std::cout << std::endl;
        
        test_acceptor_commit();
        std::cout << std::endl;
        
        test_cas_condition();
        std::cout << std::endl;
        
        test_proposer_ballot_generation();
        std::cout << std::endl;
        
        test_versioned_value();
        std::cout << std::endl;
        
        test_caspaxos_basic_operations();
        std::cout << std::endl;
        
        test_caspaxos_delete();
        std::cout << std::endl;
        
        std::cout << "========================================" << std::endl;
        std::cout << "  ✓ All tests passed!" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
