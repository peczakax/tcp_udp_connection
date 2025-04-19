#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

// Include public network interfaces
#include "network/network.h"
#include "network/tcp_socket.h"
#include "network/platform_factory.h"
#include "network/byte_utils.h"  // Added byte utils header

// Default port for the chat server
constexpr int DEFAULT_PORT = 8084;
constexpr int DEFAULT_BUFFER_SIZE = 1024;

// Signal handler for graceful termination
std::atomic<bool> running(true);

// Structure to represent a connected client
struct Client {
    std::unique_ptr<ITcpSocket> socket;
    std::string username;
    std::unique_ptr<std::thread> handler;
    bool authenticated;
    std::time_t lastActivity;
    std::atomic<bool> running;
    
    Client() : authenticated(false), lastActivity(0), running(true) {}
    
    // Move constructor
    Client(Client&& other) noexcept 
        : socket(std::move(other.socket)), 
          username(std::move(other.username)),
          handler(std::move(other.handler)),
          authenticated(other.authenticated),
          lastActivity(other.lastActivity),
          running(other.running.load()) {}
    
    // Move assignment operator
    Client& operator=(Client&& other) noexcept {
        if (this != &other) {
            socket = std::move(other.socket);
            username = std::move(other.username);
            handler = std::move(other.handler);
            authenticated = other.authenticated;
            lastActivity = other.lastActivity;
            running = other.running.load();
        }
        return *this;
    }
    
    // Delete copy constructor and assignment
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
};

class TCPLiveChatServer {
private:
    std::unique_ptr<ITcpListener> server;
    std::mutex clientsMutex;
    std::unordered_map<int, Client> clients;
    std::atomic<bool> running;
    NetworkAddress serverAddress;
    
    // Helper function to get current timestamp as string
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
        
        // Use ctime_s instead of ctime for safer string handling on Windows
        char timestamp[26];
#ifdef _WIN32
        ctime_s(timestamp, sizeof(timestamp), &time_t_now);
#else
        // Standard ctime on non-Windows platforms (less safe but more portable)
        std::string temp = std::ctime(&time_t_now);
        std::strncpy(timestamp, temp.c_str(), sizeof(timestamp) - 1);
        timestamp[sizeof(timestamp) - 1] = '\0';
#endif
        
        // Remove newline from timestamp
        size_t len = std::strlen(timestamp);
        if (len > 0 && (timestamp[len-1] == '\n' || timestamp[len-1] == '\r'))
            timestamp[len-1] = '\0';
            
