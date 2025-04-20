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

// Include network first for winsock2.h before windows.h
#include "network/network.h"
#include "network/udp_socket.h"
#include "network/platform_factory.h"
#include "network/byte_utils.h"  // Added byte utils header

// Platform-specific headers
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#endif

// Default port for the chat server
constexpr int DEFAULT_PORT = 8085;

// Structure to represent a connected client
struct UdpClient {
    NetworkAddress address; // Using NetworkAddress instead of sockaddr_in
    std::string username;
    std::time_t lastActivity;
};

// Custom hash function for NetworkAddress
struct NetworkAddressHash {
    std::size_t operator()(const NetworkAddress& addr) const {
        // Hash combination of IP and port
        return std::hash<std::string>()(addr.ipAddress) ^ std::hash<unsigned short>()(addr.port);
    }
};

// Custom equality function for NetworkAddress
struct NetworkAddressEqual {
    bool operator()(const NetworkAddress& a, const NetworkAddress& b) const {
        return (a.ipAddress == b.ipAddress && a.port == b.port);
    }
};

// Signal handler for graceful termination
std::atomic<bool> running(true);

// Helper function to print address information
void printAddressInfo(const NetworkAddress& addr) {
    std::cout << addr.ipAddress << ":" << addr.port;
}

class UdpLiveChatServer {
private:
    std::unique_ptr<IUdpSocket> socket; // Replace UdpServer with IUdpSocket
    int serverPort;
    std::mutex clientsMutex;
    std::unordered_map<NetworkAddress, UdpClient, NetworkAddressHash, NetworkAddressEqual> clients;
    std::thread receiveThread;
    std::thread inactivityThread;
    std::atomic<bool> isRunning{false};
    
    // Helper function to get current timestamp as string
    std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        
        // Use safer ctime_s instead of ctime
        char timeBuffer[26];
#ifdef _WIN32
        ctime_s(timeBuffer, sizeof(timeBuffer), &time);
#else
        // For non-Windows platforms that don't have ctime_s
        std::strncpy(timeBuffer, std::ctime(&time), sizeof(timeBuffer));
        timeBuffer[sizeof(timeBuffer) - 1] = '\0'; // Ensure null termination
#endif
        std::string timestamp(timeBuffer);
        
