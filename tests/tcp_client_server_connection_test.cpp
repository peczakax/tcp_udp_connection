// filepath: c:\Users\admin\Development\tcp_udp_connection\tests\tcp_client_server_connection_test.cpp
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "network/network.h"
#include "network/platform_factory.h"
#include "network/tcp_socket.h"
#include "network/byte_utils.h"

// Constants for tests
namespace {
    // Port numbers
    constexpr int DEFAULT_SERVER_PORT = 45000;
    constexpr int MULTI_CONN_SERVER_PORT = 45200;
    constexpr int NON_BLOCKING_SERVER_PORT = 45300;
    
    // Timeout values (in milliseconds)
    constexpr int SERVER_DATA_WAIT_TIMEOUT_MS = 100;
    constexpr int SERVER_START_TIMEOUT_SEC = 2;
    constexpr int CLIENT_PROCESSING_TIME_MS = 100;
    constexpr int CONNECTION_TIMEOUT_MS = 1000;
    constexpr int TIMEOUT_MARGIN_MS = 500;
    constexpr int INTER_CLIENT_DELAY_MS = 200;
    constexpr int NON_BLOCKING_TIMEOUT_MS = 100;
    constexpr int NON_BLOCKING_WAIT_TIME_MS = 200;
    
    // Other constants
    constexpr int SERVER_BACKLOG_SIZE = 5;
    constexpr int NUM_TEST_CLIENTS = 3;
}

// Use a static factory to ensure Winsock stays initialized for the test duration
static std::unique_ptr<INetworkSocketFactory> g_factory = INetworkSocketFactory::CreatePlatformFactory();

// Helper class to run a TCP server in a separate thread
class TestTcpServer {
private:
    std::unique_ptr<ITcpListener> listener;
    std::thread serverThread;
    std::atomic<bool> running{false};
    std::mutex mutex;
    std::condition_variable cv;
    NetworkAddress serverAddress;
    std::string receivedMessage;
    bool messageReceived = false;
    std::string errorMessage;

public:
    // Use a higher port number that's less likely to be in use
    TestTcpServer(int port = DEFAULT_SERVER_PORT) : serverAddress("127.0.0.1", port) {
        listener = g_factory->CreateTcpListener();
    }

    ~TestTcpServer() {
        stop();
    }

    bool start() {
        if (!listener->IsValid()) {
            errorMessage = "Failed to create valid listener";
            return false;
        }

        if (!listener->Bind(serverAddress)) {
            errorMessage = "Failed to bind to address: " + serverAddress.ipAddress + ":" + std::to_string(serverAddress.port);
            return false;
        }

        if (!listener->Listen(SERVER_BACKLOG_SIZE)) {
            errorMessage = "Failed to listen on address: " + serverAddress.ipAddress + ":" + std::to_string(serverAddress.port);
            return false;
        }

        running = true;
        serverThread = std::thread(&TestTcpServer::run, this);

        // Wait for server to start
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(SERVER_START_TIMEOUT_SEC), [this] { return running.load(); });
    }

    void stop() {
        running = false;

        if (listener) {
            listener->Close();
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

private:
    void run() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.notify_all(); // Signal that the server has started
        }

        while (running) {
            // Wait for connection with timeout to allow checking running flag
            if (listener->WaitForDataWithTimeout(SERVER_DATA_WAIT_TIMEOUT_MS)) {
                auto clientSocket = listener->Accept();
                if (clientSocket && clientSocket->IsValid()) {
                    handleClient(std::move(clientSocket));
                }
            }
        }
    }

    void handleClient(std::unique_ptr<IConnectionOrientedSocket> clientSocket) {
        // Simple echo server implementation
        std::vector<std::byte> buffer;
        int bytesRead = clientSocket->Receive(buffer);

        if (bytesRead > 0) {
            // Store the message
            {
                std::unique_lock<std::mutex> lock(mutex);
                receivedMessage = NetworkUtils::BytesToString(buffer);
                messageReceived = true;
            }

            // Echo back the same message
            clientSocket->Send(buffer);
        }

        clientSocket->Close();
    }
};

// Test fixture for TCP client-server tests
class TcpClientServerConnectionTest : public ::testing::Test {
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
        server = std::make_unique<TestTcpServer>(port);
        if (!server->start()) {
            return false;
        }
        return true;
    }

    // Helper method to create and connect a client
    bool CreateAndConnectClient(const NetworkAddress& serverAddr) {
        client = g_factory->CreateTcpSocket();
        if (!client->Connect(serverAddr)) {
            return false;
        }
        return true;
    }

    std::unique_ptr<TestTcpServer> server;
    std::unique_ptr<ITcpSocket> client;
};

