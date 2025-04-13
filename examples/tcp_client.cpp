#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running TCP client example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto socket = factory->CreateTcpSocket();
    socket->SetNoDelay(true);
    
    std::cout << "Connecting to 127.0.0.1:8080..." << std::endl;
    
    if (socket->Connect(NetworkAddress("127.0.0.1", 8080))) {
        std::cout << "Connected to server!" << std::endl;
        
        // Send data
        std::string message = "Hello, server!";
        int bytesSent = socket->Send(std::vector<char>(message.begin(), message.end()));
        std::cout << "Sent " << bytesSent << " bytes: " << message << std::endl;
        
        // Receive response
        std::vector<char> response;
        int bytesRead = socket->Receive(response);
        
        if (bytesRead > 0) {
            std::cout << "Received: " << std::string(response.begin(), response.end()) << std::endl;
        } else {
            std::cout << "Failed to receive response" << std::endl;
        }
    } else {
        std::cout << "Failed to connect to server" << std::endl;
    }
    
    return 0;
}
