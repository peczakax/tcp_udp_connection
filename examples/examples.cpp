#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Example: TCP Client
void RunTcpClient() {
    std::cout << "Running TCP client example..." << std::endl;

    // Create platform-specific factory
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a TCP socket
    auto socket = factory->CreateTcpSocket();
    
    // Enable TCP_NODELAY option (disable Nagle's algorithm)
    socket->SetNoDelay(true);
    
    std::cout << "Connecting to 127.0.0.1:8080..." << std::endl;
    // Connect to a server
    if (socket->Connect(NetworkAddress("127.0.0.1", 8080))) {
        std::cout << "Connected to server!" << std::endl;
        
        // Send data
        std::string message = "Hello, server!";
        std::vector<char> data(message.begin(), message.end());
        int bytesSent = socket->Send(data);
        
        std::cout << "Sent " << bytesSent << " bytes: " << message << std::endl;
        
        // Receive response
        std::vector<char> response;
        int bytesRead = socket->Receive(response);
        
        if (bytesRead > 0) {
            std::string responseStr(response.begin(), response.end());
            std::cout << "Received " << bytesRead << " bytes: " << responseStr << std::endl;
        } else {
            std::cout << "Failed to receive response" << std::endl;
        }
    } else {
        std::cout << "Failed to connect to server" << std::endl;
    }
}

// Example: TCP Server
void RunTcpServer() {
    std::cout << "Running TCP server example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a TCP listener
    auto listener = factory->CreateTcpListener();
    
    // Bind to an address and port
    if (listener->Bind(NetworkAddress("0.0.0.0", 8080))) {
        std::cout << "Bound to port 8080" << std::endl;
        
        // Start listening
        if (listener->Listen(10)) {
            std::cout << "Listening for connections..." << std::endl;
            
            // Accept a connection
            auto clientSocket = listener->Accept();
            
            if (clientSocket) {
                std::cout << "Client connected from " 
                         << clientSocket->GetRemoteAddress().ipAddress 
                         << ":" << clientSocket->GetRemoteAddress().port
                         << std::endl;
                
                // Receive data
                std::vector<char> buffer;
                int bytesRead = clientSocket->Receive(buffer);
                
                if (bytesRead > 0) {
                    std::string message(buffer.begin(), buffer.end());
                    std::cout << "Received " << bytesRead << " bytes: " << message << std::endl;
                    
                    // Send response
                    std::string response = "Hello, client! Your message was received.";
                    std::vector<char> responseData(response.begin(), response.end());
                    int bytesSent = clientSocket->Send(responseData);
                    
                    std::cout << "Sent " << bytesSent << " bytes response" << std::endl;
                }
            } else {
                std::cout << "Failed to accept connection" << std::endl;
            }
        } else {
            std::cout << "Failed to listen on socket" << std::endl;
        }
    } else {
        std::cout << "Failed to bind to port 8080" << std::endl;
    }
}

// Example: UDP Sender
void RunUdpSender() {
    std::cout << "Running UDP sender example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a UDP socket
    auto socket = factory->CreateUdpSocket();
    
    // Bind to a local port (optional for sending)
    if (socket->Bind(NetworkAddress("0.0.0.0", 8081))) {
        std::cout << "UDP sender bound to port 8081" << std::endl;
        
        // Send a datagram
        std::string message = "Hello, UDP receiver!";
        std::vector<char> data(message.begin(), message.end());
        
        int bytesSent = socket->SendTo(data, NetworkAddress("127.0.0.1", 8082));
        
        if (bytesSent > 0) {
            std::cout << "Sent " << bytesSent << " bytes to 127.0.0.1:8082" << std::endl;
        } else {
            std::cout << "Failed to send UDP datagram" << std::endl;
        }
    } else {
        std::cout << "Failed to bind UDP socket" << std::endl;
    }
}

