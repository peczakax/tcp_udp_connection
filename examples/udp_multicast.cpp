#include "network/platform_factory.h"
#include "network/byte_utils.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    std::cout << "Running UDP multicast example..." << std::endl;

    auto& factory = NetworkFactorySingleton::GetInstance();
    auto sender = factory.CreateUdpSocket();
    auto receiver = factory.CreateUdpSocket();
    
    // Multicast configuration
    NetworkAddress multicastGroup("239.255.1.1", 8083);
    std::string message = "Hello, multicast group!";
    
    // Set up receiver
    if (!receiver->Bind(NetworkAddress("0.0.0.0", 8083))) {
        std::cout << "Failed to bind receiver" << std::endl;
        return 1;
    }
    
    if (!receiver->JoinMulticastGroup(multicastGroup)) {
        std::cout << "Failed to join multicast group" << std::endl;
        return 1;
    }
    
    std::cout << "Joined multicast group " << multicastGroup.ipAddress << std::endl;
    
    // Start receive thread
    std::thread receiveThread([&receiver]() {
        std::vector<std::byte> buffer;
        NetworkAddress sender;
        
        if (receiver->ReceiveFrom(buffer, sender) > 0) {
            std::string message = NetworkUtils::BytesToString(buffer);
            std::cout << "Received: " << message << std::endl;
        }
    });
    
    // Send multicast message
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::byte> data = NetworkUtils::StringToBytes(message);
    
    std::cout << "Sending message..." << std::endl;
    if (sender->SendTo(data, multicastGroup) > 0) {
        std::cout << "Message sent successfully" << std::endl;
    } else {
        std::cout << "Failed to send message" << std::endl;
    }
    
    receiveThread.join();
    receiver->LeaveMulticastGroup(multicastGroup);
    
    return 0;
}
