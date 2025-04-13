#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network_lib/tcp_socket.h"

// Mock class for TCP socket
class MockTcpSocket : public ITcpSocket {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, Connect, (const NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(int, Send, (const std::vector<char>& data), (override));
    MOCK_METHOD(int, Receive, (std::vector<char>& buffer, int maxSize), (override));
    MOCK_METHOD(NetworkAddress, GetRemoteAddress, (), (const, override));
    MOCK_METHOD(bool, SetNoDelay, (bool enable), (override));
};

// Mock class for TCP listener
class MockTcpListener : public ITcpListener {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(bool, Listen, (int backlog), (override));
    MOCK_METHOD(std::unique_ptr<IConnectionOrientedSocket>, Accept, (), (override));
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
    std::vector<char> testData = {'H', 'e', 'l', 'l', 'o'};
    std::vector<char> receiveBuffer(5);
    
    // Set expectations
    EXPECT_CALL(mockSocket, Send(testing::Eq(testData)))
        .WillOnce(testing::Return(5));
    
    EXPECT_CALL(mockSocket, Receive(testing::_, testing::_))
        .WillOnce([&receiveBuffer](std::vector<char>& buffer, int maxSize) {
            buffer = {'W', 'o', 'r', 'l', 'd'};
            return 5;
        });
    
    // Test sending data
    int bytesSent = mockSocket.Send(testData);
    EXPECT_EQ(bytesSent, 5);
    
    // Test receiving data - make sure to pass the maxSize parameter
    int bytesReceived = mockSocket.Receive(receiveBuffer, -1);
    EXPECT_EQ(bytesReceived, 5);
    EXPECT_EQ(receiveBuffer, std::vector<char>({'W', 'o', 'r', 'l', 'd'}));
}

// Sample test for TCP listener
TEST(TcpListenerTest, ListenAndAccept) {
    // Create mock objects
    MockTcpListener mockListener;
    
    // Set expectations
    EXPECT_CALL(mockListener, Bind(testing::_))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockListener, Listen(5))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockListener, Accept())
        .WillOnce(testing::Return(std::make_unique<MockTcpSocket>()));
    
    // Test the listener
    EXPECT_TRUE(mockListener.Bind(NetworkAddress("0.0.0.0", 8080)));
    EXPECT_TRUE(mockListener.Listen(5));
    
    // Test accepting a connection
    auto clientSocket = mockListener.Accept();
    EXPECT_NE(clientSocket, nullptr);
}