#ifndef NETWORK_H
#define NETWORK_H

#include <vector>
#include <string>
#include <memory>
#include <cstddef> // For std::byte

// Structure to hold network address information (IP and port)
struct NetworkAddress {
    std::string ipAddress;
    unsigned short port;
    
    // Constructor for easy initialization
    NetworkAddress(const std::string& ip = "", const unsigned short p = 0)
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
    virtual bool WaitForDataWithTimeout(int timeoutMs) = 0;
    
    // Socket options
    virtual void SetReuseAddr(bool enable) = 0; // Enables/disables SO_REUSEADDR socket option
};

// Interface for connection-oriented sockets (like TCP)
class IConnectionOrientedSocket : public ISocketBase {
public:
    // Operations specific to connection-oriented sockets
    virtual bool Connect(const NetworkAddress& remoteAddress) = 0;
    virtual int Send(const std::vector<std::byte>& data) = 0;
    virtual int Receive(std::vector<std::byte>& buffer) = 0;
    virtual NetworkAddress GetRemoteAddress() const = 0;
    virtual bool SetConnectTimeout(int timeoutMs) = 0;
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
    virtual int SendTo(const std::vector<std::byte>& data, const NetworkAddress& remoteAddress) = 0;
    virtual int ReceiveFrom(std::vector<std::byte>& buffer, NetworkAddress& remoteAddress) = 0;
};

#endif // NETWORK_H