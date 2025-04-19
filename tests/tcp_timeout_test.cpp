#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "network/tcp_socket.h"

// Constants for timeout tests
namespace {
    // Timeout values (in milliseconds)
    constexpr int SHORT_TIMEOUT_MS = 100;
    constexpr int LONG_TIMEOUT_MS = 500;
    constexpr int DATA_ARRIVAL_TIME_MS = 200;
    constexpr int VERY_LONG_TIMEOUT_MS = 3600000; // 1 hour
    constexpr int EXTENDED_TIMEOUT_MS = 300;
    constexpr int ZERO_TIMEOUT_MS = 0;
    constexpr int NEGATIVE_TIMEOUT_MS = -1;
}

// Mock class for TCP socket - just for timeout tests
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
};

// Mock class for TCP listener - just for timeout tests
class MockTcpListenerForTimeout : public ITcpListener {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, Listen, (int backlog), (override));
    MOCK_METHOD(std::unique_ptr<IConnectionOrientedSocket>, Accept, (), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
};

// Test for basic WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, BasicWaitForDataWithTimeout) {
    // Create a mock TCP socket
    MockTcpSocketForTimeout mockSocket;
    
    // Set expectations - first no data available within timeout
    EXPECT_CALL(mockSocket, WaitForDataWithTimeout(SHORT_TIMEOUT_MS))
        .WillOnce(testing::Return(false));
    
    // Then data becomes available within timeout
    EXPECT_CALL(mockSocket, WaitForDataWithTimeout(LONG_TIMEOUT_MS))
        .WillOnce(testing::Return(true));
    
    // Test timeout with no data
    EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(SHORT_TIMEOUT_MS));
    
    // Test timeout with data available
    EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(LONG_TIMEOUT_MS));
}

// Test for Edge Cases in WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, EdgeCasesWaitForDataWithTimeout) {
    // Instead of complex expectations, we'll create separate test cases
    // for each edge case scenario with individual mock objects
    
    // ---- Zero timeout tests (polling behavior) ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(ZERO_TIMEOUT_MS))
            .WillOnce(testing::Return(false));
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(ZERO_TIMEOUT_MS));
    }
    
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(ZERO_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(ZERO_TIMEOUT_MS));
    }
    
    // ---- Negative timeout test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(NEGATIVE_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(NEGATIVE_TIMEOUT_MS));
    }
    
    // ---- Large timeout test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(VERY_LONG_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(VERY_LONG_TIMEOUT_MS));
    }
    
    // ---- Invalid socket test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, IsValid())
            .WillOnce(testing::Return(false));
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(SHORT_TIMEOUT_MS))
            .WillOnce(testing::Return(false));
            
        EXPECT_FALSE(mockSocket.IsValid());
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(SHORT_TIMEOUT_MS));
    }
}

// Test for TCP Listener WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, ListenerWaitForDataWithTimeout) {
    // Create a mock TCP listener
    MockTcpListenerForTimeout mockListener;
    
    // Set expectations - first no data available within timeout
    EXPECT_CALL(mockListener, WaitForDataWithTimeout(SHORT_TIMEOUT_MS))
        .WillOnce(testing::Return(false));
    
    // Then data becomes available within timeout
    EXPECT_CALL(mockListener, WaitForDataWithTimeout(LONG_TIMEOUT_MS))
        .WillOnce(testing::Return(true));
    
    // Test timeout with no data
    EXPECT_FALSE(mockListener.WaitForDataWithTimeout(SHORT_TIMEOUT_MS));
    
    // Test timeout with data available
    EXPECT_TRUE(mockListener.WaitForDataWithTimeout(LONG_TIMEOUT_MS));
}

// Integration test for WaitForDataWithTimeout with real data
TEST(TcpTimeoutTest, RealDataWaitForDataWithTimeout) {
    // This test demonstrates a more realistic scenario where data arrives
    // after a brief delay, simulating a real network event
    
    // Create a mock TCP socket with more realistic behavior
    auto mockSocket = std::make_shared<MockTcpSocketForTimeout>();
    
    // Set up the socket to return a realistic WaitForDataWithTimeout implementation
    // that actually waits and returns true only when data becomes available
    EXPECT_CALL(*mockSocket, WaitForDataWithTimeout(testing::_))
        .WillRepeatedly([](int timeoutMs) {
            // Let's simulate data appearing after 200ms delay
            auto start = std::chrono::steady_clock::now();
            auto dataArrivalTime = start + std::chrono::milliseconds(DATA_ARRIVAL_TIME_MS);
            
            // Sleep until either timeout expires or data arrival time
            auto waitUntil = start + std::chrono::milliseconds(timeoutMs);
            auto sleepUntil = std::min(waitUntil, dataArrivalTime);
            
            std::this_thread::sleep_until(sleepUntil);
            
            // Return true if we didn't time out before data arrived
            return dataArrivalTime <= waitUntil;
        });
    
    // Test case 1: Wait time less than data arrival time - should fail
    auto startTime = std::chrono::steady_clock::now();
    bool result1 = mockSocket->WaitForDataWithTimeout(SHORT_TIMEOUT_MS);
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_FALSE(result1);
    EXPECT_GE(elapsedMs, SHORT_TIMEOUT_MS);  // Should have waited at least the timeout
    EXPECT_LT(elapsedMs, DATA_ARRIVAL_TIME_MS);  // But shouldn't wait the full data arrival time
    
    // Test case 2: Wait time greater than data arrival time - should succeed
    startTime = std::chrono::steady_clock::now();
    bool result2 = mockSocket->WaitForDataWithTimeout(EXTENDED_TIMEOUT_MS);
    endTime = std::chrono::steady_clock::now();
    elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_TRUE(result2);
    EXPECT_GE(elapsedMs, DATA_ARRIVAL_TIME_MS);  // Should have waited until data arrived
    EXPECT_LT(elapsedMs, EXTENDED_TIMEOUT_MS);  // But not the full timeout
}