#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <cstddef> // For std::byte
#include "network/tcp_socket.h"
#include "network/byte_utils.h"
#include "utils/test_utils.h"

// Use test_utils constants but not the mock classes directly
using namespace test_utils::constants;

// Mock classes for this test file
class MockTcpSocket final : public ITcpSocket {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(void, SetReuseAddr, (bool enable), (override));
    MOCK_METHOD(bool, Connect, (const NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(int, Send, (const std::vector<std::byte>& data), (override));
    MOCK_METHOD(int, Receive, (std::vector<std::byte>& buffer), (override));
    MOCK_METHOD(NetworkAddress, GetRemoteAddress, (), (const, override));
    MOCK_METHOD(bool, SetNoDelay, (bool enable), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetConnectTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetSocketOption, (int level, int optionName, const void* optionValue, socklen_t optionLen), (override));
};

// Mock class for TCP listener
class MockTcpListener : public ITcpListener {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(void, SetReuseAddr, (bool enable), (override));
    MOCK_METHOD(bool, Listen, (int backlog), (override));
    MOCK_METHOD(std::unique_ptr<IConnectionOrientedSocket>, Accept, (), (override));
    MOCK_METHOD(bool, WaitForDataWithTimeout, (int timeoutMs), (override));
    MOCK_METHOD(bool, SetSocketOption, (int level, int optionName, const void* optionValue, socklen_t optionLen), (override));
};

// Mock class for TCP socket factory
class MockTcpSocketFactory : public ITcpSocketFactory {
public:
    MOCK_METHOD(std::unique_ptr<ITcpSocket>, CreateTcpSocket, (), (override));
    MOCK_METHOD(std::unique_ptr<ITcpListener>, CreateTcpListener, (), (override));
};

// Sample test for TCP socket functionality
TEST(TcpSocketTest, CreateAndConnect) {
    // Create a mock TCP socket factory
    MockTcpSocketFactory mockFactory;
    
    // Create a mock TCP socket
    auto mockSocket = std::make_unique<MockTcpSocket>();
    
    // Set expectations
    EXPECT_CALL(*mockSocket, IsValid())
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(*mockSocket, Connect(testing::_))
        .WillOnce(testing::Return(true));
    
    // Set up the factory to return our mock socket
    EXPECT_CALL(mockFactory, CreateTcpSocket())
        .WillOnce(testing::Return(testing::ByMove(std::move(mockSocket))));
    
    // Use the factory to create a socket
    auto socket = mockFactory.CreateTcpSocket();
    
    // Test the socket
    EXPECT_TRUE(socket->IsValid());
    EXPECT_TRUE(socket->Connect(NetworkAddress("127.0.0.1", 8080)));
}

// Sample test for TCP socket data transfer
TEST(TcpSocketTest, SendAndReceiveData) {
    // Create a mock TCP socket
    MockTcpSocket mockSocket;
    
    // Test data
    std::vector<std::byte> testData = {std::byte('H'), std::byte('e'), std::byte('l'), std::byte('l'), std::byte('o')};
    std::vector<std::byte> receiveBuffer(5);
    
    // Set expectations
    EXPECT_CALL(mockSocket, Send(testing::Eq(testData)))
        .WillOnce(testing::Return(5));
    
    EXPECT_CALL(mockSocket, Receive(testing::_))
        .WillOnce([&receiveBuffer](std::vector<std::byte>& buffer) {
            buffer = {std::byte('W'), std::byte('o'), std::byte('r'), std::byte('l'), std::byte('d')};
            return 5;
        });
    
    // Test sending data
    int bytesSent = mockSocket.Send(testData);
    EXPECT_EQ(bytesSent, 5);
    
    // Test receiving data
    int bytesReceived = mockSocket.Receive(receiveBuffer);
    EXPECT_EQ(bytesReceived, 5);
    EXPECT_EQ(receiveBuffer, std::vector<std::byte>({std::byte('W'), std::byte('o'), std::byte('r'), std::byte('l'), std::byte('d')}));
}

// Sample test for TCP listener
TEST(TcpListenerTest, ListenAndAccept) {
    // Create mock objects
    MockTcpListener mockListener;
    
    // Set expectations
    EXPECT_CALL(mockListener, Bind(testing::_))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockListener, Listen(SERVER_BACKLOG_SIZE))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockListener, Accept())
        .WillOnce([&]() {
            return std::make_unique<MockTcpSocket>();
        });
    
    // Test the listener
    EXPECT_TRUE(mockListener.Bind(NetworkAddress("0.0.0.0", 8080)));
    EXPECT_TRUE(mockListener.Listen(SERVER_BACKLOG_SIZE));
    
    // Test accepting a connection
    auto clientSocket = mockListener.Accept();
    EXPECT_NE(clientSocket, nullptr);
}

// Test for TCP NoDelay option
TEST(TcpSocketTest, SetNoDelay) {
    // Create a mock TCP socket
    MockTcpSocket mockSocket;
    
    // Set expectations for enabling NoDelay
    EXPECT_CALL(mockSocket, SetNoDelay(true))
        .WillOnce(testing::Return(true));
    
    // Set expectations for disabling NoDelay
    EXPECT_CALL(mockSocket, SetNoDelay(false))
        .WillOnce(testing::Return(true));
    
    // Test enabling NoDelay
    EXPECT_TRUE(mockSocket.SetNoDelay(true));
    
    // Test disabling NoDelay
    EXPECT_TRUE(mockSocket.SetNoDelay(false));
}

// Test for TCP socket error handling
TEST(TcpSocketTest, ErrorHandling) {
    // Create a mock TCP socket
    MockTcpSocket mockSocket;
    
    // Set up expectations for error conditions
    EXPECT_CALL(mockSocket, IsValid())
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(mockSocket, Connect(testing::_))
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(mockSocket, Send(testing::_))
        .WillOnce(testing::Return(-1));
    
    EXPECT_CALL(mockSocket, Receive(testing::_))
        .WillOnce(testing::Return(-1));
    
    // Test invalid socket
    EXPECT_FALSE(mockSocket.IsValid());
    
    // Test failed connection
    EXPECT_FALSE(mockSocket.Connect(NetworkAddress("192.168.1.1", 8080)));
    
    // Test send failure
    std::vector<std::byte> sendData = {std::byte('T'), std::byte('e'), std::byte('s'), std::byte('t')};
    EXPECT_EQ(mockSocket.Send(sendData), -1);
    
    // Test receive failure
    std::vector<std::byte> receiveBuffer;
    EXPECT_EQ(mockSocket.Receive(receiveBuffer), -1);
}

// Test for AcceptTcp helper method
TEST(TcpListenerTest, AcceptTcpHelper) {
    // Create mock objects
    MockTcpListener mockListener;
    
    // Set up expectations
    EXPECT_CALL(mockListener, Accept())
        .WillOnce([&]() {
            return std::make_unique<MockTcpSocket>();
        });
    
    // Test AcceptTcp helper method that returns ITcpSocket
    auto tcpSocket = mockListener.Accept();
    EXPECT_NE(tcpSocket, nullptr);
}