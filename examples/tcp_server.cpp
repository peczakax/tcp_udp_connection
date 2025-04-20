#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "Running TCP server example..." << std::endl;

    auto& factory = NetworkFactorySingleton::GetInstance();
    auto listener = factory.CreateTcpListener();
    
    if (!listener->Bind(NetworkAddress("0.0.0.0", 8080))) {
        std::cout << "Failed to bind to port 8080" << std::endl;
        return 1;
    }
    
    std::cout << "Bound to port 8080" << std::endl;
    
    if (!listener->Listen(10)) {
        std::cout << "Failed to listen on socket" << std::endl;
        return 1;
    }
    
    std::cout << "Listening for connections..." << std::endl;
    
    auto clientSocket = listener->Accept();
    if (!clientSocket) {
        std::cout << "Failed to accept connection" << std::endl;
        return 1;
    }
    
    std::cout << "Client connected from " 
              << clientSocket->GetRemoteAddress().ipAddress << ":" 
              << clientSocket->GetRemoteAddress().port << std::endl;
    
    std::vector<std::byte> buffer;
    int bytesRead = clientSocket->Receive(buffer);
    
    if (bytesRead > 0) {
        std::string message = NetworkUtils::BytesToString(buffer);
        std::cout << "Received " << bytesRead << " bytes: " << message << std::endl;
        
        std::string response = "Hello, client! Your message was received.";
        int bytesSent = clientSocket->Send(NetworkUtils::StringToBytes(response));
        std::cout << "Sent " << bytesSent << " bytes response" << std::endl;
    }
    
    return 0;
}
