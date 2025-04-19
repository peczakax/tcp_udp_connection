// filepath: c:\Users\admin\Development\tcp_udp_connection\tests\udp_client_server_connection_test.cpp
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "network/network.h"
#include "network/platform_factory.h"
#include "network/udp_socket.h"
#include "network/byte_utils.h"

// Use a static factory to ensure Winsock stays initialized for the test duration
static std::unique_ptr<INetworkSocketFactory> g_factory = INetworkSocketFactory::CreatePlatformFactory();

// Helper class for UDP server testing
class TestUdpServer {
private:
    std::unique_ptr<IUdpSocket> socket;
    std::thread serverThread;
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::condition_variable cv;
    NetworkAddress serverAddress;
    std::string receivedMessage;
    NetworkAddress clientAddress;
    bool messageReceived = false;
    std::string errorMessage;

public:
    // Use a higher port number that's less likely to be in use
    TestUdpServer(int port = 45100) : serverAddress("127.0.0.1", port) {
        auto& factory = g_factory;
        socket = factory->CreateUdpSocket();
    }

    ~TestUdpServer() {
        stop();
    }

    // Static method to allow time between tests for socket cleanup
    static void WaitForSocketCleanup() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    bool start() {
        if (!socket->IsValid()) {
            errorMessage = "Failed to create valid UDP socket";
            return false;
        }

        if (!socket->Bind(serverAddress)) {
            errorMessage = "Failed to bind to address: " + serverAddress.ipAddress + ":" + std::to_string(serverAddress.port);
            return false;
        }

        // If we used port 0 (dynamic), update our server address with the actual port
        if (serverAddress.port == 0) {
            serverAddress = socket->GetLocalAddress();
        }

        running = true;
        serverThread = std::thread(&TestUdpServer::run, this);

        // Wait for server to start
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(2), [this] { return running.load(); });
    }

    void stop() {
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

    NetworkAddress getClientAddress() {
        std::unique_lock<std::mutex> lock(mutex);
        return clientAddress;
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

private:
    void run() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_all(); // Signal that the server has started
        }

        while (running) {
            if (socket->WaitForDataWithTimeout(100)) {
                std::vector<std::byte> buffer(1024);
                NetworkAddress sender;
                int bytesRead = socket->ReceiveFrom(buffer, sender);

                if (bytesRead > 0) {
                    // Store the message and client address
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        buffer.resize(bytesRead);
                        receivedMessage = NetworkUtils::BytesToString(buffer);
                        clientAddress = sender;
                        messageReceived = true;
                    }

                    // Echo back the same message
                    socket->SendTo(buffer, sender);
                }
            }
        }
    }
};

// Test UDP client-server basic communication
TEST(ClientServerConnectionTest, UdpBasicCommunication) {
    // Make sure any previous test's sockets are cleaned up
    TestUdpServer::WaitForSocketCleanup();

    // Start UDP server with specific port
    TestUdpServer server(45100);
    ASSERT_TRUE(server.start()) << "Failed to start UDP server: " << server.getErrorMessage();

    // Get server address
    NetworkAddress serverAddr = server.getServerAddress();

    // Create UDP client
    auto& factory = g_factory;
    auto client = factory->CreateUdpSocket();
    ASSERT_TRUE(client->IsValid());

    // Bind client to a specific local address (optional)
    // Using port 0 here is fine since we're only sending from this port
    EXPECT_TRUE(client->Bind(NetworkAddress("127.0.0.1", 0)));

    // Send data from client to server
    std::string testMessage = "Hello, UDP Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    int bytesSent = client->SendTo(sendData, serverAddr);
    EXPECT_EQ(bytesSent, sendData.size());

    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify server received the message
    EXPECT_TRUE(server.wasMessageReceived()) << "Server didn't receive any message";
    EXPECT_EQ(server.getReceivedMessage(), testMessage);

    // Receive echo response
    std::vector<std::byte> recvBuffer(1024);
    NetworkAddress senderAddr;

    ASSERT_TRUE(client->WaitForDataWithTimeout(500)) << "Timed out waiting for UDP response";
    int bytesReceived = client->ReceiveFrom(recvBuffer, senderAddr);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive UDP response";

    recvBuffer.resize(bytesReceived);
    std::string response = NetworkUtils::BytesToString(recvBuffer);
    EXPECT_EQ(response, testMessage);

    // Clean up
    client->Close();
    server.stop();
}