        return "[" + std::string(timestamp) + "] ";
    }
    
    // Helper function to broadcast message to all clients
    void broadcastMessage(const std::string& message, int senderId = -1) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        for (auto& [id, client] : clients) {
            if (id != senderId && client.authenticated && client.socket && client.socket->IsValid()) {
                try {
                    std::string formattedMessage = getTimestamp() + message + "\n";
                    std::vector<std::byte> data = NetworkUtils::StringToBytes(formattedMessage);
                    client.socket->Send(data);
                } catch (const std::exception& e) {
                    std::cerr << "Error sending to client " << id << ": " << e.what() << std::endl;
                }
            }
        }
    }

    // Helper function to send a private message to a specific user
    bool sendPrivateMessage(const std::string& targetUsername, const std::string& message, int senderId) {
        bool userFound = false;
        std::string senderUsername;
        
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        // Get sender's username
        if (clients.find(senderId) != clients.end()) {
            senderUsername = clients[senderId].username;
        } else {
            return false; // Sender not found
        }
        
        // Find the target client by username
        for (auto& [id, client] : clients) {
            if (client.authenticated && client.username == targetUsername && client.socket && client.socket->IsValid()) {
                try {
                    std::string formattedMessage = getTimestamp() + "[Private from " + senderUsername + "]: " + message + "\n";
                    std::vector<std::byte> data = NetworkUtils::StringToBytes(formattedMessage);
                    client.socket->Send(data);
                    userFound = true;
                    break;
                } catch (const std::exception& e) {
                    std::cerr << "Error sending private message to " << targetUsername << ": " << e.what() << std::endl;
                    return false;
                }
            }
        }
        
        // Also send confirmation to the sender
        if (userFound && clients.find(senderId) != clients.end() && 
            clients[senderId].socket && clients[senderId].socket->IsValid()) {
            try {
                std::string confirmation = getTimestamp() + "[Private to " + targetUsername + "]: " + message + "\n";
                std::vector<std::byte> data = NetworkUtils::StringToBytes(confirmation);
                clients[senderId].socket->Send(data);
            } catch (const std::exception& e) {
                std::cerr << "Error sending confirmation to sender: " << e.what() << std::endl;
            }
        }
        
        return userFound;
    }
    
    // Remove disconnected client
    void removeClient(int clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        if (clients.find(clientId) != clients.end()) {
            std::string username = clients[clientId].username;
            
            // Broadcast that user has left if they were authenticated
            if (clients[clientId].authenticated && !username.empty()) {
                broadcastMessage(username + " has left the chat", clientId);
            }
            
            // Signal the thread to terminate
            clients[clientId].running = false;
            
            // Join the client handler thread if joinable, with a timeout
            if (clients[clientId].handler && clients[clientId].handler->joinable()) {
                // Set a timeout for the join using a detached thread
                std::thread joinThread([this, clientId]() {
                    if (clients[clientId].handler->joinable()) {
                        clients[clientId].handler->join();
                    }
                });
                joinThread.detach(); // Detach the join thread itself
            }
            
            // Close the socket and remove from clients map
            if (clients[clientId].socket) {
                clients[clientId].socket->Close();
            }
            clients.erase(clientId);
            
            std::cout << "Client " << clientId;
            if (!username.empty()) {
                std::cout << " (" << username << ")";
            }
            std::cout << " disconnected. Total clients: " << clients.size() << std::endl;
        }
    }
    
    // Handle client messages
    void handleClient(int clientId) {
        if (clients.find(clientId) == clients.end() || !clients[clientId].socket) {
            return;
        }
        
        std::vector<std::byte> buffer;  // Changed to std::byte buffer
        
        try {
            // First message from client should be their username
            buffer.clear();  // Ensure buffer is empty before receiving
            int bytesRead = clients[clientId].socket->Receive(buffer);
            if (bytesRead <= 0) {
                throw std::runtime_error("Client disconnected during authentication");
            }
            
            // Convert received bytes to string
            std::string username = NetworkUtils::BytesToString(buffer);
            
            // Trim any trailing newlines or whitespace from username
            auto removeSpecialChars = [](std::string& str, const std::string& chars) {
                for (char c : chars) {
                    str.erase(std::remove(str.begin(), str.end(), c), str.end());
                }
            };
            removeSpecialChars(username, "\n\r\0");
            
            // Remove leading and trailing whitespace
            size_t start = username.find_first_not_of(" \t");
            size_t end = username.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                username = username.substr(start, end - start + 1);
            } else {
                username.clear(); // All whitespace or empty
            }
            
            if (username.empty()) {
                username = "Guest" + std::to_string(clientId);
            }
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients[clientId].username = username;
                clients[clientId].authenticated = true;
                clients[clientId].lastActivity = std::time(nullptr);
                
                std::cout << "Client " << clientId << " authenticated as: '" << username << "'" << std::endl;
            }
            
            // Inform all clients about the new user
            broadcastMessage(username + " has joined the chat", clientId);
            
            // Send welcome message to the client
            std::string welcomeMsg = getTimestamp() + "Welcome to the chat, " + username + "!\n";
            std::vector<std::byte> welcomeData = NetworkUtils::StringToBytes(welcomeMsg);
            clients[clientId].socket->Send(welcomeData);
            
            // Main message processing loop
            while (running && clients[clientId].running && clients[clientId].socket->IsValid()) {
                // Wait for data with a short timeout to allow checking running status
                if (!clients[clientId].socket->WaitForDataWithTimeout(100)) {
                    continue; // Timeout, check running status
                }
                
                try {
                    buffer.clear();  // Clear the buffer before receiving new data
                    int bytesRead = clients[clientId].socket->Receive(buffer);
                    if (bytesRead <= 0) {
                        break;  // Client disconnected
                    }
                    
                    // Convert received bytes to string
                    std::string message = NetworkUtils::BytesToString(buffer);
                    
                    // Trim any trailing newlines or whitespace from message
                    message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
                    message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
                    message.erase(std::remove(message.begin(), message.end(), '\0'), message.end());
                    
                    // Update last activity time
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clients[clientId].lastActivity = std::time(nullptr);
                    }
                    
                    // Check for command messages
                    if (message == "/quit") {
                        std::string username;
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            if (clients.find(clientId) != clients.end()) {
                                username = clients[clientId].username;
                            }
                        }
                        std::cout << "Client " << clientId << " (" << username << ") quit the chat." << std::endl;
                        break;
                    } else if (message == "/users") {
                        // Send list of connected users
                        std::string userList = "Connected users:\n";
                        {
                            std::lock_guard<std::mutex> lock(clientsMutex);
                            for (const auto& [_, client] : clients) {
                                if (client.authenticated) {
                                    userList += "- " + client.username + "\n";
                                }
                            }
                        }
                        std::vector<std::byte> listData = NetworkUtils::StringToBytes(userList);
                        clients[clientId].socket->Send(listData);
                    } else if (message.rfind("/msg ", 0) == 0) {
                        // Private message command
                        size_t spacePos = message.find(' ', 5);
                        if (spacePos != std::string::npos) {
                            std::string targetUsername = message.substr(5, spacePos - 5);
                            std::string privateMessage = message.substr(spacePos + 1);
                            if (!sendPrivateMessage(targetUsername, privateMessage, clientId)) {
                                std::string errorMsg = getTimestamp() + "User " + targetUsername + " not found.\n";
                                std::vector<std::byte> errorData = NetworkUtils::StringToBytes(errorMsg);
                                clients[clientId].socket->Send(errorData);
                            }
                        } else {
                            std::string errorMsg = getTimestamp() + "Invalid private message format. Use /msg <username> <message>\n";
                            std::vector<std::byte> errorData = NetworkUtils::StringToBytes(errorMsg);
                            clients[clientId].socket->Send(errorData);
                        }
                    } else {
                        // Broadcast the message to all clients
                        broadcastMessage(username + ": " + message, clientId);
                        std::cout << "Message from " << username << ": " << message << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error handling client " << clientId << ": " << e.what() << std::endl;
                    break;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling client " << clientId << ": " << e.what() << std::endl;
        }
        
        // Client disconnected or error occurred, remove the client
        removeClient(clientId);
    }
    
    // Thread to monitor inactive clients and remove them
    void monitorInactiveClients() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            std::vector<int> inactiveClients;
            std::time_t currentTime = std::time(nullptr);
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& [id, client] : clients) {
                    // If client has been inactive for more than 5 minutes, disconnect them
                    if (difftime(currentTime, client.lastActivity) > 300) {
                        inactiveClients.push_back(id);
                    }
                }
            }
            
            for (int id : inactiveClients) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    if (clients.find(id) != clients.end()) {
                        username = clients[id].username;
                    }
                }
                std::cout << "Removed inactive client: " << id;
                if (!username.empty()) {
                    std::cout << " (" << username << ")";
                }
                std::cout << " (timeout after 5 minutes of inactivity)" << std::endl;
                broadcastMessage(username + " has timed out");
                removeClient(id);
            }
        }
    }

