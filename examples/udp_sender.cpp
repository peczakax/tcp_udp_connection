#include "network/platform_factory.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running UDP sender example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    auto socket = factory->CreateUdpSocket();
    
    std::string message = "Hello, UDP receiver!";
    std::vector<char> data(message.begin(), message.end());
    
    int bytesSent = socket->SendTo(data, NetworkAddress("127.0.0.1", 8082));
    
    if (bytesSent > 0) {
        std::cout << "Sent " << bytesSent << " bytes to 127.0.0.1:8082" << std::endl;
    } else {
        std::cout << "Failed to send UDP datagram" << std::endl;
    }
    
    return 0;
}