// Test TCP client-server basic connection
TEST_F(TcpClientServerConnectionTest, BasicConnection) {
    // Start TCP server with specific port
    ASSERT_TRUE(CreateAndStartServer(DEFAULT_SERVER_PORT)) << "Failed to start TCP server: " << server->getErrorMessage();

    // Get server address and connect client
    NetworkAddress serverAddr = server->getServerAddress();
    ASSERT_TRUE(CreateAndConnectClient(serverAddr)) << "Failed to connect to server at " 
        << serverAddr.ipAddress << ":" << serverAddr.port;

    // Send data from client to server
    std::string testMessage = "Hello, TCP Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    int bytesSent = client->Send(sendData);
    EXPECT_EQ(bytesSent, sendData.size());

    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(CLIENT_PROCESSING_TIME_MS));

    // Verify server received the message
    EXPECT_TRUE(server->wasMessageReceived()) << "Server didn't receive any message";
    EXPECT_EQ(server->getReceivedMessage(), testMessage);

    // Receive echo response
    std::vector<std::byte> recvBuffer;
    int bytesReceived = client->Receive(recvBuffer);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive echo from server";

    if (bytesReceived > 0) {
        std::string response = NetworkUtils::BytesToString(recvBuffer);
        EXPECT_EQ(response, testMessage);
    }
}

// Test multiple connections to TCP server
TEST_F(TcpClientServerConnectionTest, MultipleConnections) {
    // Start TCP server with specific port
    ASSERT_TRUE(CreateAndStartServer(MULTI_CONN_SERVER_PORT)) << "Failed to start TCP server: " << server->getErrorMessage();

    // Get server address
    NetworkAddress serverAddr = server->getServerAddress();

    // Function to run a client connection test
    auto runClient = [this, &serverAddr](int clientId) -> bool {
        auto tempClient = g_factory->CreateTcpSocket();
        if (!tempClient->Connect(serverAddr)) {
            return false;
        }

        std::string testMessage = "Hello from client " + std::to_string(clientId);
        std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
        int bytesSent = tempClient->Send(sendData);

        std::this_thread::sleep_for(std::chrono::milliseconds(CLIENT_PROCESSING_TIME_MS));

        std::vector<std::byte> recvBuffer;
        int bytesReceived = tempClient->Receive(recvBuffer);

        tempClient->Close();
        return (bytesReceived > 0);
    };

    // Run multiple clients in sequence
    for (int i = 0; i < NUM_TEST_CLIENTS; i++) {
        EXPECT_TRUE(runClient(i)) << "Client " << i << " failed to connect or receive data";
        // Give server time to reset between connections
        std::this_thread::sleep_for(std::chrono::milliseconds(INTER_CLIENT_DELAY_MS));
    }
}

// Test connection timeouts - doesn't need a server
TEST_F(TcpClientServerConnectionTest, ConnectionTimeouts) {
    // Try to connect to a server that doesn't exist
    client = g_factory->CreateTcpSocket();

    // Set a short connection timeout
    ASSERT_TRUE(client->SetConnectTimeout(CONNECTION_TIMEOUT_MS));

    auto startTime = std::chrono::steady_clock::now();
    bool connectResult = client->Connect(NetworkAddress("192.168.123.254", 8099)); // Use an unlikely-to-exist address
    auto endTime = std::chrono::steady_clock::now();

    // Connection should fail
    EXPECT_FALSE(connectResult);

    // Check if the timeout occurred within a reasonable margin of the set timeout
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    EXPECT_GE(duration.count(), CONNECTION_TIMEOUT_MS - TIMEOUT_MARGIN_MS);
    EXPECT_LT(duration.count(), CONNECTION_TIMEOUT_MS + TIMEOUT_MARGIN_MS);
}

// Test non-blocking behavior with WaitForDataWithTimeout
TEST_F(TcpClientServerConnectionTest, NonBlockingOperation) {
    // Start TCP server with specific port
    ASSERT_TRUE(CreateAndStartServer(NON_BLOCKING_SERVER_PORT)) << "Failed to start TCP server: " << server->getErrorMessage();

    // Get server address and connect client
    NetworkAddress serverAddr = server->getServerAddress();
    ASSERT_TRUE(CreateAndConnectClient(serverAddr)) << "Failed to connect to server at " 
        << serverAddr.ipAddress << ":" << serverAddr.port;

    // There should be no data available to read initially
    EXPECT_FALSE(client->WaitForDataWithTimeout(NON_BLOCKING_TIMEOUT_MS));

    // Send data from client to server
    std::string testMessage = "Hello, Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    client->Send(sendData);

    // Wait for server to process and respond
    std::this_thread::sleep_for(std::chrono::milliseconds(NON_BLOCKING_WAIT_TIME_MS));

    // Now there should be data available
    EXPECT_TRUE(client->WaitForDataWithTimeout(NON_BLOCKING_TIMEOUT_MS)) << "No data available after waiting";

    // Receive the response
    std::vector<std::byte> recvBuffer;
    int bytesReceived = client->Receive(recvBuffer);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive data";
}