public:
    TCPLiveChatServer(int port = DEFAULT_PORT) : running(false) {
        serverAddress.port = port;
        // We'll create the actual server in start()
    }
    
    void start() {
        std::cout << "Starting TCP Chat Server on port " << serverAddress.port << "..." << std::endl;
        
        // Create the platform-specific socket factory
        auto factory = INetworkSocketFactory::CreatePlatformFactory();
        if (!factory) {
            throw std::runtime_error("Failed to create platform socket factory");
        }
        
        // Create the TCP listener
        server = factory->CreateTcpListener();
        if (!server) {
            throw std::runtime_error("Failed to create TCP listener");
        }
        
        // Bind to the port
        if (!server->Bind(serverAddress)) {
            throw std::runtime_error("Failed to bind to port " + std::to_string(serverAddress.port));
        }
        
        // Start listening for connections
        if (!server->Listen(5)) {
            throw std::runtime_error("Failed to start listening for connections");
        }
        
        running = true;
        
        // Start the inactive client monitor
        std::thread monitorThread(&TCPLiveChatServer::monitorInactiveClients, this);
        
        // Main server loop - accept connections and create handler threads
        int nextClientId = 1;
        while (running) {
            try {
                if (!server->WaitForDataWithTimeout(100)) {
                    continue; // No new connections, check running status
                }
                
                auto clientSocket = server->AcceptTcp();
                if (!clientSocket) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                    continue;
                }
                
                int clientId = nextClientId++;
                std::cout << "New client connected: " << clientId << std::endl;
                
                // Create a new client and add to the map
                Client newClient;
                newClient.socket = std::move(clientSocket);
                newClient.authenticated = false;
                newClient.lastActivity = std::time(nullptr);
                
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    clients.emplace(clientId, std::move(newClient));
                }
                
                // Create a thread to handle this client
                clients[clientId].handler = std::make_unique<std::thread>(
                    &TCPLiveChatServer::handleClient, this, clientId);
                
                std::cout << "Total clients connected: " << clients.size() << std::endl;
            } catch (const std::exception& e) {
                if (running) {
                    std::cerr << "Error accepting connection: " << e.what() << std::endl;
                }
            }
        }
        
        // Wait for the monitor thread to finish
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }
    
    void stop() {
        running = false;
        
        // Close all client connections
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& [id, client] : clients) {
            client.running = false;
            if (client.handler && client.handler->joinable()) {
                client.handler->detach();
            }
            if (client.socket) {
                client.socket->Close();
            }
        }
        clients.clear();
        
        // Stop the server
        if (server) {
            server->Close();
        }
        std::cout << "Chat server stopped" << std::endl;
    }

    void forceStop() {
        stop();
        exit(0);
    }
    
    int getPort() const {
        return serverAddress.port;
    }
};

// Global pointer to access server from signal handler
static TCPLiveChatServer* gServerPtr = nullptr;

// Platform-specific signal handling
#ifdef _WIN32
BOOL WINAPI WindowsSignalHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        std::cout << "\nReceived Ctrl+C. Forcefully shutting down chat server..." << std::endl;
        if (gServerPtr != nullptr) {
            gServerPtr->forceStop();
        }
        return TRUE;
    }
    return FALSE;
}
#else
// Signal handler for immediate termination
void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived Ctrl+C. Forcefully shutting down chat server..." << std::endl;
        if (gServerPtr != nullptr) {
            gServerPtr->forceStop();
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    
    // Parse command line arguments for port
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    TCPLiveChatServer chatServer(port);
    gServerPtr = &chatServer;

    // Register signal handler
#ifdef _WIN32
    SetConsoleCtrlHandler(WindowsSignalHandler, TRUE);
#else
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
#endif
    
    try {
        // Handle Ctrl+C for clean shutdown
        std::cout << "TCP Chat Server starting on port " << chatServer.getPort() << " (press Ctrl+C to quit)..." << std::endl;
        chatServer.start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    
    chatServer.stop();
    return 0;
}