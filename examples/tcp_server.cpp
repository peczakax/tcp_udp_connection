#include "network_lib/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
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
    
    return 0;
}