// Example: UDP Receiver
void RunUdpReceiver() {
    std::cout << "Running UDP receiver example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a UDP socket
    auto socket = factory->CreateUdpSocket();
    
    // Bind to receive messages
    if (socket->Bind(NetworkAddress("0.0.0.0", 8082))) {
        std::cout << "UDP receiver bound to port 8082" << std::endl;
        std::cout << "Waiting for messages..." << std::endl;
        
        // Receive a datagram (will block until a message arrives)
        std::vector<char> buffer;
        NetworkAddress sender;
        
        int bytesRead = socket->ReceiveFrom(buffer, sender);
        
        if (bytesRead > 0) {
            std::string message(buffer.begin(), buffer.end());
            std::cout << "Received " << bytesRead << " bytes from " 
                     << sender.ipAddress << ":" << sender.port 
                     << ": " << message << std::endl;
            
            // Optionally send a response back
            std::string response = "Message received!";
            std::vector<char> responseData(response.begin(), response.end());
            
            socket->SendTo(responseData, sender);
        } else {
            std::cout << "Failed to receive UDP datagram" << std::endl;
        }
    } else {
        std::cout << "Failed to bind UDP socket to port 8082" << std::endl;
    }
}

// Example: UDP Multicast
void RunUdpMulticast() {
    std::cout << "Running UDP multicast example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a multicast sender
    auto sender = factory->CreateUdpSocket();
    
    // Create a multicast receiver
    auto receiver = factory->CreateUdpSocket();
    
    // Multicast address (must be in the range 224.0.0.0 to 239.255.255.255)
    NetworkAddress multicastGroup("239.255.1.1", 8083);
    
    // Set up the receiver
    if (receiver->Bind(NetworkAddress("0.0.0.0", 8083))) {
        // Join the multicast group
        if (receiver->JoinMulticastGroup(multicastGroup)) {
            std::cout << "Joined multicast group " << multicastGroup.ipAddress << std::endl;
            
            // Start a thread to receive multicast messages
            std::thread receiveThread([&receiver]() {
                std::cout << "Waiting for multicast messages..." << std::endl;
                
                std::vector<char> buffer;
                NetworkAddress sender;
                
                int bytesRead = receiver->ReceiveFrom(buffer, sender);
                
                if (bytesRead > 0) {
                    std::string message(buffer.begin(), buffer.end());
                    std::cout << "Received multicast: " << message << std::endl;
                }
            });
            
            // Send a multicast message
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Give receiver time to start
            
            std::string message = "Hello, multicast group!";
            std::vector<char> data(message.begin(), message.end());
            
            std::cout << "Sending multicast message..." << std::endl;
            int bytesSent = sender->SendTo(data, multicastGroup);
            
            if (bytesSent > 0) {
                std::cout << "Sent " << bytesSent << " bytes to multicast group" << std::endl;
            } else {
                std::cout << "Failed to send multicast message" << std::endl;
            }
            
            // Wait for receive thread to complete
            receiveThread.join();
            
            // Leave the multicast group
            receiver->LeaveMulticastGroup(multicastGroup);
        } else {
            std::cout << "Failed to join multicast group" << std::endl;
        }
    } else {
        std::cout << "Failed to bind multicast receiver" << std::endl;
    }
}

// Example: UDP Broadcast
void RunUdpBroadcast() {
    std::cout << "Running UDP broadcast example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create broadcast sender
    auto sender = factory->CreateUdpSocket();
    
    // Enable broadcasting
    if (sender->SetBroadcast(true)) {
        std::cout << "Broadcast enabled" << std::endl;
        
        // Send broadcast message
        std::string message = "Hello, network!";
        std::vector<char> data(message.begin(), message.end());
        
        int bytesSent = sender->SendTo(data, NetworkAddress("255.255.255.255", 8084));
        
        if (bytesSent > 0) {
            std::cout << "Sent " << bytesSent << " bytes as broadcast" << std::endl;
        } else {
            std::cout << "Failed to send broadcast message" << std::endl;
        }
    } else {
        std::cout << "Failed to enable broadcasting" << std::endl;
    }
}

// Main example runner
int main() {
    std::cout << "Network Library Examples" << std::endl;
    std::cout << "=======================\n" << std::endl;
    
    // Uncomment the example you want to run
    
    // TCP Examples
    // Note: Run the server in one terminal and the client in another
    //RunTcpServer();
    //RunTcpClient();
    
    // UDP Examples
    // Note: Run the receiver in one terminal and the sender in another
    //RunUdpReceiver();
    //RunUdpSender();
    
    // Multicast Example
    //RunUdpMulticast();
    
    // Broadcast Example
    //RunUdpBroadcast();
    
    return 0;
}