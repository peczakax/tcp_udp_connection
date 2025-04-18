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
#include "network/udp_socket.h"
#include "network/byte_utils.h"
#include "winsock_initializer.h" // Add this include to ensure Winsock is initialized

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
    TestTcpServer(int port = 45000) : serverAddress("127.0.0.1", port) {
        auto factory = INetworkSocketFactory::CreatePlatformFactory();
        listener = factory->CreateTcpListener();
    }
    
    ~TestTcpServer() {
        stop();
    }
    
    // Static method to allow time between tests for socket cleanup
    static void WaitForSocketCleanup() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
        
        if (!listener->Listen(5)) {
            errorMessage = "Failed to listen on address: " + serverAddress.ipAddress + ":" + std::to_string(serverAddress.port);
            return false;
        }
        
        running = true;
        serverThread = std::thread(&TestTcpServer::run, this);
        
        // Wait for server to start
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, std::chrono::seconds(2), [this] { return running.load(); });
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
            if (listener->WaitForDataWithTimeout(100)) {
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
        auto factory = INetworkSocketFactory::CreatePlatformFactory();
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

// Test TCP client-server basic connection
TEST(ClientServerConnectionTest, TcpBasicConnection) {
    // Make sure any previous test's sockets are cleaned up
    TestTcpServer::WaitForSocketCleanup();
    
    // Start TCP server with specific port
    TestTcpServer server(45000);
    ASSERT_TRUE(server.start()) << "Failed to start TCP server: " << server.getErrorMessage();
    
    // Get server address
    NetworkAddress serverAddr = server.getServerAddress();
    
    // Create and connect TCP client
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto client = factory->CreateTcpSocket();
    
    ASSERT_TRUE(client->Connect(serverAddr)) << "Failed to connect to server at " << 
        serverAddr.ipAddress << ":" << serverAddr.port;
    
    // Send data from client to server
    std::string testMessage = "Hello, TCP Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    int bytesSent = client->Send(sendData);
    EXPECT_EQ(bytesSent, sendData.size());
    
    // Give server time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Verify server received the message
    EXPECT_TRUE(server.wasMessageReceived()) << "Server didn't receive any message";
    EXPECT_EQ(server.getReceivedMessage(), testMessage);
    
    // Receive echo response
    std::vector<std::byte> recvBuffer;
    int bytesReceived = client->Receive(recvBuffer);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive echo from server";
    
    if (bytesReceived > 0) {
        std::string response = NetworkUtils::BytesToString(recvBuffer);
        EXPECT_EQ(response, testMessage);
    }
    
    // Clean up
    client->Close();
    server.stop();
}

// Test UDP client-server basic communication
TEST(ClientServerConnectionTest, UdpBasicCommunication) {
    // Make sure any previous test's sockets are cleaned up
    TestTcpServer::WaitForSocketCleanup();
    
    // Start UDP server with specific port
    TestUdpServer server(45100);
    ASSERT_TRUE(server.start()) << "Failed to start UDP server: " << server.getErrorMessage();
    
    // Get server address
    NetworkAddress serverAddr = server.getServerAddress();
    
    // Create UDP client
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
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

// Test multiple connections to TCP server
TEST(ClientServerConnectionTest, TcpMultipleConnections) {
    // Make sure any previous test's sockets are cleaned up
    TestTcpServer::WaitForSocketCleanup();
    
    // Start TCP server with specific port
    TestTcpServer server(45200);
    ASSERT_TRUE(server.start()) << "Failed to start TCP server: " << server.getErrorMessage();
    
    // Get server address
    NetworkAddress serverAddr = server.getServerAddress();
    
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    const int numClients = 3;
    
    // Function to run a client connection test
    auto runClient = [&](int clientId) -> bool {
        auto client = factory->CreateTcpSocket();
        if (!client->Connect(serverAddr)) {
            return false;
        }
        
        std::string testMessage = "Hello from client " + std::to_string(clientId);
        std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
        int bytesSent = client->Send(sendData);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::vector<std::byte> recvBuffer;
        int bytesReceived = client->Receive(recvBuffer);
        
        client->Close();
        return (bytesReceived > 0);
    };
    
    // Run multiple clients in sequence
    for (int i = 0; i < numClients; i++) {
        EXPECT_TRUE(runClient(i)) << "Client " << i << " failed to connect or receive data";
        // Give server time to reset between connections
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    server.stop();
}

// Test connection timeouts
TEST(ClientServerConnectionTest, ConnectionTimeouts) {
    // Try to connect to a server that doesn't exist
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto client = factory->CreateTcpSocket();
    
    // Set a shorter timeout for the test (if supported by your platform)
    // This might not work directly with your interface, but including as a note
    
    auto startTime = std::chrono::steady_clock::now();
    bool connectResult = client->Connect(NetworkAddress("192.168.123.254", 8099)); // Use an unlikely-to-exist address
    auto endTime = std::chrono::steady_clock::now();
    
    // Connection should fail
    EXPECT_FALSE(connectResult);
    
    // Connection should have timed out in reasonable time (checking for excessive delays)
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    EXPECT_LT(duration.count(), 15); // Should time out in less than 15 seconds
    
    client->Close();
}

// Test non-blocking behavior with WaitForDataWithTimeout
TEST(ClientServerConnectionTest, NonBlockingOperation) {
    // Make sure any previous test's sockets are cleaned up
    TestTcpServer::WaitForSocketCleanup();
    
    // Start TCP server with specific port
    TestTcpServer server(45300);
    ASSERT_TRUE(server.start()) << "Failed to start TCP server: " << server.getErrorMessage();
    
    // Get server address
    NetworkAddress serverAddr = server.getServerAddress();
    
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto client = factory->CreateTcpSocket();
    ASSERT_TRUE(client->Connect(serverAddr)) << "Failed to connect to server at " << 
        serverAddr.ipAddress << ":" << serverAddr.port;
    
    // There should be no data available to read initially
    EXPECT_FALSE(client->WaitForDataWithTimeout(100));
    
    // Send data from client to server
    std::string testMessage = "Hello, Server!";
    std::vector<std::byte> sendData = NetworkUtils::StringToBytes(testMessage);
    client->Send(sendData);
    
    // Wait for server to process and respond
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Now there should be data available
    EXPECT_TRUE(client->WaitForDataWithTimeout(100)) << "No data available after waiting";
    
    // Receive the response
    std::vector<std::byte> recvBuffer;
    int bytesReceived = client->Receive(recvBuffer);
    EXPECT_GT(bytesReceived, 0) << "Failed to receive data";
    
    client->Close();
    server.stop();
}