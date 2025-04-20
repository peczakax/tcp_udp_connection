#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Include Win32 headers in the correct order to avoid conflicts
#ifdef _WIN32
#define NOMINMAX // Add this line to prevent min/max macro conflicts
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <Windows.h>
#endif

#include "network/network.h"
#include "network/tcp_socket.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"
#include "network/byte_utils.h"

namespace test_utils {

// Common timeout-related constants
namespace timeouts {
    // Timeout values (in milliseconds)
    constexpr int SHORT_TIMEOUT_MS = 100;
    constexpr int LONG_TIMEOUT_MS = 500;
    constexpr int DATA_ARRIVAL_TIME_MS = 200;
    constexpr int VERY_LONG_TIMEOUT_MS = 3600000; // 1 hour
    constexpr int EXTENDED_TIMEOUT_MS = 300;
    constexpr int ZERO_TIMEOUT_MS = 0;
    constexpr int NEGATIVE_TIMEOUT_MS = -1;
    
    // Server related timeouts
    constexpr int SERVER_DATA_WAIT_TIMEOUT_MS = 100;
    constexpr int SERVER_START_TIMEOUT_SEC = 2;
    constexpr int CLIENT_PROCESSING_TIME_MS = 100;
    constexpr int CONNECTION_TIMEOUT_MS = 1000;
    constexpr int TIMEOUT_MARGIN_MS = 500;
    constexpr int INTER_CLIENT_DELAY_MS = 200;
    constexpr int NON_BLOCKING_TIMEOUT_MS = 100;
    constexpr int NON_BLOCKING_WAIT_TIME_MS = 200;
}

// Common port numbers
namespace ports {
    // TCP port numbers
    constexpr int DEFAULT_TCP_SERVER_PORT = 45000;
    constexpr int MULTI_CONN_SERVER_PORT = 45200;
    constexpr int NON_BLOCKING_SERVER_PORT = 45300;
    
    // UDP port numbers
    constexpr int DEFAULT_UDP_SERVER_PORT = 45100;
}

// Other common constants
namespace constants {
    constexpr int SERVER_BACKLOG_SIZE = 5;
    constexpr int NUM_TEST_CLIENTS = 3;
    constexpr int UDP_BUFFER_SIZE = 2048;
}

// Utility to implement timeout waiting tests
template <typename MockSocketT>
void testBasicWaitForDataWithTimeout(MockSocketT& mockSocket) {
    using namespace timeouts;
    
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

// Utility to implement edge case timeout tests
template <typename MockSocketT>
void testEdgeCasesWaitForDataWithTimeout() {
    using namespace timeouts;
    
    // ---- Zero timeout tests (polling behavior) ----
    {
        MockSocketT mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(ZERO_TIMEOUT_MS))
            .WillOnce(testing::Return(false));
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(ZERO_TIMEOUT_MS));
    }
    
    {
        MockSocketT mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(ZERO_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(ZERO_TIMEOUT_MS));
    }
    
    // ---- Negative timeout test ----
    {
        MockSocketT mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(NEGATIVE_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(NEGATIVE_TIMEOUT_MS));
    }
    
    // ---- Large timeout test ----
    {
        MockSocketT mockSocket;
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(VERY_LONG_TIMEOUT_MS))
            .WillOnce(testing::Return(true));
        EXPECT_TRUE(mockSocket.WaitForDataWithTimeout(VERY_LONG_TIMEOUT_MS));
    }
    
    // ---- Invalid socket test ----
    {
        MockSocketT mockSocket;
        EXPECT_CALL(mockSocket, IsValid())
            .WillOnce(testing::Return(false));
        EXPECT_CALL(mockSocket, WaitForDataWithTimeout(SHORT_TIMEOUT_MS))
            .WillOnce(testing::Return(false));
            
        EXPECT_FALSE(mockSocket.IsValid());
        EXPECT_FALSE(mockSocket.WaitForDataWithTimeout(SHORT_TIMEOUT_MS));
    }
}

