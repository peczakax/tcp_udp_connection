#ifndef NETWORK_H
#define NETWORK_H

#include <vector>
#include <string>
#include <memory>

// Structure to hold network address information (IP and port)
struct NetworkAddress {
    std::string ipAddress;
    unsigned short port;
    
    // Constructor for easy initialization
    NetworkAddress(const std::string& ip = "", unsigned short p = 0)
        : ipAddress(ip), port(p) {}
};

// Base interface for all socket types with common functionality
class ISocketBase {
public:
    virtual ~ISocketBase() = default;
    
    // Common operations for all socket types
    virtual void Close() = 0;
    virtual bool Bind(const NetworkAddress& localAddress) = 0;
    virtual NetworkAddress GetLocalAddress() const = 0;
    virtual bool IsValid() const = 0;
};

// Interface for connection-oriented sockets (like TCP)
class IConnectionOrientedSocket : public ISocketBase {
public:
    // Operations specific to connection-oriented sockets
    virtual bool Connect(const NetworkAddress& remoteAddress) = 0;
    virtual int Send(const std::vector<char>& data) = 0;
    virtual int Receive(std::vector<char>& buffer, int maxSize = -1) = 0;
    virtual NetworkAddress GetRemoteAddress() const = 0;
};

// Interface for server-side of connection-oriented sockets
class IConnectionListener : public ISocketBase {
public:
    // Server operations for connection-oriented protocols
    virtual bool Listen(int backlog) = 0;
    virtual std::unique_ptr<IConnectionOrientedSocket> Accept() = 0;
};

// Interface for connectionless sockets (like UDP)
class IConnectionlessSocket : public ISocketBase {
public:
    // Operations specific to connectionless sockets
    virtual int SendTo(const std::vector<char>& data, const NetworkAddress& remoteAddress) = 0;
    virtual int ReceiveFrom(std::vector<char>& buffer, NetworkAddress& remoteAddress, int maxSize = -1) = 0;
};

#endif // NETWORK_H