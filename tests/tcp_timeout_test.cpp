#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "network/tcp_socket.h"
#include "utils/test_utils.h"

using namespace test_utils;

// Specialized mock class with the same functionality as the generic version,
// but with a TCP-specific name for test clarity
class MockTcpSocketForTimeout final : public ITcpSocket {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, Connect, (const NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(int, Send, (const std::vector<std::byte>& data), (override));
    MOCK_METHOD(int, Receive, (std::vector<std::byte>& buffer), (override));
    MOCK_METHOD(NetworkAddress, GetRemoteAddress, (), (const, override));
    MOCK_METHOD(bool, SetNoDelay, (bool enable), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetConnectTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetSocketOption, (int level, int optionName, const void* optionValue, socklen_t optionLen), (override));
    MOCK_METHOD(bool, GetSocketOption, (int level, int optionName, void* optionValue, socklen_t* optionLen), (const, override));
};

// Specialized mock class for TCP listener with the same functionality
// but TCP-specific name for test clarity
class MockTcpListenerForTimeout : public ITcpListener {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, Listen, (int backlog), (override));
    MOCK_METHOD(std::unique_ptr<IConnectionOrientedSocket>, Accept, (), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetSocketOption, (int level, int optionName, const void* optionValue, socklen_t optionLen), (override));
    MOCK_METHOD(bool, GetSocketOption, (int level, int optionName, void* optionValue, socklen_t* optionLen), (const, override));
};

// Test for basic WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, BasicWaitForDataWithTimeout) {
    // Create a mock TCP socket
    MockTcpSocketForTimeout mockSocket;
    
    // Use the shared test function
    testBasicWaitForDataWithTimeout(mockSocket);
}

// Test for Edge Cases in WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, EdgeCasesWaitForDataWithTimeout) {
    // Use the shared test function that handles all edge cases
    testEdgeCasesWaitForDataWithTimeout<MockTcpSocketForTimeout>();
}

// Test for TCP Listener WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, ListenerWaitForDataWithTimeout) {
    // Create a mock TCP listener
    MockTcpListenerForTimeout mockListener;
    
    // Use the shared test function
    testBasicWaitForDataWithTimeout(mockListener);
}

// Integration test for WaitForDataWithTimeout with real data
TEST(TcpTimeoutTest, RealDataWaitForDataWithTimeout) {
    // Create a mock TCP socket with more realistic behavior
    auto mockSocket = std::make_shared<MockTcpSocketForTimeout>();
    
    // Use the shared test function for realistic data arrival simulation
    testRealDataWaitForDataWithTimeout(mockSocket);
}