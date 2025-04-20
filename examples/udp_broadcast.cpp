#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running UDP broadcast example..." << std::endl;

    auto& factory = NetworkFactorySingleton::GetInstance();
    auto socket = factory.CreateUdpSocket();
    
    // Try to broadcast a message
    if (!socket->SetBroadcast(true)) {
        std::cout << "Failed to enable broadcasting" << std::endl;
        return 1;
    }
    
    // Send message
    std::string message = "Hello, network!";
    int bytesSent = socket->SendTo(NetworkUtils::StringToBytes(message), 
                                  NetworkAddress("255.255.255.255", 8084));
    
    std::cout << (bytesSent > 0 ? 
                  "Sent " + std::to_string(bytesSent) + " bytes as broadcast" : 
                  "Failed to send broadcast message") << std::endl;
    
    return 0;
}
