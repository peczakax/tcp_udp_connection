#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network_lib/udp_socket.h"

// Mock class for UDP socket
class MockUdpSocket : public IUdpSocket {
public:
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(bool, Bind, (const NetworkAddress& localAddress), (override));
    MOCK_METHOD(NetworkAddress, GetLocalAddress, (), (const, override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(int, SendTo, (const std::vector<char>& data, const NetworkAddress& remoteAddress), (override));
    MOCK_METHOD(int, ReceiveFrom, (std::vector<char>& buffer, NetworkAddress& remoteAddress, int maxSize), (override));
    MOCK_METHOD(bool, SetBroadcast, (bool enable), (override));
    MOCK_METHOD(bool, JoinMulticastGroup, (const NetworkAddress& groupAddress), (override));
    MOCK_METHOD(bool, LeaveMulticastGroup, (const NetworkAddress& groupAddress), (override));
};

// Mock class for UDP socket factory
class MockUdpSocketFactory : public IUdpSocketFactory {
public:
    MOCK_METHOD(std::unique_ptr<IUdpSocket>, CreateUdpSocket, (), (override));
};

// Sample test for UDP socket creation
TEST(UdpSocketTest, CreateSocket) {
    // Create a mock UDP socket factory
    MockUdpSocketFactory mockFactory;
    
    // Create a mock UDP socket
    auto mockSocket = std::make_unique<MockUdpSocket>();
    
    // Set expectations
    EXPECT_CALL(*mockSocket, IsValid())
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(*mockSocket, Bind(testing::_))
        .WillOnce(testing::Return(true));
    
    // Set up the factory to return our mock socket
    EXPECT_CALL(mockFactory, CreateUdpSocket())
        .WillOnce(testing::Return(testing::ByMove(std::move(mockSocket))));
    
    // Use the factory to create a socket
    auto socket = mockFactory.CreateUdpSocket();
    
    // Test the socket
    EXPECT_TRUE(socket->IsValid());
    EXPECT_TRUE(socket->Bind(NetworkAddress("0.0.0.0", 8080)));
}

// Sample test for UDP socket data transfer
TEST(UdpSocketTest, SendToAndReceiveFrom) {
    // Create a mock UDP socket
    MockUdpSocket mockSocket;
    
    // Test data and addresses
    std::vector<char> testData = {'H', 'e', 'l', 'l', 'o'};
    std::vector<char> receiveBuffer(5);
    NetworkAddress sendToAddr("192.168.1.100", 8080);
    NetworkAddress receiveFromAddr;
    
    // Set expectations
    EXPECT_CALL(mockSocket, SendTo(testing::Eq(testData), testing::_))
        .WillOnce(testing::Return(5));
    
    EXPECT_CALL(mockSocket, ReceiveFrom(testing::_, testing::_, testing::_))
        .WillOnce([&receiveBuffer](std::vector<char>& buffer, NetworkAddress& remoteAddr, int maxSize) {
            buffer = {'W', 'o', 'r', 'l', 'd'};
            remoteAddr = NetworkAddress("192.168.1.200", 9090);
            return 5;
        });
    
    // Test sending data
    int bytesSent = mockSocket.SendTo(testData, sendToAddr);
    EXPECT_EQ(bytesSent, 5);
    
    // Test receiving data - passing the maxSize parameter
    int bytesReceived = mockSocket.ReceiveFrom(receiveBuffer, receiveFromAddr, -1);
    EXPECT_EQ(bytesReceived, 5);
    EXPECT_EQ(receiveBuffer, std::vector<char>({'W', 'o', 'r', 'l', 'd'}));
    EXPECT_EQ(receiveFromAddr.ipAddress, "192.168.1.200");
    EXPECT_EQ(receiveFromAddr.port, 9090);
}

// Sample test for UDP multicast functionality
TEST(UdpSocketTest, MulticastGroupOperations) {
    // Create a mock UDP socket
    MockUdpSocket mockSocket;
    
    // Test multicast address
    NetworkAddress multicastAddr("224.0.0.1", 5000);
    
    // Set expectations
    EXPECT_CALL(mockSocket, JoinMulticastGroup(testing::_))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockSocket, LeaveMulticastGroup(testing::_))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(mockSocket, SetBroadcast(true))
        .WillOnce(testing::Return(true));
    
    // Test joining a multicast group
    EXPECT_TRUE(mockSocket.JoinMulticastGroup(multicastAddr));
    
    // Test leaving a multicast group
    EXPECT_TRUE(mockSocket.LeaveMulticastGroup(multicastAddr));
    
    // Test setting broadcast mode
    EXPECT_TRUE(mockSocket.SetBroadcast(true));
}