        // Remove newline from timestamp
        if (!timestamp.empty() && timestamp.back() == '\n') {
            timestamp.pop_back();
        }
        return "[" + timestamp + "] ";
    }
    
    // Check if client exists in our map
    bool clientExists(const NetworkAddress& addr) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        return clients.find(addr) != clients.end();
    }
    
    // Register a new client
    void registerClient(const NetworkAddress& addr, const std::string& username) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        UdpClient client;
        client.address = addr;
        client.username = username;
        client.lastActivity = std::time(nullptr);
        
        clients[addr] = client;
        
        std::cout << "New client registered: " << username << " at ";
        printAddressInfo(addr);
        std::cout << "\nTotal clients: " << clients.size() << std::endl;
    }
    
    // Send message to a specific client
    void sendToClient(const NetworkAddress& addr, const std::string& message) {
        try {
            std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
            socket->SendTo(data, addr);
        } catch (const std::exception& e) {
            std::cerr << "Error sending to client: " << e.what() << std::endl;
        }
    }
    
    // Broadcast message to all clients except sender
    void broadcastMessage(const std::string& message, const NetworkAddress* sender = nullptr) {
        // Lock access to clients map to ensure thread safety
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        // Add timestamp to the message and append a newline character
        std::string formattedMessage = getTimestamp() + message + "\n";
        
        // Iterate through all connected clients
        for (auto& [addr, client] : clients) {
            // Skip the sender if provided (don't send message back to originator)
            if (sender == nullptr || 
                addr.ipAddress != sender->ipAddress || 
                addr.port != sender->port) {
                
                try {
                    // Send the formatted message to this client
                    std::vector<std::byte> data = NetworkUtils::StringToBytes(formattedMessage);
                    socket->SendTo(data, addr);
                } catch (const std::exception& e) {
                    std::cerr << "Error broadcasting to client: " << e.what() << std::endl;
                }
            }
        }
    }

    // Send a private message to a specific user
    bool sendPrivateMessage(const std::string& targetUsername, const std::string& message, 
                             const NetworkAddress* sender) {
        bool userFound = false;
        std::string senderUsername;
        
        // Lock access to clients map for thread safety
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        // Identify the sender by matching address
        for (const auto& [addr, client] : clients) {
            if (addr.ipAddress == sender->ipAddress && 
                addr.port == sender->port) {
                senderUsername = client.username;
                break;
            }
        }
        
        if (senderUsername.empty()) {
            return false; // Sender not found in clients map
        }
        
        // Search for the target user by username
        for (auto& [addr, client] : clients) {
            if (client.username == targetUsername) {
                try {
                    // Format the private message with the sender's name for the recipient
                    std::string formattedMessage = getTimestamp() + "[Private from " + 
                                                  senderUsername + "]: " + message + "\n";
                                                  
                    // Send the message to the recipient using byte conversion
                    std::vector<std::byte> data = NetworkUtils::StringToBytes(formattedMessage);
                    socket->SendTo(data, addr);
                    userFound = true;
                    
                    // Send a confirmation copy to the sender
                    if (sender != nullptr) {
                        std::string confirmation = getTimestamp() + "[Private to " + 
                                                 targetUsername + "]: " + message + "\n";
                                                 
                        std::vector<std::byte> confirmData = NetworkUtils::StringToBytes(confirmation);
                        socket->SendTo(confirmData, *sender);
                    }
                    break;
                } catch (const std::exception& e) {
                    std::cerr << "Error sending private message: " << e.what() << std::endl;
                    return false;
                }
            }
        }
        
        // Return true if target user was found, false otherwise
        return userFound;
    }
    
    // Remove inactive clients
    void removeInactiveClients() {
        while (isRunning.load() && running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            std::vector<NetworkAddress> toRemove;
            std::time_t currentTime = std::time(nullptr);
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& [addr, client] : clients) {
                    // If client has been inactive for more than 2 minutes, remove them
                    // We use a shorter timeout for UDP since it's connectionless
                    if (difftime(currentTime, client.lastActivity) > 120) {
                        toRemove.push_back(addr);
                    }
                }
            }
            
            for (const auto& addr : toRemove) {
                std::string username;
                
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    if (clients.find(addr) != clients.end()) {
                        username = clients[addr].username;
                        clients.erase(addr);
                    }
                }
                
                if (!username.empty()) {
                    std::cout << "Removed inactive client: " << username << " (timeout after 2 minutes of inactivity)" << std::endl;
                    broadcastMessage(username + " has timed out");
                }
            }
        }
    }
    
    // Handle client messages
    void handleMessage(const std::string& message, const NetworkAddress& clientAddr) {
        // Update client's last activity timestamp to prevent timeout
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            if (clients.find(clientAddr) != clients.end()) {
                clients[clientAddr].lastActivity = std::time(nullptr);
            }
        }
        
        // Handle registration protocol: "REGISTER:username"
        if (message.substr(0, 9) == "REGISTER:") {
            std::string username = message.substr(9);  // Extract username after "REGISTER:"
            
            // Sanitize username by removing special characters
            auto removeSpecialChars = [](std::string& str, const std::string& chars) {
                for (char c : chars) {
                    str.erase(std::remove(str.begin(), str.end(), c), str.end());
                }
            };
            removeSpecialChars(username, "\n\r\0");
            
            if (!clientExists(clientAddr)) {
                // Add new client to active clients list with the provided username
                registerClient(clientAddr, username);
                
                // Send personalized welcome message to the new client
                std::string welcome = getTimestamp() + "Welcome to the chat, " + username + "!\n";
                sendToClient(clientAddr, welcome);
                
                // Send instructions for private messaging
                std::string info = getTimestamp() + "To send a private message, use: /msg <username> <message>\n";
                sendToClient(clientAddr, info);
                
                // Notify other clients that a new user has joined
                broadcastMessage(username + " has joined the chat", &clientAddr);
            }
            return;
        }
        
        // Handle heartbeat messages that keep connection alive
        if (message == "HEARTBEAT") {
            return;  // Just update lastActivity time and do nothing else
        }
        
        // Handle quit command
        if (message == "/quit") {
            std::string username;
            
            // Remove client from active clients list
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                if (clients.find(clientAddr) != clients.end()) {
                    username = clients[clientAddr].username;
                    clients.erase(clientAddr);
                }
            }
            
            // Notify other clients if a registered user has left
            if (!username.empty()) {
                std::cout << "Client " << clientAddr.ipAddress << ":" << clientAddr.port << " (" << username << ") quit the chat." << std::endl;
                broadcastMessage(username + " has left the chat", &clientAddr);
            }
            return;
        }
        
        // Handle users list command
        if (message == "/users") {
            std::string userList = "Connected users:\n";
            
            // Build list of currently connected users
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& [_, client] : clients) {
                    userList += "- " + client.username + "\n";
                }
            }
            
            // Send the user list only to the requesting client
            sendToClient(clientAddr, userList);
            return;
        }
        
        // Handle private messaging: "/msg username message"
        if (message.rfind("/msg ", 0) == 0 && message.length() > 5) {
            size_t spacePos = message.find(' ', 5);  // Find space after username
            if (spacePos != std::string::npos) {
                // Extract recipient username and message content
                std::string targetUsername = message.substr(5, spacePos - 5);
                std::string privateMessage = message.substr(spacePos + 1);
                std::string username;
                
                // Get sender's username
                {
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    if (clients.find(clientAddr) != clients.end()) {
                        username = clients[clientAddr].username;
                    }
                }
                
                if (!username.empty()) {
                    // Send private message to target user
                    if (!sendPrivateMessage(targetUsername, privateMessage, &clientAddr)) {
                        // Notify sender if target user not found
                        std::string errorMsg = getTimestamp() + "User " + targetUsername + " not found.\n";
                        sendToClient(clientAddr, errorMsg);
                    }
                }
            } else {
                // Error message for invalid private message format
                std::string errorMsg = getTimestamp() + "Invalid private message format. Use /msg <username> <message>\n";
                sendToClient(clientAddr, errorMsg);
            }
            return;
        }
        
        // Handle regular chat messages
        std::string username;
        
        // Get the sender's username
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            if (clients.find(clientAddr) != clients.end()) {
                username = clients[clientAddr].username;
            }
        }
        
        if (!username.empty()) {
            // Log message to server console
            std::cout << "Message from " << username << ": " << message << std::endl;
            // Broadcast message to all clients except sender
            broadcastMessage(username + ": " + message, &clientAddr);
        } else {
            // Unregistered client attempted to send a message - prompt to register
            std::string registerMsg = "Please register first with REGISTER:<username>";
            sendToClient(clientAddr, registerMsg);
        }
    }
    
    // Continuously receive incoming data
    void receiveMessages() {
        // Buffer for incoming data
        std::vector<std::byte> buffer;
        
        while (isRunning.load() && running.load()) {
            try {
                if (socket->WaitForDataWithTimeout(100)) {
                    // Clear the buffer before receiving new data
                    buffer.clear();
                    buffer.resize(4096);
                    
                    // Only try to receive if data is available
                    NetworkAddress clientAddress;
                    int bytesReceived = socket->ReceiveFrom(buffer, clientAddress);
                    
                    if (bytesReceived > 0) {
                        // Resize buffer to actual received data size
                        buffer.resize(bytesReceived);
                        
                        // Convert std::byte buffer to string
                        std::string message = NetworkUtils::BytesToString(buffer);
                        
                        // Handle the received message
                        handleMessage(message, clientAddress);
                    }
                }
            } catch (const std::exception& e) {
                if (isRunning.load()) {
                    std::cerr << "Error receiving data: " << e.what() << std::endl;
                }
            }
        }
    }

