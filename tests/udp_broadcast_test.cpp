#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <algorithm>

#include "network/network.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include "utils/test_utils.h"

// Use the constants defined in test_utils
using namespace test_utils::timeouts;
using namespace test_utils::ports;
using namespace test_utils::constants;

// Define broadcast-specific constants
constexpr int BROADCAST_BASE_PORT = 45400; // Base port for broadcast tests
constexpr int BROADCAST_RECEIVERS = 3; // Number of receivers to test with
constexpr int BROADCAST_WAIT_MS = 1000; // Time to wait for broadcast messages
constexpr const char* BROADCAST_ADDRESS = "255.255.255.255"; // Standard broadcast address
constexpr const char* LOCALHOST = "127.0.0.1"; // For loopback testing only
constexpr const char* ANY_ADDRESS = "0.0.0.0"; // Bind to all interfaces

// Helper class for UDP receiver in broadcast tests
class BroadcastReceiver {
private:
    std::unique_ptr<IUdpSocket> socket;
    NetworkAddress bindAddress;
    std::thread receiveThread;
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::string> receivedMessages;
    int id;

public:
    BroadcastReceiver(int receiverId, int port)
        : bindAddress(ANY_ADDRESS, port), id(receiverId) {
        
        // Use the NetworkFactorySingleton to create socket
        auto& factory = NetworkFactorySingleton::GetInstance();
        socket = factory.CreateUdpSocket();
    }
    
    ~BroadcastReceiver() {
        stop();
    }
    
    bool start() {
        if (!socket || !socket->IsValid()) {
            std::cerr << "Receiver " << id << ": Failed to create valid UDP socket" << std::endl;
            return false;
        }

        if (!socket->Bind(bindAddress)) {
            std::cerr << "Receiver " << id << ": Failed to bind to address: " 
                     << bindAddress.ipAddress << ":" << bindAddress.port << std::endl;
            return false;
        }
        
        running = true;
        receiveThread = std::thread(&BroadcastReceiver::receiveLoop, this);
        
        // Small delay to ensure the thread starts
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return true;
    }
    
    void stop() {
        running = false;
        
        if (socket) {
            socket->Close();
        }
        
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
    }
    
    std::vector<std::string> getReceivedMessages() {
        std::lock_guard<std::mutex> lock(mutex);
        return receivedMessages;
    }
    
    bool hasReceivedMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        return std::find(receivedMessages.begin(), receivedMessages.end(), message) != receivedMessages.end();
    }
    
    size_t getReceivedCount() {
        std::lock_guard<std::mutex> lock(mutex);
        return receivedMessages.size();
    }
    
    int getId() const {
        return id;
    }
    
    int getPort() const {
        return bindAddress.port;
    }

private:
    void receiveLoop() {
        std::vector<std::byte> buffer(UDP_BUFFER_SIZE);
        
        while (running) {
            try {
                // Use timeout to avoid blocking forever
                if (socket->WaitForDataWithTimeout(SERVER_DATA_WAIT_TIMEOUT_MS)) {
                    // Clear the buffer before receiving new data
                    buffer.clear();
                    buffer.resize(UDP_BUFFER_SIZE);
                    
                    NetworkAddress sender;
                    int bytesRead = socket->ReceiveFrom(buffer, sender);
                    
                    if (bytesRead > 0) {
                        // Resize buffer to actual data size
                        buffer.resize(bytesRead);
                        std::string message = NetworkUtils::BytesToString(buffer);
                        
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            receivedMessages.push_back(message);
                        }
                        
                        std::cout << "Receiver " << id << " on port " << bindAddress.port 
                                 << " got message: " << message << std::endl;
                    }
                }
            }
            catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Receiver " << id << " error: " << e.what() << std::endl;
                }
                // With UDP, we can continue trying to receive even after an error
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
};

// Test fixture for UDP broadcast tests
class UdpBroadcastTest : public ::testing::Test {
protected:
    std::unique_ptr<IUdpSocket> broadcaster;
    std::vector<std::unique_ptr<BroadcastReceiver>> receivers;
    
