#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running UDP sender example..." << std::endl;

    auto& factory = NetworkFactorySingleton::GetInstance();
    auto socket = factory.CreateUdpSocket();
    
    std::string message = "Hello, UDP receiver!";
    std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
    
    int bytesSent = socket->SendTo(data, NetworkAddress("127.0.0.1", 8082));
    
    if (bytesSent > 0) {
        std::cout << "Sent " << bytesSent << " bytes to 127.0.0.1:8082" << std::endl;
    } else {
        std::cout << "Failed to send UDP datagram" << std::endl;
    }
    
    return 0;
}
