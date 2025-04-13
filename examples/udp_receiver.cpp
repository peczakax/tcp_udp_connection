#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
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
    
    return 0;
}