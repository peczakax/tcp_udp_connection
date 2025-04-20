#ifndef PLATFORM_FACTORY_H
#define PLATFORM_FACTORY_H

#include "tcp_socket.h"
#include "udp_socket.h"
#include <memory>

// Abstract factory interface for creating platform-specific socket implementations
class INetworkSocketFactory {
public:
    virtual ~INetworkSocketFactory() = default;
    
    // Create TCP socket implementations
    virtual std::unique_ptr<ITcpSocket> CreateTcpSocket() = 0;
    virtual std::unique_ptr<ITcpListener> CreateTcpListener() = 0;
    
    // Create UDP socket implementation
    virtual std::unique_ptr<IUdpSocket> CreateUdpSocket() = 0;
    
    // Static method to create the appropriate platform factory
    static std::unique_ptr<INetworkSocketFactory> CreatePlatformFactory();
};

// Factory singleton for creating network sockets
#include <mutex>

class NetworkFactorySingleton {
public:
    static INetworkSocketFactory & GetInstance() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!instance) {
            factory = INetworkSocketFactory::CreatePlatformFactory();
            instance = factory.get();
        }
        return *instance;
    }

private:
    static std::unique_ptr<INetworkSocketFactory> factory;
    static INetworkSocketFactory* instance;
    static std::mutex mutex_;
};

#endif // PLATFORM_FACTORY_H