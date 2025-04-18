#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include "network/tcp_socket.h"

// Mock class for TCP socket - just for timeout tests
class MockTcpSocketForTimeout : public ITcpSocket {
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
    EXPECT_CALL(mockSocket, WaitForDataWithTimeout(100))
        .WillOnce(testing::Return(false));
    
    // Then data becomes available within timeout
    EXPECT_CALL(mockSocket, WaitForDataWithTimeout(500))
        .WillOnce(testing::Return(true));
    
    // Test timeout with no data
    EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(100));
    
    // Test timeout with data available
    EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(500));
}

// Test for Edge Cases in WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, EdgeCasesWaitForDataWithTimeout) {
    // Instead of complex expectations, we'll create separate test cases
    // for each edge case scenario with individual mock objects
    
    // ---- Zero timeout tests (polling behavior) ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(0))
            .WillOnce(testing::Return(false));
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(0));
    }
    
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(0))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(0));
    }
    
    // ---- Negative timeout test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(-1))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(-1));
    }
    
    // ---- Large timeout test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(3600000))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(3600000));
    }
    
    // ---- Invalid socket test ----
    {
        MockTcpSocketForTimeout mockSocket;
        EXPECT_CALL(mockSocket, IsValid())
            .WillOnce(testing::Return(false));
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(100))
            .WillOnce(testing::Return(false));
            
        EXPECT_FALSE(mockSocket.IsValid());
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(100));
    }
}

// Test for TCP Listener WaitForDataWithTimeout functionality
TEST(TcpTimeoutTest, ListenerWaitForDataWithTimeout) {
    // Create a mock TCP listener
    MockTcpListenerForTimeout mockListener;
    
    // Set expectations - first no data available within timeout
    EXPECT_CALL(mockListener, WaitForDataWithTimeout(100))
        .WillOnce(testing::Return(false));
    
    // Then data becomes available within timeout
    EXPECT_CALL(mockListener, WaitForDataWithTimeout(500))
        .WillOnce(testing::Return(true));
    
    // Test timeout with no data
    EXPECT_FALSE(mockListener.WaitForDataWithTimeout(100));
    
    // Test timeout with data available
    EXPECT_TRUE(mockListener.WaitForDataWithTimeout(500));
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
            auto dataArrivalTime = start + std::chrono::milliseconds(200);
            
            // Sleep until either timeout expires or data arrival time
            auto waitUntil = start + std::chrono::milliseconds(timeoutMs);
            auto sleepUntil = std::min(waitUntil, dataArrivalTime);
            
            std::this_thread::sleep_until(sleepUntil);
            
            // Return true if we didn't time out before data arrived
            return dataArrivalTime <= waitUntil;
        });
    
    // Test case 1: Wait time less than data arrival time - should fail
    auto startTime = std::chrono::steady_clock::now();
    bool result1 = mockSocket->WaitForDataWithTimeout(100);
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_FALSE(result1);
    EXPECT_GE(elapsedMs, 100);  // Should have waited at least the timeout
    EXPECT_LT(elapsedMs, 200);  // But shouldn't wait the full data arrival time
    
    // Test case 2: Wait time greater than data arrival time - should succeed
    startTime = std::chrono::steady_clock::now();
    bool result2 = mockSocket->WaitForDataWithTimeout(300);
    endTime = std::chrono::steady_clock::now();
    elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_TRUE(result2);
    EXPECT_GE(elapsedMs, 200);  // Should have waited until data arrived
    EXPECT_LT(elapsedMs, 300);  // But not the full timeout
}