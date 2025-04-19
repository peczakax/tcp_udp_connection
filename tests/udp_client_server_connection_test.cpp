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
#include "utils/test_utils.h"

using namespace test_utils;
using namespace test_utils::timeouts;
using namespace test_utils::ports;
using namespace test_utils::constants;

// Helper class for UDP server testing
class TestUdpServer : public test_utils::TestServerBase<IUdpSocket> {
private:
    NetworkAddress clientAddress;

public:
    TestUdpServer(int port = DEFAULT_UDP_SERVER_PORT) : TestServerBase<IUdpSocket>("127.0.0.1", port) {
        socket = g_factory->CreateUdpSocket();
    }

    bool start() override {
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
        return cv.wait_for(lock, std::chrono::seconds(SERVER_START_TIMEOUT_SEC), [this] { return running.load(); });
    }

    NetworkAddress getClientAddress() {
        std::unique_lock<std::mutex> lock(mutex);
        return clientAddress;
    }

private:
    void run() override {
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_all(); // Signal that the server has started
        }

        while (running) {
            if (socket->WaitForDataWithTimeout(SERVER_DATA_WAIT_TIMEOUT_MS)) {
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
        client = TestServerBase<IUdpSocket>::g_factory->CreateUdpSocket();
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
    std::this_thread::sleep_for(std::chrono::milliseconds(CLIENT_PROCESSING_TIME_MS));

    // Verify server received the message
    EXPECT_TRUE(server->wasMessageReceived()) << "Server didn't receive any message";
    EXPECT_EQ(server->getReceivedMessage(), testMessage);

    // Receive echo response
    std::vector<std::byte> recvBuffer(UDP_BUFFER_SIZE);
    NetworkAddress senderAddr;

    ASSERT_TRUE(client->WaitForDataWithTimeout(LONG_TIMEOUT_MS)) << "Timed out waiting for UDP response";
    int bytesReceived = client->ReceiveFrom(recvBuffer, senderAddr);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive UDP response";

    recvBuffer.resize(bytesReceived);
    std::string response = NetworkUtils::BytesToString(recvBuffer);
    EXPECT_EQ(response, testMessage);
}