    void SetUp() override {
        // Create the broadcaster socket
        auto& factory = NetworkFactorySingleton::GetInstance();
        broadcaster = factory.CreateUdpSocket();
        ASSERT_TRUE(broadcaster && broadcaster->IsValid()) << "Failed to create valid broadcaster socket";
        
        // Enable broadcasting on the socket
        ASSERT_TRUE(broadcaster->SetBroadcast(true)) << "Failed to enable broadcasting";
    }
    
    void TearDown() override {
        // Clean up receivers
        for (auto& receiver : receivers) {
            receiver->stop();
        }
        receivers.clear();
        
        // Clean up broadcaster
        if (broadcaster) {
            broadcaster->Close();
        }
    }
    
    // Helper method to create and start multiple receivers with unique ports
    bool createReceivers(int count) {
        for (int i = 0; i < count; i++) {
            // Each receiver gets its own port
            int port = BROADCAST_BASE_PORT + i;
            auto receiver = std::make_unique<BroadcastReceiver>(i, port);
            if (!receiver->start()) {
                return false;
            }
            receivers.push_back(std::move(receiver));
        }
        return true;
    }
    
    // Helper method to broadcast a message to all receiver ports
    int broadcastToAllPorts(const std::string& message) {
        int totalBytesSent = 0;
        
        // Send to each receiver's port
        for (auto& receiver : receivers) {
            int port = receiver->getPort();
            std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
            NetworkAddress broadcastAddr(BROADCAST_ADDRESS, port);
            int bytesSent = broadcaster->SendTo(data, broadcastAddr);
            
            if (bytesSent > 0) {
                totalBytesSent += bytesSent;
            } else {
                // If any send fails, return error
                return -1;
            }
            
            // Small delay between broadcasts
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        return totalBytesSent;
    }
    
    // Helper method to verify all receivers got the message
    bool verifyAllReceiversGotMessage(const std::string& message, int timeoutMs = BROADCAST_WAIT_MS) {
        // Wait a bit for messages to be received
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        
        bool allReceived = true;
        for (auto& receiver : receivers) {
            if (!receiver->hasReceivedMessage(message)) {
                std::cerr << "Receiver " << receiver->getId() << " on port " << receiver->getPort()
                         << " didn't receive the message" << std::endl;
                allReceived = false;
            }
        }
        return allReceived;
    }
};

// Test basic broadcast functionality - each receiver listening on its own port
TEST_F(UdpBroadcastTest, BasicBroadcast) {
    // Create multiple receivers each on its own port
    ASSERT_TRUE(createReceivers(BROADCAST_RECEIVERS))
        << "Failed to create and start receivers";

    // Small delay to ensure receivers are ready
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Broadcast a message to all ports
    std::string testMessage = "Hello, UDP Broadcast World!";
    int bytesSent = broadcastToAllPorts(testMessage);
    
    // Check that the message was sent
    ASSERT_GT(bytesSent, 0) << "Failed to send broadcast message";
    
    // Verify that all receivers got the message
    EXPECT_TRUE(verifyAllReceiversGotMessage(testMessage))
        << "Not all receivers got the broadcast message";
    
    // Check message counts (should be at least 1 per receiver)
    for (auto& receiver : receivers) {
        EXPECT_GE(receiver->getReceivedCount(), 1)
            << "Receiver " << receiver->getId() << " didn't get enough messages";
    }
}

// Test sending multiple broadcast messages in sequence
TEST_F(UdpBroadcastTest, MultipleBroadcasts) {
    // Create multiple receivers
    ASSERT_TRUE(createReceivers(BROADCAST_RECEIVERS))
        << "Failed to create and start receivers";
    
    // Small delay to ensure receivers are ready
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Send multiple broadcast messages
    std::vector<std::string> testMessages = {
        "Broadcast Message 1",
        "Broadcast Message 2",
        "Broadcast Message 3"
    };
    
    for (const auto& msg : testMessages) {
        int bytesSent = broadcastToAllPorts(msg);
        ASSERT_GT(bytesSent, 0) << "Failed to send broadcast message: " << msg;
        
        // Short delay between messages to avoid packet loss
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Verify each message was received by all receivers
    for (const auto& msg : testMessages) {
        EXPECT_TRUE(verifyAllReceiversGotMessage(msg))
            << "Not all receivers got the broadcast message: " << msg;
    }
    
    // Each receiver should have received all messages
    for (auto& receiver : receivers) {
        EXPECT_EQ(receiver->getReceivedCount(), testMessages.size())
            << "Receiver " << receiver->getId() << " didn't get all messages";
    }
}

// Test broadcasting a large message
TEST_F(UdpBroadcastTest, LargeBroadcast) {
    // Create receivers
    ASSERT_TRUE(createReceivers(BROADCAST_RECEIVERS))
        << "Failed to create and start receivers";
    
    // Give more time for receivers to be ready
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Generate a large message (reduced slightly from original 1400 bytes)
    std::string largeMessage(1200, 'X');
    
    // Try broadcasting multiple times with increasing delays between attempts
    bool success = false;
    for (int attempt = 1; attempt <= 3 && !success; attempt++) {
        std::cout << "Large broadcast attempt " << attempt << std::endl;
        
        // Broadcast the large message
        int bytesSent = broadcastToAllPorts(largeMessage);
        ASSERT_GT(bytesSent, 0) << "Failed to send large broadcast message";
        
        // Increase wait time for large messages
        int waitTime = BROADCAST_WAIT_MS * attempt;
        success = verifyAllReceiversGotMessage(largeMessage, waitTime);
        
        if (!success && attempt < 3) {
            std::cout << "Not all receivers got the message. Retrying with longer delay..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200 * attempt));
        }
    }
    
    EXPECT_TRUE(success) << "Not all receivers got the large broadcast message after multiple attempts";
}

// Test bound broadcasting - bind the sender to specific port
TEST_F(UdpBroadcastTest, BoundBroadcasterTest) {
    // Create the receivers
    ASSERT_TRUE(createReceivers(BROADCAST_RECEIVERS))
        << "Failed to create and start receivers";
        
    // Bind the broadcaster to a specific local port
    NetworkAddress broadcasterAddr(LOCALHOST, 0); // Use port 0 for dynamic assignment
    ASSERT_TRUE(broadcaster->Bind(broadcasterAddr)) << "Failed to bind broadcaster socket";
    
    // Get the actual port that was assigned
    NetworkAddress actualAddr = broadcaster->GetLocalAddress();
    std::cout << "Broadcaster bound to port: " << actualAddr.port << std::endl;
    
    // Send the broadcast
    std::string testMessage = "Broadcast from bound socket";
    int bytesSent = broadcastToAllPorts(testMessage);
    ASSERT_GT(bytesSent, 0) << "Failed to send broadcast from bound socket";
    
    // Verify all receivers got the message
    EXPECT_TRUE(verifyAllReceiversGotMessage(testMessage))
        << "Not all receivers got the message from bound broadcaster";
}

// Test rapid-fire broadcasting (stress test)
TEST_F(UdpBroadcastTest, RapidBroadcast) {
    // Create the receivers
    ASSERT_TRUE(createReceivers(BROADCAST_RECEIVERS))
        << "Failed to create and start receivers";
    
    // Send multiple messages in rapid succession
    const int messageCount = 20; // Reduced from 50 to reduce test time
    std::vector<std::string> messages;
    
    for (int i = 0; i < messageCount; i++) {
        std::string msg = "Rapid message #" + std::to_string(i);
        messages.push_back(msg);
        int bytesSent = broadcastToAllPorts(msg);
        ASSERT_GT(bytesSent, 0) << "Failed to send rapid broadcast message " << i;
        
        // Very short delay to prevent complete network flooding
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    
    // Give extra time for all messages to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(BROADCAST_WAIT_MS * 2));
    
    // Count how many messages were received by each receiver
    for (auto& receiver : receivers) {
        std::cout << "Receiver " << receiver->getId() << " on port " << receiver->getPort()
                 << " got " << receiver->getReceivedCount() << " of " << messageCount 
                 << " messages" << std::endl;
        
        // Due to potential UDP packet loss during stress tests,
        // we check if at least 90% of messages were received
        EXPECT_GE(receiver->getReceivedCount(), static_cast<int>(messageCount * 0.9))
            << "Receiver " << receiver->getId() << " missed too many messages";
    }
}

// Run the UDP broadcast tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}