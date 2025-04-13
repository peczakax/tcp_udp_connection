#include "network_lib/platform_factory.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

int main() {
    std::cout << "Running UDP multicast example..." << std::endl;

    auto factory = INetworkSocketFactory::CreatePlatformFactory();
    
    // Create a multicast sender
    auto sender = factory->CreateUdpSocket();
    
    // Create a multicast receiver
    auto receiver = factory->CreateUdpSocket();
    
    // Multicast address (must be in the range 224.0.0.0 to 239.255.255.255)
    NetworkAddress multicastGroup("239.255.1.1", 8083);
    
    // Set up the receiver
    if (receiver->Bind(NetworkAddress("0.0.0.0", 8083))) {
        // Join the multicast group
        if (receiver->JoinMulticastGroup(multicastGroup)) {
            std::cout << "Joined multicast group " << multicastGroup.ipAddress << std::endl;
            
            // Start a thread to receive multicast messages
            std::thread receiveThread([&receiver]() {
                std::cout << "Waiting for multicast messages..." << std::endl;
                
                std::vector<char> buffer;
                NetworkAddress sender;
                
                int bytesRead = receiver->ReceiveFrom(buffer, sender);
                
                if (bytesRead > 0) {
                    std::string message(buffer.begin(), buffer.end());
                    std::cout << "Received multicast: " << message << std::endl;
                }
            });
            
            // Send a multicast message
            std::this_thread::sleep_for(std::chrono::seconds(1)); // Give receiver time to start
            
            std::string message = "Hello, multicast group!";
            std::vector<char> data(message.begin(), message.end());
            
            std::cout << "Sending multicast message..." << std::endl;
            int bytesSent = sender->SendTo(data, multicastGroup);
            
            if (bytesSent > 0) {
                std::cout << "Sent " << bytesSent << " bytes to multicast group" << std::endl;
            } else {
                std::cout << "Failed to send multicast message" << std::endl;
            }
            
            // Wait for receive thread to complete
            receiveThread.join();
            
            // Leave the multicast group
            receiver->LeaveMulticastGroup(multicastGroup);
        } else {
            std::cout << "Failed to join multicast group" << std::endl;
        }
    } else {
        std::cout << "Failed to bind multicast receiver" << std::endl;
    }
    
    return 0;
}