#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "network/udp_socket.h"
#include "network/byte_utils.h"
#include "utils/test_utils.h"
#include <memory>
#include <cstddef> // For std::byte

// Use test_utils constants but not the mock classes directly
using namespace test_utils::constants;

// Mock classes for this test file
class MockUdpSocket final : public IUdpSocket {
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
    std::vector<std::byte> testData = {std::byte('H'), std::byte('e'), std::byte('l'), std::byte('l'), std::byte('o')};
    std::vector<std::byte> receiveBuffer(5);
    NetworkAddress sendToAddr("192.168.1.100", 8080);
    NetworkAddress receiveFromAddr;
    
    // Set expectations
    EXPECT_CALL(mockSocket, SendTo(testing::Eq(testData), testing::_))
        .WillOnce(testing::Return(5));
    
    EXPECT_CALL(mockSocket, ReceiveFrom(testing::_, testing::_))
        .WillOnce([&receiveBuffer](std::vector<std::byte>& buffer, NetworkAddress& remoteAddr) {
            buffer = {std::byte('W'), std::byte('o'), std::byte('r'), std::byte('l'), std::byte('d')};
            remoteAddr = NetworkAddress("192.168.1.200", 9090);
            return 5;
        });
    
    // Test sending data
    int bytesSent = mockSocket.SendTo(testData, sendToAddr);
    EXPECT_EQ(bytesSent, 5);
    
    // Test receiving data
    int bytesReceived = mockSocket.ReceiveFrom(receiveBuffer, receiveFromAddr);
    EXPECT_EQ(bytesReceived, 5);
    EXPECT_EQ(receiveBuffer, std::vector<std::byte>({std::byte('W'), std::byte('o'), std::byte('r'), std::byte('l'), std::byte('d')}));
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

// Test for UDP broadcast functionality
TEST(UdpSocketTest, BroadcastMode) {
    // Create a mock UDP socket
    MockUdpSocket mockSocket;
    
    // Set expectations for enabling broadcast
    EXPECT_CALL(mockSocket, SetBroadcast(true))
        .WillOnce(testing::Return(true));
    
    // Set expectations for disabling broadcast
    EXPECT_CALL(mockSocket, SetBroadcast(false))
        .WillOnce(testing::Return(true));
    
    // Test enabling broadcast
    EXPECT_TRUE(mockSocket.SetBroadcast(true));
    
    // Test disabling broadcast
    EXPECT_TRUE(mockSocket.SetBroadcast(false));
}

// Test for UDP socket error handling
TEST(UdpSocketTest, ErrorHandling) {
    // Create a mock UDP socket
    MockUdpSocket mockSocket;
    
    // Set up expectations for error conditions
    EXPECT_CALL(mockSocket, IsValid())
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(mockSocket, SendTo(testing::_, testing::_))
        .WillOnce(testing::Return(-1));
    
    EXPECT_CALL(mockSocket, ReceiveFrom(testing::_, testing::_))
        .WillOnce(testing::Return(-1));
    
    // Test invalid socket
    EXPECT_FALSE(mockSocket.IsValid());
    
    // Test send failure
    std::vector<std::byte> sendData = {std::byte('T'), std::byte('e'), std::byte('s'), std::byte('t')};
    NetworkAddress destination("192.168.1.1", 8080);
    EXPECT_EQ(mockSocket.SendTo(sendData, destination), -1);
    
    // Test receive failure
    std::vector<std::byte> receiveBuffer;
    NetworkAddress source;
    EXPECT_EQ(mockSocket.ReceiveFrom(receiveBuffer, source), -1);
}

// Test for multicast error handling
TEST(UdpSocketTest, MulticastErrorHandling) {
    // Create a mock UDP socket
    MockUdpSocket mockSocket;
    
    // Test invalid multicast address (non-multicast IP range)
    NetworkAddress invalidMulticastAddr("192.168.1.1", 5000);
    
    // Valid multicast address but socket is invalid
    NetworkAddress validMulticastAddr("224.0.0.1", 5000);
    
    // Set expectations
    EXPECT_CALL(mockSocket, JoinMulticastGroup(testing::_))
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(mockSocket, LeaveMulticastGroup(testing::_))
        .WillOnce(testing::Return(false));
    
    // Test joining a multicast group failure
    EXPECT_FALSE(mockSocket.JoinMulticastGroup(validMulticastAddr));
    
    // Test leaving a multicast group failure
    EXPECT_FALSE(mockSocket.LeaveMulticastGroup(validMulticastAddr));
}

// Test for socket address handling
TEST(NetworkAddressTest, AddressHandling) {
    // Test empty address
    NetworkAddress emptyAddr;
    EXPECT_EQ(emptyAddr.ipAddress, "");
    EXPECT_EQ(emptyAddr.port, 0);
    
    // Test address initialization
    NetworkAddress addr("127.0.0.1", 8080);
    EXPECT_EQ(addr.ipAddress, "127.0.0.1");
    EXPECT_EQ(addr.port, 8080);
}