// Utility to implement realistic data arrival tests
template <typename MockSocketT>
void testRealDataWaitForDataWithTimeout(std::shared_ptr<MockSocketT> mockSocket) {
    using namespace timeouts;
    
    // Set up the socket to return a realistic WaitForDataWithTimeout implementation
    // that actually waits and returns true only when data becomes available
    EXPECT_CALL(*mockSocket, WaitForDataWithTimeout(testing::_))
        .WillRepeatedly([](int timeoutMs) {
            // Simulate data appearing after delay
            auto start = std::chrono::steady_clock::now();
            auto dataArrivalTime = start + std::chrono::milliseconds(DATA_ARRIVAL_TIME_MS);
            
            // Sleep until either timeout expires or data arrival time
            auto waitUntil = start + std::chrono::milliseconds(timeoutMs);
            auto sleepUntil = (std::min)(waitUntil, dataArrivalTime);
            
            std::this_thread::sleep_until(sleepUntil);
            
            // Return true if we didn't time out before data arrived
            return dataArrivalTime <= waitUntil;
        });
    
    // Test case 1: Wait time less than data arrival time - should fail
    auto startTime = std::chrono::steady_clock::now();
    bool result1 = mockSocket->WaitForDataWithTimeout(SHORT_TIMEOUT_MS);
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_FALSE(result1);
    EXPECT_GE(elapsed1, SHORT_TIMEOUT_MS);  // Should have waited at least the timeout
    EXPECT_LT(elapsed1, DATA_ARRIVAL_TIME_MS);  // But shouldn't wait the full data arrival time
    
    // Test case 2: Wait time greater than data arrival time - should succeed
    startTime = std::chrono::steady_clock::now();
    bool result2 = mockSocket->WaitForDataWithTimeout(EXTENDED_TIMEOUT_MS);
    endTime = std::chrono::steady_clock::now();
    auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    EXPECT_TRUE(result2);
    EXPECT_GE(elapsed2, DATA_ARRIVAL_TIME_MS);  // Should have waited until data arrived
    EXPECT_LT(elapsed2, EXTENDED_TIMEOUT_MS);  // But not the full timeout
}

// Factory singleton for creating network sockets in tests
class NetworkFactorySingleton {
public:
    static INetworkSocketFactory & GetInstance() {
        if (!instance) {
            factory = INetworkSocketFactory::CreatePlatformFactory();
            instance = factory.get();
        }
        return *instance;
    }

private:
    static std::unique_ptr<INetworkSocketFactory> factory;
    static INetworkSocketFactory* instance;
};

// Common base class for test server infrastructure
// T is the socket type that the server will use (ITcpListener or IUdpSocket)
template <typename SocketT>
class TestServerBase {
protected:
    std::unique_ptr<SocketT> socket;
    std::thread serverThread;
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::condition_variable cv;
    NetworkAddress serverAddress;
    std::string receivedMessage;
    bool messageReceived = false;
    std::string errorMessage;
    
public:
    // For backward compatibility with existing tests
    static INetworkSocketFactory* g_factory;
    
    TestServerBase(const std::string& ip, int port) : serverAddress(ip, port) {}
    
    virtual ~TestServerBase() {
        stop();
    }
    
    virtual bool start() = 0;
    
    virtual void stop() {
        running = false;
        
        if (socket) {
            socket->Close();
        }
        
        if (serverThread.joinable()) {
            serverThread.join();
        }
    }
    
    std::string getReceivedMessage() {
        std::unique_lock<std::mutex> lock(mutex);
        return receivedMessage;
    }
    
    bool wasMessageReceived() {
        std::unique_lock<std::mutex> lock(mutex);
        return messageReceived;
    }
    
    NetworkAddress getServerAddress() const {
        return serverAddress;
    }
    
    std::string getErrorMessage() const {
        return errorMessage;
    }
    
protected:
    // These run methods will be implemented by the concrete child classes
    virtual void run() = 0;
};

// Initialize the static member with the singleton instance
template <typename SocketT>
INetworkSocketFactory* TestServerBase<SocketT>::g_factory = &NetworkFactorySingleton::GetInstance(); // Take address of the returned reference

} // namespace test_utils

#endif // TEST_UTILS_H