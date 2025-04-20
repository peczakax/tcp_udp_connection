#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running UDP receiver example..." << std::endl;

    auto& factory = NetworkFactorySingleton::GetInstance();
    auto socket = factory.CreateUdpSocket();
    
    if (!socket->Bind(NetworkAddress("0.0.0.0", 8082))) {
        std::cout << "Failed to bind UDP socket to port 8082" << std::endl;
        return 1;
    }
    
    std::cout << "UDP receiver bound to port 8082, waiting for messages..." << std::endl;
    
    std::vector<std::byte> buffer;
    NetworkAddress sender;
    
    int bytesRead = socket->ReceiveFrom(buffer, sender);
    
    if (bytesRead <= 0) {
        std::cout << "Failed to receive UDP datagram" << std::endl;
        return 1;
    }
    
    std::string message = NetworkUtils::BytesToString(buffer);
    std::cout << "Received " << bytesRead << " bytes from " 
              << sender.ipAddress << ":" << sender.port 
              << ": " << message << std::endl;
    
    // Send response
    std::string response = "Message received!";
    socket->SendTo(NetworkUtils::StringToBytes(response), sender);
    
    return 0;
}