public:
    UdpLiveChatServer(int port = DEFAULT_PORT) : serverPort(port) {}
    
    void start() {
        // Create UDP socket using platform factory
        auto factory = INetworkSocketFactory::CreatePlatformFactory();
        socket = factory->CreateUdpSocket();
        
        if (!socket || !socket->IsValid()) {
            throw std::runtime_error("Failed to create UDP socket");
        }
        
        // Bind the socket to the server port
        NetworkAddress serverAddress("0.0.0.0", serverPort);
        if (!socket->Bind(serverAddress)) {
            throw std::runtime_error("Failed to bind UDP socket to port " + std::to_string(serverPort));
        }
        
        NetworkAddress boundAddr = socket->GetLocalAddress();
        std::cout << "Starting UDP Chat Server on port " << boundAddr.port << std::endl;
        
        // Set running flag and start worker threads
        isRunning.store(true);
        
        // Start background thread to monitor and remove inactive clients
        inactivityThread = std::thread(&UdpLiveChatServer::removeInactiveClients, this);
        
        // Start receiver thread
        receiveThread = std::thread(&UdpLiveChatServer::receiveMessages, this);
        
        // Keep main thread alive until interrupted
        while (isRunning.load() && running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void stop() {
        // Set running flag to false, which will cause receive thread to exit
        isRunning.store(false);
        
        // Close the socket
        if (socket) {
            socket->Close();
        }
        
        // Wait for the receiver thread to complete
        if (receiveThread.joinable()) {
            receiveThread.join();
        }
        
        // Wait for the inactivity monitoring thread to complete
        if (inactivityThread.joinable()) {
            inactivityThread.join();
        }
        
        // Remove all clients from the map
        clients.clear();
        
        std::cout << "UDP Chat server stopped" << std::endl;
    }

    void forceStop() {
        // Set running to false to signal threads to terminate
        isRunning.store(false);
        
        // Close the socket
        if (socket) {
            socket->Close();
        }
        
        // Detach the threads instead of joining them
        // This allows immediate termination without waiting for thread completion
        if (receiveThread.joinable()) {
            receiveThread.detach();
        }

        if (inactivityThread.joinable()) {
            inactivityThread.detach();
        }
        
        std::cout << "UDP Chat server forcefully terminated" << std::endl;
        exit(0);
    }
    
    int getPort() const {
        if (socket && socket->IsValid()) {
            return socket->GetLocalAddress().port;
        }
        return serverPort;
    }
};

// Global pointer to access server from signal handler
static UdpLiveChatServer* gServerPtr = nullptr;

// Platform-specific signal handling
#ifdef _WIN32
BOOL WINAPI WindowsSignalHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        // Print shutdown message when Ctrl+C is pressed
        std::cout << "\nReceived Ctrl+C. Forcefully shutting down chat server..." << std::endl;
        // Set global running flag to false to terminate all running threads
        running = false;
        // Call forceStop on the server instance to immediately terminate
        if (gServerPtr) {
            gServerPtr->forceStop();
        }
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT) {
        // Print shutdown message when Ctrl+C is pressed
        std::cout << "\nReceived Ctrl+C. Forcefully shutting down chat server..." << std::endl;
        // Set global running flag to false to terminate all running threads
        running = false;
        // Call forceStop on the server instance to immediately terminate
        if (gServerPtr) {
            gServerPtr->forceStop();
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    
#ifdef _WIN32
    // Set up signal handling for Windows
    SetConsoleCtrlHandler(WindowsSignalHandler, TRUE);
#else
    struct sigaction sa; // Set up signal handling for catching Ctrl+C
    sa.sa_handler = signalHandler;  // Assign our custom signal handler function
    sigemptyset(&sa.sa_mask);  // Initialize signal mask to empty set
    sa.sa_flags = 0;  // No special flags for signal handling
    sigaction(SIGINT, &sa, nullptr);  // Register handler for SIGINT (Ctrl+C)
#endif
    
    int port = DEFAULT_PORT;
    
    // Allow overriding default port via command line argument
    if (argc > 1) {
        port = std::atoi(argv[1]);  // Convert port argument to integer
    }
    
    // Create chat server instance with specified port
    UdpLiveChatServer chatServer(port);
    // Store global pointer for signal handler to access
    gServerPtr = &chatServer;
    
    try {
        std::cout << "UDP Chat Server starting (press Ctrl+C to quit)..." << std::endl;
        chatServer.start();  // Start server (blocks until server is stopped)
    } catch (const std::exception& e) {
        // Handle exceptions
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    
    // Perform clean shutdown (only reached after exception or signal handler)
    chatServer.stop();
    return 0;
}