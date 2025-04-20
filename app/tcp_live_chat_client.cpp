#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <cstddef>

// Include network first for winsock2.h before windows.h
#include "network/network.h"
#include "network/tcp_socket.h"
#include "network/platform_factory.h"
#include "network/byte_utils.h"

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

// Default server settings
constexpr int DEFAULT_PORT = 8084;
const std::string DEFAULT_SERVER = "127.0.0.1";
constexpr int DEFAULT_BUFFER_SIZE = 4096;

// Signal handler for graceful termination
std::atomic<bool> running(true);
std::condition_variable terminationCv;
std::mutex terminationMutex;

class TCPLiveChatClient {
private:
    std::unique_ptr<ITcpSocket> socket;
    std::string username;
    std::thread receiveThread;
    NetworkAddress serverAddress;

    // Function to receive and display messages from the server
    void receiveMessages() {
        std::vector<std::byte> buffer;
        
        while (running && socket && socket->IsValid()) {
            try {
                // Use the wait for data with timeout method
                if (socket->WaitForDataWithTimeout(500)) { // 500ms timeout
                    buffer.clear(); // Clear buffer before receiving
                    int bytesRead = socket->Receive(buffer);
                    
                    if (bytesRead <= 0) {
                        // Connection closed by server
                        std::cerr << "Server has closed the connection." << std::endl;
                        running = false;
                        break;
                    }
                    
                    // Convert bytes to string and display
                    std::string message = NetworkUtils::BytesToString(buffer);
                    
                    std::cout << message;
                    
                    // Add a prompt after each message for better UX, including username
                    std::cout << username << "> " << std::flush;
                }
            } catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Error receiving data from server: " << e.what() << std::endl;
                }
                running = false;
                break;
            } catch (...) {
                if (running) {
                    std::cerr << "Unknown error receiving data from server" << std::endl;
                }
                running = false;
                break;
            }
        }
    }

public:
    TCPLiveChatClient(const std::string& serverIp, int port, const std::string& username)
        : username(username), serverAddress(serverIp, port) {
        auto& factory = NetworkFactorySingleton::GetInstance();
        socket = factory.CreateTcpSocket();
    }
    
    bool connect() {
        try {
            std::cout << "Connecting to chat server at " << serverAddress.ipAddress << ":" 
                      << serverAddress.port << "..." << std::endl;
            
            // Connect to the server
            if (!socket->Connect(serverAddress)) {
                std::cerr << "Failed to connect to server" << std::endl;
                return false;
            }
            
            // Send username as the first message
            std::vector<std::byte> usernameData = NetworkUtils::StringToBytes(username);
            if (!socket->Send(usernameData)) {
                std::cerr << "Failed to send username to server" << std::endl;
                return false;
            }
            
            // Start the message receiving thread
            receiveThread = std::thread(&TCPLiveChatClient::receiveMessages, this);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Connection error: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "Unknown connection error occurred" << std::endl;
            return false;
        }
    }
    
    void run() {
        if (!socket || !socket->IsValid()) {
            std::cerr << "Not connected to server. Call connect() first." << std::endl;
            return;
        }
        
        std::cout << "Connected! Type your messages and press Enter to send." << std::endl;
        std::cout << "Type /quit to exit, /users to see who's online." << std::endl;
        std::cout << "To send a private message, use: /msg <username> <message>" << std::endl;
        std::cout << username << "> " << std::flush;
        
        std::string message;
        while (running && socket && socket->IsValid()) {
            std::getline(std::cin, message);
            
            // Check for empty messages
            if (message.empty()) {
                std::cout << username << "> " << std::flush;
                continue;
            }
            
            if (message == "/quit") {
                // Send quit command to server before disconnecting
                try {
                    std::vector<std::byte> quitMsg = NetworkUtils::StringToBytes(message);
                    socket->Send(quitMsg);
                } catch (...) {}
                
                running = false;
                break;
            }
            
            try {
                // Convert string message to byte vector
                std::vector<std::byte> msgData = NetworkUtils::StringToBytes(message);
                
                // Send the message
                if (!socket->Send(msgData)) {
                    throw std::runtime_error("Failed to send message");
                }
                
                // Don't print prompt if the command will yield a server response
                if (message != "/users" && message.rfind("/msg ", 0) != 0) {
                    std::cout << username << "> " << std::flush;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error sending message: " << e.what() << std::endl;
                running = false;
                break;
            } catch (...) {
                std::cerr << "Unknown error sending message" << std::endl;
                running = false;
                break;
            }
        }
    }
    
    void disconnect() {
        running = false;
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
        
        if (socket && socket->IsValid()) {
            socket->Close();
        }
        
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        
        std::cout << "Disconnected from chat server." << std::endl;
    }

    void forceDisconnect() {
        // Set running to false to stop all threads
        running = false;
        
        // Close the socket immediately
        if (socket && socket->IsValid()) {
            socket->Close();
        }
        
        // Detach threads instead of waiting for them to join
        if (receiveThread.joinable()) {
            receiveThread.detach();
        }
        
        std::cout << "Forcefully disconnected from TCP chat server" << std::endl;
        
        // Signal main thread to exit
        terminationCv.notify_all();
    }
};

// Global pointer to client for signal handler access
static TCPLiveChatClient* gClientPtr = nullptr;

// Platform-specific signal handling
#ifdef _WIN32
BOOL WINAPI WindowsSignalHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        std::cout << "\nReceived Ctrl+C. Forcefully disconnecting from chat server..." << std::endl;
        running = false;
        
        // Force immediate socket shutdown if client exists
        if (gClientPtr != nullptr) {
            gClientPtr->forceDisconnect();
        }
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C. Forcefully disconnecting from chat server..." << std::endl;
        running = false;
        
        // Force immediate socket shutdown if client exists
        if (gClientPtr != nullptr) {
            gClientPtr->forceDisconnect();
        }
        
        // Notify all waiting threads about termination
        terminationCv.notify_all();
    }
}
#endif

int main(int argc, char* argv[]) {
    // Set up signal handling for graceful termination
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(WindowsSignalHandler, TRUE)) {
        std::cerr << "Could not set control handler" << std::endl;
    }
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
    TCPLiveChatClient chatClient(serverIp, port, username);
    gClientPtr = &chatClient;
    
    if (chatClient.connect()) {
        // Start the client in a separate thread
        std::thread clientThread([&chatClient]() {
            chatClient.run();
        });
        
        // Wait for termination signal using condition variable
        {
            std::unique_lock<std::mutex> lock(terminationMutex);
            terminationCv.wait(lock, []{ return !running; });
        }
        
        // If we reach here, the termination signal has been received
        chatClient.disconnect();
        
        if (clientThread.joinable()) {
            clientThread.join();
        }
    }
    
    return 0;
}