#include "network/platform_factory.h"
#include "network/byte_utils.h"
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
        int bytesSent = socket->Send(NetworkUtils::StringToBytes(message));
        std::cout << "Sent " << bytesSent << " bytes: " << message << std::endl;
        
        // Receive response
        std::vector<std::byte> response;
        int bytesRead = socket->Receive(response);
        
        if (bytesRead > 0) {
            std::cout << "Received: " << NetworkUtils::BytesToString(response) << std::endl;
        } else {
            std::cout << "Failed to receive response" << std::endl;
        }
    } else {
        std::cout << "Failed to connect to server" << std::endl;
    }
    
    return 0;
}
