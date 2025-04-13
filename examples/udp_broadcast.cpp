#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
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
    
    return 0;
}