#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
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
    
    return 0;
}