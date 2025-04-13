#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

// Include public network interfaces
#include "network/network.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"

// Default server settings
constexpr int DEFAULT_PORT = 8085;  // Match UDP chat server port
constexpr int HEARTBEAT_INTERVAL_S = 30;  // Seconds between heartbeat messages
constexpr int RECEIVE_TIMEOUT_MS = 1000;  // 1000ms timeout for receive operations
constexpr const char* DEFAULT_SERVER = "127.0.0.1"; 
constexpr int DEFAULT_BUFFER_SIZE = 4096; // Default buffer size for receiving data

// Signal handler for graceful termination
std::atomic<bool> running(true);
std::condition_variable terminationCv;
std::mutex terminationMutex;

class UdpLiveChatClient {
private:
    std::unique_ptr<IUdpSocket> socket;
    NetworkAddress serverAddress;
    std::string username;
    std::thread receiveThread;
    std::thread heartbeatThread;
    bool initialized;

    // Function to receive and display messages from the server
    void receiveMessages() {
        std::vector<char> buffer;
        buffer.resize(DEFAULT_BUFFER_SIZE, 0);
        
        while (running) {
            try {
                // Use WaitForDataWithTimeout to avoid blocking indefinitely
                if (socket->WaitForDataWithTimeout(RECEIVE_TIMEOUT_MS)) {
                    // Clear buffer before receiving new data
                    buffer.clear();
                    
                    NetworkAddress sender;
                    int bytesRead = socket->ReceiveFrom(buffer, sender);
                    
                    if (bytesRead <= 0) {
                        // No data, just continue checking the running flag
                        continue;
                    }
                    
                    // Ensure the buffer is properly sized and null-terminated for string conversion
                    buffer.resize(bytesRead);
                    buffer.push_back('\0');
                    
                    // Print the message
                    std::cout << buffer.data();
                    
                    // Add a prompt after each message for better UX, including username
                    std::cout << username << "> " << std::flush;
                }
            } catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Error receiving data: " << e.what() << std::endl;
                }
                // With UDP, we can continue trying to receive even after an error
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    // Function to send periodic heartbeat messages to the server
    void sendHeartbeats() {
        const std::string heartbeatMsg = "HEARTBEAT";
        
        while (running) {
            try {
                std::vector<char> data(heartbeatMsg.begin(), heartbeatMsg.end());
                socket->SendTo(data, serverAddress);
            } catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Error sending heartbeat: " << e.what() << std::endl;
                }
            }
            
            // Instead of sleeping for the full interval, sleep in shorter increments
            // and check the running flag between sleeps
            for (int i = 0; i < HEARTBEAT_INTERVAL_S * 10 && running; i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

public:
    UdpLiveChatClient(const std::string& serverIp, int port, const std::string& username)
        : serverAddress(serverIp, port), username(username), initialized(false) {}
    
    bool connect() {
        try {
            std::cout << "Initializing connection to chat server at " << serverAddress.ipAddress 
                      << ":" << serverAddress.port << "..." << std::endl;
            
            // Create socket using platform factory
            auto factory = INetworkSocketFactory::CreatePlatformFactory();
            socket = factory->CreateUdpSocket();
            
            if (!socket || !socket->IsValid()) {
                throw std::runtime_error("Failed to create UDP socket");
            }
            
            initialized = true;
            
            // Send registration message with username
            std::string registerMsg = "REGISTER:" + username;
            std::vector<char> data(registerMsg.begin(), registerMsg.end());
            socket->SendTo(data, serverAddress);
            
            // Start the message receiving thread
            receiveThread = std::thread(&UdpLiveChatClient::receiveMessages, this);
            
            // Start the heartbeat thread
            heartbeatThread = std::thread(&UdpLiveChatClient::sendHeartbeats, this);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }
    
    void run() {
        if (!initialized || !socket || !socket->IsValid()) {
            std::cerr << "Client not initialized. Call connect() first." << std::endl;
            return;
        }
        
        std::cout << "Connected! Type your messages and press Enter to send." << std::endl;
        std::cout << "Type /quit to exit, /users to see who's online." << std::endl;
        std::cout << "To send a private message, use: /msg <username> <message>" << std::endl;
        std::cout << username << "> " << std::flush;
        
        std::string message;
        while (running) {
            std::getline(std::cin, message);
            
            if (message == "/quit") {
                // Send quit command to server before disconnecting
                try {
                    std::vector<char> data(message.begin(), message.end());
                    socket->SendTo(data, serverAddress);
                } catch (...) {}
                
                running = false;
                break;
            }
            
            if (!message.empty()) {
                try {
                    std::vector<char> data(message.begin(), message.end());
                    socket->SendTo(data, serverAddress);
                    
                    // Don't print prompt if the command will yield a server response
                    if (message != "/users" && message.rfind("/msg ", 0) != 0) {
                        std::cout << username << "> " << std::flush;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error sending message: " << e.what() << std::endl;
                    // In UDP, we can continue trying even after a send error
                }
            } else {
                std::cout << username << "> " << std::flush;
            }
        }
    }
    
    void disconnect() {
        running = false;
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
        
        // Try to send a quit message to the server
        try {            
            std::string quitMsg = "/quit";
            std::vector<char> data(quitMsg.begin(), quitMsg.end());
            socket->SendTo(data, serverAddress);
        } catch (...) {}
        
        // Close the client socket
        if (initialized && socket) {
            socket->Close();
        }
        
        // Wait for threads to finish
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        
        if (heartbeatThread.joinable()) {
            heartbeatThread.join();
        }
        
        std::cout << "Disconnected from chat server." << std::endl;
    }

    void forceDisconnect() {
        // Set running to false to stop all threads
        running = false;
        
        // Close the client socket immediately
        if (initialized && socket) {
            // Send a quick disconnect message if possible
            try {
                std::string quitMsg = "/quit";
                std::vector<char> data(quitMsg.begin(), quitMsg.end());
                socket->SendTo(data, serverAddress);
            } catch (...) {} // Ignore errors
            
            socket->Close();
        }
        
        // Detach threads instead of waiting for them to join
        if (receiveThread.joinable()) {
            receiveThread.detach();
        }
        
        if (heartbeatThread.joinable()) {
            heartbeatThread.detach();
        }
        
        std::cout << "Forcefully disconnected from UDP chat server" << std::endl;
        
        // Signal main thread to exit
        terminationCv.notify_all();
    }
    
    bool isInitialized() const {
        return initialized && socket && socket->IsValid();
    }
    
    NetworkAddress getServerAddress() const {
        return serverAddress;
    }
};

// Global pointer to client for signal handler access
static UdpLiveChatClient* gClientPtr = nullptr;

// Platform-specific signal handling
#ifdef _WIN32
BOOL WINAPI WindowsSignalHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        std::cout << "\nReceived Ctrl+C. Shutting down chat client..." << std::endl;
        running = false;
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
        
        // Force disconnect and exit immediately when Ctrl+C is pressed
        if (gClientPtr) {
            gClientPtr->forceDisconnect();
        }
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C. Shutting down chat client..." << std::endl;
        running = false;
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
        
        // Force disconnect and exit immediately when Ctrl+C is pressed
        if (gClientPtr) {
            gClientPtr->forceDisconnect();
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    // Set up signal handling for graceful termination
#ifdef _WIN32
    SetConsoleCtrlHandler(WindowsSignalHandler, TRUE);
#else
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
#endif
    
    std::string serverIp = DEFAULT_SERVER;
    int port = DEFAULT_PORT;
    std::string username;
    
    // Process command line arguments
    if (argc > 1) {
        serverIp = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }
    
    // Get username from the user
    while (username.empty()) {
        std::cout << "Enter your username: ";
        std::getline(std::cin, username);
        
        if (username.empty()) {
            std::cout << "Username cannot be empty. Please try again." << std::endl;
        }
        
        // Check for invalid characters or length
        if (username.length() > 20) {
            std::cout << "Username is too long (max 20 characters). Please try again." << std::endl;
            username.clear();
        }
    }
    
    // Create and run the chat client
    UdpLiveChatClient chatClient(serverIp, port, username);
    gClientPtr = &chatClient;
    
    if (chatClient.connect()) {
        // Start the client in a separate thread
        std::jthread clientThread([&chatClient]() {
            chatClient.run();
        });
        
        // Wait for termination signal using condition variable
        {
            std::unique_lock<std::mutex> lock(terminationMutex);
            terminationCv.wait(lock, []{ return !running; });
        }
        
        // If we reach here, the termination signal has been received
        chatClient.disconnect();
    }
    
    return 0;
}