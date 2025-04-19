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

// Constants for tests
namespace {
    // Port numbers
    constexpr int DEFAULT_UDP_SERVER_PORT = 45100;
    
    // Timeout values (in milliseconds)
    constexpr int UDP_SERVER_DATA_WAIT_TIMEOUT_MS = 100;
    constexpr int UDP_SERVER_START_TIMEOUT_SEC = 2;
    constexpr int UDP_CLIENT_PROCESSING_TIME_MS = 100;
    constexpr int UDP_RESPONSE_TIMEOUT_MS = 500;
    
    // Other constants
    constexpr int UDP_BUFFER_SIZE = 1024;
}

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
    TestUdpServer(int port = DEFAULT_UDP_SERVER_PORT) : serverAddress("127.0.0.1", port) {
        auto& factory = g_factory;
        socket = factory->CreateUdpSocket();
    }

    ~TestUdpServer() {
        stop();
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
        return cv.wait_for(lock, std::chrono::seconds(UDP_SERVER_START_TIMEOUT_SEC), [this] { return running.load(); });
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
            if (socket->WaitForDataWithTimeout(UDP_SERVER_DATA_WAIT_TIMEOUT_MS)) {
                std::vector<std::byte> buffer(UDP_BUFFER_SIZE);
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

// Test fixture for UDP client-server tests
class UdpClientServerConnectionTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {
        if (client) {
            client->Close();
            client.reset();
        }
        
        if (server) {
            server->stop();
            server.reset();
        }
    }

    // Helper method to create and start a server
    bool CreateAndStartServer(int port) {
        server = std::make_unique<TestUdpServer>(port);
        if (!server->start()) {
            return false;
        }
        return true;
    }

    // Helper method to create and bind a client
    bool CreateAndBindClient() {
        client = g_factory->CreateUdpSocket();
        if (!client->IsValid()) {
            return false;
        }
        
        // Bind client to a specific local address (optional)
        // Using port 0 here is fine since we're only sending from this port
        return client->Bind(NetworkAddress("127.0.0.1", 0));
    }

    std::unique_ptr<TestUdpServer> server;
    std::unique_ptr<IUdpSocket> client;
};

// Test UDP client-server basic communication
TEST_F(UdpClientServerConnectionTest, BasicCommunication) {
    // Start UDP server with specific port
    ASSERT_TRUE(CreateAndStartServer(DEFAULT_UDP_SERVER_PORT)) << "Failed to start UDP server: " << server->getErrorMessage();

    // Get server address
    NetworkAddress serverAddr = server->getServerAddress();

    // Create and bind UDP client
    ASSERT_TRUE(CreateAndBindClient());

    // Send data from client to server
    std::string testMessage = "Hello, UDP Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    int bytesSent = client->SendTo(sendData, serverAddr);
    EXPECT_EQ(bytesSent, sendData.size());

    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(UDP_CLIENT_PROCESSING_TIME_MS));

    // Verify server received the message
    EXPECT_TRUE(server->wasMessageReceived()) << "Server didn't receive any message";
    EXPECT_EQ(server->getReceivedMessage(), testMessage);

    // Receive echo response
    std::vector<std::byte> recvBuffer(UDP_BUFFER_SIZE);
    NetworkAddress senderAddr;

    ASSERT_TRUE(client->WaitForDataWithTimeout(UDP_RESPONSE_TIMEOUT_MS)) << "Timed out waiting for UDP response";
    int bytesReceived = client->ReceiveFrom(recvBuffer, senderAddr);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive UDP response";

    recvBuffer.resize(bytesReceived);
    std::string response = NetworkUtils::BytesToString(recvBuffer);
    EXPECT_EQ(response, testMessage);
}
