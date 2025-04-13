#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
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
    
    return 0;
}