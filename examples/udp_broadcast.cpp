#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running UDP broadcast example..." << std::endl;

    // Create factory and broadcast sender
    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto sender = factory->CreateUdpSocket();
    
    // Try to broadcast a message
    if (!sender->SetBroadcast(true)) {
        std::cout << "Failed to enable broadcasting" << std::endl;
        return 1;
    }
    
    // Send message
    std::string message = "Hello, network!";
    int bytesSent = sender->SendTo(std::vector<char>(message.begin(), message.end()), 
                                  NetworkAddress("255.255.255.255", 8084));
    
    std::cout << (bytesSent > 0 ? 
                  "Sent " + std::to_string(bytesSent) + " bytes as broadcast" : 
                  "Failed to send broadcast message") << std::endl;
    
    return 0;
}
