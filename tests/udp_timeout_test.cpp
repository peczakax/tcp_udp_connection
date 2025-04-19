#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "network/udp_socket.h"
#include "utils/test_utils.h"

using namespace test_utils;

// Specialized mock class for UDP timeout tests
class MockUdpSocketForTimeout final : public IUdpSocket {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(int, SendTo, (const std::vector<std::byte>& data, const NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(int, ReceiveFrom, (std::vector<std::byte>& buffer, NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(bool, SetBroadcast, (bool enable), (override));
    MOCK_METHOD(bool, JoinMulticastGroup, (const NetworkAddress& groupAddress), (override));
    MOCK_METHOD(bool, LeaveMulticastGroup, (const NetworkAddress& groupAddress), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
};

// Test for basic UDP WaitForDataWithTimeout functionality
TEST(UdpTimeoutTest, BasicWaitForDataWithTimeout) {
    // Create a mock UDP socket
    MockUdpSocketForTimeout mockSocket;
    
    // Use the shared test function
    testBasicWaitForDataWithTimeout(mockSocket);
}

// Test for Edge Cases in UDP WaitForDataWithTimeout functionality
TEST(UdpTimeoutTest, EdgeCasesWaitForDataWithTimeout) {
    // Use the shared test function that handles all edge cases
    testEdgeCasesWaitForDataWithTimeout<MockUdpSocketForTimeout>();
}

// Integration test for UDP WaitForDataWithTimeout with real data
TEST(UdpTimeoutTest, RealDataWaitForDataWithTimeout) {
    // Create a mock UDP socket with more realistic behavior
    auto mockSocket = std::make_shared<MockUdpSocketForTimeout>();
    
    // Use the shared test function for realistic data arrival simulation
    testRealDataWaitForDataWithTimeout(mockSocket);
}