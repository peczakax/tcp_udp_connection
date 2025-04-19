#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include "network.h"

// TCP client socket interface
class ITcpSocket : public IConnectionOrientedSocket {
public:
    // Example of a TCP-specific option
    virtual bool SetNoDelay(bool enable) = 0;
};

// TCP server socket interface
class ITcpListener : public IConnectionListener {
public:
    // Match the base class's return type
    virtual std::unique_ptr<IConnectionOrientedSocket> Accept() override = 0;
    
    // Helper method to get a TCP-specific socket
    virtual std::unique_ptr<ITcpSocket> AcceptTcp() {
        return std::unique_ptr<ITcpSocket>(
            dynamic_cast<ITcpSocket*>(Accept().release())
        );
    }
};

// Factory for creating TCP sockets
class ITcpSocketFactory {
public:
    virtual ~ITcpSocketFactory() = default;
    virtual std::unique_ptr<ITcpSocket> CreateTcpSocket() = 0;
    virtual std::unique_ptr<ITcpListener> CreateTcpListener() = 0;
};

#endif // TCP_SOCKET_H